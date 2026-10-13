"""端到端测试的公共部分（docs/testing.md）。

一个隔离的后台服务（环境变量 RC_STATE_DIR：自己的数据目录、自己的本机连接名，不碰正在用的那份）当被连的电脑，
命令行的 rcconnect 当去连的设备，连 E2E_BUILD 那份构建配置的服务器（build/debug：开发用的虚拟机；
build/production：正式的服务器）。测试用的账号测完注销，开的进程测完关掉，数据目录测完删掉。

注册要邮箱验证码：开发用的虚拟机不真的发邮件，验证码从服务端的日志里读。正式的服务器会真的发到邮箱，
所以先用自己的邮箱注册一个测试账号，经 E2E_EMAIL、E2E_PASSWORD 给出：测试登录它，测完只退出、不注销。
"""
import hashlib
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BUILD = os.path.join(ROOT, os.environ.get("E2E_BUILD", "build/debug"))
SERVICE = f"{BUILD}/App/Bilin.app/Contents/Helpers/BilinService"
RCCONNECT = f"{BUILD}/tools/rcconnect/rcconnect"
WORK = os.path.join(tempfile.gettempdir(), "rc-e2e")
EMAIL = os.environ.get("E2E_EMAIL") or f"e2e-{int(time.time())}@example.com"
PASSWORD = os.environ.get("E2E_PASSWORD") or "Test-pass-12345"
OWN_ACCOUNT = "E2E_EMAIL" in os.environ
DEV_MACHINE = os.environ.get("RC_MACHINE", "bilin-dev")
results = []
screen_unavailable = False  # 测试时被测的电脑没有可采集的屏幕（显示器关了、锁屏了）：连得上也没有画面，结果不算


def check(name, ok, detail=""):
    results.append((name, ok, detail))
    print(("通过  " if ok else "失败  ") + name + (f"    [{detail}]" if detail and not ok else ""), flush=True)


def dev_code(email, timeout=10):
    """开发用的虚拟机发给 email 的验证码：在服务端的日志里（Server/src/mail.ts 的 LogMailer）"""
    pattern = re.compile(r'"to":"%s".*?验证码是 (\d{6})' % re.escape(email))
    end = time.time() + timeout
    while time.time() < end:
        logs = subprocess.run(["orb", "-m", DEV_MACHINE, "bash", "-c", "pm2 logs bilin-server --nostream --raw --lines 400"],
                              capture_output=True, text=True).stdout
        codes = pattern.findall(logs)
        if codes:
            return codes[-1]
        time.sleep(0.5)
    return None


def finish():
    """打印结果，返回进程退出码：0 全过，1 有失败，2 屏幕采集不了、这次结果不算"""
    failed = [r for r in results if not r[1]]
    print(f"\n共 {len(results)} 项，失败 {len(failed)} 项")
    if screen_unavailable:
        print("注意：测试时这台电脑没有可采集的屏幕（显示器关了或者锁屏了），连得上也没有画面。"
              "这次的结果不算，等屏幕亮着、没锁屏时再测。")
        return 2
    return 1 if failed else 0


class Client:
    """后台服务的本机连接：一行一个 JSON（topic 是状态，event 是一次性通知）。连着它就是"有界面连着"，
    被连的一方才有人能点允许；断开了、而又有人连着本机时，后台服务会把界面程序拉起来（在这台电脑上弹出刘海）"""

    def __init__(self, state):
        name = "com.bilindesk.bilin.service-" + hashlib.sha1(state.encode()).hexdigest()[:12]
        self.path = os.environ.get("TMPDIR", "/tmp").rstrip("/") + "/" + name
        self.sock = None
        self.buffer = b""
        self.topics = {}
        self.events = []

    def connect(self, timeout=20):
        end = time.time() + timeout
        while time.time() < end:
            try:
                self.sock = socket.socket(socket.AF_UNIX)
                self.sock.connect(self.path)
                self.sock.setblocking(False)
                return True
            except OSError:
                time.sleep(0.2)
        return False

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def pump(self):
        try:
            data = self.sock.recv(1 << 20)
            if data:
                self.buffer += data
        except BlockingIOError:
            pass
        while b"\n" in self.buffer:
            line, self.buffer = self.buffer.split(b"\n", 1)
            message = json.loads(line)
            if "topic" in message:
                self.topics[message["topic"]] = message["value"]
            else:
                self.events.append(message)

    def send(self, command, **arguments):
        self.sock.sendall((json.dumps({"command": command, **arguments}) + "\n").encode())

    def wait(self, predicate, timeout=20):
        end = time.time() + timeout
        while time.time() < end:
            self.pump()
            if predicate():
                return True
            time.sleep(0.1)
        return False

    def share(self):
        self.pump()
        return self.topics.get("share", {})

    def transfers(self):
        self.pump()
        return self.topics.get("files", {}).get("transfers", [])


