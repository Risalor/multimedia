#include "BitWriter.hpp"

BitWriter::BitWriter() {

}

void BitWriter::writeBit(bool bit) {
    if(bit) {
        m_currByte |= (1 << (7 - m_currBitPosition));
    }

    m_currBitPosition++;

    if(m_currBitPosition >= 8) {
        m_currBitPosition = 0;
        m_buffer.push_back(m_currByte);
        m_currByte = 0;
    }
}

void BitWriter::writeByte(uint8_t byte) {
    for(uint8_t i = 0; i < 8; i++) {
        if((byte >> (7 - i)) & 1) {
            writeBit(1);
        } else {
            writeBit(0);
        }
    }
}

void BitWriter::writeBufferToFile(const std::string &path) {
    std::ofstream outfile(path, std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size() * sizeof(uint8_t));
    outfile.close();
}
