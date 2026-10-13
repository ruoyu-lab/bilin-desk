// 命令行去连一台设备，打印连接的每一步（测试、排查用）。和界面程序去连别人是同一条路：
// 用自己的身份（--state 目录里，第一次运行时生成）登记到服务器，信令经服务器交给对方，SDK 的观看会话先直连过去
// （P2P，带服务器给的 STUN），打不通再经中继（服务器给的 TURN）重连一次。--relay：一开始就经中继（测中继用）。
//
//   rcconnect [--state 目录] [--name 名字] [--hold 秒] [--relay] 设备ID 临时密码
//   rcconnect [...] --email 邮箱 --password 密码 --mine 设备ID     登录账号，按"我的设备"连：只靠设备密钥，
//                                                                不要临时密码，对方的公钥从账号的设备列表里取
//   rcconnect [...] --key 对方的公钥(base64) 设备ID               同样只靠设备密钥，公钥直接给（测不认识的设备被拒）
//   rcconnect [...] --send 文件 ...                                连上以后把这个文件发给对方，发完（或者没发成）退出；
//                                                                上次没发完的（--state 里记着）接着发
//
// 一步一行打印（用到的后台服务代码自己的日志不打印，出了问题的除外）：本机是谁、登录服务器、服务器给没给中继、
// 直连还是经中继、对方有没有回答、等对方允许、没被允许 / 被回绝的原因、连上（经哪条路）、能不能控制、
// 收到多少帧画面。连上以后过 --hold 秒断开（默认 5 秒）。连上过返回 0，否则 1
#include "device_store.h"
#include "server_link.h"

#include <rc/device_id.h>
#include <rc/file_transfers.h>
#include <rc/interrupted_transfers.h>
#include <rc/transport.h>

#include <algorithm>

#include <rcsdk/file_transfer.h>
#include <rcsdk/rcsdk.hpp>
#include <rcsdk/tuning.hpp>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>

