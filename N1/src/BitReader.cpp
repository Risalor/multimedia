#include "BitReader.hpp"

BitReader::BitReader(const std::string &filePath) {
    std::fstream file(filePath, std::ios::in | std::ios::binary);

    if(!file.is_open()) {
        throw std::runtime_error("The file could not be opened!");
    }

    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    m_data.resize(fileSize);

    if (!file.read(m_data.data(), fileSize)) {
        throw std::runtime_error("Error reading file content!");
    }

    file.close();
}

bool BitReader::readNextBit() {
    bool bit = (m_data[m_currByte] >> (7 - m_currBit)) & 1;
    
    m_currBit++;
    if(m_currBit > 7) {
        m_currBit = 0;
        m_currByte++;
    }
    
    return bit;
}

char BitReader::getNextByte() {
    char byte = 0;
    
    for(uint8_t i = 0; i < 8; i++) {
        bool bit = readNextBit();
        byte |= (bit << i);
    }
    
    return byte;
}

bool BitReader::isEnd() {
    return m_currByte >= m_data.size() || (m_currByte == (m_data.size() - 1) && m_currBit >= 8);
}

void BitReader::setSavePoint() {
    m_currBit_save = m_currBit;
    m_currByte_save = m_currByte;
}

void BitReader::loadFromSavePoint() {
    m_currBit = m_currBit_save;
    m_currByte = m_currByte_save;
}

void BitReader::resetToStart() {
    m_currBit = 0;
    m_currByte = 0;
}