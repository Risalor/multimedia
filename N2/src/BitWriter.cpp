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
    if(m_currBitPosition > 0) {
        m_buffer.push_back(m_currByte);
        m_currByte = 0;
        m_currBitPosition = 0;
    }

    std::ofstream outfile(path, std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(m_buffer.data()), m_buffer.size() * sizeof(uint8_t));
    outfile.close();
}

void BitWriter::flush() {
    if(m_currBitPosition > 0) {
        m_buffer.push_back(m_currByte);
        m_currByte = 0;
        m_currBitPosition = 0;
    }
}

std::vector<uint8_t> BitWriter::getBuffer() {
    return m_buffer;
}

void BitWriter::writeUint32(uint32_t value) {
    writeByte(static_cast<uint8_t>((value >> 24) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 16) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
    writeByte(static_cast<uint8_t>(value & 0xFF));
}

void BitWriter::writeUint64(uint64_t value) {
    writeByte(static_cast<uint8_t>((value >> 56) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 48) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 40) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 32) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 24) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 16) & 0xFF));
    writeByte(static_cast<uint8_t>((value >> 8) & 0xFF));
    writeByte(static_cast<uint8_t>(value & 0xFF));
}

void BitWriter::setBuffer(std::vector<uint8_t> &buffer) {
    m_buffer = buffer;
}
