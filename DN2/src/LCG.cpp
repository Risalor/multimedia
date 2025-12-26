#include "LCG.hpp"
#include <cstdint>
#include <vector>

LCG::LCG(uint32_t seed, uint32_t a, uint32_t b, uint32_t m) 
    : m_seed(seed), m_a(a), m_b(b), m_m(m) {
    if (m_m == 0) m_m = 1;
}

LCG::LCG() : m_seed(0), m_a(0), m_b(0), m_m(1) { }

uint32_t LCG::getRandom(uint32_t range_x, uint32_t range_y) {
    m_seed = (m_a * m_seed + m_b) % m_m;
    
    if (range_x == range_y) return range_x;
    
    if (range_y < range_x) {
        uint32_t temp = range_x;
        range_x = range_y;
        range_y = temp;
    }
    
    uint64_t range_size = static_cast<uint64_t>(range_y - range_x + 1);
    
    uint64_t scaled = static_cast<uint64_t>(m_seed) * range_size;
    uint32_t result = static_cast<uint32_t>(range_x + (scaled / m_m));
    
    return result;
}

std::vector<uint32_t> LCG::getNRandoms(uint32_t numOfGenerations, uint32_t range_x, uint32_t range_y) {
    std::vector<uint32_t> tempVec;
    tempVec.reserve(numOfGenerations);

    for(uint32_t i = 0; i < numOfGenerations; i++) {
        tempVec.push_back(getRandom(range_x, range_y));
    }

    return tempVec;
}