#include "smooth_scroll.hpp"
#include <iostream>
using namespace mini;
int main(){
    int failures=0,checks=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
    float first=smooth_scroll_step(0,114,16);
    check(first>0&&first<114,"wheel motion starts between rows");
    check(smooth_scroll_step(0,114,0)==0,"zero elapsed time does not move");
    float split=smooth_scroll_step(smooth_scroll_step(0,114,16),114,16);
    check(std::abs(split-smooth_scroll_step(0,114,32))<.001f,"frame rate does not change motion");
    float x=0;for(int i=0;i<40;++i){float next=smooth_scroll_step(x,114,16);check(next>=x&&next<=114,"no overshoot");x=next;}
    check(smooth_scroll_step(0,114,140)>113,"wheel response settles within 140 milliseconds");
    check(x==114,"animation reaches target and can stop its timer");
    check(smooth_scroll_step(70,0,16)<70,"direction reverses immediately");
    check(smooth_scroll_step(0,.2f,16)==.2f,"fractional destinations are retained");
    std::cout<<checks<<" smooth scrolling checks, "<<failures<<" failures\n";return failures?1:0;
}
