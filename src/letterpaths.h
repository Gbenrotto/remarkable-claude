#pragma once
#include <QImage>
#include <QPolygonF>
#include <QLineF>
#include <QSet>
#include <vector>
#include <algorithm>
#include <cmath>

// Thin the handwriting glyphs to centre lines, then follow their connected
// strokes. This avoids both scanline filling and double-outlined letters.
inline std::vector<QPolygonF> letterPaths(const QImage &source) {
    const QImage image=source.convertToFormat(QImage::Format_RGB32);
    const int w=image.width()+2,h=image.height()+2;
    std::vector<unsigned char> ink(w*h,0);
    for(int y=0;y<image.height();++y)
        for(int x=0;x<image.width();++x)
            ink[(y+1)*w+x+1]=qGray(image.pixel(x,y))<170;
    const int offsets[]={-w,-w+1,1,w+1,w,w-1,-1,-w-1};
    bool changed=true;
    while(changed) {
        changed=false;
        for(int pass=0;pass<2;++pass) {
            std::vector<int> remove;
            for(int y=1;y<h-1;++y) for(int x=1;x<w-1;++x) {
                int i=y*w+x;if(!ink[i])continue;
                int n[8],count=0,transitions=0;
                for(int k=0;k<8;++k){n[k]=ink[i+offsets[k]];count+=n[k];}
                if(count<2 || count>6)continue;
                for(int k=0;k<8;++k)if(!n[k] && n[(k+1)%8])++transitions;
                if(transitions!=1)continue;
                bool blocked=pass==0 ? (n[0]*n[2]*n[4] || n[2]*n[4]*n[6])
                                     : (n[0]*n[2]*n[6] || n[0]*n[4]*n[6]);
                if(!blocked)remove.push_back(i);
            }
            for(int i:remove)ink[i]=0;
            changed=changed || !remove.empty();
        }
    }
    auto neighbors=[&](int i) {
        std::vector<int> result;
        for(int k=0;k<8;++k) {
            int j=i+offsets[k];if(j<0 || j>=w*h || !ink[j])continue;
            // Avoid a redundant diagonal across an already connected corner.
            if((k%2) && (ink[i+offsets[(k+7)%8]] || ink[i+offsets[(k+1)%8]]))continue;
            result.push_back(j);
        }
        return result;
    };
    auto edge=[](int a,int b)->quint64 {return (quint64(std::min(a,b))<<32)|quint32(std::max(a,b));};
    auto point=[&](int i){return QPointF(i%w-1,i/w-1);};
    QSet<quint64> visited;
    std::vector<QPolygonF> paths;
    auto trace=[&](int start,int next) {
        QPolygonF path;path<<point(start);
        int previous=start,current=next;
        while(true) {
            visited.insert(edge(previous,current));path<<point(current);
            auto ns=neighbors(current);
            if(ns.size()!=2)break;
            int candidate=ns[0]==previous ? ns[1]:ns[0];
            if(visited.contains(edge(current,candidate)))break;
            previous=current;current=candidate;
        }
        paths.push_back(path);
    };
    // Endpoints and junctions first, then closed loops (o, a, e, etc.).
    for(int i=w+1;i<w*(h-1)-1;++i)if(ink[i]) {
        auto ns=neighbors(i);
        if(ns.empty()){QPolygonF dot;dot<<point(i)<<point(i)+QPointF(0.4,0);paths.push_back(dot);}
        if(ns.size()!=2)for(int j:ns)if(!visited.contains(edge(i,j)))trace(i,j);
    }
    for(int i=w+1;i<w*(h-1)-1;++i)if(ink[i])
        for(int j:neighbors(i))if(!visited.contains(edge(i,j)))trace(i,j);
    // Remove one-pixel staircase noise without moving open stroke endpoints.
    for(auto &path:paths)if(path.size()>3) {
        QPolygonF original=path;
        bool closed=original.first()==original.last();
        int count=original.size()-(closed?1:0);
        for(int i=closed?0:1;i<(closed?count:count-1);++i)
            path[i]=(original[(i+count-1)%count]+original[i]*2+original[(i+1)%count])/4;
        if(closed)path.last()=path.first();
    }
    // Group paths by text line, then proceed left-to-right within each line.
    std::stable_sort(paths.begin(),paths.end(),[](const QPolygonF &a,const QPolygonF &b){
        QRectF x=a.boundingRect(),y=b.boundingRect();
        int lineA=int(x.center().y()/60),lineB=int(y.center().y()/60);
        return lineA==lineB ? x.left()<y.left() : lineA<lineB;
    });
    return paths;
}
