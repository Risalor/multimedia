#include "BitReader.hpp"
#include "BitWriter.hpp"
#include <bits/stdc++.h>
#include <cstring>
#include <Dense>

#pragma pack(push, 1)
struct BMPFileHeader {
    uint16_t file_type{0x4D42};
    uint32_t file_size{0};
    uint16_t reserved1{0};
    uint16_t reserved2{0};
    uint32_t offset_data{0};
};

struct BMPInfoHeader {
    uint32_t size{0};
    int32_t width{0};
    int32_t height{0};
    uint16_t planes{1};
    uint16_t bit_count{0};
    uint32_t compression{0};
    uint32_t size_image{0};
    int32_t x_pixels_per_meter{0};
    int32_t y_pixels_per_meter{0};
    uint32_t colors_used{0};
    uint32_t colors_important{0};
};

struct BMPFile {
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    Eigen::MatrixXf bmp;
};
#pragma pack(pop)

struct TableElement {
    uint32_t freq = 0;
    uint64_t upper = 0;
    uint64_t lower = 0;
    float probability = 0.f;
    uint8_t element = 0;
};

std::vector<TableElement> orderedElements;

std::array<TableElement, 256> makeTable(const std::vector<uint8_t>& buffer) {

    std::array<TableElement, 256> tempTable;
    std::array<int16_t, 256> firstAppearance;
    firstAppearance.fill(-1);

    uint8_t counter = 0;
    for(const auto& byte : buffer) {
        tempTable[byte].freq++;
        if(tempTable[byte].freq == 1) {
            firstAppearance[byte] = counter;
            counter++;
            tempTable[byte].element = byte;
        }
    }

    std::vector<TableElement> toReturnTable;
    for(uint16_t i = 0; i < 256; i++) {
        if(firstAppearance[i] != -1) {
            toReturnTable.push_back(tempTable[i]);
        }
    }

    orderedElements = toReturnTable;

    toReturnTable[0].probability = static_cast<double>(toReturnTable[0].freq) / static_cast<double>(buffer.size());
    toReturnTable[0].lower = 0;
    toReturnTable[0].upper = toReturnTable[0].lower + toReturnTable[0].freq;

    for(uint16_t i = 1; i < toReturnTable.size(); i++) {
        toReturnTable[i].probability = static_cast<double>(toReturnTable[i].freq) / static_cast<double>(buffer.size());
        toReturnTable[i].lower = toReturnTable[i - 1].upper;
        toReturnTable[i].upper = toReturnTable[i].lower + toReturnTable[i].freq;
    }

    for(auto& it : toReturnTable) {
        tempTable[it.element] = it;
    }

    return tempTable;
}

std::array<TableElement, 256> makeTableFromfreq(std::vector<TableElement>& table, uint64_t freq) { 
    std::array<TableElement, 256> tempTable;

    table[0].probability = static_cast<double>(table[0].freq) / static_cast<double>(freq);
    table[0].lower = 0;
    table[0].upper = table[0].lower + table[0].freq;

    for(uint32_t i = 1; i < table.size(); i++) {
        table[i].probability = static_cast<double>(table[i].freq) / static_cast<double>(freq);
        table[i].lower = table[i - 1].upper;
        table[i].upper = table[i].lower + table[i].freq;
    }

    for(auto& it : table) {
        tempTable[it.element] = it;
    }

    return tempTable;
}

