#include "../src/images.h"
#include "../src/diagrams.h"
#include "../src/plot.h"
#include "../src/freshness.h"
#include <QGuiApplication>
#include <cassert>
#include <iostream>
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception &){caught=true;}assert(caught);}
int main(int argc,char **argv){QGuiApplication app(argc,argv);
    QImage color(32,48,QImage::Format_ARGB32);color.fill(Qt::red);color.setPixelColor(0,0,Qt::transparent);
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);color.save(&buffer,"PNG");
    auto gray=diaryDecodeImage({{"data",QJsonArray{QJsonObject{{"b64_json",QString::fromLatin1(bytes.toBase64())}}}}},1024);
    assert(gray.width()==1024&&gray.height()==1536&&gray.format()==QImage::Format_Grayscale8);assert(gray.pixelColor(0,0).red()==255);
    for(int y=0;y<gray.height();y+=50)for(int x=0;x<gray.width();x+=50){auto c=gray.pixelColor(x,y);assert(c.red()==c.green()&&c.green()==c.blue());}
    rejects([]{diaryDecodeImage({},1174);});rejects([]{diaryDecodeImage({{"data",QJsonArray{QJsonObject{{"b64_json","bad!"}}}}},1174);});
    assert(diaryImagePayload("a comic",{}).value("model")=="gpt-image-1.5");assert(diaryImagePayload("a comic",{{"image_model","custom"}}).value("model")=="custom");
    for(auto kind:{"image","mermaid","venn"}){auto raw=QJsonDocument(QJsonObject{{"kind",kind},{"text","data"},{"command","draw a comic about today"}}).toJson();auto a=decodeDiaryAnswer(raw);diaryRequireFreshSearch(a);assert(!a.search);assert(a.image||!a.diagram.isEmpty());}
    auto flow=renderDiaryDiagram("mermaid",R"(flowchart TD
A("Start")
B{"Ready?"}
C["Make tea"]
D("Finish")
A --> B
B -->|Yes| C
B -->|No| D
C --> D)",1174);assert(flow.save("flowchart-preview.png"));
    assert(!renderDiaryDiagram("mermaid","flowchart TD\nA[\"Loop\"]\nA --> A",1174).isNull());
    auto seq=renderDiaryDiagram("mermaid","sequenceDiagram\nparticipant A as Tablet\nparticipant B as OpenAI\nA->>B: Read question\nB-->>A: Return answer\nA->>A: Insert image",1174);assert(seq.save("sequence-preview.png"));
    for(int n:{2,3}){QJsonArray sets,regions;for(int i=0;i<n;++i)sets.append(QString("Set %1").arg(i+1));for(int mask=1;mask<(1<<n);++mask)regions.append(QJsonObject{{"mask",mask},{"text",QString("Region %1 content").arg(mask)}});auto data=QJsonDocument(QJsonObject{{"sets",sets},{"regions",regions}}).toJson();auto venn=renderDiaryDiagram("venn",data,1174);assert(venn.save(QString("venn-%1-preview.png").arg(n)));}
    // Exact synthetic tea request response rejected by the previous parser.
    auto tea=renderDiaryDiagram("mermaid",R"tea(flowchart TD
A[Start]
B[Boil water]
C[Put tea bag in cup]
D[Pour hot water]
E[Steep 3 to 5 min]
F{"Add milk or sugar?"}
G[Add milk or sugar]
H[Remove tea bag]
I[Serve tea]
A --> B
B --> C
C --> D
D --> E
E --> F
F -->|Yes| G
F -->|No| H
G --> H
H --> I)tea",1174);assert(tea.height()<=1000);assert(tea.save("tea-flowchart-preview.png"));
    auto simple=DiaryDiagram::parseFlow(DiaryDiagram::lines("flowchart TD; A[Start] --> B(Boil water) --> C{Ready?}; C --> |Yes| D([Drink]); C -- No --> B"));
    assert(simple.nodes.size()==4&&simple.edges.size()==4&&simple.nodes[1].text=="Boil water"&&simple.edges[2].text=="Yes"&&simple.edges[3].text=="No");
    auto implicit=DiaryDiagram::parseFlow(DiaryDiagram::lines("graph TB; A --> B; B[Tea]; A[Start]; A[Start]"));
    assert(implicit.nodes.size()==2&&implicit.nodes[1].text=="Tea");
    auto literal=DiaryDiagram::parseFlow(DiaryDiagram::lines("flowchart TD; A[\"Tea; sugar --> milk\"] --> B((End))"));
    assert(literal.nodes[0].text=="Tea; sugar --> milk"&&literal.edges.size()==1);
    assert(!renderDiaryDiagram("mermaid","flowchart TD; A[Start] --> B[Boil water] --> C[Drink]",1174).isNull());
    for(auto bad:{"flowchart TD; A[Start", "flowchart TD; A[Start} --> B", "flowchart TD; A[Start]; A[Other]", "flowchart TD; A -->|oops B", "flowchart TD; A ~~ B"})rejects([&]{DiaryDiagram::parseFlow(DiaryDiagram::lines(bad));});
    rejects([]{renderDiaryDiagram("mermaid","flowchart TD\nclick A href https://example.com",1174);});
    rejects([]{renderDiaryDiagram("mermaid","sequenceDiagram\nparticipant A\nA->>B: unknown",1174);});
    rejects([]{renderDiaryDiagram("venn","{}",1174);});
    std::cout<<"PASS: image decoding/grayscale, visual routing, flowchart/sequence/Venn rendering and invalid input rejection\n";
}
