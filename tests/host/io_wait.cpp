#include "network/io_wait.h"
#include <cassert>
int main() {
    constexpr unsigned again=(204u<<9)|123u, timeout=(205u<<9)|123u, aborted=(206u<<9)|123u;
    unsigned calls=0;
    assert(sfm::net::waitIo([&](){return ++calls<3?again:0u;},{},100,1)==0);
    assert(calls==3);
    calls=0; constexpr unsigned certificate=(301u<<9)|123u;
    assert(sfm::net::waitIo([&](){++calls;return certificate;},{},100,1)==certificate);
    assert(calls==1);
    assert(sfm::net::waitIo([&](){return again;},{},1,1)==timeout);
    calls=0;
    assert(sfm::net::waitIo([&](){++calls;return again;},[]{return false;},100,1)==aborted);
    assert(calls==0);
}
