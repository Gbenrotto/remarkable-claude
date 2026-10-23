#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <functional>
#include <stdexcept>
inline QJsonObject diaryApiRequest(const QJsonObject &payload,const QString &key,
                                  const std::function<bool()> &valid, bool imageGeneration=false) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(imageGeneration ? "https://api.openai.com/v1/images/generations" : "https://api.openai.com/v1/responses"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setRawHeader("Authorization", "Bearer " + key.toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=manager.post(request,QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop; QTimer timeout,cancelPoll;
    timeout.setSingleShot(true);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(&cancelPoll,&QTimer::timeout,reply,[&]{if(!valid())reply->abort();});
    const qint64 limit=imageGeneration ? 32*1024*1024 : 1024*1024;
    QByteArray bytes;bool oversized=false;
    auto drain=[&] { auto chunk=reply->readAll();if(bytes.size()+chunk.size()>limit){oversized=true;reply->abort();}else bytes+=chunk; };
    reply->setReadBufferSize(65536);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,drain);
    timeout.start(imageGeneration ? 300000 : 120000);cancelPoll.start(300);loop.exec();
    if(!oversized)drain();
    if(oversized)throw std::runtime_error("Response too large");
    if(!valid())throw std::runtime_error("Cancelled or page changed");
    int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(code!=200||reply->error()!=QNetworkReply::NoError)
        throw std::runtime_error(QString("OpenAI request failed (HTTP %1); check Wi-Fi, key and model/tool support").arg(code).toStdString());
    auto doc=QJsonDocument::fromJson(bytes);
    if(!doc.isObject())throw std::runtime_error("Invalid API response");
    return doc.object();
}
