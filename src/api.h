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
// Models whose refusals can be re-run server-side on Anthropic's recommended fallback.
inline bool diarySupportsFallbacks(const QString &model) {
    for(auto name:{"claude-sonnet-5-5","claude-opus-5-5","claude-opus-5","claude-fable-5-1"})if(model==name)return true;
    return false;
}
inline QJsonObject diaryWithFallbacks(QJsonObject payload) {
    if(diarySupportsFallbacks(payload.value("model").toString()))payload.insert("fallbacks","default");
    return payload;
}
inline QString diaryApiError(int code,const QByteArray &body) {
    // Anthropic errors are {"type":"error","error":{"type":...,"message":...}}; never echo the key.
    auto error=QJsonDocument::fromJson(body).object().value("error").toObject();
    QString detail=error.value("message").toString().left(160);
    if(detail.isEmpty())detail="check Wi-Fi, key and model";
    return QString("Claude request failed (HTTP %1): %2").arg(code).arg(detail);
}
inline QJsonObject diaryApiRequest(const QJsonObject &payload,const QString &key,
                                  const std::function<bool()> &valid) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://api.anthropic.com/v1/messages"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    request.setRawHeader("x-api-key", key.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");
    if(payload.contains("fallbacks"))request.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto *reply=manager.post(request,QJsonDocument(payload).toJson(QJsonDocument::Compact));
    QEventLoop loop; QTimer timeout,cancelPoll;
    timeout.setSingleShot(true);
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
    QObject::connect(&timeout,&QTimer::timeout,reply,&QNetworkReply::abort);
    QObject::connect(&cancelPoll,&QTimer::timeout,reply,[&]{if(!valid())reply->abort();});
    const qint64 limit=4*1024*1024; // Search results carry encrypted page content.
    QByteArray bytes;bool oversized=false;
    auto drain=[&] { auto chunk=reply->readAll();if(bytes.size()+chunk.size()>limit){oversized=true;reply->abort();}else bytes+=chunk; };
    reply->setReadBufferSize(65536);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,drain);
    timeout.start(180000);cancelPoll.start(300);loop.exec();
    if(!oversized)drain();
    if(oversized)throw std::runtime_error("Response too large");
    if(!valid())throw std::runtime_error("Cancelled or page changed");
    int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(code!=200||reply->error()!=QNetworkReply::NoError)
        throw std::runtime_error(diaryApiError(code,bytes).toStdString());
    auto doc=QJsonDocument::fromJson(bytes);
    if(!doc.isObject())throw std::runtime_error("Invalid API response");
    return doc.object();
}
