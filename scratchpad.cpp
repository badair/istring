/*
copyright Barrett Adair 2016
All rights reserved (for now).
*/

#include "istring.hpp"

#undef NDEBUG
#include <cassert>
#include <iostream>
#include <type_traits>

using namespace istr;

int main() {

    std::cout << make_istring("hello, world!") << std::endl;
    
    /*const char* str = "hello, world!";
    auto g = make_istring(str, 1024);*/

    auto s = make_istring("hello, world!");
    std::cout << s << " size: " << s.size() << "; buffer size: " << s.buffer_size << std::endl;
    
    istring<> x = s;
    std::cout << x << std::endl;

    assert(!s.is_empty());
    assert(!x.is_empty());
    assert(s.size() == x.size());

    // covariance!
    auto s2 = s.clone();
    auto x2 = x.clone();
    static_assert(std::is_same<decltype(s2), decltype(s)>::value, "");
    static_assert(std::is_same<decltype(x2), decltype(x)>::value, "");
    static_assert(!std::is_same<decltype(s), decltype(x)>::value, "");

    assert(s == s);
    assert(x == x);
    assert(s == x);
    assert(s2 == x2);

    for (int i = 0; i < s.size(); ++i) {
        std::cout << s[i];
    }

    std::cout << std::endl;

    for (int i = 0; i < x.size(); ++i) {
        std::cout << x[i];
    }

    std::cout << std::endl;

    for (auto& c : s) {
        std::cout << c;
    }

    std::cout << std::endl;

    for (auto& c : x) {
        std::cout << c;
    }

    std::cout << std::endl;

    std::cout << "s2 buffer size: " << s2.get_buffer_size() << std::endl;
    auto s3 = s2.resize_to(64);
    std::cout << "s3 buffer size: " << s3.get_buffer_size() << std::endl;
    auto s4 = s3.resize();
    std::cout << "s4 buffer size: " << s4.get_buffer_size() << std::endl;

    std::cout << "x2 buffer size: " << x2.get_buffer_size() << std::endl;
    x2 = x2.resize_to(64);
    std::cout << "x2 buffer size: " << x2.get_buffer_size() << std::endl;
    x2 = x2.resize();
    std::cout << "x2 buffer size: " << x2.get_buffer_size() << std::endl;

    std::cin.get();
}