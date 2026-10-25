#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QDate>
#include <QLocale>
#include <stdexcept>

inline QString diaryTemplateLiteral(QString value) {
    value.replace('\r',' ');value.replace('\n',' ');
    for(QChar ch:QString("\\`*_{}[]<>|#!~"))value.replace(QString(ch),QString("\\")+ch);
    return value;
}
inline int diaryTemplateCellLines(const QJsonObject &t) {
    double lines=t.value("cell_lines").toDouble(1);
    if(!(lines>=1&&lines<=10)||lines!=int(lines))throw std::runtime_error("Cell height must be 1 to 10 handwriting lines");
    return int(lines);
}
inline QString diaryTemplateMarkdown(const QString &kind,const QJsonObject &t) {
    if(!t.value("labels").isArray()||!t.value("columns").isArray()||!t.value("rows").isArray()||!t.value("title").isString())
        throw std::runtime_error("Invalid template fields; please retry");
    double n=t.value("count").toDouble(-1);
    if(!(n>=1&&n<=50))throw std::runtime_error("Template needs between 1 and 50 rows");
    int count=int(n);
    if(n!=count)throw std::runtime_error("Template needs between 1 and 50 rows");
    QString out;QString title=t.value("title").toString();
    if(!title.isEmpty())out="# "+diaryTemplateLiteral(title)+"\n\n";
    if(kind=="checklist") {
        auto labels=t.value("labels").toArray();
        if(!labels.isEmpty()&&labels.size()!=count)throw std::runtime_error("Checklist label count does not match requested rows; please retry");
        for(int i=0;i<count;++i){
            if(!labels.isEmpty()&&!labels[i].isString())throw std::runtime_error("Invalid checklist label");
            out+="- [ ] "+(labels.isEmpty()?QString("\u200b"):diaryTemplateLiteral(labels[i].toString()))+"\n";
        }
    } else if(kind=="table") {
        auto columns=t.value("columns").toArray();auto rows=t.value("rows").toArray();
        if(columns.isEmpty()||columns.size()>8)throw std::runtime_error("Table needs 1 to 8 columns");
        if(!rows.isEmpty()&&rows.size()!=count)throw std::runtime_error("Table row count does not match requested rows; please retry");
        QStringList header,rule;
        for(auto col:columns){if(!col.isString())throw std::runtime_error("Invalid table column");header<<diaryTemplateLiteral(col.toString());rule<<"---";}
        out+="| "+header.join(" | ")+" |\n| "+rule.join(" | ")+" |\n";
        for(int i=0;i<count;++i){
            if(!rows.isEmpty()&&(!rows[i].isArray()||rows[i].toArray().size()!=columns.size()))throw std::runtime_error("Table cells do not match columns; please retry");
            auto row=rows.isEmpty()?QJsonArray():rows[i].toArray();QStringList cells;
            for(int j=0;j<columns.size();++j){if(!row.isEmpty()&&!row[j].isString())throw std::runtime_error("Invalid table cell");QString cell=row.isEmpty()?QString():row[j].toString();cells<<(cell.isEmpty()?QString("\u200b"):diaryTemplateLiteral(cell));}
            out+="| "+cells.join(" | ")+" |\n";
        }
    }else throw std::runtime_error("Unknown template kind");
    if(out.size()>16000)throw std::runtime_error("Template too large; request fewer rows");
    return out;
}
inline QJsonObject diaryTemplateSchema() {
    QJsonObject str{{"type","string"}};
    QJsonObject strings{{"type","array"},{"items",str}};
    return {{"type","object"},{"additionalProperties",false},
        {"properties",QJsonObject{{"title",str},{"cell_lines",QJsonObject{{"type","integer"}}},{"count",QJsonObject{{"type","integer"}}},{"labels",strings},{"columns",strings},{"rows",QJsonObject{{"type","array"},{"items",strings}}}}},
        {"required",QJsonArray{"title","count","labels","columns","rows","cell_lines"}}};
}

inline QJsonObject diaryCalendarSchema() {
    QJsonObject props;
    for(auto name:{"year","month","week_start"})props.insert(name,QJsonObject{{"type","integer"}});
    return {{"type","object"},{"properties",props},{"additionalProperties",false},{"required",QJsonArray{"year","month","week_start"}}};
}
inline QJsonObject diaryCalendarTemplate(const QJsonObject &calendar,int cellLines) {
    auto integer=[&](const char *name,int lo,int hi){double n=calendar.value(name).toDouble(-1);if(n<lo||n>hi||n!=int(n))throw std::runtime_error("Invalid calendar month, year or week start");return int(n);};
    int year=integer("year",1,9999),month=integer("month",1,12),start=integer("week_start",1,7);
    if(start!=1&&start!=7)throw std::runtime_error("Calendar weeks must start Monday or Sunday");
    QDate first(year,month,1);int offset=(first.dayOfWeek()-start+7)%7;
    QJsonArray columns,rows;QLocale locale(QLocale::English);
    for(int i=0;i<7;++i)columns.append(locale.dayName((start-1+i)%7+1,QLocale::ShortFormat));
    int weeks=(offset+first.daysInMonth()+6)/7;
    for(int row=0;row<weeks;++row){QJsonArray cells;for(int col=0;col<7;++col){int day=row*7+col-offset+1;cells.append(day>=1&&day<=first.daysInMonth()?QString::number(day):QString());}rows.append(cells);}
    return {{"title",locale.monthName(month)+" "+QString::number(year)},{"labels",QJsonArray{}},{"columns",columns},{"rows",rows},{"count",weeks},{"cell_lines",cellLines}};
}
