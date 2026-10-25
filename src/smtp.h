#pragma once
// Minimal Gmail SMTP sender (implicit TLS, AUTH PLAIN) and batch sending of drafts.
#include "email.h"
#include <QSslSocket>
#include <QElapsedTimer>
#include <functional>

// DATA payload: CRLF line endings, dot-stuffed, terminated by CRLF.CRLF.
inline QByteArray diarySmtpData(const QByteArray &mime) {
    auto lines=QByteArray(mime).replace("\r\n","\n").split('\n');
    if(mime.endsWith('\n')) lines.removeLast(); // the final CRLF is re-added below
    QByteArray out;
    for(QByteArray line:lines) {
        if(line.startsWith('.')) line.prepend('.');
        out+=line+"\r\n";
    }
    return out+".\r\n";
}
inline QStringList diarySmtpRecipients(const QStringList &to,const QStringList &cc) {
    QStringList out;
    for(const auto &a:to+cc) if(!out.contains(diaryAddress(a))) out<<diaryAddress(a);
    return out;
}
inline bool diarySmtpSend(const QString &user,const QString &pass,const QStringList &rcpt,const QByteArray &mime,QString *error) {
    QSslSocket s;QByteArray buf;
    auto reply=[&](int expect)->bool {
        QElapsedTimer t;t.start();
        while(true) {
            int eol;
            while((eol=buf.indexOf("\r\n"))>=0) {
                QByteArray line=buf.left(eol);buf.remove(0,eol+2);
                if(line.size()>=4 && line[3]=='-') continue; // multi-line reply
                if(line.left(3).toInt()==expect) return true;
                *error=QString::fromUtf8(line).left(140);return false;
            }
            if(t.elapsed()>60000){*error="Gmail SMTP did not answer in time";return false;}
            if(!s.waitForReadyRead(1000) && s.state()!=QAbstractSocket::ConnectedState){*error="Gmail SMTP connection closed";return false;}
            buf+=s.readAll();
        }
    };
    auto send=[&](const QByteArray &line,int expect){s.write(line);s.waitForBytesWritten(30000);return reply(expect);};
    s.connectToHostEncrypted("smtp.gmail.com",465);
    if(!s.waitForEncrypted(30000)){*error="Gmail SMTP connection failed: "+s.errorString();return false;}
    if(!reply(220)) return false;
    if(!send("EHLO ink-diary\r\n",250)) return false;
    QByteArray auth=QByteArray("\0",1)+user.toUtf8()+QByteArray("\0",1)+pass.toUtf8();
    if(!send("AUTH PLAIN "+auth.toBase64()+"\r\n",235)) {*error="Gmail login failed: "+*error;return false;}
    if(!send("MAIL FROM:<"+diaryAddress(user).toLatin1()+">\r\n",250)) return false;
    for(const auto &r:rcpt) if(!send("RCPT TO:<"+r.toLatin1()+">\r\n",250)) return false;
    if(!send("DATA\r\n",354)) return false;
    if(!send(diarySmtpData(mime),250)) return false;
    send("QUIT\r\n",221);
    return true;
}
// Sends every pending draft; sent ones leave the state, failed ones stay. Returns the status line.
inline QString diarySendDrafts(DiaryEmailState &s,const std::function<bool(const DiaryDraft&,QString*)> &send) {
    int sent=0;QStringList failures;QVector<DiaryDraft> keep;
    for(const auto &d:s.drafts) {
        QString error;
        if(send(d,&error)) ++sent;
        else {keep<<d;failures<<QString("draft %1 failed: %2").arg(d.n).arg(error);}
    }
    s.drafts=keep;
    QString status=QString("Sent %1").arg(sent);
    if(!failures.isEmpty()) status+="; "+failures.join("; ");
    return status;
}
