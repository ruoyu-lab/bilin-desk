#include "control_server.h"
#include "device_store.h"
#include "test_keys.h"

#include <rc/line_channel.h>
#include <rc/ipc/endpoint.h>
#include <rc/transfer_list.h>

#include <QJsonArray>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>

namespace protocol = rc::protocol;

// 界面那一头：连上服务端，收主题、发命令
class TestControlServer : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *directory_ = nullptr;
    DeviceStore *store_ = nullptr;
    ControlServer *server_ = nullptr;
    QString endpoint_;
    rc::LineChannel *client_ = nullptr;
    QList<QJsonObject> received_;

    // 最近一次收到的这个主题
    QJsonObject latest(QLatin1StringView topic) const
    {
        for (auto it = received_.crbegin(); it != received_.crend(); ++it) {
            if (it->value(QLatin1StringView("topic")).toString() == topic)
                return it->value(QLatin1StringView("value")).toObject();
        }
        return {};
    }

private slots:
    void init()
    {
        endpoint_ = rc::ipc::userScopedName(QStringLiteral("rc-test-") + QUuid::createUuid().toString(QUuid::Id128));
        directory_ = new QTemporaryDir;
        store_ = new DeviceStore(directory_->path(), testDeviceKey);
        server_ = new ControlServer(*store_, {QStringLiteral("/path/to/service"), 42});
        QVERIFY(server_->listen(endpoint_));

        received_.clear();
        auto *socket = new QLocalSocket;
        client_ = new rc::LineChannel(socket);
        connect(client_, &rc::LineChannel::received, this, [this](const QJsonObject &m) { received_.append(m); });
        socket->connectToServer(endpoint_);
        QTRY_COMPARE(received_.size(), 2);
    }

    void cleanup()
    {
        delete client_;
        delete server_;
        delete store_;
        delete directory_;
    }

    void sendsEveryTopicOnConnect()
    {
        QCOMPARE(protocol::ServiceInfo::fromJson(latest(protocol::ServiceTopic)),
                 (protocol::ServiceInfo{QStringLiteral("/path/to/service"), 42}));
        QCOMPARE(protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)), store_->state());
    }

    void commandsChangeTheDevice()
    {
        client_->send(protocol::commandMessage(protocol::SetDeviceName, {{QStringLiteral("name"), QStringLiteral("客厅电脑")}}));
        QTRY_COMPARE(protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)).deviceName, QStringLiteral("客厅电脑"));

        client_->send(protocol::commandMessage(protocol::SetAcceptIncoming, {{QStringLiteral("value"), false}}));
        QTRY_VERIFY(!protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)).acceptIncoming);

        client_->send(protocol::commandMessage(protocol::SetLanDiscovery, {{QStringLiteral("value"), false}}));
        QTRY_VERIFY(!protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)).lanDiscovery);

        client_->send(protocol::commandMessage(protocol::SetUnattended, {{QStringLiteral("value"), false}}));
        QTRY_VERIFY(!protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)).unattended);

        const int before = received_.size();
        client_->send(protocol::commandMessage(protocol::RegeneratePassword));
        QTRY_COMPARE(received_.size(), before + 1);
        QCOMPARE(protocol::DeviceState::fromJson(latest(protocol::DeviceTopic)), store_->state());
    }

    void everyClientHearsChanges()
    {
        QList<QJsonObject> other;
        auto *socket = new QLocalSocket;
        rc::LineChannel second(socket);
        connect(&second, &rc::LineChannel::received, this, [&other](const QJsonObject &m) { other.append(m); });
        socket->connectToServer(endpoint_);
        QTRY_COMPARE(other.size(), 2);

        client_->send(protocol::commandMessage(protocol::SetLanDiscovery, {{QStringLiteral("value"), false}}));
        QTRY_COMPARE(other.size(), 3);
        QVERIFY(!protocol::DeviceState::fromJson(other.last().value(QLatin1StringView("value")).toObject()).lanDiscovery);
    }

    void signalsAndEvents()
    {
        QSignalSpy requested(server_, &ControlServer::signalRequested);
        client_->send(protocol::commandMessage(protocol::Signal, {{QStringLiteral("to"), QStringLiteral("222222222")},
                                                                  {QStringLiteral("address"), QStringLiteral("192.168.1.20")},
                                                                  {QStringLiteral("session"), QStringLiteral("s1")},
                                                                  {QStringLiteral("text"), QStringLiteral("offer")},
                                                                  {QStringLiteral("transport"), QStringLiteral("relay")},
                                                                  {QStringLiteral("access"), QStringLiteral("device")},
                                                                  {QStringLiteral("sdk"), rc::sdkVersion()}}));
        QTRY_COMPARE(requested.count(), 1);
        const rc::ConnectionMode mode{rc::RelayTransport, rc::DeviceAccess};
        QCOMPARE(requested.at(0), (QVariantList{QStringLiteral("222222222"), QStringLiteral("192.168.1.20"),
                                                QStringLiteral("s1"), QStringLiteral("offer"), QVariant::fromValue(mode)}));

        const int before = received_.size();
        server_->sendEvent(protocol::Undeliverable, {{QStringLiteral("to"), QStringLiteral("222222222")},
                                                     {QStringLiteral("session"), QStringLiteral("s1")}});
        QTRY_COMPARE(received_.size(), before + 1);
        QCOMPARE(received_.last(), (QJsonObject{{QStringLiteral("event"), QStringLiteral("undeliverable")},
                                                {QStringLiteral("to"), QStringLiteral("222222222")},
                                                {QStringLiteral("session"), QStringLiteral("s1")}}));
    }

    // 界面说这次连接结束了、界面程序断开了：不再替它转信令
    void signalingEnds()
    {
        QSignalSpy requested(server_, &ControlServer::signalRequested);
        QSignalSpy ended(server_, &ControlServer::signalEnded);
        for (const QString session : {QStringLiteral("s1"), QStringLiteral("s2")}) {
            QJsonObject signal{{QStringLiteral("to"), QStringLiteral("222222222")},
                               {QStringLiteral("session"), session},
                               {QStringLiteral("text"), QStringLiteral("offer")}};
            rc::ConnectionMode{}.write(signal);
            client_->send(protocol::commandMessage(protocol::Signal, signal));
        }
        QTRY_COMPARE(requested.count(), 2);
        client_->send(protocol::commandMessage(protocol::EndSignal, {{QStringLiteral("session"), QStringLiteral("s1")}}));
        QTRY_COMPARE(ended.count(), 1);
        QCOMPARE(ended.at(0).at(0).toString(), QStringLiteral("s1"));
        client_->close();
        QTRY_COMPARE(ended.count(), 2);
        QCOMPARE(ended.at(1).at(0).toString(), QStringLiteral("s2"));
    }

    void publishedTopicsReachEveryClient()
    {
        protocol::ShareState share;
        share.accepting = true;
        share.screenPermission = true;
        share.canControl = true;
        share.clipboard = false;
        share.requests = {{3, QStringLiteral("222222222"), QStringLiteral("小林的笔记本"), QStringLiteral("Windows"), false}};
        share.viewers = {{1, QStringLiteral("333333333"), QStringLiteral("书房的 iMac"), QStringLiteral("macOS"), true},
                         {2, QStringLiteral("444444444"), QStringLiteral("小林的手机"), QStringLiteral("iOS"), false}};
        server_->publish(protocol::ShareTopic, share.toJson());
        QTRY_COMPARE(protocol::ShareState::fromJson(latest(protocol::ShareTopic)), share);

        // 之后连上的界面也收到
        QList<QJsonObject> other;
        auto *socket = new QLocalSocket;
        rc::LineChannel second(socket);
        connect(&second, &rc::LineChannel::received, this, [&other](const QJsonObject &m) { other.append(m); });
        socket->connectToServer(endpoint_);
        QTRY_COMPARE(other.size(), 3);
        QCOMPARE(other.last().value(QLatin1StringView("topic")).toString(), QStringLiteral("share"));
        QCOMPARE(protocol::ShareState::fromJson(other.last().value(QLatin1StringView("value")).toObject()), share);
    }

    void shareCommandsAreAskedFor()
    {
        QSignalSpy admit(server_, &ControlServer::admitRequested);
        QSignalSpy refuse(server_, &ControlServer::refuseRequested);
        QSignalSpy disconnect(server_, &ControlServer::disconnectRequested);
        QSignalSpy control(server_, &ControlServer::controlRequested);
        QSignalSpy clipboard(server_, &ControlServer::clipboardRequested);
        QSignalSpy permission(server_, &ControlServer::permissionRequested);
        QSignalSpy restart(server_, &ControlServer::restartShareRequested);
        client_->send(protocol::commandMessage(protocol::Admit, {{QStringLiteral("id"), 3}, {QStringLiteral("control"), true}}));
        client_->send(protocol::commandMessage(protocol::Refuse, {{QStringLiteral("id"), 4}}));
        client_->send(protocol::commandMessage(protocol::Disconnect, {{QStringLiteral("id"), 1}}));
        client_->send(protocol::commandMessage(protocol::SetControl, {{QStringLiteral("id"), 1}, {QStringLiteral("value"), false}}));
        client_->send(protocol::commandMessage(protocol::SetClipboard, {{QStringLiteral("value"), false}}));
        client_->send(protocol::commandMessage(protocol::RequestPermission, {{QStringLiteral("permission"), protocol::InputPermission}}));
        client_->send(protocol::commandMessage(protocol::RestartShare));
        QTRY_COMPARE(restart.count(), 1);
        QCOMPARE(permission.at(0), (QVariantList{QString(protocol::InputPermission)}));
        QCOMPARE(admit.at(0), (QVariantList{3, true}));
        QCOMPARE(refuse.at(0), (QVariantList{4}));
        QCOMPARE(disconnect.at(0), (QVariantList{1}));
        QCOMPARE(control.at(0), (QVariantList{1, false}));
        QCOMPARE(clipboard.at(0), (QVariantList{false}));
    }

    void accountChangesAreAskedFor()
    {
        QSignalSpy nickname(server_, &ControlServer::nicknameRequested);
        QSignalSpy password(server_, &ControlServer::changePasswordRequested);
        QSignalSpy remove(server_, &ControlServer::deleteAccountRequested);
        client_->send(protocol::commandMessage(protocol::SetNickname, {{QStringLiteral("nickname"), QStringLiteral("新昵称")}}));
        client_->send(protocol::commandMessage(protocol::ChangePassword, {{QStringLiteral("oldPassword"), QStringLiteral("a")},
                                                                          {QStringLiteral("newPassword"), QStringLiteral("b")}}));
        client_->send(protocol::commandMessage(protocol::DeleteAccount, {{QStringLiteral("password"), QStringLiteral("b")}}));
        QTRY_COMPARE(remove.count(), 1);
        QCOMPARE(nickname.at(0), (QVariantList{QStringLiteral("新昵称")}));
        QCOMPARE(password.at(0), (QVariantList{QStringLiteral("a"), QStringLiteral("b")}));
        QCOMPARE(remove.at(0), (QVariantList{QStringLiteral("b")}));
    }

    void chatIsAskedFor()
    {
        QSignalSpy chat(server_, &ControlServer::chatRequested);
        client_->send(protocol::commandMessage(protocol::SendChat, {{QStringLiteral("id"), 2},
                                                                    {QStringLiteral("ref"), 15},
                                                                    {QStringLiteral("text"), QStringLiteral("好的")}}));
        QTRY_COMPARE(chat.count(), 1);
        QCOMPARE(chat.at(0), (QVariantList{2, 15, QStringLiteral("好的")}));
    }

    void fileCommandsAreAskedFor()
    {
        QSignalSpy send(server_, &ControlServer::sendFilesRequested);
        QSignalSpy accept(server_, &ControlServer::acceptTransferRequested);
        QSignalSpy reject(server_, &ControlServer::rejectTransferRequested);
        QSignalSpy cancel(server_, &ControlServer::cancelTransferRequested);
        QSignalSpy retry(server_, &ControlServer::retryTransferRequested);
        QSignalSpy remove(server_, &ControlServer::removeTransferRequested);
        const QStringList paths{QStringLiteral("/Users/me/报告.pdf"), QStringLiteral("/Users/me/photos")};
        const QString directory = QStringLiteral("/Users/me/Downloads");
        client_->send(protocol::commandMessage(
            protocol::SendFiles, {{QStringLiteral("viewer"), 2}, {QStringLiteral("paths"), QJsonArray::fromStringList(paths)}}));
        client_->send(protocol::commandMessage(
            protocol::AcceptTransfer, {{QStringLiteral("id"), 5}, {QStringLiteral("directory"), directory}}));
        client_->send(protocol::commandMessage(protocol::RejectTransfer, {{QStringLiteral("id"), 6}}));
        client_->send(protocol::commandMessage(protocol::CancelTransfer, {{QStringLiteral("id"), 7}}));
        client_->send(protocol::commandMessage(protocol::RetryTransfer, {{QStringLiteral("id"), 8}}));
        client_->send(protocol::commandMessage(protocol::RemoveTransfer, {{QStringLiteral("id"), 9}}));
        QTRY_COMPARE(remove.count(), 1);
        QCOMPARE(send.at(0), (QVariantList{2, paths}));
        QCOMPARE(accept.at(0), (QVariantList{5, directory}));
        QCOMPARE(reject.at(0), (QVariantList{6}));
        QCOMPARE(cancel.at(0), (QVariantList{7}));
        QCOMPARE(retry.at(0), (QVariantList{8}));
        QCOMPARE(remove.at(0), (QVariantList{9}));
    }

    void filesTopicReachesClients()
    {
        rc::Transfer offered;
        offered.id = 1;
        offered.viewer = 2;
        offered.name = QStringLiteral("photos");
        offered.entries = 1;
        offered.files = 25;
        offered.totalBytes = 340'000'000;
        offered.status = rc::transfer::Offered;
        server_->publish(protocol::FilesTopic, rc::transfersToJson({offered}));
        QTRY_COMPARE(rc::transfersFromJson(latest(protocol::FilesTopic)), QList<rc::Transfer>{offered});
    }

    void countsClients()
    {
        QCOMPARE(server_->clientCount(), 1);
        QSignalSpy changed(server_, &ControlServer::clientsChanged);
        client_->close();
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(server_->clientCount(), 0);
    }

    void quitIsAskedFor()
    {
        QSignalSpy quit(server_, &ControlServer::quitRequested);
        QSignalSpy appQuitting(server_, &ControlServer::appQuittingRequested);
        client_->send(protocol::commandMessage(protocol::AppQuitting));
        client_->send(protocol::commandMessage(protocol::Quit));
        QTRY_COMPARE(quit.count(), 1);
        QCOMPARE(appQuitting.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestControlServer)
#include "tst_control_server.moc"
