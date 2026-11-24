# 连接统计

去连别的设备的一方（观看端）每次连接结束后发一条记录给服务器，作者在管理后台里看（[admin.md](admin.md#连接)）：
连上没有、为什么没连上、多久连上、连了多久、走的哪条路、什么系统连什么系统。用来找连不上的原因、看大家怎么用。

- 默认打开，设置 › 通用 › "发送连接统计"能关（电脑、手机都有）。关掉以后不再发，后台服务里还没发出去的也丢掉
- 隐私政策里写了收什么、不收什么、90 天删除（`App/legal/privacy.md`）
- 不记画面、声音、输入、对话、剪贴板、文件，也不记网络地址；按局域网 IP 地址连的，连对方的设备 ID 都没有

## 一条记录

`Server/src/protocol.ts` 的 `ConnectionReport`，客户端是 `Core/src/rc/connection_report.h`（两边的格式一样）：

| 字段 | 说明 |
| --- | --- |
| `target` | 对方的设备 ID；按局域网地址连的是空的 |
| `access` | `device`（我的设备）、`password`（临时密码） |
| `result` | `connected`（连上过）、`failed`（没连上，有原因）、`cancelled`（还没连上用户就关了） |
| `reason` | 告诉用户的原因（≤ 200 字）：没连上的、连上以后断开的（"对方关机了"之类）；用户自己断开的是空的 |
| `routes` | 依次试过的路：`direct`、`relay`、`relayTls`（≤ 8 个，连着重复的只记一次） |
| `path` | 连上以后走的路：`lan`、`direct`、`relay`；没连上是空的 |
| `connectMs` | 从开始连到第一次连上（毫秒）；没连上是 -1 |
| `durationS` | 连上以后连了多久（秒） |
| `reconnects` | 中途断了又重连的次数 |
| `approval` / `control` | 等对方点过"允许"、对方让本机控制过 |
| `codec` / `resolution` | 画面的编码格式、分辨率（最后一次的） |
| `fps` / `mbps` / `latencyMs` | 连着时每秒取一次的平均：帧率、码率、往返延迟 |

一次"连接"从点连接开始，到连上以后断开或者放弃为止：直连不通改走中继、中继改走 TLS、换临时密码再连，都算这一次。

## 客户端

- 界面的 `ViewerSession`（`App/src/session/viewer_session.cpp`）边连边记：`open()` 开始，状态变化时（等允许、连上、重连、
  没连上、对方重启/注销/关机/睡眠）记下，结束时 `protocol::ReportConnection` 交给后台服务；会话没结束就被销毁时按用户关掉算
- 后台服务（`Service/src/service_wiring.cpp` 的 `connectConnectionStats`）看设置（`DeviceState.connectionStats`，
  存在设备状态里），开着才经 `ServerLink::reportConnection` 发给服务器；没连上服务器时最多攒 20 条，连上（welcome）以后一起发
- 被连的一方不发：一次连接只有一条

## 服务器

- 长连接上的消息 `connectionReport`（字段同上）。格式不对的和别的消息一样直接断开，所以**先部署服务器、再发客户端**
- `Hub` 补上去连的一方的系统、版本（hello 里的）和被连的一方的（登记表里的），存进 MongoDB 的 `connections`
  （`Server/src/connections.ts`）；`createdAt` 上的 TTL 索引到 90 天删掉，按两边的设备 ID 各有一个索引
- 管理后台的接口：`GET /api/connections?deviceId=&result=`（新的在前，最多 300 条；设备 ID 去连、被连都算）、
  `GET /api/connections/summary?days=7|30`（`summarize`：按服务器的本地日期分天）
