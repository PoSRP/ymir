#include "ymir/image.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <image.img>\n", argv[0]);
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (fread(buf.data(), 1, buf.size(), f) != buf.size()) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return ymir::image::validate(reinterpret_cast<uintptr_t>(buf.data())) ? 0 : 1;
}
