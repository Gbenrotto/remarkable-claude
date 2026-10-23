#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <iostream>
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);
 if(argc<2||argc>3){std::cerr<<"Usage: config-model MODEL [config-file]\n";return 1;}
 QString model=QString::fromLocal8Bit(argv[1]);
 if(!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$").match(model).hasMatch()){std::cerr<<"Invalid model name\n";return 1;}
 QString path=argc==3?QString::fromLocal8Bit(argv[2]):QString("/home/root/.config/ink-diary/config.json");
 QFile input(path);
 if(!input.open(QIODevice::ReadOnly)){std::cerr<<"Configure an API key first\n";return 1;}
 auto doc=QJsonDocument::fromJson(input.readAll());input.close();
 if(!doc.isObject()||doc.object().value("api_key").toString().trimmed().isEmpty()){std::cerr<<"Invalid configuration or missing API key; nothing changed\n";return 1;}
 auto config=doc.object();config.insert("model",model);
 QSaveFile output(path);output.setDirectWriteFallback(false);
 if(!output.open(QIODevice::WriteOnly)||!output.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)){std::cerr<<"Cannot securely save configuration\n";return 1;}
 auto bytes=QJsonDocument(config).toJson();
 if(output.write(bytes)!=bytes.size()||!output.commit()){std::cerr<<"Cannot save configuration; original preserved\n";return 1;}
 std::cout<<"Model updated to "<<model.toStdString()<<". API key and other settings preserved. No restart needed.\n";
}
