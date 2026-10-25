#pragma once
// Gmail on the page: message model, reply-all addressing, MIME and saved state.
#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <stdexcept>

struct DiaryMail {
    qint64 uid=0;
    QString messageId, references, from, replyTo, subject, date, body;
    QStringList to, cc;
};
struct DiaryDraft {
    int n=0; qint64 uid=0;
    QStringList to, cc;
    QString subject, inReplyTo, references, body;
};
struct DiaryEmailState { QMap<int,DiaryMail> items; QVector<DiaryDraft> drafts; };

inline QString diaryDecodeCharset(const QByteArray &bytes,const QByteArray &charset) {
    QStringDecoder decoder(charset.isEmpty() ? QByteArray("UTF-8") : charset);
    if(!decoder.isValid()) decoder=QStringDecoder(QStringDecoder::Latin1);
    return decoder.decode(bytes);
}
inline QByteArray diaryQuotedPrintableDecode(QByteArray in,bool header=false) {
    if(header) in.replace('_',' ');
    QByteArray out;
    for(int i=0;i<in.size();++i) {
        if(in[i]=='=' && i+2<in.size() && in[i+1]=='\r' && in[i+2]=='\n') {i+=2;continue;}
        if(in[i]=='=' && i+1<in.size() && in[i+1]=='\n') {i+=1;continue;}
        if(in[i]=='=' && i+2<in.size()) {
            bool ok=false;int v=in.mid(i+1,2).toInt(&ok,16);
            if(ok){out+=char(v);i+=2;continue;}
        }
        out+=in[i];
    }
    return out;
}
// RFC 2047 encoded words; whitespace between adjacent encoded words is dropped.
inline QString diaryDecodeHeader(const QByteArray &raw) {
    static const QRegularExpression word(R"(=\?([^?]+)\?([BbQq])\?([^?]*)\?=)");
    QString out;int pos=0;bool lastWasWord=false;
    auto it=word.globalMatch(QString::fromLatin1(raw));
    while(it.hasNext()) {
        auto m=it.next();
        QString between=QString::fromLatin1(raw.mid(pos,m.capturedStart()-pos));
        if(!(lastWasWord && between.trimmed().isEmpty())) out+=diaryDecodeCharset(raw.mid(pos,m.capturedStart()-pos),"UTF-8");
        QByteArray text=m.captured(3).toLatin1();
        QByteArray bytes=m.captured(2).toUpper()=="B" ? QByteArray::fromBase64(text) : diaryQuotedPrintableDecode(text,true);
        out+=diaryDecodeCharset(bytes,m.captured(1).section('*',0,0).toLatin1());
        pos=m.capturedEnd();lastWasWord=true;
    }
    out+=diaryDecodeCharset(raw.mid(pos),"UTF-8");
    return out.simplified();
}
inline QString diaryAddress(const QString &header) {
    int lt=header.lastIndexOf('<'),gt=header.lastIndexOf('>');
    QString a=lt>=0&&gt>lt ? header.mid(lt+1,gt-lt-1) : header;
    return a.trimmed().toLower();
}
// Split an address list on commas outside quotes and angle brackets.
inline QStringList diarySplitAddresses(const QString &list) {
    QStringList out;QString cur;bool quoted=false;int angle=0;
    for(QChar c:list) {
        if(c=='"')quoted=!quoted;
        else if(c=='<'&&!quoted)++angle;
        else if(c=='>'&&!quoted&&angle>0)--angle;
        if(c==','&&!quoted&&angle==0){if(!cur.trimmed().isEmpty())out<<cur.trimmed();cur.clear();continue;}
        cur+=c;
    }
    if(!cur.trimmed().isEmpty())out<<cur.trimmed();
    return out;
}
inline void diaryReplyRecipients(const DiaryMail &m,const QString &self,QStringList &to,QStringList &cc) {
    to.clear();cc.clear();
    QString me=diaryAddress(self);
    QStringList seen;
    QString primary=m.replyTo.trimmed().isEmpty() ? m.from : m.replyTo;
    for(const auto &a:diarySplitAddresses(primary)) if(diaryAddress(a)!=me && !seen.contains(diaryAddress(a))) {to<<a;seen<<diaryAddress(a);}
    for(const auto &a:m.to+m.cc) if(diaryAddress(a)!=me && !seen.contains(diaryAddress(a))) {cc<<a;seen<<diaryAddress(a);}
    if(to.isEmpty() && !cc.isEmpty()) to<<cc.takeFirst();
}
inline QString diaryReplySubject(const QString &subject) {
    static const QRegularExpression prefix(R"(^\s*re\s*:\s*)",QRegularExpression::CaseInsensitiveOption);
    QString s=subject.trimmed();
    while(prefix.match(s).hasMatch()) s=s.mid(prefix.match(s).capturedLength());
    return "Re: "+s;
}
inline QByteArray diaryEncodeWord(const QString &text) {
    for(QChar c:text) if(c.unicode()>126 || c.unicode()<32) return "=?UTF-8?B?"+text.toUtf8().toBase64()+"?=";
    return text.toLatin1();
}
inline QByteArray diaryEncodeAddress(const QString &a) {
    int lt=a.lastIndexOf('<');
    if(lt<=0) return diaryAddress(a).toLatin1();
    QString name=a.left(lt).trimmed();
    if(name.startsWith('"')&&name.endsWith('"')&&name.size()>=2) name=name.mid(1,name.size()-2);
    QByteArray enc=diaryEncodeWord(name);
    if(enc==name.toLatin1()) enc="\""+QByteArray(enc).replace("\"","")+"\"";
    return enc+" <"+diaryAddress(a).toLatin1()+">";
}
inline QByteArray diaryQuotedPrintable(const QString &text) {
    QByteArray out;
    const auto lines=text.toUtf8().replace("\r\n","\n").split('\n');
    for(int li=0;li<lines.size();++li) {
        QByteArray line;int width=0;
        auto put=[&](const QByteArray &piece){if(width+piece.size()>75){line+="=\r\n";width=0;}line+=piece;width+=piece.size();};
        const QByteArray &l=lines[li];
        for(int i=0;i<l.size();++i) {
            unsigned char c=l[i];
            bool trailingSpace=(c==' '||c=='\t')&&i==l.size()-1;
            if((c>=33&&c<=126&&c!='=')||((c==' '||c=='\t')&&!trailingSpace)) put(QByteArray(1,char(c)));
            else put("="+QByteArray::number(c,16).toUpper().rightJustified(2,'0'));
        }
        out+=line;
        if(li+1<lines.size())out+="\r\n";
    }
    return out;
}
inline QByteArray diaryMime(const DiaryDraft &d,const QString &from) {
    auto list=[](const QStringList &a){QByteArrayList out;for(const auto &x:a)out<<diaryEncodeAddress(x);return out.join(", ");};
    QByteArray m;
    m+="From: "+from.toLatin1()+"\r\n";
    m+="To: "+list(d.to)+"\r\n";
    if(!d.cc.isEmpty()) m+="Cc: "+list(d.cc)+"\r\n";
    m+="Subject: "+diaryEncodeWord(d.subject)+"\r\n";
    m+="Date: "+QDateTime::currentDateTime().toString(Qt::RFC2822Date).toLatin1()+"\r\n";
    m+="Message-ID: <"+QUuid::createUuid().toString(QUuid::WithoutBraces).toLatin1()+"@ink-diary>\r\n";
    if(!d.inReplyTo.isEmpty()) m+="In-Reply-To: "+d.inReplyTo.toLatin1()+"\r\n";
    if(!d.references.isEmpty()) m+="References: "+d.references.toLatin1()+"\r\n";
    m+="MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nContent-Transfer-Encoding: quoted-printable\r\n\r\n";
    m+=diaryQuotedPrintable(d.body)+"\r\n";
    return m;
}
inline QJsonObject diaryMailJson(const DiaryMail &m) {
    return {{"uid",double(m.uid)},{"message_id",m.messageId},{"references",m.references},{"from",m.from},{"reply_to",m.replyTo},
        {"subject",m.subject},{"date",m.date},{"body",m.body},{"to",QJsonArray::fromStringList(m.to)},{"cc",QJsonArray::fromStringList(m.cc)}};
}
inline QStringList diaryStrings(const QJsonValue &v){QStringList out;for(auto x:v.toArray())out<<x.toString();return out;}
inline DiaryMail diaryMailFromJson(const QJsonObject &o) {
    DiaryMail m;m.uid=qint64(o.value("uid").toDouble());m.messageId=o.value("message_id").toString();m.references=o.value("references").toString();
    m.from=o.value("from").toString();m.replyTo=o.value("reply_to").toString();m.subject=o.value("subject").toString();
    m.date=o.value("date").toString();m.body=o.value("body").toString();m.to=diaryStrings(o.value("to"));m.cc=diaryStrings(o.value("cc"));
    return m;
}
inline DiaryEmailState diaryLoadEmailState(const QString &path) {
    DiaryEmailState s;QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return s;
    auto o=QJsonDocument::fromJson(f.readAll()).object();
    auto items=o.value("items").toObject();
    for(auto it=items.begin();it!=items.end();++it) s.items[it.key().toInt()]=diaryMailFromJson(it.value().toObject());
    for(auto v:o.value("drafts").toArray()) {
        auto d=v.toObject();
        s.drafts.push_back({d.value("n").toInt(),qint64(d.value("uid").toDouble()),diaryStrings(d.value("to")),diaryStrings(d.value("cc")),
            d.value("subject").toString(),d.value("in_reply_to").toString(),d.value("references").toString(),d.value("body").toString()});
    }
    return s;
}
inline void diarySaveEmailState(const DiaryEmailState &s,const QString &path) {
    QJsonObject items;for(auto it=s.items.begin();it!=s.items.end();++it) items.insert(QString::number(it.key()),diaryMailJson(it.value()));
    QJsonArray drafts;
    for(const auto &d:s.drafts) drafts.append(QJsonObject{{"n",d.n},{"uid",double(d.uid)},{"to",QJsonArray::fromStringList(d.to)},
        {"cc",QJsonArray::fromStringList(d.cc)},{"subject",d.subject},{"in_reply_to",d.inReplyTo},{"references",d.references},{"body",d.body}});
    QSaveFile out(path);out.setDirectWriteFallback(false);
    if(!out.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot save email state");
    out.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
    auto bytes=QJsonDocument(QJsonObject{{"items",items},{"drafts",drafts}}).toJson(QJsonDocument::Compact);
    if(out.write(bytes)!=bytes.size()||!out.commit()) throw std::runtime_error("Cannot save email state");
}
