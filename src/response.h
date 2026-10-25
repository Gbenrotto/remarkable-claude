#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <stdexcept>
inline void diaryRequireComplete(const QJsonObject &root) {
    QString stop=root.value("stop_reason").toString();
    if(stop=="refusal")throw std::runtime_error("Claude declined this request");
    if(stop=="max_tokens")throw std::runtime_error("Answer incomplete; try a shorter question");
    if(stop!="end_turn")throw std::runtime_error(stop.isEmpty() ? "Invalid API response" : QString("Answer stopped early (%1); please try again").arg(stop).toStdString());
}
// After a server-side fallback the content holds a fallback block; only what follows it is the answer.
inline QJsonArray diaryFinalContent(const QJsonObject &root) {
    auto content=root.value("content").toArray();
    int start=0;
    for(int i=0;i<content.size();++i)if(content[i].toObject().value("type").toString()=="fallback")start=i+1;
    QJsonArray out;for(int i=start;i<content.size();++i)out.append(content[i]);
    return out;
}
inline QString responseText(const QJsonObject &root) {
    diaryRequireComplete(root);
    QString text;
    for(auto value:diaryFinalContent(root)) {
        auto item=value.toObject();
        if(item.value("type").toString()=="text") text+=item.value("text").toString();
    }
    if(text.trimmed().isEmpty() || text.size()>16000)
        throw std::runtime_error("Empty or oversized answer");
    return text.trimmed();
}
