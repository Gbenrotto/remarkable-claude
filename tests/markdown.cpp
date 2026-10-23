#include <QGuiApplication>
#include <QFontDatabase>
#include <QFile>
#include <cassert>
#include <iostream>
#include "../src/markdown.h"
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 int id=QFontDatabase::addApplicationFont("fonts/Caveat.ttf");assert(id>=0);
 assert(QFontDatabase::addApplicationFont("fonts/DejaVuSansMono.ttf")>=0);
 QString family=QFontDatabase::applicationFontFamilies(id).first();
 QString sample=QString::fromUtf8(R"(# Markdown on your notebook
A **bold idea**, *a little emphasis*, and ~~a correction~~. Inline code: `x = 2`.

## A small comparison
| Feature | Before | Now |
| :--- | :---: | ---: |
| Headings | ## marks | Larger text |
| Tables | Plain text | Aligned cells |
| Code | Handwriting | Monospace |

### Lists and tasks
1. Write a question.
2. Tap the ChatGPT icon.
   - The answer appears below.
   - Scroll down to read more.

- [x] Keep the fast renderer
- [ ] Try another question

> A quote can span multiple lines and still have a clear margin.

```python
# Markdown inside code stays literal: **bold**
for x in range(3):
    print(x * x)
```

---
[OpenAI](https://openai.com) • Escaped \*asterisks\* • café
)");
 auto out=renderDiaryMarkdown(sample,family,1174,"Checked at: 2026-10-20 10:00 AM EDT\nSources\n[1] Example source\nhttps://example.com");
 assert(!out.image.isNull()&&out.image.width()==1174&&out.image.height()>1000);
 assert(out.html.contains("<table")&&out.html.contains("<h1")&&out.html.contains("<ol")&&out.html.contains("DejaVu Sans Mono"));
 assert(out.html.contains("**bold**")&&out.html.contains("href=\"https://openai.com\""));
 QImage preview(out.image.size(),QImage::Format_RGB32);preview.fill(Qt::white);QPainter painter(&preview);painter.drawImage(0,0,out.image);painter.end();assert(preview.save("markdown-preview.png"));
 auto unsafe=renderDiaryMarkdown("![private](file:///etc/passwd)\n\n[bad](javascript:alert)\n\n<script>alert(1)</script>",family,1174);
 assert(!unsafe.html.contains("<img")&&!unsafe.html.contains("href=\"javascript:")&&!unsafe.html.contains("<script"));
 auto wide=renderDiaryMarkdown("```\n"+QString(150,'x')+"\n```\n\n| Long | Short |\n|---|---|\n|"+QString(200,'a')+"|ok|",family,1174);
 assert(wide.image.width()==1174);
 auto cited=renderDiaryMarkdown("A sourced **answer** [1]",family,1174,"[1] Source\nhttps://example.com",{"https://example.com"});
 assert(cited.html.contains("href=\"https://example.com\""));
 // Blank items must remain ten distinct task blocks and ten visible rows.
 for(const QString &item:QStringList{"- [ ]","- [ ] ","☐","[ ]","- [x]"}) {
  QStringList items;for(int i=0;i<10;++i)items.append(item);
  QString input=items.join('\n');DiaryMarkdownDocument tasks;tasks.setMarkdown(diaryBlankChecklistMarkdown(input),QTextDocument::MarkdownDialectGitHub);
  int markers=0;for(auto b=tasks.begin();b.isValid();b=b.next())if(b.blockFormat().marker()!=QTextBlockFormat::MarkerType::NoMarker)++markers;
  assert(markers==10);auto checklist=renderDiaryMarkdown(input,family,1174);assert(checklist.image.height()>500);
  int rows=0;bool previous=false;for(int y=0;y<checklist.image.height();++y){bool ink=false;for(int x=0;x<100;++x){auto px=checklist.image.pixelColor(x,y);if(px.alpha()>100&&px.red()<100){ink=true;break;}}if(ink&&!previous)++rows;previous=ink;}assert(rows==10);
  if(item=="- [ ]") {QImage preview(checklist.image.size(),QImage::Format_RGB32);preview.fill(Qt::white);QPainter p(&preview);p.drawImage(0,0,checklist.image);p.end();assert(preview.save("checklist-preview.png"));}
 }
 for(const QString &literal:QStringList{"```markdown\n- [ ]\n☐\n```","~~~\n- [ ]\n~~~","    - [ ]","- [ ] Existing label","☐ ☐ ☐"})assert(diaryBlankChecklistMarkdown(literal)==literal);
 bool rejected=false;try{renderDiaryMarkdown(QString(16001,'x'),family,1174);}catch(...){rejected=true;}assert(rejected);
 std::cout<<"Markdown layout and safety checks passed; preview "<<out.image.height()<<" pixels tall\n";
}
