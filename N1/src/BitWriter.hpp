#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

class BitWriter {
private:
    std::vector<uint8_t> m_buffer;
    uint8_t m_currByte = 0;

    uint8_t m_currBitPosition = 0;
public:
    BitWriter();
    void writeBit(bool bit);
    void writeByte(uint8_t byte);
    void writeBufferToFile(const std::string& path);
};