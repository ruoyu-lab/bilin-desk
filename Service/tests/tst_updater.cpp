#include "updater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

// 假的服务器：发布了 windows-x64 0.2（安装包是 "hello world"），用测试的密钥签的（同 Core/tests/tst_release.cpp）
namespace {

const QByteArray TestKey = QByteArray::fromBase64(
    "BMDu9gQexeV0+ED2yhdf69xxd4YTuU6HpTAB6p/PoMTvXSgf408P4HwjlYeZOlZL3CQbnuX3IFbY1HruAQMDVzA=");
const QByteArray Installer = "hello world";

class FakeSource : public UpdateSource
{
public:
    QJsonObject release{
        {QStringLiteral("version"), QStringLiteral("0.2")},
        {QStringLiteral("size"), 11},
        {QStringLiteral("sha256"), QString::fromLatin1(QCryptographicHash::hash(Installer, QCryptographicHash::Sha256).toHex())},
        {QStringLiteral("signature"), QStringLiteral("irKO1hSz1eGLwXzJiKLZHpsS+JODUElA3rw629qj6z+sEWYeu220xnwJ29FKwjOMAJHq/sPbxbmG3ye9HAmLng==")},
        {QStringLiteral("url"), QStringLiteral("/updates/windows-x64/0.2/Bilin-Setup-0.2.exe")},
    };
    QByteArray served = Installer;
    int latestAsked = 0;
    int downloads = 0;

    // 问不了服务器、还没发布过
    bool reachable = true;
    bool published = true;

    void latest(const QString &platform, std::function<void(bool, std::optional<QJsonObject>)> done) override
    {
        QCOMPARE(platform, QStringLiteral("windows-x64"));
        ++latestAsked;
        if (!reachable)
            done(false, std::nullopt);
        else
            done(true, published ? std::optional(release) : std::nullopt);
    }

    void download(const QString &, const QString &file, qint64 maxBytes, std::function<void(qint64)> progress,
                  std::function<void(bool)> done) override
    {
        ++downloads;
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        // 分两次收到：界面看得到进度
        const QByteArray bytes = served.left(maxBytes);
        output.write(bytes.left(5));
        progress(5);
        output.write(bytes.mid(5));
        progress(bytes.size());
        output.close();
        done(true);
    }
};

} // namespace

