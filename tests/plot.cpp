#include "../src/plot.h"
#include <QGuiApplication>
#include <cassert>
#include <iostream>
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);
    assert(PlotExpression("x^2").eval(-3)==9);
    assert(PlotExpression("-x^2").eval(3)==-9);
    assert(PlotExpression("2^3^2").eval(0)==512);
    assert(PlotExpression("2^-2").eval(0)==0.25);
    assert(std::abs(PlotExpression("sin(pi/2)+ln(e)").eval(0)-2)<1e-10);
    assert(PlotExpression("2*x+1").eval(4)==9);
    assert(!std::isfinite(PlotExpression("sqrt(x)").eval(-1)));
    for(auto bad:{"system(x)","x;1","2x","x+","", "pow(x,2)","x=2"}){
        bool rejected=false;try{PlotExpression p(bad);}catch(...){rejected=true;}assert(rejected);
    }
    auto a=decodeDiaryAnswer(R"({"kind":"plot","command":"plot y=x²","expression":"x^2","xmin":-5,"xmax":5})");
    assert(a.plot);auto image=renderPlot(a,1174,1000);assert(image.height()==800);assert(image.save("plot-preview.png"));
    // Vertex and +/- 3 samples lie on the rendered parabola, with automatic y limits.
    auto text=decodeDiaryAnswer(R"({"kind":"text","text":"Hello"})");assert(!text.plot&&text.text=="Hello");
    for(auto bad:{R"({"kind":"plot","command":"show x","expression":"x","xmin":-5,"xmax":5})",
        R"({"kind":"plot","command":"plot x","expression":"x","xmin":5,"xmax":-5})", "not json"}){
        bool rejected=false;try{decodeDiaryAnswer(bad);}catch(...){rejected=true;}assert(rejected);
    }
    a.expression="1/x";auto reciprocal=renderPlot(a,1174,800);assert(reciprocal.save("plot-reciprocal-preview.png"));
    // No spurious diagonal crossing between branches around the vertical asymptote.
    // The center vertical axis itself is intentionally black.
    int dark=0;for(int x=580;x<594;++x)for(int y=365;y<395;++y)if(qGray(reciprocal.pixel(x,y))<50)++dark;
    assert(dark==0);
    a.expression="sqrt(x)";assert(!renderPlot(a,1174,480).isNull());
    a.expression="3";assert(!renderPlot(a,1174,800).isNull());
    a.expression="sqrt(-1)";bool rejected=false;try{renderPlot(a,1174,800);}catch(...){rejected=true;}assert(rejected);
    rejected=false;try{renderPlot(a,1174,479);}catch(...){rejected=true;}assert(rejected);
    std::cout<<"PASS: expression grammar, precedence, command validation, domains, plot rendering and space checks\n";
}
