#!/usr/bin/env python3
"""跨电脑：Mac 上隔离的后台服务当被连的电脑，Windows 上的 rcconnect 去连（身份存在 Windows 上，DPAPI 加密）。
测 Windows 去连 Mac：我的设备免密码、别人要点允许、不认识的设备被拒、发文件。只用 Windows 上的一个目录，测完删掉。

    E2E_WINDOWS=me@192.168.1.20 E2E_WINDOWS_DIR='C:\\Users\\me\\Desktop\\Bilin' \\
        E2E_WINDOWS_QT='F:\\Qt\\6.11.2\\msvc2022_64\\bin' scripts/e2e/windows.py
    E2E_WINDOWS_VM='Windows 11' E2E_WINDOWS_DIR='C:\\src' E2E_WINDOWS_BUILD='bilin-build' \\
        E2E_WINDOWS_QT='C:\\Qt\\6.11.2\\msvc2022_arm64\\bin' scripts/e2e/windows.py

Windows 经 SSH（E2E_WINDOWS），或者是这台 Mac 上的 Parallels 虚拟机（E2E_WINDOWS_VM，prlctl exec）。
E2E_WINDOWS_DIR 下的构建目录（E2E_WINDOWS_BUILD，默认 build）里要有编好的 rcconnect.exe，E2E_WINDOWS_QT 是 Qt 的 DLL 目录
（不在 PATH 里它启动不了）。开发服务器要 Windows 也连得到（编译时的 RC_SERVER_URL 是 Mac 的局域网地址，虚拟机里是 Mac 在
虚拟机网络里的地址）。
"""
import base64
import hashlib
import os
import subprocess
import sys
import time

from common import EMAIL, PASSWORD, WORK, Sharer, check, finish, sha256

HOST = os.environ.get("E2E_WINDOWS")
VM = os.environ.get("E2E_WINDOWS_VM")
DIRECTORY = os.environ["E2E_WINDOWS_DIR"]
BUILD = os.environ.get("E2E_WINDOWS_BUILD", "build")
QT = os.environ["E2E_WINDOWS_QT"]
STATE = DIRECTORY + "\\e2e-state"
SIZE = 30 << 20


def windows(command, timeout=180, on_poll=None):
    """在 Windows 上跑一条命令，返回 (退出码, 输出)"""
    remote = ["ssh", "-o", "ConnectTimeout=10", HOST, command] if HOST else ["prlctl", "exec", VM, "cmd", "/c", command]
    process = subprocess.Popen(remote, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    end = time.time() + timeout
    while process.poll() is None and time.time() < end:
        if on_poll:
            on_poll()
        time.sleep(0.2)
    if process.poll() is None:
        process.kill()
    return process.returncode, process.communicate()[0].decode("utf-8", errors="replace")


def viewer(args, sharer, name, on_poll=None):
    # 先 cd：cmd 会吃掉"以引号开头、里面又有多个引号"的命令行的引号
    command = f'cd /d "{DIRECTORY}" && set PATH={QT};%PATH% && {BUILD}\\rcconnect.exe --state "{STATE}\\{name}" --name {name} --hold 3 ' + args
    code, out = windows(command, on_poll=on_poll)
    print("      | " + out.strip().replace("\n", "\n      | "), flush=True)
    return code, out


def main():
    windows(f'rmdir /s /q "{STATE}" 2>nul')
    with Sharer("e2e-Mac") as sharer:
        client = sharer.client
        sharer_id = sharer.device["deviceId"]
        public_key = base64.b64decode(sharer.device["deviceKey"])[:65]
        sharer.register()
        account = f"--email {EMAIL} --password {PASSWORD}"

        # 1. Windows → Mac：我的设备，不要密码
        code, out = viewer(f"{account} --mine {sharer_id}", sharer, "win-a")
        if not (code == 0 and "已连上" in out):  # 刚登录账号时被连的一方也许还没收到账号下的新设备
            code, out = viewer(f"{account} --mine {sharer_id}", sharer, "win-a")
        check("Windows 去连 Mac：我的设备，按设备密钥连上，没问密码、没等点允许",
              code == 0 and "已连上" in out and "点允许" not in out, out[-300:])
        check("Windows 去连 Mac：收到了 Mac 的画面", "帧画面" in out and not out.strip().endswith("0 帧画面，连接正常"), out[-200:])
        check("Windows 去连 Mac：能控制", "能控制对方" in out, out[-200:])

        # 2. 不认识的设备（没登录这个账号）：被拒
        code, out = viewer(f"--key {base64.b64encode(public_key).decode()} {sharer_id}", sharer, "win-stranger")
        check("Windows 上不认识的设备：被拒", code != 0 and "refused" in out, out[-300:])

        # 3. 别人：临时密码，Mac 上的人点允许
        pending = {}

        def admit_when_asked():
            requests = client.share().get("requests", [])
            if requests and not pending.get("done"):
                pending["done"] = True
                client.send("admit", id=requests[0]["id"], control=False)

        client.pump()
        code, out = viewer(f"{sharer_id} {client.topics['device']['password']}", sharer, "win-others", on_poll=admit_when_asked)
        check("Windows 去连 Mac：别人，临时密码对、Mac 上点了允许以后连上", code == 0 and "点允许" in out and "已连上" in out, out[-300:])

        # 4. Windows 发文件给 Mac（Windows 读、Mac 写），内容对
        receive = f"{WORK}/received"
        os.makedirs(receive, exist_ok=True)
        windows(f'fsutil file createnew "{STATE}\\payload.bin" {SIZE} >nul')
        accepted = set()

        def accept_offers():
            for t in client.transfers():
                if t["status"] == "offered" and t["id"] not in accepted:
                    accepted.add(t["id"])
                    client.send("acceptTransfer", id=t["id"], directory=receive)

        code, out = viewer(f'{account} --mine --send "{STATE}\\payload.bin" {sharer_id}', sharer, "win-a", on_poll=accept_offers)
        landed = [f for f in os.listdir(receive) if f.startswith("payload") and not f.endswith(".rcsdk-part")]
        check("Windows 发文件给 Mac：发完了", code == 0 and "发完了" in out, out[-300:])
        expected = hashlib.sha256(bytes(SIZE)).hexdigest()
        check("Windows 发文件给 Mac：Mac 上收到的和发的一样（大小、sha256）",
              len(landed) == 1 and os.path.getsize(f"{receive}/{landed[0]}") == SIZE and sha256(f"{receive}/{landed[0]}") == expected,
              str(landed))
        # 5. Windows 上身份是加密存的
        _, text = windows(f'type "{STATE}\\win-a\\identity.json"')
        check("Windows 上的设备令牌和私钥是加密存的（identity.json 里没有明文的 token、deviceKey）",
              '"secrets"' in text and '"token"' not in text and '"deviceKey"' not in text, text[:200])

        sharer.delete_account()
    windows(f'rmdir /s /q "{STATE}" 2>nul')
    return finish()


if __name__ == "__main__":
    sys.exit(main())
