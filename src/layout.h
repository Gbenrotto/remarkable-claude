#pragma once
#include <QImage>
#include <QRect>
#include <QSize>
#include <QList>
#include <array>
#include <vector>
#include <algorithm>
// Supported portrait screens. The reMarkable 2 and the Paper Pro run the same notebook
// QML at almost the same pixel density (226 vs 229 ppi), so the toolbar, the close button
// cell and the bottom margin keep their pixel sizes; only the page area grows.
inline QSize diaryRm2Screen() { return QSize(1404,1872); }
inline QSize diaryPaperProScreen() { return QSize(1620,2160); }
inline bool diarySupportedScreen(const QSize &screen) { return screen==diaryRm2Screen() || screen==diaryPaperProScreen(); }
inline const char *diaryScreenName(const QSize &screen) {
    return screen==diaryPaperProScreen() ? "reMarkable Paper Pro" : screen==diaryRm2Screen() ? "reMarkable 2" : "unsupported screen";
}
// Include the top edge; only the side toolbar (150 px) and bottom button margin (120 px)
// are excluded, plus an 80 px strip on the right under our own button column.
inline QRect diaryCaptureArea(const QSize &screen=diaryRm2Screen()) { return QRect(150,0,screen.width()-230,screen.height()-120); }
// The Claude button (inkDiary.qmd): one toolbar cell under the close button, anchored top-right.
inline QRect diaryButtonRect(const QSize &screen=diaryRm2Screen()) { return QRect(screen.width()-112,112,112,112); }
// The Send button (inkDiary.qmd) appears left of it while email drafts are pending.
inline QRect diarySendButtonRect(const QSize &screen=diaryRm2Screen()) { return QRect(screen.width()-224,112,124,112); }
// Vertical pixel budget for one answer image: everything below the question, less the margins.
inline int diaryAnswerBudget(const QSize &screen=diaryRm2Screen()) { return screen.height()-290; }
// Keep our own buttons out of the page photo, the ink search and the unchanged-page check.
inline void diaryMaskControls(QImage &screen) {
    for(const QRect &r:{diaryButtonRect(screen.size()),diarySendButtonRect(screen.size())})
        for(int y=r.top();y<=r.bottom()&&y<screen.height();++y)
            for(int x=r.left();x<=r.right()&&x<screen.width();++x)
                screen.setPixelColor(x,y,Qt::white);
}
// The masked cells, in the coordinates of a capture-area crop, so the ink search can leave
// them out of its template-rule statistics instead of seeing a rule cut short by white.
inline QList<QRect> diaryMaskedInCapture(const QSize &screen) {
    QList<QRect> out;const QRect area=diaryCaptureArea(screen);
    for(const QRect &r:{diaryButtonRect(screen),diarySendButtonRect(screen)}) {
        QRect inside=r.intersected(area);
        if(!inside.isEmpty()) out.append(inside.translated(-area.topLeft()));
    }
    return out;
}
// A pale, nearly uniform full-width/height line is a template rule.
// Keep darker strokes on that line, and never filter black rules/content.
inline int ruleThreshold(const std::array<int,256> &histogram, int length) {
    int mode=int(std::max_element(histogram.begin(),histogram.end())-histogram.begin());
    if(mode<100 || mode>=210) return 210;
    int uniform=0;
    for(int i=std::max(0,mode-8);i<=std::min(255,mode+8);++i) uniform+=histogram[i];
    return uniform*100>=length*90 ? mode-40 : 210;
}
// ignore: regions (our own buttons) that are neither ink nor evidence about the template.
inline int lastInkRow(const QImage &source, const QList<QRect> &ignore={}) {
    if(source.isNull()) return -1;
    QImage image=source.convertToFormat(QImage::Format_RGB32);
    const int W=image.width(),H=image.height();
    std::vector<unsigned char> skip(size_t(W)*H,0);
    std::vector<int> rowLength(H,W),colLength(W,H);
    for(const QRect &r:ignore) {
        const QRect c=r.intersected(image.rect());
        for(int y=c.top();y<=c.bottom();++y) for(int x=c.left();x<=c.right();++x) {
            unsigned char &s=skip[size_t(y)*W+x];
            if(!s) {s=1;--rowLength[y];--colLength[x];}
        }
    }
    std::vector<int> rows(H,210),columns(W,210);
    std::vector<std::array<int,256>> colHist(W);
    for(int y=0;y<H;++y) {
        std::array<int,256> hist{};
        const QRgb *pixels=reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for(int x=0;x<W;++x) {if(skip[size_t(y)*W+x]) continue;int gray=qGray(pixels[x]);++hist[gray];++colHist[x][gray];}
        rows[y]=rowLength[y]>0 ? ruleThreshold(hist,rowLength[y]) : 210;
    }
    for(int x=0;x<W;++x) columns[x]=colLength[x]>0 ? ruleThreshold(colHist[x],colLength[x]) : 210;
    // An opaque plot can hide the middle of a template margin. Recognize
    // remaining long, thin pale segments rather than demanding a full-height
    // line. Keep this fallback vertical-only so pale handwritten text survives.
    std::vector<int> partial(image.width(),210);
    for(int x=0;x<image.width();++x) {
        if(columns[x]<210) continue;
        const auto &hist=colHist[x];
        int mode=100+int(std::max_element(hist.begin()+100,hist.begin()+210)-(hist.begin()+100));
        int count=0,run=0,longest=0;
        for(int y=0;y<image.height();++y) {
            if(skip[size_t(y)*W+x]) continue;
            int gray=qGray(image.pixel(x,y));
            if(std::abs(gray-mode)<=8) ++count;
            // Horizontal rule intersections darken the same vertical line.
            if(gray>=mode-60 && gray<=mode+8) {++run;longest=std::max(longest,run);}
            else run=0;
        }
        if(count*5>=image.height() && longest>=std::max(64,image.height()/10)) partial[x]=mode-40;
    }
    // A template margin is a thin line, not a shaded region or broad marker.
    for(int x=0;x<image.width();) {
        if(partial[x]==210) {++x;continue;}
        int end=x+1;while(end<image.width() && partial[end]<210)++end;
        if(end-x<=4)for(int i=x;i<end;++i)columns[i]=partial[i];
        x=end;
    }
    for(int y=image.height()-1;y>=0;--y) {
        const QRgb *pixels=reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for(int x=0;x<image.width();++x) {
            if(skip[size_t(y)*W+x]) continue;
            int threshold=std::min(rows[y],columns[x]);
            // Two translucent rules darken where they cross (e.g. 191 -> 143).
            if(rows[y]<210 && columns[x]<210)
                threshold=(rows[y]+40)*(columns[x]+40)/255-40;
            if(qGray(pixels[x])<threshold) return y;
        }
    }
    return -1;
}
