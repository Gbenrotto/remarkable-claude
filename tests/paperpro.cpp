// Host simulation of one tap on each supported tablet, from screenshot to insertion
// coordinates, without a device: synthetic ruled-template screenshots with a handwritten
// question, the real capture/mask/ink-search path from layout.h, and the real answer
// renderers at the device's own width and height budget. Writes page previews.
#include <cassert>
#include <iostream>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QPainter>
#include "../src/layout.h"
#include "../src/markdown.h"
#include "../src/plot.h"
#include "../src/diagrams.h"
#include "../src/webanswer.h"

namespace {
QString caveat;
// A screenshot as rm-shot returns it: full screen, white page, pale ruled template, a left
// toolbar strip, the close button cell and our own button drawn in the top-right cell.
QImage syntheticScreen(const QSize &screen, bool colour) {
    QImage s(screen, colour ? QImage::Format_RGB32 : QImage::Format_Grayscale8);
    s.fill(Qt::white);
    QPainter p(&s);
    p.fillRect(QRect(0,0,150,screen.height()), QColor(245,245,245));          // toolbar background
    for(int y=0;y<screen.height();y+=56) p.fillRect(QRect(150,y,screen.width()-150,1), QColor(191,191,191)); // rules
    p.fillRect(QRect(230,0,1,screen.height()), QColor(191,191,191));           // margin line
    p.fillRect(QRect(screen.width()-112,0,112,112), QColor(230,230,230));      // close-button cell
    const QRect button(screen.width()-26-60,138,60,60);                        // inkDiary.qmd button
    p.setPen(QPen(Qt::black,2)); p.setBrush(Qt::white); p.drawRoundedRect(button,6,6);
    p.setBrush(Qt::black); p.drawEllipse(button.center(),18,18);
    return s;
}
void writeQuestion(QImage &s, const QString &text, int y, const QColor &ink) {
    QPainter p(&s);
    QFont f(caveat); f.setPixelSize(58); p.setFont(f); p.setPen(ink);
    p.drawText(QRect(260,y,s.width()-260-140,400), Qt::TextWordWrap, text);
}
struct Tap { QRect area; int questionBottom=-1, top=0, width=0, budget=0; QImage question; };
// The capture half of diary.cpp's work(): mask, crop, find the last ink row, budget the answer.
Tap simulateCapture(QImage screen) {
    assert(diarySupportedScreen(screen.size()));
    diaryMaskControls(screen);
    Tap t; t.area=diaryCaptureArea(screen.size());
    t.question=screen.copy(t.area);
    t.questionBottom=lastInkRow(t.question,diaryMaskedInCapture(screen.size()));
    assert(t.questionBottom>=0);
    t.top=t.area.y()+t.questionBottom+45;
    t.width=t.area.width(); t.budget=diaryAnswerBudget(screen.size());
    return t;
}
// Where diary.cpp asks the notebook to centre the image, and a preview of the result.
QImage compose(const QImage &screen, const Tap &t, const QImage &ink, const char *label) {
    const double cx=t.area.x()+t.width/2.0, cy=t.top+ink.height()/2.0;
    assert(cx>t.area.left() && cx<t.area.right());
    assert(cy>t.top);                                    // always below the question
    assert(ink.width()<=t.width);                        // never wider than the page area
    QImage page(screen.width(), qMax(screen.height(), t.top+ink.height()+60), QImage::Format_RGB32);
    page.fill(Qt::white);
    QPainter p(&page);
    p.drawImage(0,0,screen);
    if(page.height()>screen.height()) p.fillRect(QRect(0,screen.height(),page.width(),page.height()-screen.height()),QColor(250,250,250));
    p.drawImage(QPointF(cx-ink.width()/2.0, cy-ink.height()/2.0), ink);
    p.setPen(QPen(QColor(200,0,0),2,Qt::DashLine)); p.setBrush(Qt::NoBrush);
    p.drawRect(t.area);                                  // capture area
    p.drawLine(0,screen.height(),page.width(),screen.height()); // viewport bottom
    p.end();
    std::cout<<"  "<<label<<": question bottom at screen row "<<t.area.y()+t.questionBottom
             <<", answer "<<ink.width()<<"x"<<ink.height()<<" centred at ("<<cx<<", "<<cy<<")"
             <<(cy+ink.height()/2.0>screen.height()?" (extends below the viewport; scroll)":"")<<"\n";
    return page;
}
}

