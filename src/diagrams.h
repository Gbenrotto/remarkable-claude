#pragma once
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QMap>
#include <QSet>
#include <cmath>
#include <stdexcept>

namespace DiaryDiagram {
inline void need(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
inline QFont font(int px){QFont f("DejaVu Sans");f.setPixelSize(px);return f;}
inline void arrow(QPainter &p,const QPainterPath &path){
    p.setBrush(Qt::NoBrush);p.drawPath(path);
    auto end=path.pointAtPercent(1),before=path.pointAtPercent(.98);
    double a=std::atan2(end.y()-before.y(),end.x()-before.x());
    p.setBrush(Qt::black);p.drawPolygon(QPolygonF{end,end-QPointF(17*cos(a-.45),17*sin(a-.45)),end-QPointF(17*cos(a+.45),17*sin(a+.45))});p.setBrush(Qt::NoBrush);
}
inline void label(QPainter &p,QRectF r,const QString &text,int px=28){
    p.setFont(font(px));QFontMetrics fm(p.font());
    need(fm.boundingRect(r.toRect(),Qt::AlignCenter|Qt::TextWordWrap,text).height()<=r.height(),"Diagram label too long; ask for shorter labels");
    p.drawText(r,Qt::AlignCenter|Qt::TextWordWrap,text);
}
inline QStringList lines(QString source){
    source=source.trimmed();
    if(source.startsWith("```mermaid\n")&&source.endsWith("```"))source=source.mid(11,source.size()-14).trimmed();
    need(source.size()<=12000,"Diagram too large");
    QStringList result;for(auto l:source.split('\n')) {l=l.trimmed();if(l.isEmpty()||l.startsWith("%%"))continue;result<<l;}
    need(!result.isEmpty(),"Empty diagram");return result;
}
struct Node {QString id,text;QChar shape;QRectF box;int rank=0;};
struct Edge {int from,to;QString text;};
struct FlowGraph {QVector<Node> nodes;QVector<Edge> edges;};
inline FlowGraph parseFlow(const QStringList &source){
    // Split statements only outside labels: semicolons/arrows in quoted text are literal.
    QStringList statements;QString current;QVector<QChar> stack;bool quoted=false;
    for(QChar c:source.join('\n')){
        if(c=='"')quoted=!quoted;
        if(!quoted){
            if(c=='['||c=='('||c=='{')stack.append(c);
            if(c==']'||c==')'||c=='}'){need(!stack.isEmpty(),"Unbalanced flowchart label");QChar open=stack.takeLast();need((open=='['&&c==']')||(open=='('&&c==')')||(open=='{'&&c=='}'),"Unbalanced flowchart label");}
        }
        if(!quoted&&stack.isEmpty()&&(c==';'||c=='\n')){if(!current.trimmed().isEmpty())statements<<current.trimmed();current.clear();}else current+=c;
    }
    need(!quoted&&stack.isEmpty(),"Unclosed flowchart label");if(!current.trimmed().isEmpty())statements<<current.trimmed();
    need(!statements.isEmpty()&&QRegularExpression("^(flowchart|graph)\\s+(TD|TB)$").match(statements.takeFirst()).hasMatch(),"Supported flowcharts use top-down (TD or TB) direction");
    FlowGraph graph;QMap<QString,int> ids;QSet<QString> declared;
    for(auto line:statements){int pos=0;
        auto skip=[&]{while(pos<line.size()&&line[pos].isSpace())++pos;};
        auto readNode=[&]() -> int {
            skip();auto match=QRegularExpression("[A-Za-z][A-Za-z0-9_]*").match(line,pos,QRegularExpression::NormalMatch,QRegularExpression::AnchorAtOffsetMatchOption);
            need(match.hasMatch(),"Expected a flowchart node");QString id=match.captured();pos+=id.size();skip();
            QString text=id;QChar shape='[';bool explicitLabel=false;
            if(pos<line.size()&&(line[pos]=='['||line[pos]=='('||line[pos]=='{')){
                explicitLabel=true;shape=line[pos++];QChar close=shape=='['?']':shape=='('?')':'}';QString content;int depth=1;bool quote=false;
                while(pos<line.size()&&depth){QChar c=line[pos++];if(c=='"')quote=!quote;if(!quote){if(c==shape)++depth;if(c==close)--depth;}if(depth)content+=c;}
                need(depth==0&&!quote,"Unclosed flowchart node");content=content.trimmed();
                // Mermaid stadium ([label]) and circular ((label)) nodes use rounded outlines.
                if(shape=='('&&content.size()>=2&&((content.front()=='['&&content.back()==']')||(content.front()=='('&&content.back()==')')))content=content.mid(1,content.size()-2).trimmed();
                if(content.startsWith('"')&&content.endsWith('"')&&content.size()>=2)content=content.mid(1,content.size()-2);
                need(!content.isEmpty()&&content.size()<=100,"Flowchart label is empty or too long");text=content;
            }
            if(!ids.contains(id)){need(graph.nodes.size()<12,"Too many flowchart nodes");ids[id]=graph.nodes.size();graph.nodes.append({id,text,shape,{},0});}
            int index=ids[id];if(explicitLabel){need(!declared.contains(id)||(graph.nodes[index].text==text&&graph.nodes[index].shape==shape),"Conflicting flowchart node labels");graph.nodes[index].text=text;graph.nodes[index].shape=shape;declared.insert(id);}
            return index;
        };
        int from=readNode();skip();
        while(pos<line.size()){
            QString text;
            if(line.mid(pos,3)=="-->"){pos+=3;skip();if(pos<line.size()&&line[pos]=='|'){int end=line.indexOf('|',++pos);need(end>=pos,"Unclosed arrow label");text=line.mid(pos,end-pos).trimmed();pos=end+1;}}
            else if(line.mid(pos,2)=="--"){pos+=2;int end=line.indexOf("-->",pos);need(end>=pos,"Unsupported flowchart arrow");text=line.mid(pos,end-pos).trimmed();pos=end+3;}
            else need(false,"Unsupported flowchart syntax; use nodes connected by --> arrows");
            if(text.startsWith('"')&&text.endsWith('"')&&text.size()>=2)text=text.mid(1,text.size()-2);
            need(text.size()<=30&&graph.edges.size()<24,"Too many arrows or arrow label too long");
            int to=readNode();graph.edges.append({from,to,text});from=to;skip();
        }
    }
    need(!graph.nodes.isEmpty(),"Diagram has no nodes");return graph;
}
inline QImage flow(const QStringList &source){
    auto graph=parseFlow(source);auto &nodes=graph.nodes;auto &edges=graph.edges;
    QVector<int> incoming(nodes.size(),0);for(auto e:edges)if(e.from!=e.to)incoming[e.to]++;
    QVector<int> queue;for(int i=0;i<nodes.size();++i)if(!incoming[i])queue<<i;
    QSet<int> seen;for(int k=0;k<queue.size();++k){int a=queue[k];seen.insert(a);for(auto e:edges)if(e.from==a&&e.to!=a){nodes[e.to].rank=qMax(nodes[e.to].rank,nodes[a].rank+1);if(--incoming[e.to]==0)queue<<e.to;}}
    // Cyclic graphs use declaration order so loop edges cannot cause unbounded layout.
    if(seen.size()!=nodes.size())for(int i=0;i<nodes.size();++i)nodes[i].rank=i;
    // Wrap topological order across three columns, reversing alternate rows.
    // Arrows retain the graph semantics; long/return edges use row gaps and side gutters.
    QVector<int> order;for(int i=0;i<nodes.size();++i)order<<i;
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return nodes[a].rank<nodes[b].rank;});
    int cols=qMin(3,int(nodes.size())),rows=(nodes.size()+cols-1)/cols;
    for(int i=0;i<order.size();++i){int row=i/cols,col=i%cols;if(row%2)col=cols-1-col;nodes[order[i]].box=QRectF(80+col*365,60+row*310,260,160);}
    QImage out(1174,rows*310+30,QImage::Format_RGB32);out.fill(Qt::white);QPainter p(&out);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(Qt::black,3));
    int gutter=0;
    for(auto e:edges){auto a=nodes[e.from].box,b=nodes[e.to].box;QPainterPath path;QRectF textBox;
        if(a.top()==b.top()&&qAbs(a.center().x()-b.center().x())==365){
            bool right=b.left()>a.left();auto start=QPointF(right?a.right():a.left(),a.center().y()),end=QPointF(right?b.left():b.right(),b.center().y());
            path.moveTo(start);path.lineTo(end);textBox=QRectF((start.x()+end.x())/2-65,start.y()-65,130,58);
        }else if(a.center().x()==b.center().x()&&b.top()-a.top()==310){
            path.moveTo(a.center().x(),a.bottom());path.lineTo(b.center().x(),b.top());textBox=QRectF(a.center().x()+8,a.bottom()+35,135,58);
        }else{
            int lane=gutter++%5;double gx=15+lane*11,sy=a.bottom()+35+lane*12,ty=b.top()-25;
            path.moveTo(a.center().x(),a.bottom());path.lineTo(a.center().x(),sy);path.lineTo(gx,sy);path.lineTo(gx,ty);path.lineTo(b.center().x(),ty);path.lineTo(b.center().x(),b.top());
            textBox=QRectF(gx+8,sy+4,130,58);
        }
        arrow(p,path);if(!e.text.isEmpty()){p.fillRect(textBox,Qt::white);label(p,textBox,e.text,20);}
    }
    for(auto n:nodes){p.setBrush(Qt::white);if(n.shape=='{'){p.drawPolygon(QPolygonF{QPointF(n.box.center().x(),n.box.top()),QPointF(n.box.right(),n.box.center().y()),QPointF(n.box.center().x(),n.box.bottom()),QPointF(n.box.left(),n.box.center().y())});}else if(n.shape=='(')p.drawRoundedRect(n.box,30,30);else p.drawRect(n.box);
        p.setBrush(Qt::NoBrush);label(p,n.box.adjusted(n.shape=='{'?45:12,30,n.shape=='{'?-45:-12,-30),n.text,25);}
    return out;
}
inline QImage sequence(const QStringList &source){
    QVector<QString> names;QMap<QString,int> ids;struct Msg{int a,b;QString text;bool dashed;};QVector<Msg> messages;
    QRegularExpression participant(R"(^participant ([A-Za-z][A-Za-z0-9_]*)(?: as (.+))?$)"),message(R"(^([A-Za-z][A-Za-z0-9_]*)(-->>|->>)([A-Za-z][A-Za-z0-9_]*):\s*(.+)$)");
    for(int i=1;i<source.size();++i){auto m=participant.match(source[i]);if(m.hasMatch()){need(!ids.contains(m.captured(1))&&names.size()<5,"Too many or duplicate participants");ids[m.captured(1)]=names.size();names<< (m.captured(2).isEmpty()?m.captured(1):m.captured(2));}else{m=message.match(source[i]);need(m.hasMatch()&&ids.contains(m.captured(1))&&ids.contains(m.captured(3)),"Supported sequence diagrams: participants then ->> or -->> messages");need(messages.size()<20&&m.captured(4).size()<=100,"Too many or long sequence messages");messages.append({ids[m.captured(1)],ids[m.captured(3)],m.captured(4),m.captured(2)=="-->>"});}}
    need(names.size()>=2&&!messages.isEmpty(),"Sequence diagram needs two participants and a message");
    int height=210+messages.size()*140;QImage out(1174,height,QImage::Format_RGB32);out.fill(Qt::white);QPainter p(&out);p.setRenderHint(QPainter::Antialiasing);auto x=[&](int i){return 130+i*914.0/(names.size()-1);};
    for(int i=0;i<names.size();++i){p.setPen(QPen(Qt::black,3));p.drawRect(QRectF(x(i)-95,20,190,100));label(p,QRectF(x(i)-90,25,180,90),names[i],25);p.setPen(QPen(Qt::black,3,Qt::DashLine));p.drawLine(QPointF(x(i),120),QPointF(x(i),height-25));}
    int y=230;for(auto m:messages){p.setPen(QPen(Qt::black,3,m.dashed?Qt::DashLine:Qt::SolidLine));QPainterPath path;path.moveTo(x(m.a),y);QRectF r;
        if(m.a==m.b){double dx=m.a==names.size()-1?-75:75;path.lineTo(x(m.a)+dx,y);path.lineTo(x(m.a)+dx,y+45);path.lineTo(x(m.a),y+45);r=QRectF(x(m.a)-100,y-80,200,75);}else{path.lineTo(x(m.b),y);r=QRectF(qMin(x(m.a),x(m.b))+10,y-80,qAbs(x(m.a)-x(m.b))-20,75);}
        arrow(p,path);p.fillRect(r,Qt::white);p.setPen(QPen(Qt::black,3));label(p,r,m.text,23);y+=140;}
    return out;
}
inline QImage venn(const QString &source){
    auto doc=QJsonDocument::fromJson(source.toUtf8());need(doc.isObject(),"Invalid Venn diagram data");auto o=doc.object();auto sets=o.value("sets").toArray(),regions=o.value("regions").toArray();int n=sets.size();need(n==2||n==3,"Venn diagrams support two or three sets");need(regions.size()==(1<<n)-1,"Venn diagram must specify every region");
    for(auto s:sets)need(s.isString()&&!s.toString().isEmpty()&&s.toString().size()<=60,"Invalid Venn set label");
    QMap<int,QString> texts;for(auto r:regions){auto v=r.toObject();int mask=v.value("mask").toInt(-1);need(v.value("mask").toDouble(-1)==mask&&mask>0&&mask<(1<<n)&&!texts.contains(mask)&&v.value("text").isString()&&v.value("text").toString().size()<=200,"Invalid Venn region");texts[mask]=v.value("text").toString();}
    QFontMetrics fm(font(28));QStringList legends;QVector<int> heights;
    int h=n==2?740:940;
    for(auto it=texts.begin();it!=texts.end();++it){QStringList members;for(int i=0;i<n;++i)if(it.key()&(1<<i))members<<QString(QChar('A'+i));
        QString t=QString("%1 (%2 only): %3").arg(it.key()).arg(members.join(" + "),it.value().isEmpty()?QString("—"):it.value());
        int height=qMax(55,fm.boundingRect(QRect(0,0,1074,1000),Qt::TextWordWrap,t).height()+20);legends<<t;heights<<height;h+=height;}
    h+=20;
    need(h<4000,"Venn labels too long");QImage out(1174,h,QImage::Format_RGB32);out.fill(Qt::white);QPainter p(&out);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(Qt::black,4));
    QStringList titles;for(int i=0;i<n;++i)titles<<QString("%1: %2").arg(QChar('A'+i)).arg(sets[i].toString());label(p,QRectF(40,15,1094,140),titles.join("\n"),28);
    QVector<QPointF> centers=n==2?QVector<QPointF>{{437,450},{737,450}}:QVector<QPointF>{{447,410},{727,410},{587,652}};
    for(auto c:centers)p.drawEllipse(c,250,250);
    QMap<int,QPointF> positions=n==2?QMap<int,QPointF>{{1,{320,450}},{2,{854,450}},{3,{587,450}}}:QMap<int,QPointF>{{1,{340,340}},{2,{834,340}},{3,{587,285}},{4,{587,820}},{5,{410,615}},{6,{764,615}},{7,{587,495}}};
    for(auto it=positions.begin();it!=positions.end();++it)label(p,QRectF(it.value().x()-35,it.value().y()-25,70,50),QString::number(it.key()),32);
    int y=n==2?740:940;
    p.setFont(font(28));for(int i=0;i<legends.size();++i){p.drawText(QRect(50,y,1074,heights[i]),Qt::TextWordWrap,legends[i]);y+=heights[i];}
    p.end();need(y<=out.height(),"Venn legend overflow");return out.copy(0,0,1174,y+10);
}
}
inline QImage renderDiaryDiagram(const QString &kind,const QString &text,int width){
    QImage out;if(kind=="venn")out=DiaryDiagram::venn(text);else{auto source=DiaryDiagram::lines(text);if(source.first()=="sequenceDiagram")out=DiaryDiagram::sequence(source);else out=DiaryDiagram::flow(source);}
    return out.scaledToWidth(width,Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);
}
