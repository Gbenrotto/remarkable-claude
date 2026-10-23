#pragma once
#include <QBuffer>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QPainter>
#include <stdexcept>

inline QJsonObject diaryImagePayload(const QString &prompt,const QJsonObject &config) {
    if(prompt.trimmed().isEmpty() || prompt.size()>12000)throw std::runtime_error("Image prompt is empty or too long");
    return {{"model",config.value("image_model").toString("gpt-image-1.5")},
        {"prompt",QString("Create artwork for a grayscale e-ink notebook. Use black ink, white paper and grayscale shading ONLY, no color. "
          "Use strong readable outlines, generous contrast and no tiny details. For comics or manga use a clear vertical reading order and large readable lettering. "
          "Follow the user's story and requested panel count faithfully. User request:\n")+prompt},
        {"n",1},{"size","1024x1536"},{"quality","medium"},{"output_format","png"},{"background","opaque"}};
}
inline QImage diaryGrayscaleImage(const QImage &source,int width) {
    if(source.isNull() || width<300 || width>1600)throw std::runtime_error("Invalid generated image");
    QImage scaled=source.scaledToWidth(width,Qt::SmoothTransformation);
    if(scaled.height()>8000)throw std::runtime_error("Generated image is too tall");
    QImage white(scaled.size(),QImage::Format_RGB32);white.fill(Qt::white);
    {QPainter p(&white);p.drawImage(0,0,scaled);}
    return white.convertToFormat(QImage::Format_Grayscale8);
}
inline QImage diaryDecodeImage(const QJsonObject &response,int width) {
    auto data=response.value("data").toArray();
    if(data.size()!=1)throw std::runtime_error("Image API did not return one image");
    auto encoded=data[0].toObject().value("b64_json").toString().toLatin1();
    if(encoded.isEmpty() || encoded.size()>32*1024*1024)throw std::runtime_error("Image response missing or too large");
    auto decoded=QByteArray::fromBase64Encoding(encoded,QByteArray::AbortOnBase64DecodingErrors);
    if(!decoded)throw std::runtime_error("Invalid image encoding");
    QBuffer buffer(&decoded.decoded);buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer,"PNG");auto size=reader.size();
    if(size.width()<1 || size.height()<1 || size.width()>4096 || size.height()>4096 || qint64(size.width())*size.height()>10000000)
        throw std::runtime_error("Generated image dimensions are unsupported");
    return diaryGrayscaleImage(reader.read(),width);
}
