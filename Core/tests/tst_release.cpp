#include <rc/release.h>

#include <QCryptographicHash>
#include <QTest>

// 测试用的一对密钥签的（node 的 crypto，和 scripts/sign-release.mjs 一样签）；不是发布密钥
namespace {

const QByteArray TestKey = QByteArray::fromBase64(
    "BMDu9gQexeV0+ED2yhdf69xxd4YTuU6HpTAB6p/PoMTvXSgf408P4HwjlYeZOlZL3CQbnuX3IFbY1HruAQMDVzA=");
const QByteArray TestSignature = QByteArray::fromBase64(
    "irKO1hSz1eGLwXzJiKLZHpsS+JODUElA3rw629qj6z+sEWYeu220xnwJ29FKwjOMAJHq/sPbxbmG3ye9HAmLng==");

QByteArray testText(const QString &version = QStringLiteral("0.2"))
{
    const QByteArray file = "hello world";
    return rc::release::text(QStringLiteral("windows-x64"), version, file.size(),
                             QCryptographicHash::hash(file, QCryptographicHash::Sha256).toHex());
}

} // namespace

class TestRelease : public QObject
{
    Q_OBJECT

private slots:
    // 和服务器、签名脚本拼出来的一字不差
    void textMatchesTheServer()
    {
        QCOMPARE(testText(), QByteArray("bilin-release\nplatform=windows-x64\nversion=0.2\nsize=11\n"
                                        "sha256=b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9\n"));
    }

    void acceptsTheSignedText()
    {
        QVERIFY(rc::release::signatureValid(TestKey, testText(), TestSignature));
    }

    // 改了一个字（版本号、大小、哈希）、换了钥匙、签名坏了都不认
    void rejectsAnythingElse()
    {
        QVERIFY(!rc::release::signatureValid(TestKey, testText(QStringLiteral("0.3")), TestSignature));
        QByteArray broken = TestSignature;
        broken[10] = char(broken[10] ^ 1);
        QVERIFY(!rc::release::signatureValid(TestKey, testText(), broken));
        QVERIFY(!rc::release::signatureValid(rc::release::publicKey(), testText(), TestSignature));
        QVERIFY(!rc::release::signatureValid(TestKey.left(33), testText(), TestSignature));
        QVERIFY(!rc::release::signatureValid(TestKey, testText(), TestSignature.left(63)));
    }

    void builtInKeyIsAPublicKey()
    {
        QCOMPARE(rc::release::publicKey().size(), 65);
        QCOMPARE(rc::release::publicKey().at(0), char(0x04));
    }
};

QTEST_GUILESS_MAIN(TestRelease)
#include "tst_release.moc"
