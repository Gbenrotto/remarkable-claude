#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <stdexcept>
inline QString responseText(const QJsonObject &root) {
    if(root.value("status").toString()!="completed")
        throw std::runtime_error("Answer incomplete; try a shorter question");
    QString text;
    for(auto value:root.value("output").toArray())
        for(auto content:value.toObject().value("content").toArray()) {
            auto item=content.toObject();
            if(item.value("type").toString()=="output_text") text+=item.value("text").toString();
            if(item.value("type").toString()=="refusal") text+=item.value("refusal").toString();
        }
    if(text.trimmed().isEmpty() || text.size()>16000)
        throw std::runtime_error("Empty or oversized answer");
    return text.trimmed();
}
