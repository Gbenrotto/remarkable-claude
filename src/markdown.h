#pragma once
#include <QAbstractTextDocumentLayout>
#include <QImage>
#include <QPainter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTextLayout>
#include <QUrl>
#include <QRegularExpression>
#include <cmath>
#include <functional>
#include <stdexcept>

// Markdown is content, never a request to read local files or fetch resources.
class DiaryMarkdownDocument : public QTextDocument {
protected:
    QVariant loadResource(int, const QUrl &) override { return QVariant(); }
};
// Empty task items need content for Qt's Markdown parser to retain the marker.
// A zero-width space reserves a normal-height blank line without a visible label.
inline QString diaryBlankChecklistMarkdown(const QString &text) {
    QStringList lines=text.split('\n');QChar fence;int fenceSize=0;
    static const QRegularExpression fenceLine(R"(^ {0,3}(`{3,}|~{3,})(.*)$)");
    static const QRegularExpression task(R"(^([ ]{0,3}(?:[-+*]|[0-9]+[.)]) +\[[ xX]\])[ \t]*$)");
    static const QRegularExpression bare(R"(^[ ]{0,3}(\[[ xX]\]|☐|□|☑|☒)[ \t]*$)");
    for(QString &line:lines) {
        QString clean=line; if(clean.endsWith('\r'))clean.chop(1);
        auto f=fenceLine.match(clean);
        if(!fence.isNull()) {
            if(f.hasMatch()&&f.captured(1).front()==fence&&f.captured(1).size()>=fenceSize&&f.captured(2).trimmed().isEmpty())fence=QChar();
            continue;
        }
        if(f.hasMatch()){fence=f.captured(1).front();fenceSize=f.captured(1).size();continue;}
        auto t=task.match(clean);
        if(t.hasMatch()){line=t.captured(1)+" \u200b";continue;}
        auto b=bare.match(clean);
        if(b.hasMatch()){
            QString mark=b.captured(1);bool checked=mark.contains('x',Qt::CaseInsensitive)||mark=="☑"||mark=="☒";
            line=QString(checked?"- [x] ":"- [ ] ")+"\u200b";
        }
    }
    return lines.join('\n');
}
struct DiaryMarkdownImage { QImage image; QString html; };
inline DiaryMarkdownImage renderDiaryMarkdown(const QString &text, const QString &family,
                                              int width, const QString &footer=QString(), const QStringList &sourceUrls={}, int cellLines=1) {
    if(text.size()>16000 || width<300 || width>1600 || cellLines<1 || cellLines>10)
        throw std::runtime_error("Markdown answer exceeds size limit");
    DiaryMarkdownDocument doc;
    QFont body(family); body.setPixelSize(48); body.setWeight(QFont::Medium);
    doc.setDefaultFont(body); doc.setDocumentMargin(16); doc.setIndentWidth(44);
    QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    doc.setDefaultTextOption(option);
    doc.setMarkdown(diaryBlankChecklistMarkdown(text),QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub)|QTextDocument::MarkdownNoHTML);
    // Replace image objects with explicit descriptions. No broken-image icons or
    // implicit network access; generated function plots have their own renderer.
    QVector<QPair<int,QTextImageFormat>> images;
    for(auto b=doc.begin();b.isValid();b=b.next())
        for(auto it=b.begin();!it.atEnd();++it) {
            auto f=it.fragment();
            if(f.charFormat().isImageFormat()) images.append({f.position(),f.charFormat().toImageFormat()});
        }
    for(auto it=images.crbegin();it!=images.crend();++it) {
        QTextCursor c(&doc);c.setPosition(it->first);c.movePosition(QTextCursor::NextCharacter,QTextCursor::KeepAnchor);
        QString alt=it->second.stringProperty(QTextFormat::ImageAltText);
        c.insertText("[Image: "+(alt.isEmpty()?QString("external image"):alt)+"]",QTextCharFormat());
    }
    for(auto b=doc.begin();b.isValid();b=b.next()) {
        QTextCursor c(b); auto bf=b.blockFormat();
        bool code=bf.hasProperty(QTextFormat::BlockCodeFence)||bf.hasProperty(QTextFormat::BlockCodeLanguage);
        bool table=c.currentTable()!=nullptr;
        bf.setNonBreakableLines(false);bf.setTopMargin(6);bf.setBottomMargin(10);
        if(bf.intProperty(QTextFormat::BlockQuoteLevel)>0) bf.setLeftMargin(36*bf.intProperty(QTextFormat::BlockQuoteLevel));
        c.setBlockFormat(bf);
        c.select(QTextCursor::BlockUnderCursor);
        QTextCharFormat format;format.setForeground(Qt::black);
        if(table||code){format.setFontFamilies({code?"DejaVu Sans Mono":"DejaVu Sans"});format.setProperty(QTextFormat::FontPixelSize,28);}
        if(code){format.setBackground(QColor(235,235,235));}
        int level=bf.headingLevel();
        if(level){format.setProperty(QTextFormat::FontPixelSize, qMax(40,68-level*4));format.setFontWeight(QFont::Bold);}
        c.mergeCharFormat(format);
        for(auto it=b.begin();!it.atEnd();++it) {
            auto f=it.fragment();auto cf=f.charFormat();QTextCharFormat change;
            if(cf.fontFixedPitch()){change.setFontFamilies({"DejaVu Sans Mono"});change.setProperty(QTextFormat::FontPixelSize,28);change.setBackground(QColor(235,235,235));}
            if(cf.isAnchor()) {
                QString scheme=QUrl(cf.anchorHref()).scheme().toLower();
                if(scheme!="https"&&scheme!="http"&&scheme!="mailto") {change.setAnchor(false);change.setAnchorHref(QString());}
                change.setForeground(Qt::black);change.setFontUnderline(true);
            }
            QTextCursor span(&doc);span.setPosition(f.position());span.setPosition(f.position()+f.length(),QTextCursor::KeepAnchor);span.mergeCharFormat(change);
        }
    }
    // Equal, bounded column widths prevent long cells from widening the image.
    std::function<void(QTextFrame*)> tables=[&](QTextFrame *frame){
        for(auto child:frame->childFrames()) {
            if(auto table=dynamic_cast<QTextTable*>(child)) {
                if(table->columns()>12) throw std::runtime_error("Table too wide; ask for fewer columns");
                auto tf=table->format();tf.setWidth(QTextLength(QTextLength::PercentageLength,100));
                tf.setBorderCollapse(true);tf.setBorder(3);tf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);tf.setCellPadding(8);tf.setCellSpacing(0);
                QList<QTextLength> cols;for(int i=0;i<table->columns();++i)cols.append(QTextLength(QTextLength::PercentageLength,100.0/table->columns()));
                tf.setColumnWidthConstraints(cols);table->setFormat(tf);
                for(int r=0;r<table->rows();++r)for(int col=0;col<table->columns();++col){
                    auto cell=table->cellAt(r,col);auto cf=cell.format().toTableCellFormat();
                    if(r>0 && cellLines>1)cf.setBottomPadding(8+(cellLines-1)*60);
                    cf.setBorder(3);cf.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);cf.setBorderBrush(Qt::black);cell.setFormat(cf);
                }
            }
            tables(child);
        }
    };tables(doc.rootFrame());
    if(!footer.isEmpty()) {
        QTextCursor c(&doc);c.movePosition(QTextCursor::End);c.insertBlock(QTextBlockFormat(),QTextCharFormat());
        QTextCharFormat small;small.setFontFamilies({"DejaVu Sans"});small.setProperty(QTextFormat::FontPixelSize,23);small.setForeground(Qt::black);
        c.insertText("\n"+footer,small);
    }
    for(int i=0;i<sourceUrls.size();++i) {
        const QString url=sourceUrls[i];
        if(QUrl(url).scheme()!="https"&&QUrl(url).scheme()!="http")continue;
        for(const QString &needle:QStringList{QString("[%1]").arg(i+1),url}) {
            QTextCursor found(&doc);
            while(!(found=doc.find(needle,found)).isNull()) {
                QTextCharFormat link;link.setAnchor(true);link.setAnchorHref(url);link.setForeground(Qt::black);link.setFontUnderline(true);found.mergeCharFormat(link);
            }
        }
    }
    doc.setTextWidth(width);
    QSizeF size=doc.documentLayout()->documentSize();
    if(size.width()>width+1 || !std::isfinite(size.height()) || size.height()>8000)
        throw std::runtime_error("Formatted answer too large; ask for a shorter answer or narrower table");
    QImage image(width,qMax(1,int(std::ceil(size.height()))),QImage::Format_ARGB32_Premultiplied);
    if(image.isNull())throw std::runtime_error("Cannot allocate formatted answer image");
    image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    QAbstractTextDocumentLayout::PaintContext context;context.palette.setColor(QPalette::Text,Qt::black);
    doc.documentLayout()->draw(&p,context);
    p.setPen(QPen(Qt::black,3));
    for(auto b=doc.begin();b.isValid();b=b.next())if(b.blockFormat().intProperty(QTextFormat::BlockQuoteLevel)>0){
        QRectF r=doc.documentLayout()->blockBoundingRect(b);p.drawLine(QPointF(r.left()-12,r.top()),QPointF(r.left()-12,r.bottom()));
    }
    for(auto b=doc.begin();b.isValid();b=b.next()) {
        if(b.blockFormat().hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
            auto r=doc.documentLayout()->blockBoundingRect(b);p.setPen(QPen(Qt::black,3));
            p.drawLine(QPointF(r.left(),r.center().y()),QPointF(r.right(),r.center().y()));
        }
        auto marker=b.blockFormat().marker();
        if(!b.textList()||marker==QTextBlockFormat::MarkerType::NoMarker||!b.layout()||!b.layout()->lineCount())continue;
        auto line=b.layout()->lineAt(0);auto bounds=doc.documentLayout()->blockBoundingRect(b);
        qreal textX=bounds.left()+line.x();qreal top=bounds.top()+line.y();
        // Replace Qt's hairline task marker, retaining list indentation and labels.
        p.save();p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.fillRect(QRectF(textX-55,top-2,51,line.height()+4),Qt::transparent);p.restore();
        QRectF box(textX-44,top+(line.height()-36)/2,36,36);
        p.setPen(QPen(Qt::black,3));p.setBrush(Qt::NoBrush);p.drawRect(box);
        if(marker==QTextBlockFormat::MarkerType::Checked){p.drawLine(box.topLeft()+QPointF(6,6),box.bottomRight()-QPointF(6,6));p.drawLine(box.topRight()+QPointF(-6,6),box.bottomLeft()+QPointF(6,-6));}
    }
    p.end();return {image,doc.toHtml()};
}
