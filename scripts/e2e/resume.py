#!/usr/bin/env python3
"""互传文件的断点续传（docs/devices-and-accounts.md、Core/src/rc/interrupted_transfers.h）：发一个大文件，
传到一半把发送端 kill -9，等被连的一方发现断了（SDK 等对方回来一分钟），再启动发送端接着传。
核对：接着传了（不是从头）、存回原来的文件夹、最后的文件和原来一模一样、半截文件和记录都清掉了。

    scripts/e2e/resume.py
"""
import glob
import json
import os
import signal
import subprocess
import sys
import time

import common
from common import EMAIL, PASSWORD, RCCONNECT, WORK, Sharer, check, finish, sha256

SIZE = 300 << 20
KILL_AT = 40 << 20


def start_viewer(sharer, sharer_id, big):
    command = [RCCONNECT, "--state", f"{WORK}/sender", "--name", "sender", "--email", EMAIL, "--password", PASSWORD,
               "--mine", "--send", big, sharer_id]
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    sharer.processes.append(process)
    return process


def main():
    with Sharer("e2e-收文件的电脑") as sharer:
        client = sharer.client
        sharer_id = sharer.device["deviceId"]
        receive = f"{WORK}/received"
        os.makedirs(receive)
        big = f"{WORK}/big.bin"
        with open(big, "wb") as f:
            for _ in range(SIZE >> 20):
                f.write(os.urandom(1 << 20))
        original = sha256(big)
        sharer.register()

        accepted, offers = set(), []

        def accept_offers():
            for t in client.transfers():
                if t["status"] == "offered" and t["id"] not in accepted:
                    accepted.add(t["id"])
                    offers.append(dict(t))
                    # 接着上次传的存回上次的文件夹（界面程序里也是这样定的）
                    client.send("acceptTransfer", id=t["id"], directory=t["directory"] if t.get("resumed") else receive)

        # 第一次：传到一半 kill -9
        first = start_viewer(sharer, sharer_id, big)
        killed_at = 0
        end = time.time() + 120
        while time.time() < end and first.poll() is None:
            accept_offers()
            running = [t for t in client.transfers() if t["status"] == "running" and t["doneBytes"] > KILL_AT]
            if running:
                killed_at = running[0]["doneBytes"]
                first.send_signal(signal.SIGKILL)
                break
            time.sleep(0.05)
        first.kill()
        print("      | " + first.communicate()[0].strip().replace("\n", "\n      | "), flush=True)
        check("第一次：传到一半（收到约 %d MB）发送端被 kill -9" % (killed_at >> 20), killed_at > 0)
        if not killed_at:
            return finish()

        # 被连的一方发现断了：SDK 等对方回来一分钟，之后算这次传输没完成，记下来
        def failed():
            accept_offers()
            return any(t["status"] == "failed" for t in client.transfers())

        check("被连的一方发现断了、算没传完", client.wait(failed, 120), str(client.transfers()))
        broken = [t for t in client.transfers() if t["status"] == "failed"]
        check("没传完的标着\"下次接着收\"", bool(broken) and broken[0].get("resumable") is True, str(broken))
        parts = glob.glob(f"{receive}/*.rcsdk-part")
        records = glob.glob(f"{receive}/.rcsdk-resume-*")
        check("收到的一半留在文件夹里（半截文件和记录）", len(parts) == 1 and len(records) == 1, str((parts, records)))
        with open(f"{sharer.state}/share-transfers.json") as f:
            recorded = json.load(f)["transfers"]
        check("被连的一方记下了（share-transfers.json）", len(recorded) == 1 and not recorded[0]["sending"], str(recorded))

        # 第二次：同一个发送端接着发
        offers.clear()
        second = start_viewer(sharer, sharer_id, big)
        end = time.time() + 180
        while time.time() < end and second.poll() is None:
            accept_offers()
            time.sleep(0.1)
        second.kill()
        out = second.communicate()[0]
        print("      | " + out.strip().replace("\n", "\n      | "), flush=True)
        check("第二次：发送端说上次发过、接着发", "接着发" in out and "上次发了" in out, out[-400:])
        check("第二次：发完了", second.returncode == 0 and "发完了" in out, out[-300:])
        check("第二次：对方发来的是\"接着上次的\"，上次收了一部分",
              bool(offers) and offers[0].get("resumed") is True and offers[0]["doneBytes"] > 0, str(offers))
        check("第二次：存回上次的文件夹", bool(offers) and offers[0]["directory"] == receive, str(offers))
        landed = [p for p in glob.glob(f"{receive}/big*.bin") if not p.endswith(".rcsdk-part")]
        check("收到的文件在原来的文件夹里、只有一份", len(landed) == 1, str(landed))
        check("文件和原来一模一样（sha256）", bool(landed) and sha256(landed[0]) == original)
        check("半截文件和记录都清掉了",
              not glob.glob(f"{receive}/*.rcsdk-part") and not glob.glob(f"{receive}/.rcsdk-resume-*"), str(os.listdir(receive)))
        with open(f"{sharer.state}/share-transfers.json") as f:
            check("被连的一方不再记着这次传输", not json.load(f)["transfers"])
        with open(f"{WORK}/sender/viewer-transfers.json") as f:
            check("发送端也不再记着", not json.load(f)["transfers"])
        sharer.delete_account()
    return finish()


if __name__ == "__main__":
    sys.exit(main())
