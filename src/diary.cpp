// Ink Diary prototype. GPL-3.0-only. See THIRD_PARTY.md.
#include <atomic>
#include <thread>
#include <stdexcept>
#include <QBuffer>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTemporaryDir>
#include <QUuid>
#include <QMetaMethod>
#include <QTimer>
#include <QFileInfo>
#include <functional>
#include <QVersionNumber>
#include "inserter.inc"
#include "layout.h"
#include "response.h"
#include "plot.h"
#include "api.h"
#include "diagrams.h"
#include "webanswer.h"
#include "markdown.h"
#include "freshness.h"
#include "reading.h"
#include "imap.h"
#include "smtp.h"
#include "mailprompts.h"
#include "letterpaths.h"
#include "pathwriter.inc"

namespace {
std::atomic<bool> busy{false}, cancelled{false};
QMutex stateMutex;
QString message = "Claude";
QPointer<QObject> originController, originView;
QString originPage;
const QString base = "/home/root/.config/ink-diary";
std::atomic<bool> lastError{false};
// Status shown by the button. Errors stay visible until the next tap (see inkDiary.qmd).
void state(const QString &text) { QMutexLocker lock(&stateMutex); message = text; lastError=false; }
void fail(const QString &text) { state(text); lastError=true; }
QString currentMessage() { QMutexLocker lock(&stateMutex); return message; }
void require(bool valid, const char *why) { if (!valid) throw std::runtime_error(why); }
const QString emailStatePath = "/home/root/.local/share/ink-diary/email-state.json";
std::atomic<int> pendingDrafts{-1};
int draftCount() {
    if(pendingDrafts<0) pendingDrafts=int(diaryLoadEmailState(emailStatePath).drafts.size());
    return pendingDrafts;
}
void saveEmailState(const DiaryEmailState &s) {
    QDir().mkpath(QFileInfo(emailStatePath).absolutePath());
    diarySaveEmailState(s,emailStatePath);pendingDrafts=int(s.drafts.size());
}
QJsonObject claudeJson(const QJsonObject &payload,const QJsonObject &config,const std::function<bool()> &guard) {
    auto reply=diaryApiRequest(payload,config.value("api_key").toString(),guard);
    { // Last raw email response, root-only like the answer files, for diagnosing on-device failures.
        QSaveFile f("/home/root/.local/share/ink-diary/last-email-response.json");
        if(f.open(QIODevice::WriteOnly)){f.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);f.write(QJsonDocument(reply).toJson(QJsonDocument::Compact));f.commit();}
    }
    auto doc=QJsonDocument::fromJson(responseText(reply).toUtf8());
    require(doc.isObject(),"Could not read Claude's answer; please try again");
    return doc.object();
}
// Sends every pending draft over SMTP and saves what is left; sets the status line.
void sendPending(const QJsonObject &config,DiaryEmailState &st) {
    const QString user=config.value("gmail_address").toString().trimmed(),pass=config.value("gmail_app_password").toString().trimmed();
    require(!user.isEmpty()&&!pass.isEmpty(),"Set gmail_address and gmail_app_password (configure.sh --gmail)");
    state("Sending...");
    QString status=diarySendDrafts(st,[&](const DiaryDraft &d,QString *error){
        return diarySmtpSend(user,pass,diarySmtpRecipients(d.to,d.cc),diaryMime(d,user),error);});
    saveEmailState(st);
    if(st.drafts.isEmpty()) state(status); else fail(status);
}
// Turns an email request into page Markdown; afterInsert runs only once the image is on the page.
QString handleEmail(const DiaryAnswer &a,const QJsonObject &config,std::function<void()> &afterInsert,QString &done,
                    const std::function<bool()> &guard) {
    const QString user=config.value("gmail_address").toString().trimmed(),pass=config.value("gmail_app_password").toString().trimmed();
    require(!user.isEmpty()&&!pass.isEmpty(),"Set gmail_address and gmail_app_password (configure.sh --gmail)");
    DiaryEmailState st=diaryLoadEmailState(emailStatePath);
    if(!a.emailReplies.isEmpty()) {
        state("Writing drafts...");
        bool needClaude=false;for(const auto &r:a.emailReplies) if(r.second.compare("skip",Qt::CaseInsensitive)!=0) needClaude=true;
        QJsonObject result=needClaude ? diaryRequestDrafts(st,a.emailReplies,config,[&](const QJsonObject &payload){return claudeJson(payload,config,guard);})
                                      : QJsonObject{{"drafts",QJsonArray{}}};
        QString md=diaryApplyDrafts(st,result,a.emailReplies,user);
        afterInsert=[st]{saveEmailState(st);};
        done=st.drafts.isEmpty() ? "No drafts pending" : QString("Drafts added; tap Send %1").arg(st.drafts.size());
        return md;
    }
    require(st.drafts.isEmpty(),"Send or skip the pending drafts first");
    DiaryImap imap(guard);
    if(a.emailCheck) {
        state("Checking email...");
        imap.login(user,pass);imap.select("INBOX",true);
        auto unread=imap.unseen();auto newest=unread.mid(qMax(0,int(unread.size())-50));
        auto mails=imap.fetch(newest);
        QMap<int,DiaryMail> numbered;QString md;
        if(mails.isEmpty()) md="Nothing needs you. No unread email.";
        else md=diaryTriageMarkdown(claudeJson(diaryTriagePayload(mails,config,a.command),config,guard),mails,int(unread.size()-newest.size()),&numbered);
        QVector<qint64> checked;for(const auto &m:mails) checked<<m.uid;
        afterInsert=[numbered,checked,user,pass]{
            DiaryEmailState fresh;fresh.items=numbered;saveEmailState(fresh); // numbering first: replies work even if marking read fails
            DiaryImap mark({});mark.login(user,pass);mark.select("INBOX",false);mark.markSeen(checked);
        };
        done=numbered.isEmpty() ? "Email checked; nothing needs you" : QString("Email checked; %1 need you").arg(numbered.size());
        return md;
    }
    state("Searching email...");
    imap.login(user,pass);bool everything=false;imap.select(imap.searchFolder(&everything),true);
    auto found=imap.gmailSearch(a.emailQuery);auto newest=found.mid(qMax(0,int(found.size())-10));
    auto mails=imap.fetch(newest);
    QStringList footer;
    if(found.size()>newest.size()) footer<<QString("+%1 more matches").arg(found.size()-newest.size());
    if(!everything) footer<<"Searched the inbox only (All Mail is hidden from IMAP in Gmail settings)";
    QMap<int,DiaryMail> numbered;QString md;
    if(mails.isEmpty()) md=QString("No emails matched %1.").arg(diaryMarkdownLiteral(a.emailQuery))+(footer.isEmpty()?QString():"\n\n"+footer.join("\n\n"));
    else if(a.emailFull && mails.size()==1) {numbered[1]=mails.first();md=diaryFullMailMarkdown(mails.first());}
    else md=diaryMailListMarkdown(claudeJson(diarySummaryPayload(mails,config,a.command),config,guard),mails,footer,QString(),&numbered);
    afterInsert=[numbered]{DiaryEmailState fresh;fresh.items=numbered;saveEmailState(fresh);};
    done=QString("Found %1 email%2").arg(numbered.size()).arg(numbered.size()==1?"":"s");
    return md;
}

