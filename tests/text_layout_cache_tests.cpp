#include "text_layout_cache.hpp"
#include <iostream>
using namespace mini;
int main(){
    Com<IDWriteFactory> factory;Com<IDWriteTextFormat> font;
    if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(factory.put()))))return 1;
    if(FAILED(factory->CreateTextFormat(L"Segoe UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,14,L"en-us",font.put())))return 1;
    TextLayoutCache cache;int checks=0,failures=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
    auto first=cache.get(factory.p,font.p,L"Repeated row",200,24);
    bool reused=first!=nullptr;
    for(int frame=0;frame<1000;++frame)reused&=cache.get(factory.p,font.p,L"Repeated row",200,24)==first;
    check(reused&&cache.size()==1,"1000 repeated frames reuse a single text layout");
    check(cache.get(factory.p,font.p,L"Repeated row",100,24)!=first,"changed width reflows text");
    font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
    check(cache.get(factory.p,font.p,L"Repeated row",200,24)!=first,"alignment uses a separate layout");
    font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    check(cache.get(factory.p,font.p,L"Repeated row",200,24)==first,"restored alignment reuses original layout");
    for(int i=0;i<600;++i)cache.get(factory.p,font.p,std::to_wstring(i),200,24);
    check(cache.size()==TextLayoutCache::capacity,"scrolling through many rows keeps memory bounded");
    cache.clear();check(cache.size()==0,"rebuild clears retained text");
    std::cout<<checks<<" text layout cache checks, "<<failures<<" failures\n";return failures?1:0;
}
