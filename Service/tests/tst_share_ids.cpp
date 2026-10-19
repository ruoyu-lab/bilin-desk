#include "share_ids.h"

#include <QTest>

using share_ids::Session;

class TestShareIds : public QObject
{
    Q_OBJECT

private slots:
    // 几个会话的同一个编号合成不一样的，再拆回来还是原来的
    void roundTrip()
    {
        for (const Session session : {Session::Others, Session::Mine, Session::WebMine}) {
            for (const int local : {1, 2, 7, 1000}) {
                const int global = share_ids::global(session, local);
                QCOMPARE(share_ids::session(global), session);
                QCOMPARE(share_ids::local(global), local);
            }
        }
        QVERIFY(share_ids::global(Session::Others, 3) != share_ids::global(Session::Mine, 3));
        QVERIFY(share_ids::global(Session::Mine, 3) != share_ids::global(Session::WebMine, 3));
    }
};

QTEST_GUILESS_MAIN(TestShareIds)
#include "tst_share_ids.moc"
