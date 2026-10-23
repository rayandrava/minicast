#include "instance_identity.hpp"
#include <iostream>

int main(){
    int checks=0,failures=0;
    auto check=[&](bool valid,const char* name){++checks;if(!valid){++failures;std::cerr<<"FAIL: "<<name<<'\n';}};
    const auto first=mini::instance_class(L"c:\\tests\\profile-a");
    check(mini::instance_class(L"")==L"MinicastPalette","normal launch retains existing instance identity");
    check(first!=mini::instance_class(L""),"test profile cannot target the normal user's instance");
    check(first!=mini::instance_class(L"c:\\tests\\profile-b"),"different test profiles are independent");
    check(first==mini::instance_class(L"c:\\tests\\profile-a"),"same profile reuses its instance");
    check(first.size()<64,"long profile paths have a short bounded class name");
    check(mini::instance_class(std::wstring(32768,L'a')).size()==first.size(),"class size is independent of path length");
    check(mini::instance_class(L"c:\\tests\\\u65e5\u672c")!=first,"Unicode profiles retain independent identity");
    std::cout<<checks<<" instance identity checks, "<<failures<<" failures\n";
    return failures?1:0;
}
