#include "../src/webanswer.h"
#include <QFileInfo>
#include "../src/api.h"
#include "../src/reading.h"
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
 auto timedPayload=diarySearchPayload("Weather in Lowell MA","claude-sonnet-5-5","America/New_York",example);
 assert(timedPayload["system"].toString().contains("2026-10-19 09:42 PM EDT"));
 auto payload=diarySearchPayload("Search today's weather in Boston","claude-sonnet-5-5");
 auto tool=payload["tools"].toArray()[0].toObject();assert(tool["type"]=="web_search_20250305"&&tool["name"]=="web_search");
 // Opus 5.5 / Sonnet 5.5 reject forced tool_choice; the system prompt asks for the search instead.
 assert(!payload.contains("tool_choice"));assert(!payload.contains("output_config"));
 assert(payload["messages"].toArray()[0].toObject()["content"]=="Search today's weather in Boston");
 assert(diaryWithFallbacks(payload).value("fallbacks")=="default");
 assert(!diaryWithFallbacks(diarySearchPayload("q","claude-haiku-5-5")).contains("fallbacks"));
 auto route=decodeDiaryAnswer(R"JSON({"kind":"search","text":"weather in Boston today"})JSON");assert(route.search&&!route.plot);
 // Shape of the documented Messages API web search response: preamble, search call, results, cited text.
 auto root=QJsonDocument::fromJson(R"JSON({"role":"assistant","stop_reason":"end_turn","content":[
  {"type":"text","text":"I'll search for when Claude Shannon was born."},
  {"type":"server_tool_use","id":"srvtoolu_01","name":"web_search","input":{"query":"claude shannon birth date"}},
  {"type":"web_search_tool_result","tool_use_id":"srvtoolu_01","content":[{"type":"web_search_result","url":"https://en.wikipedia.org/wiki/Claude_Shannon","title":"Claude Shannon - Wikipedia","encrypted_content":"Eqgf","page_age":"April 30, 2025"}]},
  {"type":"text","text":"Based on the search results, "},
  {"type":"text","text":"Claude Shannon was born on April 30, 1916, in Petoskey, Michigan","citations":[{"type":"web_search_result_location","url":"https://en.wikipedia.org/wiki/Claude_Shannon","title":"Claude <Shannon> - Wikipedia","encrypted_index":"Eo8B","cited_text":"Claude Elwood Shannon (April 30, 1916 ..."}]},
  {"type":"text","text":". He also studied at MIT.","citations":[{"type":"web_search_result_location","url":"https://example.org/a?x=1&y=2","title":"","encrypted_index":"x","cited_text":"..."},{"type":"web_search_result_location","url":"https://en.wikipedia.org/wiki/Claude_Shannon","title":"Claude Shannon - Wikipedia","encrypted_index":"y","cited_text":"..."}]}
 ]})JSON").object();
 auto result=parseDiarySearch(root);
 assert(result.text=="Based on the search results, Claude Shannon was born on April 30, 1916, in Petoskey, Michigan [1]. He also studied at MIT. [2][1]");
 assert(result.sources.size()==2);assert(result.sources[1].title=="example.org");
 result.checkedAt=local;assert(diarySourcesText(result).contains("Checked at: "+local));
 auto html=diarySourcesHtml(result);assert(html.contains("09:42 PM EDT"));assert(html.contains("href=\"https://example.org/a?x=1&amp;y=2\""));assert(!html.contains("Claude <Shannon>"));
 assert(diarySourcesText(result).contains("https://en.wikipedia.org/wiki/Claude_Shannon"));
 // A turn with no search result must not be shown as a web answer.
 auto content=root["content"].toArray();content.removeAt(2);root["content"]=content;
 bool rejected=false;try{parseDiarySearch(root);}catch(...){rejected=true;}assert(rejected);
 // Search errors arrive as HTTP 200 with an error object instead of a result list.
 auto failed=QJsonDocument::fromJson(R"JSON({"stop_reason":"end_turn","content":[{"type":"server_tool_use","id":"s","name":"web_search","input":{"query":"q"}},
  {"type":"web_search_tool_result","tool_use_id":"s","content":{"type":"web_search_tool_result_error","error_code":"too_many_requests"}},{"type":"text","text":"Sorry."}]})JSON").object();
 try{parseDiarySearch(failed);assert(false);}catch(const std::runtime_error &e){assert(QString(e.what()).contains("too_many_requests"));}
 // A completed search with uncited text keeps the answer and reports missing links.
 auto uncited=QJsonDocument::fromJson(R"JSON({"stop_reason":"end_turn","content":[{"type":"web_search_tool_result","tool_use_id":"s","content":[]},{"type":"text","text":"Unverified"}]})JSON").object();
 result=parseDiarySearch(uncited);assert(result.text=="Unverified");assert(result.sources.isEmpty());
 assert(diarySourcesText(result).contains("no source links"));assert(diarySourcesHtml(result).contains("no source links"));
 // pause_turn resumes by sending the paused assistant content back unchanged.
 auto paused=QJsonDocument::fromJson(R"JSON({"stop_reason":"pause_turn","content":[{"type":"server_tool_use","id":"s","name":"web_search","input":{"query":"q"}}]})JSON").object();
 rejected=false;try{parseDiarySearch(paused);}catch(...){rejected=true;}assert(rejected);
 auto resumed=diaryContinueSearch(payload,paused);auto msgs=resumed["messages"].toArray();
 assert(msgs.size()==2&&msgs[1].toObject()["role"]=="assistant"&&msgs[1].toObject()["content"]==paused["content"]);
 // Refusals and truncation surface as errors, not as answers.
 for(auto stop:{"refusal","max_tokens"}){auto r=uncited;r["stop_reason"]=stop;rejected=false;try{parseDiarySearch(r);}catch(...){rejected=true;}assert(rejected);}
 // A circled word plus "tell me about these" must scope to the marked words (2026-10-25 regression:
 // without this rule 4 of 5 live runs also answered about the unmarked basking shark).
 {auto reading=diaryReadingPayload(QByteArray("png"),QJsonObject{{"model","claude-sonnet-5-5"}})["system"].toString();
  assert(reading.contains("Handwritten circles, underlines, brackets and lines drawn from text"));assert(reading.contains("answer only about the marked part"));}
 // Live claude-sonnet-5-5 response (encrypted fields stripped): search, results, cited text split over blocks.
 {QFile live("tests/fixtures/search-tallest-building.json");assert(live.open(QIODevice::ReadOnly));
  auto tallest=parseDiarySearch(QJsonDocument::fromJson(live.readAll()).object());
  assert(tallest.sources.size()==1&&tallest.sources[0].url=="https://en.wikipedia.org/wiki/List_of_tallest_buildings");
  assert(tallest.text.startsWith("**Burj Khalifa**, Dubai, UAE."));assert(tallest.text.contains("completed in 2010. [1]"));}
 if(argc>1){
  QFile input(QString::fromLocal8Bit(argv[1]));assert(input.open(QIODevice::ReadOnly));
  auto live=parseDiarySearch(QJsonDocument::fromJson(input.readAll()).object());live.checkedAt=local;
  int id=QFontDatabase::addApplicationFont(QFileInfo("fonts/Caveat.ttf").absoluteFilePath());assert(id>=0);
  QFont font(QFontDatabase::applicationFontFamilies(id).first());font.setPixelSize(48);font.setWeight(QFont::Medium);QFontMetrics fm(font);
  for(auto ch:live.text)assert(ch.isSpace()||fm.inFont(ch));
  QRect box=fm.boundingRect(QRect(0,0,1150,10000),Qt::TextWordWrap,live.text);
  QFont sourceFont("DejaVu Sans");sourceFont.setPixelSize(23);QFontMetrics sm(sourceFont);
  QString sources=diarySourcesText(live);QRect sb=sm.boundingRect(QRect(0,0,1150,10000),Qt::TextWrapAnywhere,sources);
  QImage image(1174,box.height()+54+sb.height(),QImage::Format_ARGB32);image.fill(Qt::white);QPainter p(&image);p.setPen(Qt::black);p.setFont(font);p.drawText(QRect(12,8,1150,box.height()+8),Qt::TextWordWrap,live.text);p.setFont(sourceFont);p.drawText(QRect(12,box.height()+34,1150,sb.height()+8),Qt::TextWrapAnywhere,sources);p.end();assert(image.save(argc>2?QString::fromLocal8Bit(argv[2]):QString("web-answer-preview.png")));
 }
 std::cout<<"PASS: Eastern date rollover and DST, checked-at timestamp, search routing, search payload, citation markers, escaped links, search errors, pause_turn resume, missing-source notice/no-search rejection\n";
}