BitWriter encode(std::vector<uint8_t>& bts) {
    std::array<TableElement, 256> table = makeTable(bts);
    BitWriter writer;

    uint64_t comFreq = 0;

    writer.writeByte(static_cast<uint8_t>(orderedElements.size()));

    for(const auto& it : orderedElements) {
        writer.writeByte(it.element);
        writer.writeUint32(it.freq);
        comFreq += it.freq;
    }

    uint64_t lowerBound = 0;
    uint64_t upperBound = std::pow(2, 64 - 1) - 1;

    uint64_t secondQuarter = std::floor((static_cast<double>(upperBound) + 1.f) / 2.f);
    uint64_t firstQuarter = std::floor(static_cast<double>(secondQuarter) / 2.f);
    uint64_t thirdQuarter = std::floor(static_cast<double>(firstQuarter) * 3.f);

    uint8_t E3_counter = 0;
    for(const auto& bit : bts) {
        uint64_t step = (upperBound - lowerBound + 1) / comFreq;

        upperBound = lowerBound + step * table[bit].upper - 1;
        lowerBound = lowerBound + step * table[bit].lower;

        while(true) {
            if(upperBound < secondQuarter) {
                lowerBound = lowerBound * 2;
                upperBound = upperBound * 2 + 1;
                writer.writeBit(0);

                for(uint8_t i = 0; i < E3_counter; i++) {
                    writer.writeBit(1);
                }

                E3_counter = 0;

            } else if (lowerBound >= secondQuarter) {
                lowerBound = 2 * (lowerBound - secondQuarter);
                upperBound = 2 * (upperBound - secondQuarter) + 1;
                writer.writeBit(1);
                for(uint8_t i = 0; i < E3_counter; i++) {
                    writer.writeBit(0);
                }

                E3_counter = 0;

            } else break;
        }

        while((lowerBound >= firstQuarter) && (upperBound < thirdQuarter)) {
            lowerBound = 2 * (lowerBound - firstQuarter);
            upperBound = 2 * (upperBound - firstQuarter) + 1;
            E3_counter++;
        }
    }

    if(lowerBound < firstQuarter) {
        writer.writeBit(0);
        writer.writeBit(1);

        for(uint8_t i = 0; i < E3_counter; i++) {
            writer.writeBit(1);
        }
    } else {
        writer.writeBit(1);
        writer.writeBit(0);

        for(uint8_t i = 0; i < E3_counter; i++) {
            writer.writeBit(0);
        }
    }

    return writer;
}

uint8_t getSymbolFromTable(const std::array<TableElement, 256>& table, uint64_t value) {
    for(const auto& it : table) {
        if(it.freq != 0) {
            if(value >= it.lower && value < it.upper) {
                return it.element;
            }
        }
    }
    return 0;
}

std::vector<uint8_t> decode(BitReader& reader) {
    uint64_t comFreq = 0;
    uint64_t lowerBound = 0;
    uint64_t upperBound = std::pow(2, 64 - 1) - 1;

    uint64_t secondQuarter = std::floor((static_cast<double>(upperBound) + 1.f) / 2.f);
    uint64_t firstQuarter = std::floor(static_cast<double>(secondQuarter) / 2.f);
    uint64_t thirdQuarter = std::floor(static_cast<double>(firstQuarter) * 3.f);

    uint16_t eleC = static_cast<uint8_t>(reader.readNextByte());

    if(eleC == 0) {
        eleC = 256;
    }

    std::vector<TableElement> tempTab(eleC);

    for(int i = 0; i < eleC; i++) {
        tempTab[i].element = reader.readNextByte();
        tempTab[i].freq = reader.readUint32();
        comFreq += tempTab[i].freq;
    }

    std::array<TableElement, 256> table = makeTableFromfreq(tempTab, comFreq);

    uint64_t code = 0;
    for (int i = 0; i < 63; ++i) {
        code = (code << 1) | reader.readNextBit();
    }

    std::vector<uint8_t> out;
    out.reserve(comFreq);

    for (uint64_t i = 0; i < comFreq; ++i) {
        uint64_t step = (upperBound - lowerBound + 1) / comFreq;
        uint64_t value = (code - lowerBound) / step;

        uint8_t symbol = getSymbolFromTable(table, value);
        out.push_back(symbol);

        upperBound = lowerBound + step * table[symbol].upper - 1;
        lowerBound = lowerBound + step * table[symbol].lower;

        while (true) {
            if (upperBound < secondQuarter) {
                lowerBound = lowerBound * 2;
                upperBound = upperBound * 2 + 1;
                code = code * 2 + reader.readNextBit();
            }
            else if (lowerBound >= secondQuarter) {
                lowerBound = 2 * (lowerBound - secondQuarter);
                upperBound = 2 * (upperBound - secondQuarter) + 1;
                code = 2 * (code - secondQuarter) + reader.readNextBit();
            }
            else break;

        }

        while (lowerBound >= firstQuarter && upperBound < thirdQuarter) {
            lowerBound = 2 * (lowerBound - firstQuarter);
            upperBound = 2 * (upperBound - firstQuarter) + 1;
            code = 2 * (code - firstQuarter) + reader.readNextBit();
        }
    }

    return out;
}

