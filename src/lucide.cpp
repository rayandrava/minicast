#include "lucide.hpp"
#include <initializer_list>
#include <cstdlib>
#include <cctype>
#include <cmath>

namespace mini {
namespace {
enum class Shape {Path,Rect,Circle};
struct Primitive {Shape shape;const char* path;float n[5];};
struct IconData {const char* name;std::initializer_list<Primitive> shapes;};
#include "lucide_data.inc"

// This parser only consumes our build-time, vendored icon paths, never user data.
class PathReader {
    const char* p;
    ID2D1GeometrySink* sink;
    D2D1_POINT_2F current{},start{},control{};
    char previous=0;
    bool open=false;
    void whitespace(){while(*p==' '||*p==','||*p=='\n'||*p=='\r')++p;}
    float number(){whitespace();char* end=nullptr;float value=strtof(p,&end);if(end==p){p+=(*p!=0);return 0;}p=end;return value;}
    D2D1_POINT_2F point(bool relative){float x=number(),y=number();return D2D1::Point2F(x+(relative?current.x:0),y+(relative?current.y:0));}
public:
    PathReader(const char* text,ID2D1GeometrySink* output):p(text),sink(output){}
    void run(){
        char command=0;
        while(*p){
            whitespace();if(!*p)break;
            if(std::isalpha(static_cast<unsigned char>(*p)))command=*p++;
            bool relative=std::islower(static_cast<unsigned char>(command));char op=static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
            if(op=='Z'){if(open)sink->EndFigure(D2D1_FIGURE_END_CLOSED);open=false;current=start;command=0;previous=op;continue;}
            if(op=='M'){auto pos=point(relative);if(open)sink->EndFigure(D2D1_FIGURE_END_OPEN);sink->BeginFigure(pos,D2D1_FIGURE_BEGIN_HOLLOW);open=true;current=start=pos;command=relative?'l':'L';}
            else if(op=='L'){current=point(relative);sink->AddLine(current);}
            else if(op=='H'){float x=number();current.x=relative?current.x+x:x;sink->AddLine(current);}
            else if(op=='V'){float y=number();current.y=relative?current.y+y:y;sink->AddLine(current);}
            else if(op=='A'){float rx=number(),ry=number(),rotation=number(),large=number(),sweep=number();auto end=point(relative);sink->AddArc(D2D1::ArcSegment(end,D2D1::SizeF(std::abs(rx),std::abs(ry)),rotation,sweep?D2D1_SWEEP_DIRECTION_CLOCKWISE:D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,large?D2D1_ARC_SIZE_LARGE:D2D1_ARC_SIZE_SMALL));current=end;}
            else if(op=='C'){auto c1=point(relative),c2=point(relative),end=point(relative);sink->AddBezier(D2D1::BezierSegment(c1,c2,end));control=c2;current=end;}
            else if(op=='S'){auto c1=(previous=='C'||previous=='S')?D2D1::Point2F(2*current.x-control.x,2*current.y-control.y):current;auto c2=point(relative),end=point(relative);sink->AddBezier(D2D1::BezierSegment(c1,c2,end));control=c2;current=end;}
            else break;
            previous=op;
        }
        if(open)sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }
};
void circle(ID2D1GeometrySink* sink,float x,float y,float r){sink->BeginFigure(D2D1::Point2F(x-r,y),D2D1_FIGURE_BEGIN_HOLLOW);sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x+r,y),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x-r,y),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));sink->EndFigure(D2D1_FIGURE_END_CLOSED);}
void rectangle(ID2D1GeometrySink* sink,const float* n){float x=n[0],y=n[1],w=n[2],h=n[3],r=n[4];sink->BeginFigure(D2D1::Point2F(x+r,y),D2D1_FIGURE_BEGIN_HOLLOW);auto line=[&](float a,float b){sink->AddLine(D2D1::Point2F(a,b));};auto arc=[&](float a,float b){if(r)sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(a,b),D2D1::SizeF(r,r),0,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));else line(a,b);};line(x+w-r,y);arc(x+w,y+r);line(x+w,y+h-r);arc(x+w-r,y+h);line(x+r,y+h);arc(x,y+h-r);line(x,y+r);arc(x+r,y);sink->EndFigure(D2D1_FIGURE_END_CLOSED);}
}
void Lucide::draw(ID2D1Factory* factory,ID2D1RenderTarget* target,ID2D1Brush* brush,const std::string& name,float x,float y,float size,float width){
    if(!stroke){auto props=D2D1::StrokeStyleProperties();props.startCap=props.endCap=props.dashCap=D2D1_CAP_STYLE_ROUND;props.lineJoin=D2D1_LINE_JOIN_ROUND;factory->CreateStrokeStyle(props,nullptr,0,stroke.put());}
    auto found=paths.find(name);
    if(found==paths.end()){
        const IconData* data=nullptr;for(auto& icon:iconData)if(name==icon.name){data=&icon;break;}if(!data)return;
        Com<ID2D1PathGeometry> geometry;if(FAILED(factory->CreatePathGeometry(geometry.put())))return;Com<ID2D1GeometrySink> sink;if(FAILED(geometry->Open(sink.put())))return;
        for(auto& shape:data->shapes){if(shape.shape==Shape::Path)PathReader(shape.path,sink.p).run();else if(shape.shape==Shape::Circle)circle(sink.p,shape.n[0],shape.n[1],shape.n[2]);else rectangle(sink.p,shape.n);}
        if(FAILED(sink->Close()))return;found=paths.emplace(name,std::move(geometry)).first;
    }
    D2D1_MATRIX_3X2_F old;target->GetTransform(&old);auto transform=D2D1::Matrix3x2F::Scale(size/24,size/24)*D2D1::Matrix3x2F::Translation(x,y)*old;target->SetTransform(transform);target->DrawGeometry(found->second.p,brush,width*24/size,stroke.p);target->SetTransform(old);
}
}
