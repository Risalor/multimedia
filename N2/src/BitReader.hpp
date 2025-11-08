#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

class BitReader {
private:
    std::vector<uint8_t> m_data;
    uint8_t m_currBit = 0;
    uint64_t m_currByte = 0;

    uint8_t m_currBit_save = 0;
    uint64_t m_currByte_save = 0;
public:
    BitReader(const std::string& filePath);
    bool readNextBit();
    uint8_t readNextByte();
    bool isEnd();
    void setSavePoint();
    void loadFromSavePoint();
    void resetToStart();
    uint32_t readUint32();
    uint64_t readUint64();
    std::vector<uint8_t> getBuffer();
};