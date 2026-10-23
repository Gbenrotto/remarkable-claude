#include "../src/plot.h"
#include "../src/freshness.h"
#include "../src/markdown.h"
#include <QGuiApplication>
#include <cassert>
#include <iostream>
int main(int argc,char **argv){QGuiApplication app(argc,argv);
for(int year:{1900,2000,2024,2026,2100})for(int month=1;month<=12;++month)for(int start:{1,7}){
    auto t=diaryCalendarTemplate({{"year",year},{"month",month},{"week_start",start}},3);auto rows=t["rows"].toArray();int expected=1;
    for(int r=0;r<rows.size();++r)for(int c=0;c<7;++c){auto text=rows[r].toArray()[c].toString();if(text.isEmpty())continue;int day=text.toInt();assert(day==expected++);assert(QDate(year,month,day).dayOfWeek()==(start-1+c)%7+1);}
    assert(expected==QDate(year,month,1).daysInMonth()+1);
}
auto t=diaryCalendarTemplate({{"year",2026},{"month",10},{"week_start",7}},3);assert(t["rows"].toArray()[0].toArray()[4]=="1");assert(t["rows"].toArray()[4].toArray()[6]=="31");
auto raw=QJsonDocument(QJsonObject{{"kind","calendar"},{"calendar",QJsonObject{{"year",2026},{"month",10},{"week_start",7}}},{"template",QJsonObject{{"cell_lines",3}}},{"command","calendar this month"}}).toJson();auto a=decodeDiaryAnswer(raw);diaryRequireFreshSearch(a);assert(a.calendar&&!a.search&&a.cellLines==3);
auto rendered=renderDiaryMarkdown(a.text,"DejaVu Sans",1174,QString(),{},a.cellLines);QImage preview(rendered.image.size(),QImage::Format_RGB32);preview.fill(Qt::white);{QPainter p(&preview);p.drawImage(0,0,rendered.image);}assert(preview.save("october-calendar-preview.png"));
for(auto bad:{QJsonObject{{"year",2026},{"month",13},{"week_start",7}},QJsonObject{{"year",2026},{"month",10},{"week_start",2}},QJsonObject{{"year",2026.5},{"month",10},{"week_start",7}}}){bool rejected=false;try{diaryCalendarTemplate(bad,3);}catch(...){rejected=true;}assert(rejected);}
std::cout<<"PASS: calendar dates, Sunday/Monday starts, leap years, October 2026 Thursday start, routing and rendering\n";
}