bool getBmpDimensions(const std::string& filename, BMPFile& bmpFile) {
    std::ifstream file(filename, std::ios_base::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&bmpFile.fileHeader), sizeof(bmpFile.fileHeader));
    
    if (bmpFile.fileHeader.file_type != 0x4D42) {
        std::cerr << "Not a valid BMP file" << std::endl;
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&bmpFile.infoHeader), sizeof(bmpFile.infoHeader));
    
    bmpFile.infoHeader.width;
    bmpFile.infoHeader.height = abs(bmpFile.infoHeader.height);

    bmpFile.bmp = Eigen::MatrixXf(bmpFile.infoHeader.height, bmpFile.infoHeader.width);

    for(uint32_t i = 0; i < bmpFile.infoHeader.height; i++) {
        for (uint32_t j = 0; j < bmpFile.infoHeader.width; j++) {
            uint8_t byte;
            file.read(reinterpret_cast<char*>(&byte), sizeof(byte));
            bmpFile.bmp(i, j) = static_cast<float>(byte>>1);
        }
    }
    
    return true;
}

Eigen::MatrixXf hMat() {
    Eigen::MatrixXf H(8, 8);
    H.setZero();
    
    for(uint8_t i = 0; i < 8; i++) {
        H(i, 0) = sqrt(8.0/64.0);
    }

    for(uint8_t i = 0; i < 8; i++) {
        if(i < 4) {
            H(i, 1) = sqrt(8.0/64.0);
        } else {
            H(i, 1) = -sqrt(8.0/64.0);
        }
    }

    for(uint8_t i = 0; i < 8; i++) {
        if(i < 4) {
            if(i < 2) {
                H(i, 2) = sqrt(1.0/2.0);
            } else {
                H(i, 2) = -sqrt(1.0/2.0);
            }
        } else {
            if(i < 6) {
                H(i, 3) = sqrt(1.0/2.0);
            } else {
                H(i, 3) = -sqrt(1.0/2.0);
            }
        }
    }

    for(uint8_t i = 0, j = 4; i < 8 && j < 8; i += 2, j++) {
        H(i, j) = sqrt(1.0/2.0);
        H(i + 1, j) = -sqrt(1.0/2.0);
    }

    return H;
}

std::vector<std::vector<int>> quantization_table(uint8_t q) {
    std::vector<std::vector<int>> table;
    for(uint8_t i = 0; i < 8; i++) {
        table.push_back(std::vector<int>());
        for(uint8_t j = 0; j < 8; j++) {
            table[i].push_back(1 + (1 + i + j) * q);
        }
    }

    return table;
}

