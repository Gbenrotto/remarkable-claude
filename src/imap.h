#pragma once
// Minimal Gmail IMAP client (implicit TLS) with pure, host-testable parsers.
#include "email.h"
#include <QSslSocket>
#include <QElapsedTimer>
#include <functional>

// One server response unit: its text lines, with each {n} literal cut out by length.
struct DiaryImapUnit { QByteArray text; QVector<QByteArray> literals; };

// Splits complete units from a response stream; *consumed is where the first incomplete unit starts.
inline QVector<DiaryImapUnit> diaryImapUnits(const QByteArray &raw,int *consumed) {
    static const QRegularExpression literal(R"(\{(\d+)\+?\}$)");
    QVector<DiaryImapUnit> units;int pos=0;*consumed=0;
    while(pos<raw.size()) {
        DiaryImapUnit unit;int cur=pos;bool complete=false;
        while(true) {
            int eol=raw.indexOf("\r\n",cur);
            if(eol<0) break;
            QByteArray line=raw.mid(cur,eol-cur);
            auto m=literal.match(QString::fromLatin1(line));
            if(m.hasMatch()) {
                qint64 n=m.captured(1).toLongLong();
                if(eol+2+n>raw.size()) break;
                unit.text+=line.left(m.capturedStart())+"{}";
                unit.literals<<raw.mid(eol+2,n);
                cur=eol+2+n;
                continue;
            }
            unit.text+=line;cur=eol+2;complete=true;break;
        }
        if(!complete) break;
        units<<unit;pos=cur;*consumed=pos;
    }
    return units;
}
inline QVector<qint64> diaryParseSearch(const QByteArray &response) {
    QVector<qint64> out;
    for(const auto &line:response.split('\n')) {
        QByteArray l=line.trimmed();
        if(!l.startsWith("* SEARCH")) continue;
        for(const auto &t:l.mid(8).split(' ')) {bool ok=false;qint64 v=t.toLongLong(&ok);if(ok)out<<v;}
    }
    return out;
}
inline QString diaryAllMailFolder(const QByteArray &listResponse) {
    static const QRegularExpression all(R"re(^\* LIST \(([^)]*)\) "[^"]*" "((?:[^"\\]|\\.)*)")re");
    for(const auto &line:listResponse.split('\n')) {
        auto m=all.match(QString::fromUtf8(line.trimmed()));
        if(m.hasMatch() && m.captured(1).split(' ').contains("\\All")) return m.captured(2).replace("\\\"","\"").replace("\\\\","\\");
    }
    return QString();
}
inline QByteArray diaryImapQuote(const QString &s) {
    QByteArray b=s.toUtf8();b.replace("\\","\\\\");b.replace("\"","\\\"");
    return "\""+b+"\"";
}

