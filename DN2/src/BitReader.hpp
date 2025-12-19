#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <cstdint>

class BitReader {
private:
    std::vector<char> m_data;
    uint8_t m_currBit = 0;
    uint64_t m_currByte = 0;

    uint8_t m_currBit_save = 0;
    uint64_t m_currByte_save = 0;
public:
    BitReader(const std::string& filePath);
    bool readNextBit();
    char getNextByte();
    bool isEnd();
    void setSavePoint();
    void loadFromSavePoint();
    void resetToStart();
};