std::vector<uint8_t> mergeAndSplitVectors(const std::vector<int16_t>& vec1, const std::vector<int16_t>& vec2) {
    std::vector<int16_t> merged;
    merged.reserve(vec1.size() + vec2.size());
    merged.insert(merged.end(), vec1.begin(), vec1.end());
    merged.insert(merged.end(), vec2.begin(), vec2.end());
    
    std::vector<uint8_t> result;
    result.reserve(merged.size() * 2);
    
    for (int16_t value : merged) {
        result.push_back(static_cast<uint8_t>(value & 0xFF));
        result.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
    
    return result;
}

void compressAsBlocks(BMPFile& bmpFile, uint8_t thc) {
    Eigen::MatrixXf H = hMat();
    uint32_t size = (bmpFile.infoHeader.width + bmpFile.infoHeader.width % 8) * (bmpFile.infoHeader.height + bmpFile.infoHeader.height % 8);
    uint32_t size_DC = size / 64;
    std::vector<int16_t> flattened_AC, flattened_DC;
    flattened_AC.reserve(size);
    flattened_AC.resize(size);
    flattened_DC.reserve(size_DC);
    flattened_DC.resize(size_DC);

    int zigzag[8][8] = {
        {0, 1, 5, 6,14,15,27,28},
        {2, 4, 7,13,16,26,29,42},
        {3, 8,12,17,25,30,41,43},
        {9,11,18,24,31,40,44,53},
        {10,19,23,32,39,45,52,54},
        {20,22,33,38,46,51,55,60},
        {21,34,37,47,50,56,59,61},
        {35,36,48,49,57,58,62,63}
    };

    std::vector<std::vector<int>> q_table = quantization_table(0);

    uint32_t block_index = 0;

    for(uint32_t i = 0; i < bmpFile.infoHeader.height; i += 8) {
        for(uint32_t j = 0; j < bmpFile.infoHeader.width; j += 8) {
            Eigen::MatrixXf temp;
            if((i + 8 > bmpFile.infoHeader.height) || (j + 8 > bmpFile.infoHeader.width)) {
                temp = Eigen::MatrixXf(8, 8);
                temp.setZero();
                uint32_t copy_height = std::min(8u, bmpFile.infoHeader.height - i);
                uint32_t copy_width = std::min(8u, bmpFile.infoHeader.width - j);
                temp.topLeftCorner(copy_height, copy_width) = bmpFile.bmp.block(i, j, copy_height, copy_width);
            } else {
                temp = H.transpose() * bmpFile.bmp.block(i, j, 8, 8) * H;
            }

            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    if(x == 0 && y == 0) {
                        flattened_DC[block_index] = static_cast<int>(temp(y, x)) / q_table[y][x];
                    } else if(abs(temp(x, y)) < thc) {
                        flattened_AC[zigzag[y][x] + (block_index * 64)] = 0;
                    } else {
                        flattened_AC[zigzag[y][x] + (block_index * 64)] = static_cast<int>(temp(y, x)) / q_table[y][x];
                    }
                }
            }

            block_index++;
        }
    }

    //std::cout << "Size: " << flattened_DC.size() << "\n";

    std::vector<int16_t> DC_pred;
    DC_pred.reserve(size_DC);
    DC_pred.resize(size_DC);

    DC_pred[0] = flattened_DC[0];

    for(uint32_t i = 1; i < size_DC; i++) {
        DC_pred[i] = flattened_DC[i-1] - flattened_DC[i];
    }

    /*for(auto it : flattened_AC) {
        std::cout << " " << it;
    }

    std::cout << "\n\n";*/

    std::vector<uint8_t> tem = mergeAndSplitVectors(DC_pred, flattened_AC);

    constexpr double BYTES_PER_MIB = 1024.0 * 1024.0;

    std::cout << static_cast<double>(encode(tem).getBuffer().size()) / BYTES_PER_MIB << "\n";
}

int main() {
    BMPFile bmpFile;
    if (getBmpDimensions("/home/risalor/Desktop/Multimedija/V2/slike BMP/Mercury.bmp", bmpFile)) {
        std::cout << "Width: " << bmpFile.infoHeader.width << ", Height: " << bmpFile.infoHeader.height << std::endl;
    }

    compressAsBlocks(bmpFile, 0);

    return 0;
}