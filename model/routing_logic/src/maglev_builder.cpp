#include "maglev_builder.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <map>
#include <iomanip>


MaglevBuilder::MaglevBuilder(const std::vector<BackendScore>& backends)
    : backends(backends) {}

uint32_t MaglevBuilder::hash1(uint32_t val) const {
    return (val * 2654435761u) % M;
}

uint32_t MaglevBuilder::hash2(uint32_t val) const {
    return (val * 40503u) % (M - 1) + 1;
}

std::vector<uint32_t> MaglevBuilder::build_table() {
    uint32_t N = backends.size();
    if(N == 0){
        return std::vector<uint32_t>(M, 0);
    }

    std::vector<std::vector<uint32_t>> perm(N, std::vector<uint32_t>(M));
    for(uint32_t i = 0; i < N; ++i){
        uint32_t offset = hash1(backends[i].backend_id) % M;
        uint32_t skip = hash2(backends[i].backend_id) % M;
        for(uint32_t j = 0; j < M; ++j){
            perm[i][j] = (offset + j * skip) % M;
        }
    }

    double max_score = 0.0;
    for(const auto& backend : backends){
        if(backend.score > max_score){
            max_score = backend.score;
        }
    }

    std::vector<int> counters(N);
    for (int i = 0; i < N; i++){
        counters[i] = std::max(1, (int)((backends[i].score / max_score) * 100));
    }
    
    std::vector<uint32_t> table(M, 0xffffffff);
    std::vector<uint32_t> next(N, 0);
    uint32_t filled = 0;

    while (filled < M) {
        for (uint32_t i = 0; i < N && filled < M; ++i) {
            for (int c = 0; c < counters[i] && filled < M; ++c) {
                uint32_t pos = perm[i][next[i]];

                while (table[pos] != 0xffffffff) {
                    next[i]++;
                    pos = perm[i][next[i]];
                }

                table[pos] = backends[i].backend_id;
                next[i]++;
                filled++;
            }   
        }
    }

    return table;
}

void MaglevBuilder::test_maglev_builder(const std::vector<uint32_t>& table, const std::vector<BackendScore>& backends){
    std::map<uint32_t, int> counters;
    for(const auto& b : backends){
        counters[b.backend_id] = 0;
    }

    for(uint32_t entry : table){
        if(counters.find(entry) != counters.end()){
            counters[entry]++;
        }
    }

    std::cout << "Maglev Table Distribution:\n";
    for(const auto& b : backends){
        double percentage = (double)counters[b.backend_id] / table.size() * 100.0;
        std::cout << "Backend ID: " << b.backend_id 
                  << ", Count: " << counters[b.backend_id] 
                  << ", Percentage: " << std::fixed << std::setprecision(2) <<
                    percentage << "%\n";
    }
}