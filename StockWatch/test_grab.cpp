#include <QApplication>
#include <QWidget>
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
#include "timesharechart.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QNetworkAccessManager net;

    QUrl url("https://web.ifzq.gtimg.cn/appstock/app/minute/query");
    QUrlQuery q; q.addQueryItem("code", "sz002555"); url.setQuery(q);
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Mozilla/5.0");
    req.setRawHeader("Referer", "https://gu.qq.com/");
    req.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    QNetworkReply *r = net.get(req);
    QEventLoop loop;
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    loop.exec();

    if (r->error() != QNetworkReply::NoError) {
        fprintf(stderr, "NET FAIL: %s\n", qPrintable(r->errorString()));
        return 1;
    }
    QJsonDocument doc = QJsonDocument::fromJson(r->readAll());
    QJsonObject one = doc.object().value("data").toObject().value("sz002555").toObject();
    QJsonArray qtArr = one.value("qt").toObject().value("sz002555").toArray();
    double prevClose = qtArr.size() > 4 ? qtArr.at(4).toVariant().toDouble() : 0;
    QJsonObject dObj = one.value("data").toObject();
    QString date = dObj.value("date").toString();
    QJsonArray arr = dObj.value("data").toArray();
    fprintf(stderr, "prevClose=%.2f date=%s points=%d\n", prevClose, qPrintable(date), arr.size());

    QVector<QString> lines;
    for (const QJsonValue &v : arr) lines.append(v.toString());
    TimeShareData d = TimeShare::parseTencent("002555", QString::fromUtf8("三七互娱"), prevClose, date, lines);
    fprintf(stderr, "parsed valid=%d points=%d\n", d.valid, d.points.size());

    TimeShareChart w;
    w.resize(1000, 620);
    w.setData(d);
    w.show();
    QTimer::singleShot(1200, &app, [&]() {
        QPixmap pm = w.grab();
        bool ok = pm.save("F:/StockWatch/_grab.png", "PNG");
        fprintf(stderr, "grab saved=%d size=%dx%d\n", ok, pm.width(), pm.height());
        app.quit();
    });
    return app.exec();
}
