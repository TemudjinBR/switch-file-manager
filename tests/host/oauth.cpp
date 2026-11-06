#include "security/oauth.h"
#include <iostream>
int main(int argc, char** argv) {
    if (argc == 2) std::cout << sfm::oauth::challenge(argv[1]);
    else { std::string token; if (!sfm::oauth::randomToken(token)) return 1; std::cout << token; }
}
