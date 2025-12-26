#include <cstdint>
#include <vector>
#include <string>

class LCG {
private:
    uint32_t m_seed = 0;
    uint32_t m_a = 0;
    uint32_t m_b = 0;
    uint32_t m_m = 0;
    
public:
    LCG(uint32_t seed, uint32_t a, uint32_t b, uint32_t m);
    LCG();
    uint32_t getRandom(uint32_t range_x, uint32_t range_y);
    std::vector<uint32_t> getNRandoms(uint32_t numOfGenerations, uint32_t range_x, uint32_t range_y);
};