// --- MIME message parsing -------------------------------------------------
inline QMap<QByteArray,QByteArray> diaryHeaders(const QByteArray &head) {
    QMap<QByteArray,QByteArray> h;QByteArray key;
    for(QByteArray line:QByteArray(head).replace("\r\n","\n").split('\n')) {
        if((line.startsWith(' ')||line.startsWith('\t')) && !key.isEmpty()) {h[key]+=" "+line.trimmed();continue;}
        int colon=line.indexOf(':');if(colon<=0)continue;
        key=line.left(colon).trimmed().toLower();
        if(!h.contains(key)) h[key]=line.mid(colon+1).trimmed(); else key="\x01"; // keep first occurrence only
    }
    h.remove("\x01");
    return h;
}
inline QByteArray diaryHeaderParam(const QByteArray &value,const QByteArray &name) {
    QRegularExpression re(QString(R"re((?:^|;)\s*%1\s*=\s*(?:"([^"]*)"|([^;\s]+)))re").arg(QString::fromLatin1(name)),QRegularExpression::CaseInsensitiveOption);
    auto m=re.match(QString::fromLatin1(value));
    return m.hasMatch() ? (m.captured(1).isNull() ? m.captured(2) : m.captured(1)).toLatin1() : QByteArray();
}
inline QString diaryHtmlText(QString html) {
    html.remove(QRegularExpression(R"(<(style|script|head)[^>]*>.*?</\1>)",QRegularExpression::CaseInsensitiveOption|QRegularExpression::DotMatchesEverythingOption));
    html.replace(QRegularExpression(R"(<\s*(br|/p|/div|/tr|/li|/h\d)[^>]*>)",QRegularExpression::CaseInsensitiveOption),"\n");
    html.remove(QRegularExpression("<[^>]*>"));
    html.replace("&nbsp;"," ").replace("&amp;","&").replace("&lt;","<").replace("&gt;",">").replace("&quot;","\"").replace("&#39;","'");
    QRegularExpression num("&#(\\d+);");QRegularExpressionMatch m;
    while((m=num.match(html)).hasMatch()) html.replace(m.capturedStart(),m.capturedLength(),QChar(m.captured(1).toInt()));
    QStringList lines;for(auto l:html.split('\n')) {l=l.simplified();if(!l.isEmpty())lines<<l;}
    return lines.join('\n');
}
inline void diaryWalkMime(const QByteArray &entity,QStringList &plain,QStringList &html,QStringList &attachments,int depth=0) {
    int split=entity.indexOf("\r\n\r\n");int skip=4;
    if(split<0){split=entity.indexOf("\n\n");skip=2;}
    QByteArray head=split<0?entity:entity.left(split),body=split<0?QByteArray():entity.mid(split+skip);
    auto h=diaryHeaders(head);
    QByteArray type=h.value("content-type","text/plain").split(';').first().trimmed().toLower();
    QByteArray filename=diaryHeaderParam(h.value("content-disposition"),"filename");
    if(filename.isEmpty()) filename=diaryHeaderParam(h.value("content-type"),"name");
    if(type.startsWith("multipart/") && depth<8) {
        QByteArray boundary="--"+diaryHeaderParam(h.value("content-type"),"boundary");
        if(boundary.size()<=2) return;
        auto parts=body.split('\n');QByteArray cur;bool inPart=false;
        for(const auto &rawLine:parts) {
            QByteArray line=rawLine.endsWith('\r')?rawLine.chopped(1):rawLine;
            if(line==boundary||line==boundary+"--") {
                if(inPart) diaryWalkMime(cur,plain,html,attachments,depth+1);
                cur.clear();inPart=line==boundary;continue;
            }
            if(inPart) cur+=line+"\r\n";
        }
        return;
    }
    if(!filename.isEmpty() || h.value("content-disposition").toLower().startsWith("attachment")) {
        attachments<<diaryDecodeHeader(filename.isEmpty()?QByteArray("attachment"):filename);return;
    }
    QByteArray enc=h.value("content-transfer-encoding").trimmed().toLower(),data=body;
    if(enc=="base64") data=QByteArray::fromBase64(QByteArray(body).replace("\r","").replace("\n","").replace(" ",""));
    else if(enc=="quoted-printable") data=diaryQuotedPrintableDecode(body);
    QString text=diaryDecodeCharset(data,diaryHeaderParam(h.value("content-type"),"charset"));
    if(type=="text/plain") plain<<text.replace("\r\n","\n").trimmed();
    else if(type=="text/html") html<<diaryHtmlText(text);
}
inline QString diaryMailText(const QByteArray &rawMessage) {
    QStringList plain,html,attachments;
    diaryWalkMime(rawMessage,plain,html,attachments);
    QString text=(plain.join("\n\n").trimmed().isEmpty() ? html.join("\n\n") : plain.join("\n\n")).trimmed();
    if(text.size()>4000) text=text.left(4000)+"…";
    if(!attachments.isEmpty()) text+=(text.isEmpty()?"":"\n")+QString("[Attachments: %1]").arg(attachments.join(", "));
    return text;
}
inline DiaryMail diaryParseMessage(const QByteArray &raw) {
    int split=raw.indexOf("\r\n\r\n");if(split<0)split=raw.indexOf("\n\n");
    auto h=diaryHeaders(split<0?raw:raw.left(split));
    auto list=[&](const char *k){QStringList out;for(const auto &a:diarySplitAddresses(diaryDecodeHeader(h.value(k))))out<<a;return out;};
    DiaryMail m;
    m.from=diaryDecodeHeader(h.value("from"));m.replyTo=diaryDecodeHeader(h.value("reply-to"));
    m.to=list("to");m.cc=list("cc");m.subject=diaryDecodeHeader(h.value("subject"));m.date=QString::fromLatin1(h.value("date"));
    m.messageId=QString::fromLatin1(h.value("message-id")).trimmed();m.references=QString::fromLatin1(h.value("references")).simplified();
    m.body=diaryMailText(raw);
    return m;
}
inline QVector<DiaryMail> diaryParseFetch(const QVector<DiaryImapUnit> &units) {
    static const QRegularExpression uid(R"(\bUID (\d+))");
    QVector<DiaryMail> out;
    for(const auto &u:units) {
        if(!u.text.startsWith("* ") || !u.text.contains(" FETCH ") || u.literals.isEmpty()) continue;
        auto m=uid.match(QString::fromLatin1(u.text));if(!m.hasMatch())continue;
        DiaryMail mail=diaryParseMessage(u.literals.first());mail.uid=m.captured(1).toLongLong();
        out<<mail;
    }
    return out;
}

// --- Client ----------------------------------------------------------------
class DiaryImap {
public:
    explicit DiaryImap(std::function<bool()> valid):valid(std::move(valid)){}
    ~DiaryImap(){if(sock.state()==QAbstractSocket::ConnectedState){sendRaw(nextTag()+" LOGOUT\r\n");sock.waitForBytesWritten(2000);}}
    void login(const QString &user,const QString &pass) {
        sock.connectToHostEncrypted("imap.gmail.com",993);
        if(!sock.waitForEncrypted(30000)) throw std::runtime_error(("Gmail connection failed: "+sock.errorString()).toStdString());
        readUntil([](const DiaryImapUnit &u){return u.text.startsWith("* ");});
        auto r=run("LOGIN "+diaryImapQuote(user)+" "+diaryImapQuote(pass),"Gmail login failed");
        Q_UNUSED(r);
    }
    void select(const QString &folder,bool readOnly){run((readOnly?"EXAMINE ":"SELECT ")+diaryImapQuote(folder),"Cannot open mailbox");}
    QString allMail(){return diaryAllMailFolder(joined(run("LIST \"\" \"*\"","Cannot list mailboxes")));}
    // All Mail when Gmail exposes it over IMAP (its "Show in IMAP" label setting), otherwise the inbox.
    QString searchFolder(bool *everything){QString all=allMail();*everything=!all.isEmpty();return all.isEmpty()?QString("INBOX"):all;}
    QVector<qint64> unseen(){return diaryParseSearch(joined(run("UID SEARCH UNSEEN","Gmail search failed")));}
    QVector<qint64> gmailSearch(const QString &query) {
        bool ascii=true;for(QChar c:query)if(c.unicode()>126)ascii=false;
        if(ascii) return diaryParseSearch(joined(run("UID SEARCH X-GM-RAW "+diaryImapQuote(query),"Gmail search failed")));
        return diaryParseSearch(joined(run("UID SEARCH CHARSET UTF-8 X-GM-RAW","Gmail search failed",query.toUtf8())));
    }
    QVector<DiaryMail> fetch(const QVector<qint64> &uids) {
        if(uids.isEmpty()) return {};
        auto mails=diaryParseFetch(run("UID FETCH "+uidSet(uids)+" (UID BODY.PEEK[]<0.65536>)","Gmail fetch failed"));
        std::sort(mails.begin(),mails.end(),[](const DiaryMail &a,const DiaryMail &b){return a.uid>b.uid;});
        return mails;
    }
    void markSeen(const QVector<qint64> &uids){if(!uids.isEmpty())run("UID STORE "+uidSet(uids)+" +FLAGS.SILENT (\\Seen)","Cannot mark email as read");}
private:
    QSslSocket sock;int counter=0;QByteArray buf;std::function<bool()> valid;
    QByteArray nextTag(){return "A"+QByteArray::number(++counter).rightJustified(3,'0');}
    static QByteArray uidSet(const QVector<qint64> &uids){QByteArrayList s;for(auto u:uids)s<<QByteArray::number(u);return s.join(',');}
    static QByteArray joined(const QVector<DiaryImapUnit> &units){QByteArray out;for(const auto &u:units)out+=u.text+"\r\n";return out;}
    void sendRaw(const QByteArray &b){sock.write(b);sock.waitForBytesWritten(30000);}
    void waitData(QElapsedTimer &t) {
        if(valid && !valid()) throw std::runtime_error("Cancelled or page changed");
        if(t.elapsed()>60000) throw std::runtime_error("Gmail did not answer in time");
        if(!sock.waitForReadyRead(300) && sock.state()!=QAbstractSocket::ConnectedState) throw std::runtime_error("Gmail connection closed");
        buf+=sock.readAll();
    }
    QVector<DiaryImapUnit> readUntil(const std::function<bool(const DiaryImapUnit&)> &done) {
        QVector<DiaryImapUnit> all;QElapsedTimer t;t.start();
        while(true) {
            int consumed=0;auto units=diaryImapUnits(buf,&consumed);buf.remove(0,consumed);
            for(const auto &u:units){all<<u;if(done(u))return all;}
            waitData(t);
        }
    }
    QVector<DiaryImapUnit> run(const QByteArray &command,const char *what,const QByteArray &literal=QByteArray()) {
        QByteArray tag=nextTag();
        if(literal.isNull()) sendRaw(tag+" "+command+"\r\n");
        else {
            sendRaw(tag+" "+command+" {"+QByteArray::number(literal.size())+"}\r\n");
            QElapsedTimer t;t.start();
            while(!buf.contains("\r\n")) waitData(t);
            if(!buf.startsWith("+")) throw std::runtime_error(std::string(what)+": server refused the search");
            buf.remove(0,buf.indexOf("\r\n")+2);
            sendRaw(literal+"\r\n");
        }
        auto units=readUntil([&](const DiaryImapUnit &u){return u.text.startsWith(tag+" ");});
        QByteArray status=units.last().text.mid(tag.size()+1);
        if(!status.startsWith("OK")) throw std::runtime_error((QString(what)+": "+QString::fromUtf8(status.mid(3)).left(120)).toStdString());
        units.removeLast();
        return units;
    }
};