QImage capture(const QString &path) {
    QFile::remove(path);
    QByteArray arg = (path + ",0,0").toUtf8();
    char *result = rm_shot$screenshotHandler(arg.data());
    bool started = result && QByteArray(result) == "success";
    free(result);
    require(started, "Screenshot failed to start");
    // rm-shot writes asynchronously. Decode rather than accepting file existence.
    for (int i=0; i<100; ++i) {
        require(diaryGuard(), "Cancelled or page changed");
        QThread::msleep(100);
        QImage image(path);
        if (!image.isNull()) { diaryMaskControls(image); return image; }
    }
    throw std::runtime_error("Screenshot timed out");
}

QString answer(const QImage &image, const QJsonObject &config, int /*lines*/) {
    QString key = config.value("api_key").toString().trimmed();
    QString model = config.value("model").toString().trimmed();
    require(!key.isEmpty() && !model.isEmpty(), "Set api_key and model in config.json");
    QByteArray png;
    QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); image.save(&buffer,"PNG");
    auto payload=diaryReadingPayload(png,config);
    return responseText(diaryApiRequest(payload,key,diaryGuard));
}

// sendMode: the Send button. Hand edits on printed drafts are applied and shown first; otherwise the drafts are sent.
void work(bool sendMode) {
    try {
        QTemporaryDir temp("/tmp/ink-diary-XXXXXX");
        require(temp.isValid(), "Cannot create private temporary directory");
        QFile file(base+"/config.json");
        require(file.open(QIODevice::ReadOnly), "Create ink-diary/config.json first");
        QJsonObject config=QJsonDocument::fromJson(file.readAll()).object();
        require(!config.isEmpty(), "Invalid config.json");
        state("Reading page...");
        // Let the button switch to its stable busy appearance before capture.
        QThread::msleep(800);
        auto screen=capture(temp.path()+"/question.png");
        require(screen.width()==1404 && screen.height()==1872,
                "First version requires rM2 portrait (1404 x 1872)");
        QSize canvas;
        runOnGuiSync([&] {canvas=activeCanvasSize();});
        require(canvas==screen.size(), "Screen/input coordinates differ; refusing to draw");
        const QRect area=diaryCaptureArea();
        QImage question=screen.copy(area);
        int last=lastInkRow(question);
        require(last>=0, "Write a question on a blank page first");
        int top=area.y()+last+45;
        // Native scene images can extend below the current viewport. Render at
        // a fixed readable size, independent of empty space on screen.
        int width=area.width(), available=1582;
        std::function<void()> afterInsert;QString emailDone;
        DiaryAnswer resultAnswer;
        if(sendMode) {
            auto st=diaryLoadEmailState(emailStatePath);
            if(st.drafts.isEmpty()) {fail("No drafts to send");busy=false;return;}
            state("Checking drafts for edits...");
            QByteArray png;{QBuffer buffer(&png);buffer.open(QIODevice::WriteOnly);question.save(&buffer,"PNG");}
            QString md;
            // Hand edits are applied and sent in the same tap (the user's choice: no second draft).
            auto edited=diaryApplyEdits(st,claudeJson(diaryEditCheckPayload(png,st,config),config,diaryGuard),&md);
            sendPending(config,st);
            if(!edited.isEmpty() && !lastError) {
                QStringList numbers;for(int n:edited) numbers<<QString::number(n);
                state(currentMessage()+QString(" (with your edits to draft %1)").arg(numbers.join(", ")));
            }
            busy=false;return;
        } else {
            state("Thinking...");
            bool demo=config.value("demo_mode").toBool(false);
            if(demo) resultAnswer.text="This is a handwriting test. Live answers will work after you configure an Anthropic API key.";
            else resultAnswer=decodeDiaryAnswer(answer(question,config,qMin(9,available/60)));
            if(!demo)diaryRequireFreshSearch(resultAnswer);
            if(resultAnswer.emailCheck||resultAnswer.emailSearch||!resultAnswer.emailReplies.isEmpty())
                resultAnswer.text=handleEmail(resultAnswer,config,afterInsert,emailDone,diaryGuard);
        }
        DiaryWebAnswer web;
        if(resultAnswer.search) {
            state("Searching the web...");
            QString key=config.value("api_key").toString();
            auto payload=diaryWithFallbacks(diarySearchPayload(resultAnswer.text,config.value("model").toString(),config.value("timezone").toString("America/New_York")));
            auto reply=diaryApiRequest(payload,key,diaryGuard);
            for(int resumes=0;reply.value("stop_reason").toString()=="pause_turn"&&resumes<3;++resumes) {
                payload=diaryContinueSearch(payload,reply);
                reply=diaryApiRequest(payload,key,diaryGuard);
            }
            web=parseDiarySearch(reply);
            web.checkedAt=diaryLocalClock(QDateTime::currentDateTimeUtc(),config.value("timezone").toString("America/New_York"));
            resultAnswer.text=web.text;
        }
        QString text=resultAnswer.text;
        QImage ink; QString renderError, renderedHtml;
        runOnGuiSync([&] {
            if(!resultAnswer.diagram.isEmpty()) {
                try {ink=renderDiaryDiagram(resultAnswer.diagram,text,width);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            if(resultAnswer.plot) {
                try {ink=renderPlot(resultAnswer,width,available);}
                catch(const std::exception &error){renderError=QString::fromUtf8(error.what());}
                return;
            }
            static int fontId=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
            static int monoId=QFontDatabase::addApplicationFont(base+"/DejaVuSansMono.ttf");
            if(monoId<0) {renderError="Code font missing; reinstall Ink Diary";return;}
            if(fontId<0) {renderError="Caveat font missing";return;}
            try {
                QStringList sourceUrls;for(const auto &source:web.sources)sourceUrls.append(source.url);
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(fontId).first(),width,
                    resultAnswer.search?diarySourcesText(web):QString(),sourceUrls,resultAnswer.cellLines);
                ink=rendered.image;renderedHtml=rendered.html;
            } catch(const std::exception &error) {renderError=QString::fromUtf8(error.what());}
        });
        if(!renderError.isEmpty()) throw std::runtime_error(renderError.toStdString());
        state("Checking page...");
        auto current=capture(temp.path()+"/check.png");
        require(current.copy(area)==question, "Page changed while thinking; tap Claude again");
        require(diaryGuard(), "Cancelled or page changed");
        const QString answerDir="/home/root/.local/share/ink-diary/answers";
        require(QDir().mkpath(answerDir), "Cannot create answer image directory");
        QFile::setPermissions(answerDir,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);
        QString imagePath=answerDir+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces)+".png";
        require(ink.save(imagePath), "Cannot save answer image");
        QFile::setPermissions(imagePath,QFileDevice::ReadOwner|QFileDevice::WriteOwner);
        QString htmlPath=imagePath+".html";
        if(!renderedHtml.isEmpty()) {
            QFile html(htmlPath);
            require(html.open(QIODevice::WriteOnly),"Cannot save source links");
            html.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            QByteArray bytes=renderedHtml.toUtf8();
            require(html.write(bytes)==bytes.size(),"Cannot save source links");
        }
        QString sourcePath=imagePath+(!resultAnswer.diagram.isEmpty() ? ".source.txt" : ".md");
        if(!resultAnswer.plot) {
            QFile raw(sourcePath);
            require(raw.open(QIODevice::WriteOnly),"Cannot save answer Markdown");
            raw.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner);
            auto bytes=text.toUtf8();require(raw.write(bytes)==bytes.size(),"Cannot save answer Markdown");
        }
        require(diaryGuard(), "Cancelled or page changed");
        state("Inserting image...");
        auto result=insertSceneImage(QJsonObject{{"path",imagePath},{"coordinateSpace","screen"},
            {"x",area.x()+width/2.0},{"y",top+ink.height()/2.0},{"appendBelowContent",true}});
        if(!result.value("ok").toBool()) {
            QFile::remove(imagePath);
            QFile::remove(htmlPath);
            QFile::remove(sourcePath);
            throw std::runtime_error(result.value("message").toString().toStdString());
        }
        if(afterInsert) {afterInsert();state(emailDone);busy=false;return;}
        state(!resultAnswer.diagram.isEmpty() ? "Diagram added; scroll down if needed" : resultAnswer.plot ? "Plot added; scroll down if needed" : resultAnswer.search ? (web.sources.isEmpty() ? "Web answer added; source links unavailable" : "Web answer + sources added") : "Answer added; scroll down if needed");

    } catch(const std::exception &error) { fail(QString::fromUtf8(error.what())); }
    busy=false;
}
}