namespace {

// 等一次连接最多多久：对方没人点允许时 SDK 一分钟后拒绝
constexpr int GiveUpMs = 90 * 1000;
// 有中继可以改走时，直连的连通检查等多久（和界面程序一样）
constexpr uint32_t DirectCheckMs = 5000;

// 输出一律 UTF-8（脚本按 UTF-8 读）：不跟着系统的编码走，中文 Windows 上是 GBK
void say(const QString &text)
{
    std::printf("%s\n", text.toUtf8().constData());
    std::fflush(stdout);
}

// 后台服务代码（DeviceStore、ServerLink）的日志：一般的不打印，这里自己一步一步说；出了问题的照样打出来
void quietLibraries(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (type == QtDebugMsg || type == QtInfoMsg)
        return;
    std::fprintf(stderr, "注意：%s\n", message.toUtf8().constData());
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
#ifdef Q_OS_WIN
    // 在控制台里直接看时也显示得对
    SetConsoleOutputCP(CP_UTF8);
#endif
    qInstallMessageHandler(quietLibraries);
    // 会话用到的 SDK 模块要先注册（--send 用互传文件）
    rcsdk::registerModule(*rcs_file_transfer_module());
    QCoreApplication::setApplicationName(QStringLiteral("rcconnect"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("命令行去连一台设备，打印连接的每一步"));
    parser.addHelpOption();
    const QCommandLineOption stateOption(QStringLiteral("state"), QStringLiteral("身份存在这个目录"),
                                         QStringLiteral("目录"), QDir::temp().filePath(QStringLiteral("rcconnect")));
    const QCommandLineOption nameOption(QStringLiteral("name"), QStringLiteral("对方看到的名字"), QStringLiteral("名字"),
                                        QStringLiteral("命令行测试"));
    const QCommandLineOption holdOption(QStringLiteral("hold"), QStringLiteral("连上以后多少秒断开"), QStringLiteral("秒"),
                                        QStringLiteral("5"));
    const QCommandLineOption relayOption(QStringLiteral("relay"), QStringLiteral("一开始就经中继，不先直连"));
    const QCommandLineOption emailOption(QStringLiteral("email"), QStringLiteral("登录这个账号"), QStringLiteral("邮箱"));
    const QCommandLineOption passwordOption(QStringLiteral("password"), QStringLiteral("账号的密码"), QStringLiteral("密码"));
    const QCommandLineOption mineOption(QStringLiteral("mine"), QStringLiteral("按我的设备连：只靠设备密钥，不要临时密码"));
    const QCommandLineOption keyOption(QStringLiteral("key"), QStringLiteral("只靠设备密钥，对方的公钥（base64）直接给"),
                                       QStringLiteral("公钥"));
    const QCommandLineOption sendOption(QStringLiteral("send"), QStringLiteral("连上以后发这个文件（没发完的接着发）"),
                                        QStringLiteral("文件"));
    parser.addOptions({stateOption, nameOption, holdOption, relayOption, emailOption, passwordOption, mineOption, keyOption,
                       sendOption});
    parser.addPositionalArgument(QStringLiteral("设备ID"), QStringLiteral("要连的设备"));
    parser.addPositionalArgument(QStringLiteral("[临时密码]"), QStringLiteral("对方的临时密码（按设备密钥连时不要）"));
    parser.process(app);
    const QStringList arguments = parser.positionalArguments();
    const QString target = arguments.value(0).isEmpty() ? QString() : rc::DeviceId::parse(arguments.value(0)).digits();
    const bool byDeviceKey = parser.isSet(mineOption) || parser.isSet(keyOption);
    if (arguments.size() != (byDeviceKey ? 1 : 2) || target.isEmpty() || (parser.isSet(mineOption) && !parser.isSet(emailOption)))
        parser.showHelp(1);
    const QString pin = byDeviceKey ? QString() : arguments.at(1);
    const QString email = parser.value(emailOption);
    QByteArray peerKey = QByteArray::fromBase64(parser.value(keyOption).toLatin1());
    const int holdMs = parser.value(holdOption).toInt() * 1000;

    // 身份存在这里，下次还是同一台设备（不然每次都登记成一台新设备）
    const QString stateDirectory = parser.value(stateOption);
    if (!QDir().mkpath(stateDirectory)) {
        say(QStringLiteral("建不了存身份的目录 %1").arg(stateDirectory));
        return 1;
    }
    DeviceStore store(stateDirectory, [] {
        const rcsdk::DeviceKey key = rcsdk::generateDeviceKey();
        return QByteArray(reinterpret_cast<const char *>(key.data()), qsizetype(key.size()));
    });
    store.setDeviceName(parser.value(nameOption));
    say(QStringLiteral("1. 本机是 %1（身份存在 %2）")
            .arg(rc::DeviceId::parse(store.state().deviceId).display(), QDir::toNativeSeparators(stateDirectory)));
    say(QStringLiteral("2. 登录服务器 %1").arg(QStringLiteral(RC_SERVER_URL)));
    ServerLink link(store, QUrl(QStringLiteral(RC_SERVER_URL)), {});
    // 互传文件：没发完的记在 --state 里，下次接着发
    rc::InterruptedTransfers interrupted(stateDirectory + QStringLiteral("/viewer-transfers.json"));
    rc::FileTransfers files(interrupted);
    files.setPeer(0, target);
    const QString sendPath = parser.value(sendOption);
    bool sent = false; // 已经开始发了
    // 连上了、互传文件这个模块两边也确认了才能发（SDK 在连上之后才通知对方能不能用）
    bool running = false;
    bool moduleActive = false;
    bool relay = parser.isSet(relayOption);
    QString connection; // 这一次连接的编号（信令里的 session）：改走中继时换一个
    bool reached = false; // 这一次连通过（到了等允许那一步）
    bool connected = false;
    std::atomic<uint64_t> frames = 0;
    std::unique_ptr<rcsdk::Session> session;

    bool finished = false;
    const auto finish = [&] {
        // 收尾时 files.detach() 会再通知一次列表变了，那边又会来结束：只结束一次
        if (std::exchange(finished, true))
            return;
        files.detach();
        session.reset();
        say(connected ? QStringLiteral("一共收到 %1 帧画面，连接正常").arg(frames.load()) : QStringLiteral("没连上"));
        QCoreApplication::exit(connected ? 0 : 1);
    };
    const auto mode = [&] {
        return rc::ConnectionMode{relay ? rc::RelayTransport : rc::DirectTransport,
                                  byDeviceKey ? rc::DeviceAccess : rc::PasswordAccess};
    };

    // 发 --send 的文件：上次没发完的（同一个文件）接着发，否则新发
    const auto startSending = [&] {
        if (!running || !moduleActive || std::exchange(sent, true))
            return;
        const QString name = QFileInfo(sendPath).fileName();
        const QList<rc::Transfer> all = files.transfers();
        const auto earlier = std::find_if(all.cbegin(), all.cend(), [&](const rc::Transfer &t) {
            return t.sending && t.resumable && t.name == name;
        });
        if (earlier != all.cend()) {
            say(QStringLiteral("   上次发了 %1 / %2 字节，接着发").arg(earlier->doneBytes).arg(earlier->totalBytes));
            files.retry(earlier->id);
        } else {
            files.send(0, {QFileInfo(sendPath).absoluteFilePath()});
        }
    };
    qint64 lastSaid = -1;
    QObject::connect(&files, &rc::FileTransfers::changed, &app, [&] {
        for (const rc::Transfer &t : files.transfers()) {
            if (!t.sending || t.name != QFileInfo(sendPath).fileName())
                continue;
            if (t.finished() && t.resumable) {
                say(QStringLiteral("   没发完（%1 / %2 字节），记下了，下次接着发").arg(t.doneBytes).arg(t.totalBytes));
                finish();
            } else if (t.status == rc::transfer::Completed) {
                say(QStringLiteral("   发完了：%1 字节").arg(t.totalBytes));
                QTimer::singleShot(500, &app, finish);
            } else if (t.finished()) {
                say(QStringLiteral("   没发成（%1）：%2").arg(t.status, t.reason));
                finish();
            } else if (t.doneBytes / (16 << 20) != lastSaid) {
                lastSaid = t.doneBytes / (16 << 20);
                say(QStringLiteral("   发了 %1 / %2 字节").arg(t.doneBytes).arg(t.totalBytes));
            }
        }
    });

    // 开一次观看会话：直连带 STUN，中继带 TURN
    std::function<void()> open;
    const auto failed = [&] {
        const rcsdk::SessionFailure failure = session->failure();
        say(QStringLiteral("   连接结束（%1）：%2")
                .arg(QString::fromUtf8(rcs_result_message(failure.result)), QString::fromStdString(failure.message)));
        if (failure.result == RCS_ERROR_NETWORK && !relay && !reached && link.ice().turn) {
            say(QStringLiteral("   直连打不通，改走中继"));
            relay = true;
            open();
            return;
        }
        finish();
    };
    open = [&] {
        session.reset();
        connection = QUuid::createUuid().toString(QUuid::Id128);
        reached = false;
        const rc::IceServers ice = link.ice();
        rcsdk::SessionConfig config(RCS_ROLE_VIEW);
        config.setAcceleration(RCS_ACCELERATION_HARDWARE_FIRST);
        if (byDeviceKey) {
            const QByteArray own = store.deviceKey();
            rcsdk::DeviceKey ownKey{};
            std::copy(own.cbegin(), own.cend(), ownKey.begin());
            rcsdk::PublicKey trusted{};
            std::copy(peerKey.cbegin(), peerKey.cend(), trusted.begin());
            config.setSecret(RCS_SECRET_NONE).setDeviceKey(ownKey).setTrustedKeys({trusted});
        } else {
            config.setPin(pin.toStdString());
        }
        if (relay) {
            const rc::TurnServer &turn = *ice.turn;
            say(QStringLiteral("4. 经中继 %1（%2）连过去").arg(turn.server, turn.tcp ? QStringLiteral("TCP") : QStringLiteral("UDP")));
            config.setRelay(turn.server.toStdString(), turn.username.toStdString(), turn.password.toStdString(),
                            turn.tcp ? RCS_TURN_TCP : RCS_TURN_UDP);
        } else {
            say(QStringLiteral("4. 先试直连（打洞用的 STUN：%1）")
                    .arg(ice.stun.isEmpty() ? QStringLiteral("没有") : ice.stun.join(u", ")));
            config.setTransport(RCS_TRANSPORT_P2P).setStunServers(ice.stunServers());
            if (ice.turn)
                config.extension<rcs_network_tuning_config>().ice_connect_ms = DirectCheckMs;
        }

        if (!sendPath.isEmpty())
            rc::FileTransfers::configure(config);

        rcsdk::Callbacks callbacks;
        // SDK 的线程上：搬到主线程再处理。只处理这一次连接的：改走中继以后，上一次排着的不再算数
        const auto later = [&app, &connection, current = connection](auto &&work) {
            QMetaObject::invokeMethod(&app, [&connection, current, work = std::forward<decltype(work)>(work)] {
                if (current == connection)
                    work();
            }, Qt::QueuedConnection);
        };
        callbacks.onSignal = [&, later](const std::string &text, uint64_t) {
            later([&, text = QString::fromStdString(text)] { link.sendSignal(target, connection, text, mode()); });
        };
        callbacks.onGpuFrame = [&frames](const rcsdk::GpuFrame &) { ++frames; };
        callbacks.onEvent = [&, later](const rcs_event &event) {
            if (auto file = rc::FileTransfers::read(event)) {
                later([&, file = std::move(*file)] {
                    files.handle(file);
                    if (const auto *peer = std::get_if<rc::FileEvent::Peer>(&file.what); peer && peer->active) {
                        moduleActive = true;
                        if (!sendPath.isEmpty())
                            startSending();
                    }
                });
                return;
            }
            if (event.type == RCS_EVENT_STATE) {
                const rcs_state state = event.state.value;
                later([&, state] {
                    if (state == RCS_STATE_RUNNING && !connected) {
                        connected = reached = true;
                        if (sendPath.isEmpty()) {
                            say(QStringLiteral("6. 已连上，经 %1；%2 秒后断开")
                                    .arg(QString::fromUtf8(session->stats<rcs_network_stats>().path))
                                    .arg(holdMs / 1000));
                            QTimer::singleShot(holdMs, &app, finish);
                        } else {
                            say(QStringLiteral("6. 已连上，经 %1；发 %2")
                                    .arg(QString::fromUtf8(session->stats<rcs_network_stats>().path),
                                         QFileInfo(sendPath).fileName()));
                            running = true;
                            startSending();
                        }
                    } else if (state == RCS_STATE_RECONNECTING) {
                        say(QStringLiteral("   路断了，正在重连"));
                    } else if (state == RCS_STATE_FAILED) {
                        failed();
                    }
                });
            } else if (event.type == RCS_EVENT_ADMISSION) {
                const rcs_admission_state state = event.admission.state;
                const QString message = QString::fromUtf8(event.admission.message);
                const QString result = QString::fromUtf8(rcs_result_message(event.admission.result));
                later([&, state, message, result] {
                    reached = true;
                    if (state == RCS_ADMISSION_WAITING)
                        say(QStringLiteral("5. 密码对了，等对方的人点允许"));
                    else if (state == RCS_ADMISSION_ADMITTED)
                        say(QStringLiteral("   对方允许了"));
                    else if (state == RCS_ADMISSION_REFUSED)
                        say(QStringLiteral("   没被允许（%1）：%2").arg(result, message));
                });
            } else if (event.type == RCS_EVENT_CONTROL) {
                const bool granted = event.control.granted != 0;
                later([granted] { say(granted ? QStringLiteral("   能控制对方") : QStringLiteral("   只能看，不能控制")); });
            }
        };

        try {
            session = std::make_unique<rcsdk::Session>(config, std::move(callbacks));
        } catch (const rcsdk::Error &error) {
            say(QStringLiteral("观看会话建不起来：%1").arg(QString::fromUtf8(error.what())));
            QCoreApplication::exit(1);
            return;
        }
        files.attach(session->handle());
        if (session->start() != RCS_OK) {
            say(QStringLiteral("观看会话启动不了：%1").arg(QString::fromStdString(session->failure().message)));
            QCoreApplication::exit(1);
        }
    };

    QObject::connect(&link, &ServerLink::signalReceived, &app,
                     [&](const QString &from, const QString &name, const QString &id, const QString &text) {
                         if (from != target || id != connection)
                             return;
                         say(QStringLiteral("   对方（%1）回答了，两边开始打通").arg(name));
                         session->signal(text.toStdString());
                     });
    QObject::connect(&link, &ServerLink::declined, &app,
                     [&](const QString &from, const QString &id, const QString &reason) {
                         if (from != target || id != connection)
                             return;
                         say(QStringLiteral("   对方回绝了：%1").arg(reason));
                         finish();
                     });
    QObject::connect(&link, &ServerLink::undeliverable, &app, [&](const QString &to, const QString &id) {
        if (to != target || id != connection)
            return;
        say(QStringLiteral("   对方不在线，或者设备 ID 不对"));
        finish();
    });
    // 登录好了、要的话账号也登录了、要的话对方的公钥也知道了，就去连
    bool signInSent = false;
    bool keyMissingSaid = false;
    const auto tryOpen = [&] {
        if (link.state() != ServerLink::State::Online || session)
            return;
        if (!email.isEmpty() && !link.account().signedIn) {
            if (!signInSent) {
                signInSent = true;
                say(QStringLiteral("   登录账号 %1").arg(email));
                link.signIn(email, parser.value(passwordOption));
            }
            return;
        }
        if (parser.isSet(mineOption) && peerKey.isEmpty()) {
            const auto devices = link.account().devices;
            const auto found = std::find_if(devices.cbegin(), devices.cend(),
                                            [&](const rc::protocol::AccountDevice &d) { return d.deviceId == target; });
            if (found == devices.cend() || found->publicKey.isEmpty()) {
                if (!std::exchange(keyMissingSaid, true))
                    say(QStringLiteral("   账号下没有设备 %1（它还没登录这个账号？），等它").arg(rc::DeviceId::parse(target).display()));
                return;
            }
            peerKey = QByteArray::fromBase64(found->publicKey.toLatin1());
            say(QStringLiteral("   账号下有 %1（%2），按我的设备连：只靠设备密钥").arg(found->name, rc::DeviceId::parse(target).display()));
        }
        say(link.ice().turn ? QStringLiteral("3. 已登录，服务器给了中继 %1；去连 %2").arg(link.ice().turn->server,
                                                                                     rc::DeviceId::parse(target).display())
                            : QStringLiteral("3. 已登录，服务器没给中继；去连 %1").arg(rc::DeviceId::parse(target).display()));
        if (relay && !link.ice().turn) {
            say(QStringLiteral("   服务器没有给中继，没法经中继连"));
            QCoreApplication::exit(1);
            return;
        }
        open();
    };
    QObject::connect(&link, &ServerLink::stateChanged, &app, tryOpen);
    QObject::connect(&link, &ServerLink::accountChanged, &app, tryOpen);
    QObject::connect(&link, &ServerLink::accountFailed, &app, [&](const QString &reason) {
        say(QStringLiteral("   账号出错了：%1").arg(reason));
        QCoreApplication::exit(1);
    });
    QTimer::singleShot(GiveUpMs, &app, [&] {
        say(QStringLiteral("   等太久了，放弃"));
        finish();
    });
    link.start();
    return QCoreApplication::exec();
}