class Sharer:
    """隔离的后台服务（被连的电脑）和它的本机连接。with 里用，出来时按顺序收拾干净"""

    def __init__(self, device_name):
        self.state = f"{WORK}/sharer"
        self.device_name = device_name
        self.processes = []  # 测试里开的 rcconnect：先于后台服务结束
        self.client = None
        self.service = None

    def __enter__(self):
        shutil.rmtree(WORK, ignore_errors=True)
        os.makedirs(self.state)
        with open(f"{self.state}/settings.json", "w") as settings:
            # 不在局域网里宣布自己：不打扰这个网络里别的设备
            json.dump({"acceptIncoming": True, "lanDiscovery": False, "deviceName": self.device_name}, settings)
        self.log = open(f"{WORK}/service.out", "w")
        self.service = subprocess.Popen([SERVICE], env=dict(os.environ, RC_STATE_DIR=self.state), stdout=self.log,
                                        stderr=self.log)
        self.client = Client(self.state)
        check("隔离的后台服务起来了、本机连接连得上", self.client.connect())
        self.client.wait(lambda: "device" in self.client.topics and "share" in self.client.topics)
        self.device = self.client.topics["device"]
        print(f"      被连的电脑 {self.device['deviceId']}", flush=True)
        return self

    def request(self, command, done, **arguments):
        """发一个账号的命令，等到 done() 或者 accountFailed。刚启动时还没登录上服务器（连正式的服务器要几秒），
        后台服务回"没连上服务器"：等一会儿再来"""
        end = time.time() + 30
        while time.time() < end:
            self.client.events.clear()
            self.client.send(command, **arguments)
            self.client.wait(lambda: done() or any(e["event"] == "accountFailed" for e in self.client.events), 5)
            if not any(e["event"] == "accountFailed" and e["reason"] == "noServer" for e in self.client.events):
                return
            time.sleep(1)

    def signed_in(self):
        return self.client.topics.get("account", {}).get("signedIn") is True

    def register(self):
        if OWN_ACCOUNT:
            self.request("signIn", self.signed_in, email=EMAIL, password=PASSWORD)
            check("登录测试账号", self.signed_in(), str(self.client.events))
            return
        self.request("sendCode", lambda: any(e["event"] == "codeSent" for e in self.client.events),
                     email=EMAIL, purpose="register")
        code = dev_code(EMAIL)
        check("收到注册的验证码", code is not None, str(self.client.events))
        self.request("registerAccount", self.signed_in, email=EMAIL, password=PASSWORD, nickname="e2e", code=code or "")
        check("注册测试账号", self.signed_in(), str(self.client.events))

    def delete_account(self):
        if OWN_ACCOUNT:
            self.client.send("signOut")
            check("退出测试账号", self.client.wait(lambda: self.client.topics.get("account", {}).get("signedIn") is False, 10),
                  str(self.client.topics.get("account")))
            return
        self.client.send("deleteAccount", password=PASSWORD)
        check("注销测试账号", self.client.wait(lambda: self.client.topics.get("account", {}).get("signedIn") is False, 10),
              str(self.client.topics.get("account")))

    def __exit__(self, *exception):
        # 先关去连的，再关后台服务，最后才断开本机连接：连着的人还没发现走了的时候断开界面，
        # 后台服务会以为界面没了、把界面程序拉起来
        for process in self.processes:
            if process.poll() is None:
                process.kill()
        self.service.send_signal(signal.SIGTERM)
        try:
            self.service.wait(10)
        except subprocess.TimeoutExpired:
            self.service.kill()
        self.client.close()
        self.log.close()
        log = f"{self.state}/service.log"
        if os.path.exists(log):
            lines = open(log).read().splitlines()
            global screen_unavailable
            screen_unavailable = any("Screen capture failed" in l or "secure desktop (lock screen" in l for l in lines)
            # 警告全部打出来（出了问题时最先要看的），再加最后几行
            print("---- 被连的电脑的日志：警告 ----\n" + "\n".join(l[:240] for l in lines if " W " in l)
                  + "\n---- 最后几行 ----\n" + "\n".join(l[:240] for l in lines[-6:]), flush=True)
        leftover = subprocess.run(["pgrep", "-f", f"{BUILD}/(tools|App)"], capture_output=True, text=True).stdout.split()
        if leftover:
            print(f"注意：还有这次测试开出来的进程没结束：{' '.join(leftover)}", flush=True)
        shutil.rmtree(WORK, ignore_errors=True)


def viewer(args, state, sharer, timeout=120, on_poll=None, hold=3):
    """跑一次 rcconnect 到结束，返回 (退出码, 输出)"""
    command = [RCCONNECT, "--state", f"{WORK}/{state}", "--name", state, "--hold", str(hold), *args]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sharer.processes.append(process)
    end = time.time() + timeout
    while process.poll() is None and time.time() < end:
        if on_poll:
            on_poll()
        time.sleep(0.2)
    if process.poll() is None:
        process.kill()
    output = process.communicate()[0]
    print("      | " + output.strip().replace("\n", "\n      | "), flush=True)
    return process.returncode, output


def frames(output):
    found = re.search(r"收到 (\d+) 帧", output)
    return int(found.group(1)) if found else 0


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(8 << 20):
            digest.update(chunk)
    return digest.hexdigest()