static bool diaryGuard() {
    if(cancelled) return false;
    bool valid=false;
    runOnGuiSync([&] {
        auto now=findActivePage();
        valid=originController && originView && now.controller==originController.data()
            && now.documentView==originView.data() && now.pageId==originPage;
    });
    if(!valid) cancelled=true;
    return valid;
}
extern "C" char *diary_start(const char *) {
    if(busy.exchange(true)) return response(QJsonObject{{"ok",false},{"message","Already busy"}});
    cancelled=false;
    bool valid=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        originController=context.controller; originView=context.documentView; originPage=context.pageId;
        valid=originController && originView && !originPage.isEmpty();
    });
    if(!valid) {busy=false; fail("Open a notebook page first"); return response(QJsonObject{{"ok",false}});}
    state("Starting...");
    std::thread([]{work(false);}).detach();
    return response(QJsonObject{{"ok",true}});
}
extern "C" char *diary_cancel(const char *) { cancelled=true; return response(QJsonObject{{"ok",true}}); }
// The Send button: like a tap, it photographs the page first so hand edits on printed drafts are not lost.
extern "C" char *diary_send(const char *) {
    if(busy.exchange(true)) return response(QJsonObject{{"ok",false},{"message","Already busy"}});
    cancelled=false;
    bool valid=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        originController=context.controller; originView=context.documentView; originPage=context.pageId;
        valid=originController && originView && !originPage.isEmpty();
    });
    if(!valid) {busy=false; fail("Open the notebook page with the drafts"); return response(QJsonObject{{"ok",false}});}
    state("Starting...");
    std::thread([]{work(true);}).detach();
    return response(QJsonObject{{"ok",true}});
}
extern "C" char *diary_status(const char *) {
    QFile config(base+"/config.json");
    bool demo=config.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(config.readAll()).object().value("demo_mode").toBool();
    QMutexLocker lock(&stateMutex);
    return response(QJsonObject{{"busy",busy.load()},{"drafts",draftCount()},{"error",lastError.load()},{"message",message=="Claude" && demo ? "Claude (demo)" : message}});
}
// Load on any firmware whose Qt is binary compatible with the SDK we were built
// against (same major, same or newer minor). Notebook-app internals are probed at
// runtime instead; missing ones produce a status message, never a guess.
extern "C" char _xovi_shouldLoad() {
    auto runtime=QVersionNumber::fromString(QString::fromLatin1(qVersion()));
    auto built=QVersionNumber::fromString(QStringLiteral(QT_VERSION_STR));
    if(runtime.majorVersion()!=built.majorVersion() || runtime.minorVersion()<built.minorVersion()) {
        qWarning("[ink-diary] not loading: built for Qt %s, firmware has Qt %s", QT_VERSION_STR, qVersion());
        return false;
    }
    return dlsym(RTLD_DEFAULT,"_Z21qRegisterResourceDataiPKhS0_S0_")!=nullptr;
}
// Read-only email diagnostics over the broker pipe: "email" or "email-search:<gmail query>" starts a run
// (fetch with BODY.PEEK + Claude, nothing inserted or marked read); "email-result" returns its outcome.
QMutex probeMutex;QJsonObject probeResult{{"state","idle"}};
extern "C" char *diary_probe(const char *arg) {
    if(arg && QByteArray(arg)=="email-result") {QMutexLocker lock(&probeMutex);return response(probeResult);}
    if(arg && QByteArray(arg).startsWith("email")) {
        if(busy.exchange(true)) return response(QJsonObject{{"ok",false},{"message","Already busy"}});
        {QMutexLocker lock(&probeMutex);probeResult=QJsonObject{{"state","running"}};}
        QByteArray request(arg);
        std::thread([request] {
            QJsonObject out{{"state","done"}};
            try {
                QFile file(base+"/config.json");require(file.open(QIODevice::ReadOnly),"Create ink-diary/config.json first");
                auto config=QJsonDocument::fromJson(file.readAll()).object();
                DiaryAnswer a;a.command=request.startsWith("email-search:") ? "find emails" : "check my email";
                if(request.startsWith("email-search:")) {a.emailSearch=true;a.emailQuery=QString::fromUtf8(request.mid(13));}
                else a.emailCheck=true;
                std::function<void()> afterInsert;QString done;
                out.insert("markdown",handleEmail(a,config,afterInsert,done,[]{return true;}));
                out.insert("status",done);
            } catch(const std::exception &error) {out.insert("error",QString::fromUtf8(error.what()));}
            {QMutexLocker lock(&probeMutex);probeResult=out;}
            state("Claude");busy=false;
        }).detach();
        return response(QJsonObject{{"ok",true}});
    }
    if(arg && QByteArray(arg)=="checklist") {
        QJsonObject check;
        runOnGuiSync([&] {
            try {
                int id=QFontDatabase::addApplicationFont(base+"/Caveat.ttf");
                QString text;for(int i=0;i<10;++i)text+="- [ ]\n";
                auto rendered=renderDiaryMarkdown(text,QFontDatabase::applicationFontFamilies(id).first(),1174);
                check.insert("checkboxes",rendered.html.count("class=\"unchecked\""));
                check.insert("height",rendered.image.height());
            } catch(const std::exception &e){check.insert("error",QString::fromUtf8(e.what()));}
        });
        return response(check);
    }
    auto result=capabilities();
    bool imageSupport=false,mapping=false;
    runOnGuiSync([&] {
        auto context=findActivePage();
        if(!context.controller)return;
        for(int i=0;i<context.controller->metaObject()->methodCount();++i)
            if(context.controller->metaObject()->method(i).name()=="insertImageFileAsSceneItem")imageSupport=true;
        for(auto object:runtimeObjects()) {
            if(!qobject_cast<QQuickItem *>(object) || !isVisibleObject(object) || controllerProperty(object,"controller")!=context.controller)continue;
            for(int i=0;i<object->metaObject()->methodCount();++i)
                if(object->metaObject()->method(i).name()=="viewToScene")mapping=true;
        }
    });
    result.insert("nativeImageInsertAvailable",imageSupport);result.insert("screenMappingAvailable",mapping);
    return response(result);
}
