#pragma once
#include "platform.hpp"
#include <dwrite.h>
#include <map>
#include <tuple>
#include <algorithm>
namespace mini {
class TextLayoutCache {
    using Key=std::tuple<std::wstring,uintptr_t,float,float,DWRITE_TEXT_ALIGNMENT,DWRITE_PARAGRAPH_ALIGNMENT>;
    struct Entry {Com<IDWriteTextLayout> layout;uint64_t used=0;};
    std::map<Key,Entry> entries;uint64_t clock=0;
public:
    static constexpr size_t capacity=256;
    void clear(){entries.clear();}
    size_t size()const{return entries.size();}
    IDWriteTextLayout* get(IDWriteFactory* factory,IDWriteTextFormat* font,const std::wstring& text,float width,float height){
        if(!factory||!font)return nullptr;
        // Position and color do not change glyph layout. Alignment does.
        Key key{text,reinterpret_cast<uintptr_t>(font),width,height,font->GetTextAlignment(),font->GetParagraphAlignment()};
        auto found=entries.find(key);
        if(found==entries.end()){
            Entry entry;
            if(FAILED(factory->CreateTextLayout(text.c_str(),UINT32(text.size()),font,width,height,entry.layout.put())))return nullptr;
            if(entries.size()>=capacity){auto oldest=std::min_element(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.second.used<b.second.used;});entries.erase(oldest);}
            found=entries.emplace(std::move(key),std::move(entry)).first;
        }
        found->second.used=++clock;return found->second.layout.p;
    }
};
}
