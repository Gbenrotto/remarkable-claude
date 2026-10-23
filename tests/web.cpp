#include "../src/webanswer.h"
#include "../src/plot.h"
#include "../src/freshness.h"
#include <QGuiApplication>
#include <QFile>
#include <QFontDatabase>
#include <cassert>
#include <iostream>
int main(int argc,char **argv){
 QGuiApplication app(argc,argv);
 auto example=QDateTime::fromString("2026-10-20T01:42:00Z",Qt::ISODate);
 auto local=diaryLocalClock(example);assert(local.contains("2026-10-19 09:42 PM EDT"));
 assert(local.contains("-04:00"));
 auto winter=diaryLocalClock(QDateTime::fromString("2026-12-20T02:42:00Z",Qt::ISODate));assert(winter.contains("2026-12-19 09:42 PM EST"));assert(winter.contains("-05:00"));
 auto spring=diaryLocalClock(QDateTime::fromString("2026-03-08T07:00:00Z",Qt::ISODate));assert(spring.contains("03:00 AM EDT"));
 auto fall=diaryLocalClock(QDateTime::fromString("2026-11-01T06:00:00Z",Qt::ISODate));assert(fall.contains("01:00 AM EST"));
 assert(diaryCurrentContext(example,"America/New_York").contains("2026-10-19 09:42 PM EDT"));
 for(const auto &question:{"Who is the current president?","Who is the US president today?","Who is the president of France?","What happened recently?","Search lunar eclipses"}) {
  DiaryAnswer a;a.command=question;a.text="An old answer";diaryRequireFreshSearch(a);assert(a.search&&a.text==question);
 }
 for(const auto &question:{"Who was president in 1990?","Explain electric current","Why does the Moon shine?"}) {
  DiaryAnswer a;a.command=question;diaryRequireFreshSearch(a);assert(!a.search);
 }
 DiaryAnswer graph;graph.plot=true;graph.command="plot y=x today";diaryRequireFreshSearch(graph);assert(!graph.search);
 auto timedPayload=diarySearchPayload("Weather in Lowell MA","gpt-4.1-mini","America/New_York",example);
 assert(timedPayload["instructions"].toString().contains("2026-10-19 09:42 PM EDT"));
 auto payload=diarySearchPayload("Search today's weather in Boston","gpt-4.1-mini");
 assert(payload.value("tool_choice")=="required");assert(!payload.contains("text"));
 auto route=decodeDiaryAnswer(R"JSON({"kind":"search","text":"weather in Boston today"})JSON");assert(route.search&&!route.plot);
 auto root=QJsonDocument::fromJson(R"JSON({"status":"completed","output":[{"type":"web_search_call","status":"completed"},{"type":"message","content":[{"type":"output_text","text":"A fact.","annotations":[{"type":"url_citation","start_index":0,"end_index":7,"url":"https://example.org/a?x=1&y=2","title":"A <source>"}]}]}]})JSON").object();
 auto result=parseDiarySearch(root);assert(result.text=="A fact. [1]");assert(result.sources.size()==1);
 result.checkedAt=local;assert(diarySourcesText(result).contains("Checked at: "+local));
 auto html=diarySourcesHtml(result);assert(html.contains("09:42 PM EDT"));assert(html.contains("href=\"https://example.org/a?x=1&amp;y=2\""));assert(!html.contains("A <source>"));
 assert(diarySourcesText(result).contains("https://example.org/a"));
 auto output=root.value("output").toArray();output.removeAt(0);root.insert("output",output);
 bool rejected=false;try{parseDiarySearch(root);}catch(...){rejected=true;}assert(rejected);
 root=QJsonDocument::fromJson(R"JSON({"status":"completed","output":[{"type":"web_search_call","status":"completed"},{"type":"message","content":[{"type":"output_text","text":"Unverified","annotations":[]}]}]})JSON").object();
 result=parseDiarySearch(root);assert(result.text=="Unverified");assert(result.sources.isEmpty());
 assert(diarySourcesText(result).contains("no source links"));assert(diarySourcesHtml(result).contains("no source links"));
 // Hosted weather's uncited UTC preamble must not override the local clock.
 auto weatherRoot=root;auto wo=weatherRoot["output"].toArray();auto wm=wo[1].toObject();auto wc=wm["content"].toArray();auto wt=wc[0].toObject();
 wt["text"]="As of 1:42 AM on Tuesday, October 20, 2026, the weather is clear.\n\n## Weather for Lowell, MA:\nCurrent Conditions: Clear, 41 F";
 wc[0]=wt;wm["content"]=wc;wo[1]=wm;weatherRoot["output"]=wo;
 auto weather=parseDiarySearch(weatherRoot);assert(!weather.text.contains("1:42"));assert(weather.text.startsWith("Weather for Lowell"));assert(weather.text.contains("41 F"));
 weather.checkedAt=local;assert(diarySourcesText(weather).contains("09:42 PM EDT"));
 // Real API citation style: replace the link markup with a compact marker.
 root=QJsonDocument::fromJson(R"JSON({"status":"completed","output":[{"type":"web_search_call","status":"completed"},{"type":"message","content":[{"type":"output_text","text":"Fact. ([example.org](https://example.org))","annotations":[{"type":"url_citation","start_index":6,"end_index":42,"url":"https://example.org","title":""}]}]}]})JSON").object();
 // Derive the fixture's end from its actual text to avoid incidental counting errors.
 auto items=root["output"].toArray();auto message=items[1].toObject();auto contents=message["content"].toArray();auto c=contents[0].toObject();auto annotations=c["annotations"].toArray();auto citation=annotations[0].toObject();citation["end_index"]=c["text"].toString().size();annotations[0]=citation;c["annotations"]=annotations;contents[0]=c;message["content"]=contents;items[1]=message;root["output"]=items;
 result=parseDiarySearch(root);assert(result.text=="Fact. [1]");assert(result.sources[0].title=="example.org");
 if(argc>1){
  QFile input(QString::fromLocal8Bit(argv[1]));assert(input.open(QIODevice::ReadOnly));
  auto live=parseDiarySearch(QJsonDocument::fromJson(input.readAll()).object());live.checkedAt=local;
  int id=QFontDatabase::addApplicationFont("fonts/Caveat.ttf");assert(id>=0);
  QFont font(QFontDatabase::applicationFontFamilies(id).first());font.setPixelSize(48);font.setWeight(QFont::Medium);QFontMetrics fm(font);
  for(auto ch:live.text)assert(ch.isSpace()||fm.inFont(ch));
  QRect box=fm.boundingRect(QRect(0,0,1150,10000),Qt::TextWordWrap,live.text);
  QFont sourceFont("DejaVu Sans");sourceFont.setPixelSize(23);QFontMetrics sm(sourceFont);
  QString sources=diarySourcesText(live);QRect sb=sm.boundingRect(QRect(0,0,1150,10000),Qt::TextWrapAnywhere,sources);
  QImage image(1174,box.height()+54+sb.height(),QImage::Format_ARGB32);image.fill(Qt::white);QPainter p(&image);p.setPen(Qt::black);p.setFont(font);p.drawText(QRect(12,8,1150,box.height()+8),Qt::TextWordWrap,live.text);p.setFont(sourceFont);p.drawText(QRect(12,box.height()+34,1150,sb.height()+8),Qt::TextWrapAnywhere,sources);p.end();assert(image.save(argc>2?QString::fromLocal8Bit(argv[2]):QString("web-answer-preview.png")));
 }
 std::cout<<"PASS: Eastern date rollover and DST, checked-at timestamp, search routing, required tool, real citation metadata, escaped links and missing-source notice/no-search rejection\n";
}
