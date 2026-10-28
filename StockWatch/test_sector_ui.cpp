// 离屏渲染板块窗口，用真实数据验证视觉
#include <QApplication>
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include "sectordata.h"
#include "sectordialog.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QFile log(QStringLiteral("F:/StockWatch/sector_ui_result.txt"));
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream ts(&log);

    QNetworkAccessManager net;
    QUrl url(QStringLiteral("https://push2.eastmoney.com/api/qt/slist/get"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("spt"), QStringLiteral("3"));
    q.addQueryItem(QStringLiteral("fltt"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("invt"), QStringLiteral("2"));
    q.addQueryItem(QStringLiteral("fields"), QStringLiteral("f12,f13,f14,f3"));
    q.addQueryItem(QStringLiteral("secid"), QStringLiteral("0.002555"));
    q.addQueryItem(QStringLiteral("pn"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("pz"), QStringLiteral("30"));
    q.addQueryItem(QStringLiteral("po"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("np"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("ut"), QStringLiteral("b2884a393a59ad64002292a3e90d46a5"));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    req.setRawHeader("Referer", "https://quote.eastmoney.com/");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    QNetworkReply *r = net.get(req);
    QEventLoop loop;
    bool done = false;
    QObject::connect(r, &QNetworkReply::finished, &loop,
                     [&]() { done = true; loop.quit(); });
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    loop.exec();

    QVector<SectorItem> sectors;
    if (done && r->error() == QNetworkReply::NoError) {
        const QJsonArray diff = QJsonDocument::fromJson(r->readAll()).object()
                                    .value(QStringLiteral("data")).toObject()
                                    .value(QStringLiteral("diff")).toArray();
        for (const QJsonValue &v : diff) {
            const QJsonObject o = v.toObject();
            SectorItem s;
            s.code = o.value(QStringLiteral("f12")).toString();
            s.name = o.value(QStringLiteral("f14")).toString();
            const QJsonValue pv = o.value(QStringLiteral("f3"));
            s.changePct = pv.isString() ? pv.toString().toDouble()
                                        : pv.toVariant().toDouble();
            if (!s.valid()) continue;
            s.kind = SectorCalc::kindOf(s.code, s.name);
            sectors.append(s);
        }
        ts << "fetched boards=" << sectors.size() << "\n";
    } else {
        ts << "network failed, using sample data\n";
        // 网络失败时用样本数据，保证 UI 仍可验证
        struct { const char *code; const char *name; double pct; } sample[] = {
            {"BK1046","游戏Ⅱ",2.62}, {"BK1301","游戏Ⅲ",2.62},
            {"BK0509","网络游戏",2.31}, {"BK0486","传媒",2.35},
            {"BK0149","安徽板块",1.63}, {"BK0706","脑机接口",2.53},
            {"BK1009","元宇宙概念",1.86}, {"BK0854","华为概念",2.25}
        };
        for (const auto &s : sample) {
            SectorItem it;
            it.code = QString::fromUtf8(s.code);
            it.name = QString::fromUtf8(s.name);
            it.changePct = s.pct;
            it.kind = SectorCalc::kindOf(it.code, it.name);
            sectors.append(it);
        }
        ts << "sample boards=" << sectors.size() << "\n";
    }

    const SectorLink link = SectorCalc::build(
        QStringLiteral("002555"), QStringLiteral("三七互娱"), 3.12, sectors);
    ts << "primary=" << link.primary.name
       << " level=" << SectorCalc::levelName(link.level)
       << " rel=" << QString::number(link.relStrength, 'f', 2) << "\n";

    SectorDialog dlg(QStringLiteral("002555"), QStringLiteral("三七互娱"));
    dlg.setLink(link);
    dlg.resize(680, 520);
    dlg.show();

    QTimer::singleShot(1200, &app, [&]() {
        QPixmap pm = dlg.grab();
        bool ok = pm.save(QStringLiteral("F:/StockWatch/_sector_ui.png"), "PNG");
        ts << "grab saved=" << (ok ? 1 : 0)
           << " size=" << pm.width() << "x" << pm.height() << "\n";
        ts.flush();
        app.quit();
    });
    return app.exec();
}
