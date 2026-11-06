#include "diagnostics/build_info.h"
#include <iostream>
int main(int argc,char**argv) {
    if(argc<2)return 2;
    std::cout<<sfm::build::identity()<<'\n';
    unsigned polls=0;
    auto result=sfm::build::inspect(argv[1],[&](){return argc<3 || ++polls<3;});
    for(const auto& copy:result.copies)std::cout<<copy.path<<'|'<<copy.version<<'|'<<copy.build<<'\n';
    std::cout<<"limited="<<result.limited<<" cancelled="<<result.cancelled<<'\n';
}