class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QVERIFY(directory_.isValid());
        QDir(directory_.path()).removeRecursively();
        QDir().mkpath(directory_.path());
        source_ = FakeSource();
        installs_ = 0;
        installedBytes_.clear();
        installResult_ = true;
    }

    // 有新版本：告诉界面，不自己下载、不自己装
    void announcesButWaitsForTheUser()
    {
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(updater->state().available, QStringLiteral("0.2"));
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));
        QTest::qWait(30);
        QCOMPARE(source_.downloads, 0);
        QCOMPARE(installs_, 0);
    }

    // 点了"更新"：下载（界面看得到进度）、核对、记下要装的版本、启动安装
    void installsWhenAsked()
    {
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(updater->state().available, QStringLiteral("0.2"));
        QList<double> progress;
        connect(updater.get(), &Updater::stateChanged, this, [&] {
            if (updater->state().phase == u"downloading")
                progress.append(updater->state().progress);
        });
        updater->installNow();
        QCOMPARE(installs_, 1);
        QCOMPARE(installedBytes_, Installer);
        QVERIFY(progress.size() >= 2);
        QCOMPARE(progress.last(), 1.0);
        QCOMPARE(updater->state().phase, QStringLiteral("installing"));
        QCOMPARE(readRecord().value(QLatin1StringView("version")).toString(), QStringLiteral("0.2"));
        // 装的时候再点不会再装一次
        updater->installNow();
        QCOMPARE(installs_, 1);
    }

    void ignoresVersionsNotNewer()
    {
        auto updater = make(QStringLiteral("0.2"));
        updater->start();
        QTRY_COMPARE(source_.latestAsked, 1);
        QVERIFY(updater->state().available.isEmpty());
        updater->installNow();
        QCOMPARE(source_.downloads, 0);
    }

    // 服务器说的和签名对不上（被人改过）：不告诉界面，点了也不下载
    void refusesBadSignatures()
    {
        source_.release[QStringLiteral("size")] = 12;
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(source_.latestAsked, 1);
        QVERIFY(updater->state().available.isEmpty());
        updater->installNow();
        QCOMPARE(source_.downloads, 0);
    }

    // 下载下来的和签了名的不一样：不装、不留着，告诉界面没下载成，还能再点
    void refusesWrongDownloads()
    {
        source_.served = "hello WORLD";
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(updater->state().available, QStringLiteral("0.2"));
        updater->installNow();
        QCOMPARE(installs_, 0);
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));
        QCOMPARE(updater->state().failed, QStringLiteral("download"));
        QVERIFY(QDir(directory_.path()).entryList({QStringLiteral("Bilin-update*")}, QDir::Files).isEmpty());
        source_.served = Installer;
        updater->installNow();
        QCOMPARE(installs_, 1);
    }

    void installerThatWontStart()
    {
        installResult_ = false;
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(updater->state().available, QStringLiteral("0.2"));
        updater->installNow();
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));
        QCOMPARE(updater->state().failed, QStringLiteral("install"));
        QVERIFY(readRecord().isEmpty());
    }

    // 交给系统的安装界面、那边取消了（Android）：回到"有新版本"，再点直接装刚才下载好的，不再下载
    void installAbandoned()
    {
        auto updater = make(QStringLiteral("0.1"));
        updater->start();
        QTRY_COMPARE(updater->state().available, QStringLiteral("0.2"));
        updater->installAbandoned();
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));
        updater->installNow();
        QCOMPARE(updater->state().phase, QStringLiteral("installing"));
        updater->installAbandoned();
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));
        QCOMPARE(updater->state().available, QStringLiteral("0.2"));
        QVERIFY(updater->state().failed.isEmpty());
        QVERIFY(readRecord().isEmpty());
        updater->installNow();
        QCOMPARE(source_.downloads, 1);
        QCOMPARE(installs_, 2);
        QCOMPARE(installedBytes_, Installer);
    }

    // 设置里点了检查更新：已是最新、问不了服务器、有新版本（这时结果为空，界面说有新版本）
    void checksWhenAsked()
    {
        auto updater = make(QStringLiteral("0.2"));
        updater->checkNow();
        QCOMPARE(updater->state().checked, QStringLiteral("upToDate"));
        QCOMPARE(updater->state().phase, QStringLiteral("idle"));

        source_.reachable = false;
        updater->checkNow();
        QCOMPARE(updater->state().checked, QStringLiteral("failed"));

        source_.reachable = true;
        source_.published = false;
        updater->checkNow();
        QCOMPARE(updater->state().checked, QStringLiteral("upToDate"));

        auto older = make(QStringLiteral("0.1"));
        source_.published = true;
        older->checkNow();
        QCOMPARE(older->state().available, QStringLiteral("0.2"));
        QVERIFY(older->state().checked.isEmpty());
        QCOMPARE(older->state().phase, QStringLiteral("idle"));
    }

    // 装完以后新的后台服务启动：记录清掉，算装好了
    void knowsItJustUpdated()
    {
        writeRecord(QStringLiteral("0.2"));
        auto updater = make(QStringLiteral("0.2"));
        QVERIFY(updater->justUpdated());
        QVERIFY(updater->state().failed.isEmpty());
        QVERIFY(readRecord().isEmpty());
    }

    // 装了却还是旧的版本（安装包里的版本号不对、安装出了错）：告诉界面没装上
    void knowsTheInstallDidNotTake()
    {
        writeRecord(QStringLiteral("0.2"));
        auto updater = make(QStringLiteral("0.1"));
        QVERIFY(!updater->justUpdated());
        QCOMPARE(updater->state().failed, QStringLiteral("install"));
        QVERIFY(readRecord().isEmpty());
    }

    // 真的服务器上发布了的 macOS 安装包：问到、下载下来，和服务器说的一样大、sha256 一样。
    // 环境变量 RC_UPDATE_SERVER 是服务器长连接的地址（比如开发用的虚拟机 ws://localhost:8800/ws）；没设时跳过
    void downloadsFromARealServer()
    {
        const QString server = qEnvironmentVariable("RC_UPDATE_SERVER");
        if (server.isEmpty())
            QSKIP("没设 RC_UPDATE_SERVER：不连真的服务器");
        HttpUpdateSource source(HttpUpdateSource::baseFor(QUrl(server)));
        std::optional<QJsonObject> latest;
        bool answered = false;
        source.latest(QStringLiteral("macos-arm64"), [&](bool, std::optional<QJsonObject> release) {
            latest = std::move(release);
            answered = true;
        });
        QTRY_VERIFY(answered);
        QVERIFY2(latest, "服务器上没有发布过 macOS 的安装包");
        const QString file = QDir(directory_.path()).filePath(QStringLiteral("download.pkg"));
        const qint64 size = latest->value(QLatin1StringView("size")).toInteger();
        bool ok = false;
        answered = false;
        qint64 received = 0;
        source.download(latest->value(QLatin1StringView("url")).toString(), file, size,
                        [&](qint64 bytes) { received = bytes; }, [&](bool result) {
                            ok = result;
                            answered = true;
                        });
        QTRY_VERIFY_WITH_TIMEOUT(answered, 60000);
        QVERIFY(ok);
        QCOMPARE(received, size);
        QFile downloaded(file);
        QVERIFY(downloaded.open(QIODevice::ReadOnly));
        QCOMPARE(downloaded.size(), size);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(&downloaded);
        QCOMPARE(hash.result().toHex(), latest->value(QLatin1StringView("sha256")).toString().toLatin1());
        // 下载的比说的大：不要
        answered = false;
        source.download(latest->value(QLatin1StringView("url")).toString(), file, size - 1, [](qint64) {},
                        [&](bool result) {
                            ok = result;
                            answered = true;
                        });
        QTRY_VERIFY_WITH_TIMEOUT(answered, 60000);
        QVERIFY(!ok);
    }

private:
    std::unique_ptr<Updater> make(const QString &version)
    {
        Updater::Options options;
        options.installer = QDir(directory_.path()).filePath(QStringLiteral("Bilin-update.exe"));
        options.platform = QStringLiteral("windows-x64");
        options.version = QVersionNumber::fromString(version);
        options.publicKey = TestKey;
        options.firstCheckMs = 0;
        return std::make_unique<Updater>(options, source_, [this, installer = options.installer] {
            ++installs_;
            QFile file(installer);
            if (file.open(QIODevice::ReadOnly))
                installedBytes_ = file.readAll();
            return installResult_;
        });
    }

    QJsonObject readRecord() const
    {
        QFile file(QDir(directory_.path()).filePath(QStringLiteral("update.json")));
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    void writeRecord(const QString &version)
    {
        QFile file(QDir(directory_.path()).filePath(QStringLiteral("update.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("version"), version}}).toJson());
    }

    QTemporaryDir directory_;
    FakeSource source_;
    int installs_ = 0;
    QByteArray installedBytes_;
    bool installResult_ = true;
};

QTEST_GUILESS_MAIN(TestUpdater)
#include "tst_updater.moc"