int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    int id=QFontDatabase::addApplicationFont(QFileInfo("fonts/Caveat.ttf").absoluteFilePath()); assert(id>=0);
    assert(QFontDatabase::addApplicationFont(QFileInfo("fonts/DejaVuSansMono.ttf").absoluteFilePath())>=0);
    caveat=QFontDatabase::applicationFontFamilies(id).first();
    struct Device { const char *name; QSize screen; bool colour; } devices[]={
        {"rm2", diaryRm2Screen(), false}, {"paperpro", diaryPaperProScreen(), true}};
    const QString markdown=QString::fromUtf8(
        "The Moon shines because it **reflects sunlight**; it makes no light of its own.\n\n"
        "| Phase | Lit fraction |\n|---|---:|\n| New | 0% |\n| Half | 50% |\n| Full | 100% |\n\n"
        "- Albedo about 0.12\n- Brightest at full moon");
    for(const auto &d:devices) {
        std::cout<<d.name<<" ("<<d.screen.width()<<"x"<<d.screen.height()<<")\n";
        // 1. Text answer under a question written near the top, in the tablet's pen colour.
        QImage screen=syntheticScreen(d.screen,d.colour);
        writeQuestion(screen,"Why does the Moon shine?",180, d.colour?QColor(0,0,160):QColor(Qt::black));
        Tap tap=simulateCapture(screen);
        assert(tap.area.y()+tap.questionBottom>180 && tap.area.y()+tap.questionBottom<320);
        auto text=renderDiaryMarkdown(markdown,caveat,tap.width);
        assert(text.image.width()==tap.width && text.image.height()<=8000);
        compose(screen,tap,text.image,"text").save(QString("%1-text-preview.png").arg(d.name));
        // 2. Plot: fixed 800 px graph, full capture width.
        auto plot=decodeDiaryAnswer(R"({"kind":"plot","command":"plot y=x^2","expression":"x^2","xmin":-5,"xmax":5})");
        QImage graph=renderPlot(plot,tap.width,tap.budget);
        assert(graph.width()==tap.width && graph.height()==800);
        compose(screen,tap,graph,"plot").save(QString("%1-plot-preview.png").arg(d.name));
        // 3. Flowchart at the device width.
        QImage flow=renderDiaryDiagram("mermaid","flowchart TD\nA[Boil water]\nB[Steep tea]\nC[Drink]\nA --> B\nB --> C",tap.width);
        assert(!flow.isNull() && flow.width()<=tap.width);
        compose(screen,tap,flow,"flowchart").save(QString("%1-flow-preview.png").arg(d.name));
        // 4. Question written low on the page: the answer centre lands below the viewport, and
        //    nothing is rejected (native insertion extends the page; the user scrolls).
        QImage low=syntheticScreen(d.screen,d.colour);
        writeQuestion(low,"And one more near the bottom?",d.screen.height()-360,Qt::black);
        Tap lowTap=simulateCapture(low);
        assert(lowTap.area.y()+lowTap.questionBottom<lowTap.area.bottom());
        compose(low,lowTap,text.image,"low question").save(QString("%1-low-preview.png").arg(d.name));
        // 5. A page with nothing but the template and our button is reported as blank.
        QImage blank=syntheticScreen(d.screen,d.colour); diaryMaskControls(blank);
        assert(lastInkRow(blank.copy(diaryCaptureArea(d.screen)),diaryMaskedInCapture(d.screen))==-1);
        // 6. The page-changed check compares the masked crops, so a status message or a busy
        //    square inside our button cell does not count as a change.
        QImage again=screen; {QPainter p(&again); p.fillRect(diaryButtonRect(d.screen).adjusted(30,30,-30,-30),Qt::black);}
        diaryMaskControls(again); assert(again.copy(tap.area)==tap.question);
        // ...but new writing does.
        again.setPixelColor(tap.area.center(),Qt::black); assert(again.copy(tap.area)!=tap.question);
    }
    // Fixed-offset timezones for tablets without zone data: Darwin is UTC+9:30 all year.
    const QDateTime utc(QDate(2026,10,29),QTime(10,21),QTimeZone::utc());
    assert(diaryLocalClock(utc,"+09:30").startsWith("2026-10-29 07:51 PM"));
    assert(diaryLocalClock(utc,"+0930").startsWith("2026-10-29 07:51 PM"));
    assert(diaryLocalClock(utc,"-05:00").startsWith("2026-10-29 05:21 AM"));
    if(diaryTimeZone("Australia/Darwin").isValid()) assert(diaryLocalClock(utc,"Australia/Darwin").startsWith("2026-10-29 07:51 PM"));
    assert(diaryLocalClock(utc,"+0545").startsWith("2026-10-29 04:06 PM"));   // Nepal-style quarter-hour offset
    for(auto bad:{"+25:00","09:30","+09:75","+9","Mars/Olympus"}) {bool threw=false;try{diaryLocalClock(utc,bad);}catch(const std::runtime_error&){threw=true;}assert(threw);}
    std::cout<<"PASS: capture, masking, ink search, answer budget and insertion coordinates on rM2 and Paper Pro\n";
}
