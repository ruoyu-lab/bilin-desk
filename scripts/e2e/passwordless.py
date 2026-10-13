#!/usr/bin/env python3
"""我的设备之间不要密码、别人要临时密码和点允许（docs/devices-and-accounts.md）：

    scripts/e2e/passwordless.py                              连开发用的服务器
    E2E_BUILD=build/production scripts/e2e/passwordless.py   连正式的服务器（用安装包同一份构建）
"""
import base64
import sys
import time

from common import EMAIL, PASSWORD, Sharer, check, finish, frames, viewer


def main():
    with Sharer("e2e-被连的电脑") as sharer:
        client = sharer.client
        sharer_id = sharer.device["deviceId"]
        public_key = base64.b64decode(sharer.device["deviceKey"])[:65]
        sharer.register()
        client.wait(lambda: client.share().get("accepting"), 10)
        check("别人的分享会话在运行", client.share().get("accepting") is True, str(client.share()))

        # 账号：改昵称、改密码（要旧密码）
        client.send("setNickname", nickname="e2e-新昵称")
        check("改昵称", client.wait(lambda: client.topics["account"].get("nickname") == "e2e-新昵称", 10),
              str(client.topics.get("account")))
        client.events.clear()
        client.send("changePassword", oldPassword="不是这个密码", newPassword=PASSWORD + "x")
        check("改密码：旧密码不对被拒",
              client.wait(lambda: any(e["event"] == "accountFailed" and e["reason"] == "wrongPassword" for e in client.events), 10),
              str(client.events))

        # 1. 我的设备：同一个账号，不要密码
        account = ["--email", EMAIL, "--password", PASSWORD]
        code, out = viewer(account + ["--mine", sharer_id], "mine-a", sharer)
        if not (code == 0 and "已连上" in out):  # 刚登录账号时被连的一方也许还没收到账号下的新设备：再来一次
            print("      第一次没连上（刚登录账号），再试一次", flush=True)
            code, out = viewer(account + ["--mine", sharer_id], "mine-a", sharer)
        check("我的设备：按设备密钥连上，没有\"等对方点允许\"、没问密码",
              code == 0 and "已连上" in out and "点允许" not in out, out[-300:])
        check("我的设备：收到了画面", frames(out) > 0, out[-200:])
        check("我的设备：能控制", "能控制对方" in out, out[-200:])

        # 2. 没登录这个账号的设备拿着公钥来：被拒
        code, out = viewer(["--key", base64.b64encode(public_key).decode(), sharer_id], "stranger", sharer)
        check("不认识的设备（有公钥、不在账号里）：被拒", code != 0 and "已连上" not in out, out[-300:])
        check("不认识的设备：说的是被拒绝，不是别的错", "refused" in out, out[-300:])

        # 3. 别人：临时密码，本机的人点允许（后台服务连着界面，所以有人能点）
        pending = {}

        def admit_when_asked():
            requests = client.share().get("requests", [])
            if requests and not pending.get("done"):
                pending["done"] = True
                client.send("admit", id=requests[0]["id"], control=False)

        code, out = viewer([sharer_id, client.topics["device"]["password"]], "others-d", sharer, on_poll=admit_when_asked)
        check("别人：临时密码对、等点允许、点了以后连上", code == 0 and "点允许" in out and "已连上" in out, out[-300:])
        check("别人：只能看，不能控制（点允许时选的）", "能控制对方" not in out, out[-300:])

        # 4. 别人：错的临时密码
        code, out = viewer([sharer_id, "000000"], "others-w", sharer)
        check("别人：临时密码不对被拒", code != 0 and "已连上" not in out, out[-300:])
        # 被连的一方要等约 5 秒（SDK 的 connect_timeout_ms）才确认这次握手失败、记下来：别让后面的连接和它交错
        time.sleep(7)
        client.pump()  # 睡的时候后台服务发来的新临时密码、记录

        # 5. 别人：没有界面连着后台服务时，没人能点允许，直接拒绝（这时没有人连着，不会拉起界面程序）
        client.pump()
        password = client.topics["device"]["password"]
        client.close()
        code, out = viewer([sharer_id, password], "others-e", sharer)
        check("别人：没有界面时被拒（没人能点允许）", code != 0 and "已连上" not in out and "unattended" in out, out[-300:])
        client.connect()
        client.wait(lambda: "device" in client.topics)

        # 6. 关掉"允许别人连接本机"：别人连不上，我的设备照样连
        client.send("setAcceptIncoming", value=False)
        client.wait(lambda: client.topics["device"]["acceptIncoming"] is False)
        client.pump()
        code, out = viewer([sharer_id, client.topics["device"]["password"]], "others-g", sharer)
        check("关着\"允许别人连接本机\"：别人被回绝", code != 0 and "回绝" in out, out[-300:])
        code, out = viewer(account + ["--mine", sharer_id], "mine-a", sharer)
        check("关着\"允许别人连接本机\"：我的设备照样连上", code == 0 and "已连上" in out, out[-300:])
        client.send("setAcceptIncoming", value=True)

        # 6b. 关掉"界面程序没开着时也接受我的设备连接"（装好了系统服务时是设置里的"开机时自动启动"）：
        # 界面连着时照样连，没有界面时连不上
        client.send("setUnattended", value=False)
        client.wait(lambda: client.topics["device"]["unattended"] is False)
        code, out = viewer(account + ["--mine", sharer_id], "mine-a", sharer)
        check("关着无人值守：界面连着时我的设备照样连上", code == 0 and "已连上" in out, out[-300:])
        # 连着的人都走了以后才断开界面：不然后台服务以为界面没了，把界面程序拉起来
        client.wait(lambda: not client.share().get("viewers"), 15)
        client.close()
        time.sleep(1)
        code, out = viewer(account + ["--mine", sharer_id], "mine-a", sharer, timeout=60)
        check("关着无人值守：没有界面时我的设备连不上", code != 0 and "已连上" not in out, out[-300:])
        client.connect()
        client.wait(lambda: "device" in client.topics)
        client.send("setUnattended", value=True)

        # 7. 账号里的设备被移出以后，不能再免密码连
        devices = [d for d in client.topics.get("account", {}).get("devices", []) if d["deviceId"] != sharer_id]
        check("账号下有去连的那台设备", len(devices) >= 1, str(client.topics.get("account")))
        for d in devices:
            client.send("removeDevice", deviceId=d["deviceId"])
        client.wait(lambda: len(client.topics["account"].get("devices", [])) == 1, 10)
        code, out = viewer(["--key", base64.b64encode(public_key).decode(), sharer_id], "mine-a", sharer)
        check("被移出账号的设备：不能再免密码连", code != 0 and "已连上" not in out, out[-300:])

        # 8. 本机的连接记录（铃铛里的通知）：连过的、没成的、输错密码的
        client.wait(lambda: any(e["kind"] == "wrongPin" for e in client.topics.get("history", {}).get("entries", [])), 10)
        history = client.topics.get("history", {}).get("entries", [])

        def entries(kind, name=None):
            return [e for e in history if e["kind"] == kind and (name is None or e["name"] == name)]

        check("连接记录：我的设备连进来过，能控制", any(e["control"] for e in entries("connection", "mine-a")), str(history))
        check("连接记录：别人点了允许连进来过，只能看",
              any(not e["control"] for e in entries("connection", "others-d")), str(history))
        check("连接记录：连进来的人走了以后记着结束的时间",
              all(e["endedAt"] > 0 for e in entries("connection")) and bool(entries("connection")), str(history))
        check("连接记录：没人能点允许的请求记着", any(e["reason"] == "unattended" for e in entries("refused")), str(history))
        check("连接记录：输错临时密码记着", bool(entries("wrongPin")), str(history))

        sharer.delete_account()
    return finish()


if __name__ == "__main__":
    sys.exit(main())
