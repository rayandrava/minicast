#pragma once
#include "platform.hpp"
#include <d2d1.h>
#include <map>
namespace mini {
class Lucide {
    std::map<std::string,Com<ID2D1PathGeometry>> paths;
    Com<ID2D1StrokeStyle> stroke;
public:
    void draw(ID2D1Factory* factory,ID2D1RenderTarget* target,ID2D1Brush* brush,
              const std::string& name,float x,float y,float size=20,float width=1.7f);
};
}
