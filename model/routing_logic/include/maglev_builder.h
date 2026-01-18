#pragma once

#include <vector>
#include <stdint.h>
#define EMPTY 0xffffffff

struct BackendScore{
    int backend_id;
    double score;
};

class MaglevBuilder{
public:
    static const uint32_t M = 65537;
    MaglevBuilder(const std::vector<BackendScore>& backends);

    std::vector<uint32_t> build_table();
    void test_maglev_builder(const std::vector<uint32_t>& table, const std::vector<BackendScore>& backends);
private:
    std::vector<BackendScore> backends;
    uint32_t hash1(uint32_t val)const;
    uint32_t hash2(uint32_t val)const;
};
