# 反馈

用户在程序里提交反馈：说了什么（必填）、程序的日志（默认带上，发送前能看全部内容，也能去掉）、版本和系统（自动带上），
联系方式（登录了是账号的邮箱，没登录可以自己填，也可以不填）。作者在管理后台里看（[admin.md](admin.md#用户反馈)），
90 天后自动删除（隐私政策里这么说的）。

## 服务端

`POST /feedback`（`Server/src/server.ts`，存储在 `Server/src/feedback.ts`）。长连接的消息最大 64 KB 装不下日志，所以走 HTTP；
Nginx 那里这个地址的请求体最大 8 MB。

- 不用登录：设备 ID 和设备令牌对得上（`Registry.owns`）就收。设备的名字、平台从登记表里拿；登录了账号的，联系方式换成账号的邮箱
- 请求体（JSON）：`deviceId`、`token`、`version`（≤ 32 字）、`system`（≤ 100 字）、`contact`（≤ 100 字，可以是空的）、
  `text`（1 ~ 5000 字，两头的空白去掉）、`logs`（最多 12 份 `{ name, text }`，不压缩时加起来 ≤ 6 MB，可以是空的）
- 回答：200 `{ ok: true }`；400 `invalid`（缺了、太长、日志太大）；401 `device`（令牌不对、没登记）；
  429 `tooMany`（同一台设备一天 5 条、同一个地址一天 20 条）
- 存在 MongoDB 的 `feedback` 里，日志接成一份文本、gzip 压缩以后放在同一条记录里；`createdAt` 上的 TTL 索引到 90 天删掉
- 管理后台设了通知邮箱时发一封通知邮件（`mail.ts` 的 `feedbackMail`，一天最多 20 封）；没发出去也照样收下，记一次发信失败

## 程序

设置 › 反馈问题（`App/qml/shared/FeedbackSettings.qml`，点进去的日志列表 `FeedbackLogs.qml`、一份日志 `FeedbackLog.qml`；电脑、平板、手机共用，手机上一页一页推进来；QML 单例 `Feedback`，`App/src/feedback/`）。

- 说明必填（最多 5000 字）；没登录时可以留邮箱、微信或 QQ，登录着时服务器用账号的邮箱
- "附带日志"默认开着，"查看日志"里能一份一份看全部内容，提交的就是看到的这些。版本和系统写在下面，一起交上去
- 日志每份只带最后 512 KB（`rc/feedback_logs.h`）：
  - 界面程序读它自己的：数据目录的 `app.log`、`app.log.previous`；macOS 装好时还有登录的人的助手 `~/Library/Logs/Bilin/agent.log`（和 `.previous`）
  - 后台服务读它的（装好时只有 SYSTEM / root 读得了）：`service.log`、`service.log.previous`；装好时还有 Windows 系统服务的看护者
    `system-service.log`、macOS 登录界面上的助手 `agent-loginwindow.log`（和 `.previous`）。手机上后台服务在程序的进程里，
    日志都在 `app.log`
- 界面和后台服务之间一条消息最多 1 MB：日志经 `qCompress` 压缩、base64 以后再传（`feedbackLogs`、`sendFeedback`，
  `rc/protocol.h`），回答只发给要的那个界面。后台服务解开（先看压缩数据里写的大小，不超过 512 KB 才解），
  带上设备 ID、令牌、版本、系统交给服务器（`Service/src/feedback_sender.h`）；界面程序拿不到令牌
- 没交上时说原因：今天提交得太多了（429）、连不上服务器、别的错

在 Mac 上用手机的界面试：手机预览版（[mobile.md](mobile.md)）配置时加 `-DRC_SERVER_URL=ws://localhost:8800/ws` 连开发服务器，
运行时用单独的数据目录（`RC_STATE_DIR`），免得和电脑开发版用同一个设备身份、互相挤下线。

`Service/tests/tst_feedback_sender` 里有一项连真的服务器（先登记、再提交）：

```sh
RC_FEEDBACK_SERVER=ws://localhost:8800/ws build/debug/Service/tests/tst_feedback_sender
```
