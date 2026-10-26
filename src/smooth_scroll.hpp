#pragma once
#include <algorithm>
#include <cmath>
namespace mini {
// Time-based easing keeps motion consistent when frames are delayed.
inline float smooth_scroll_step(float current,float target,float elapsedMs){
    float next=current+(target-current)*(1.f-std::exp(-std::max(0.f,elapsedMs)/28.f));
    return std::abs(target-next)<.25f?target:next;
}
}
