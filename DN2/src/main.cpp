#include "BitReader.hpp"
#include "BitWriter.hpp"
#include <bits/stdc++.h>
#include <cstring>
#include <Dense>

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

bool getBmpFile(const std::string& filename, BMPFile& bmpFile) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    file.read(reinterpret_cast<char*>(&bmpFile.fileHeader), sizeof(bmpFile.fileHeader));
    file.read(reinterpret_cast<char*>(&bmpFile.infoHeader), sizeof(bmpFile.infoHeader));

    if (bmpFile.fileHeader.file_type != 0x4D42) return false;
    if (bmpFile.infoHeader.bit_count != 8) return false;

    const int width = bmpFile.infoHeader.width;
    const int height = std::abs(bmpFile.infoHeader.height);
    const bool bottom_up = bmpFile.infoHeader.height > 0;

    const uint32_t row_bytes = width;
    const uint32_t row_padding = (4 - (row_bytes % 4)) % 4;

    file.seekg(bmpFile.fileHeader.offset_data, std::ios::beg);

    bmpFile.bmp = Eigen::MatrixXf(height, width);

    for (int i = 0; i < height; ++i) {
        int row = bottom_up ? (height - 1 - i) : i;

        for (int j = 0; j < width; ++j) {
            uint8_t pixel;
            file.read(reinterpret_cast<char*>(&pixel), 1);
            bmpFile.bmp(row, j) = static_cast<float>(pixel);
        }

        file.ignore(row_padding);
    }

    return true;
}

bool saveBmpFile(const std::string& filename, BMPFile& bmpFile) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) return false;

    const int width  = bmpFile.infoHeader.width;
    const int height = bmpFile.bmp.rows();

    const uint32_t row_bytes   = width;
    const uint32_t row_padding = (4 - (row_bytes % 4)) % 4;
    const uint32_t row_size    = row_bytes + row_padding;
    const uint32_t pixel_bytes = row_size * height;
    const uint32_t palette_size = 256 * 4;

    bmpFile.infoHeader.size = sizeof(BMPInfoHeader);
    bmpFile.infoHeader.bit_count = 8;
    bmpFile.infoHeader.height = height;
    bmpFile.infoHeader.size_image = pixel_bytes;
    bmpFile.infoHeader.colors_used = 256;

    bmpFile.fileHeader.offset_data =
        sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + palette_size;

    bmpFile.fileHeader.file_size =
        bmpFile.fileHeader.offset_data + pixel_bytes;

    file.write(reinterpret_cast<const char*>(&bmpFile.fileHeader), sizeof(bmpFile.fileHeader));
    file.write(reinterpret_cast<const char*>(&bmpFile.infoHeader), sizeof(bmpFile.infoHeader));

    for (int i = 0; i < 256; ++i) {
        uint8_t entry[4] = { (uint8_t)i, (uint8_t)i, (uint8_t)i, 0 };
        file.write(reinterpret_cast<char*>(entry), 4);
    }

    uint8_t pad = 0;

    for (int i = height - 1; i >= 0; --i) {
        for (int j = 0; j < width; ++j) {
            float v = bmpFile.bmp(i, j);
            v = std::clamp(v, 0.0f, 255.0f);
            uint8_t pixel = static_cast<uint8_t>(std::round(v));
            file.write(reinterpret_cast<char*>(&pixel), 1);
        }
        file.write(reinterpret_cast<char*>(&pad), row_padding);
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

std::vector<int16_t> splitAndReconstructVectors(const std::vector<uint8_t>& mergedBytes) {
    std::vector<int16_t> allValues;
    allValues.reserve(mergedBytes.size() / 2);
    
    for (size_t i = 0; i < mergedBytes.size(); i += 2) {
        uint16_t low = mergedBytes[i];
        uint16_t high = mergedBytes[i + 1];
        int16_t value = static_cast<int16_t>((high << 8) | low);
        allValues.push_back(value);
    }
    
    return allValues;
}

std::vector<int16_t> reverseDCPrediction(const std::vector<int16_t>& DC_pred) {
    if(DC_pred.empty()) return {};
    
    std::vector<int16_t> flattened_DC(DC_pred.size());
    
    flattened_DC[0] = DC_pred[0];
    
    for(uint32_t i = 1; i < DC_pred.size(); i++) {
        flattened_DC[i] = flattened_DC[i-1] - DC_pred[i];
    }
    
    return flattened_DC;
}

void decompressToBMP(BMPFile& bmpFile, const std::vector<int16_t>& flattened_DC, const std::vector<int16_t>& flattened_AC) {
    
    Eigen::MatrixXf H = hMat();
    
    uint32_t blocks_x = (bmpFile.infoHeader.width + 7) / 8;
    uint32_t blocks_y = (bmpFile.infoHeader.height + 7) / 8;
    uint32_t total_blocks = blocks_x * blocks_y;
    
    if(flattened_DC.size() != total_blocks) {
        std::cerr << "ERROR: DC size mismatch! Expected " << total_blocks << ", got " << flattened_DC.size() << std::endl;
        return;
    }
    
    if(flattened_AC.size() != total_blocks * 63) {
        std::cerr << "ERROR: AC size mismatch! Expected " << (total_blocks * 63) << ", got " << flattened_AC.size() << std::endl;
        return;
    }
    
    bmpFile.bmp = Eigen::MatrixXf(bmpFile.infoHeader.height, bmpFile.infoHeader.width);
    bmpFile.bmp.setZero();
    
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
    
    uint32_t block_index = 0;
    
    for(uint32_t block_y = 0; block_y < blocks_y; block_y++) {
        for(uint32_t block_x = 0; block_x < blocks_x; block_x++) {
            uint32_t img_i = block_y * 8;
            uint32_t img_j = block_x * 8;
            
            Eigen::MatrixXf coeff_matrix(8, 8);
            coeff_matrix.setZero();
            
            float dc_value = static_cast<float>(flattened_DC[block_index]);
            coeff_matrix(0, 0) = dc_value;
            
            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    if(x == 0 && y == 0) continue;
                    
                    int zigzag_idx = zigzag[x][y];
                    int ac_index = block_index * 63 + (zigzag_idx - 1);
                    
                    if(ac_index < flattened_AC.size()) {
                        int16_t ac_val = flattened_AC[ac_index];
                        
                        if(ac_val != 0) {
                            float ac_value = static_cast<float>(ac_val);
                            coeff_matrix(x, y) = ac_value;
                        }
                    } else {
                        std::cerr << "ERROR: AC index out of bounds!" << std::endl;
                    }
                }
            }
            
            Eigen::MatrixXf block_reconstructed = H * coeff_matrix * H.transpose();
            
            uint32_t copy_height = std::min(8u, bmpFile.infoHeader.height - img_i);
            uint32_t copy_width = std::min(8u, bmpFile.infoHeader.width - img_j);
            
            bmpFile.bmp.block(img_i, img_j, copy_height, copy_width) = 
                block_reconstructed.topLeftCorner(copy_height, copy_width);
            
            block_index++;
        }
    }
}

std::vector<uint32_t> generateMRandoms(LCG& gen, uint8_t M, uint8_t thc) {
    std::vector<uint32_t> indexes;
    
    int max_index = 32;
    
    if (thc > 0) {
        max_index = 64 - thc;
        if (max_index > 32) max_index = 32;
        if (max_index < 4) max_index = 4;
    }
    
    int min_start = 4;
    int max_start = max_index - 3;
    
    if (min_start > max_start) {
        return indexes;
    }
    
    for (int i = 0; i < M; i++) {
        bool valid_position = false;
        int attempts = 0;
        const int MAX_ATTEMPTS = 100;
        
        while (!valid_position && attempts < MAX_ATTEMPTS) {
            attempts++;
    
            uint32_t rand_val = gen.getRandom(0, UINT32_MAX);
            uint32_t start_idx = min_start + (rand_val % (max_start - min_start + 1));
            
            valid_position = true;
            for (size_t j = 0; j < indexes.size(); j += 3) {
                uint32_t existing_start = indexes[j];
                if (!(start_idx + 2 < existing_start || start_idx > existing_start + 2)) {
                    valid_position = false;
                    break;
                }
            }
            
            if (valid_position) {
                indexes.push_back(start_idx);
                indexes.push_back(start_idx + 1);
                indexes.push_back(start_idx + 2);
            }
        }
        
        if (!valid_position) {
            break;
        }
    }
    return indexes;
}

std::vector<uint8_t> createMessageBuffer(const std::string& message) {
    uint32_t messageLengthInBits = static_cast<uint32_t>(message.size() * 8);
    
    std::vector<uint8_t> buffer;
    buffer.reserve(sizeof(uint32_t) + message.size());
    
    buffer.push_back(static_cast<uint8_t>((messageLengthInBits >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((messageLengthInBits >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((messageLengthInBits >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(messageLengthInBits & 0xFF));
    
    buffer.insert(buffer.end(), message.begin(), message.end());
    
    return buffer;
}


void encodeMassage(std::vector<int16_t>& flattened_AC, uint8_t M, uint8_t thc, BitReader& reader) {
    size_t total_bits = reader.getBuffer().size() * 8;
    size_t bit_pos = 0;
    uint32_t total_blocks = flattened_AC.size() / 63;

    LCG gen(thc * M, 1664525, 1013904223, 4294967296u - 1);
    
    for (uint32_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        if (bit_pos + 1 >= total_bits) {
            break;
        }
        
        std::vector<uint32_t> block_triplets = generateMRandoms(gen, M, thc);
        
        if (block_triplets.empty()) {
            continue;
        }
        
        size_t triplet_idx = 0;
        
        while (triplet_idx + 2 < block_triplets.size() && bit_pos + 1 < total_bits) {
            uint8_t x1 = reader.readNextBit();
            uint8_t x2 = reader.readNextBit();
            
            uint32_t rel_idx1 = block_triplets[triplet_idx];
            uint32_t rel_idx2 = block_triplets[triplet_idx + 1];
            uint32_t rel_idx3 = block_triplets[triplet_idx + 2];
            
            uint32_t abs_idx1 = block_idx * 63 + rel_idx1;
            uint32_t abs_idx2 = block_idx * 63 + rel_idx2;
            uint32_t abs_idx3 = block_idx * 63 + rel_idx3;
            
            if (abs_idx3 >= flattened_AC.size()) {
                break;
            }
            
            uint8_t C1 = flattened_AC[abs_idx1] & 1;
            uint8_t C2 = flattened_AC[abs_idx2] & 1;
            uint8_t C3 = flattened_AC[abs_idx3] & 1;
            
            uint8_t current_x1 = C1 ^ C2;
            uint8_t current_x2 = C2 ^ C3;
            
            if (x1 == current_x1 && x2 != current_x2) {
                flattened_AC[abs_idx3] ^= 1;
            } else if (x1 != current_x1 && x2 == current_x2) {
                flattened_AC[abs_idx1] ^= 1;
            } else if (x1 != current_x1 && x2 != current_x2) {
                flattened_AC[abs_idx2] ^= 1;
            }
            
            triplet_idx += 3;
            bit_pos += 2;
        }
    }
}

std::string decodeMessage(std::vector<int16_t>& flattened_AC, uint8_t M, uint8_t thc) {
    std::vector<uint8_t> extracted_bits;
    std::vector<uint8_t> message_bytes;
    
    uint32_t total_blocks = flattened_AC.size() / 63;
    LCG gen(thc * M, 1664525, 1013904223, 4294967296u - 1);
    
    for (uint32_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        std::vector<uint32_t> block_triplets = generateMRandoms(gen, M, thc);
        
        if (block_triplets.empty()) {
            continue;
        }
        
        size_t triplet_idx = 0;
        
        while (triplet_idx + 2 < block_triplets.size()) {
            uint32_t rel_idx1 = block_triplets[triplet_idx];
            uint32_t rel_idx2 = block_triplets[triplet_idx + 1];
            uint32_t rel_idx3 = block_triplets[triplet_idx + 2];
            
            uint32_t abs_idx1 = block_idx * 63 + rel_idx1;
            uint32_t abs_idx2 = block_idx * 63 + rel_idx2;
            uint32_t abs_idx3 = block_idx * 63 + rel_idx3;
            
            if (abs_idx3 >= flattened_AC.size()) {
                break;
            }
            
            uint8_t C1 = flattened_AC[abs_idx1] & 1;
            uint8_t C2 = flattened_AC[abs_idx2] & 1;
            uint8_t C3 = flattened_AC[abs_idx3] & 1;
            
            uint8_t x1 = C1 ^ C2;
            uint8_t x2 = C2 ^ C3;
            
            extracted_bits.push_back(x1);
            extracted_bits.push_back(x2);
            
            triplet_idx += 3;
            
            if (extracted_bits.size() >= 32) {
                uint32_t message_length_bits = 0;
                for (int i = 0; i < 32; i++) {
                    message_length_bits |= (extracted_bits[i] << (31 - i));
                }
                
                size_t total_bits_needed = 32 + message_length_bits;
                
                if (extracted_bits.size() >= total_bits_needed) {
                    for (size_t i = 32; i < total_bits_needed; i += 8) {
                        if (i + 7 < extracted_bits.size()) {
                            uint8_t byte = 0;
                            for (int j = 0; j < 8; j++) {
                                byte |= (extracted_bits[i + j] << (7 - j));
                            }
                            message_bytes.push_back(byte);
                        }
                    }
                    
                    return std::string(message_bytes.begin(), message_bytes.end());
                }
            }
        }
    }
    
    if (!message_bytes.empty()) {
        return std::string(message_bytes.begin(), message_bytes.end());
    }
    
    return "";
}

void compressAsBlocks(const std::string compFile_input, const std::string compFile_output, uint8_t thc, std::string massage, uint8_t M) {
    BMPFile bmpFile;

    std::vector<uint8_t> msg = createMessageBuffer(massage);
    BitReader MassageReader(msg);

    if(!getBmpFile(compFile_input, bmpFile)) {
        throw std::runtime_error("Could not open bmp file!");
    }

    Eigen::MatrixXf H = hMat();
    
    uint32_t blocks_x = (bmpFile.infoHeader.width + 7) / 8;
    uint32_t blocks_y = (bmpFile.infoHeader.height + 7) / 8;
    uint32_t total_blocks = blocks_x * blocks_y;
    
    uint32_t size_DC = total_blocks;
    uint32_t size_AC = total_blocks * 63;
    
    std::vector<int16_t> flattened_DC(size_DC, 0);
    std::vector<int16_t> flattened_AC(size_AC, 0);
    
    std::cout << "Blocks: " << blocks_x << "x" << blocks_y << " = " << total_blocks << std::endl;
    std::cout << "DC size: " << flattened_DC.size() << ", AC size: " << flattened_AC.size() << std::endl;
    
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
    
    uint32_t block_index = 0;
    
    for(uint32_t i = 0; i < bmpFile.infoHeader.height; i += 8) {
        for(uint32_t j = 0; j < bmpFile.infoHeader.width; j += 8) {
            Eigen::MatrixXf block(8, 8);
            uint32_t copy_height = std::min(8u, bmpFile.infoHeader.height - i);
            uint32_t copy_width = std::min(8u, bmpFile.infoHeader.width - j);
            
            if(copy_height < 8 || copy_width < 8) {
                block.setZero();
                block.topLeftCorner(copy_height, copy_width) = 
                    bmpFile.bmp.block(i, j, copy_height, copy_width);
            } else {
                block = bmpFile.bmp.block(i, j, 8, 8);
            }
            
            Eigen::MatrixXf temp = H.transpose() * block * H;
            
            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    float coeff = temp(x, y);
                    
                    if(x == 0 && y == 0) {
                        flattened_DC[block_index] = static_cast<int16_t>(std::round(coeff));
                    } else {
                        int zigzag_idx = zigzag[x][y];
                        int ac_index = block_index * 63 + (zigzag_idx - 1);
                        
                        flattened_AC[ac_index] = static_cast<int16_t>(std::round(coeff));
                    }
                }
            }
            
            for (size_t k = 0; k < thc; k++) {
                if (k < 63) {
                    int idx = (block_index * 63) + (63 - 1 - k);
                    if (idx >= 0 && idx < flattened_AC.size()) {
                        flattened_AC[idx] = 0;
                    }
                }
            }

            block_index++;
        }
    }
    
    if(block_index != total_blocks) {
        std::cerr << "ERROR: Processed " << block_index << " blocks, expected " << total_blocks << std::endl;
    }
    
    std::vector<int16_t> DC_pred(size_DC, 0);
    DC_pred[0] = flattened_DC[0];
    
    for(uint32_t i = 1; i < size_DC; i++) {
        DC_pred[i] = flattened_DC[i-1] - flattened_DC[i];
    }

    encodeMassage(flattened_AC, M, thc, MassageReader);
    
    std::vector<uint8_t> tem = mergeAndSplitVectors(DC_pred, flattened_AC);

    tem = encode(tem).getBuffer();

    std::ofstream file(compFile_output, std::ios::binary);
    file.write(reinterpret_cast<const char*>(&total_blocks), sizeof(total_blocks));
    file.write(reinterpret_cast<const char*>(&bmpFile.fileHeader), sizeof(bmpFile.fileHeader));
    file.write(reinterpret_cast<const char*>(&bmpFile.infoHeader), sizeof(bmpFile.infoHeader));

    file.write(reinterpret_cast<const char*>(tem.data()), tem.size() * sizeof(int8_t));
}

std::vector<uint8_t> readRemaining(std::ifstream& file) {
    std::streampos currentPos = file.tellg();
    
    file.seekg(0, std::ios::end);
    std::streampos endPos = file.tellg();
    
    std::streamsize remainingBytes = endPos - currentPos;
    
    file.seekg(currentPos);
    
    std::vector<uint8_t> data(remainingBytes);
    file.read(reinterpret_cast<char*>(data.data()), remainingBytes);
    
    return data;
}

void decompress(const std::string& filePath_input, const std::string& filePath_output, uint8_t M, uint8_t N) {
    std::ifstream file(filePath_input, std::ios::binary);
    uint32_t total_blocks;
    BMPFile bmpFile;

    file.read(reinterpret_cast<char*>(&total_blocks), sizeof(total_blocks));
    file.read(reinterpret_cast<char*>(&bmpFile.fileHeader), sizeof(bmpFile.fileHeader));
    file.read(reinterpret_cast<char*>(&bmpFile.infoHeader), sizeof(bmpFile.infoHeader));
    std::vector<uint8_t> data = readRemaining(file);

    BitReader reader(data);

    data = decode(reader);

    std::vector<int16_t> all = splitAndReconstructVectors(data);

    std::vector<int16_t> DC_pred(total_blocks, 0);
    DC_pred = std::vector<int16_t>(all.begin(), all.begin() + total_blocks);
    DC_pred = reverseDCPrediction(DC_pred);
    std::vector<int16_t> flattened_AC = std::vector<int16_t>(all.begin() + DC_pred.size(), all.end());

    std::cout << "\nThe hidden massage is: " << decodeMessage(flattened_AC, M, N) << "\n";

    decompressToBMP(bmpFile, DC_pred, flattened_AC);
    
    saveBmpFile(filePath_output, bmpFile);
}

double calculatePSNR(const BMPFile& original, const BMPFile& compressed) {
    if (original.bmp.rows() != compressed.bmp.rows() ||
        original.bmp.cols() != compressed.bmp.cols()) {
        throw std::runtime_error("Image dimensions don't match!");
    }
    
    int rows = original.bmp.rows();
    int cols = original.bmp.cols();
    
    double mse = 0.0;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            double diff = original.bmp(i, j) - compressed.bmp(i, j);
            mse += diff * diff;
        }
    }
    mse /= (rows * cols);
    
    if (mse == 0.0) return INFINITY;
    
    const double MAX_VALUE = 255.0;
    return 20 * log10(MAX_VALUE) - 10 * log10(mse);
}

double calculateEntropy(const BMPFile& image) {
    std::map<int, int> histogram;
    int totalPixels = image.bmp.rows() * image.bmp.cols();
    
    for (int i = 0; i < image.bmp.rows(); i++) {
        for (int j = 0; j < image.bmp.cols(); j++) {
            int pixel = static_cast<int>(std::round(image.bmp(i, j)));
            histogram[pixel]++;
        }
    }
    
    double entropy = 0.0;
    for (const auto& pair : histogram) {
        double probability = static_cast<double>(pair.second) / totalPixels;
        entropy -= probability * log2(probability);
    }
    
    return entropy;
}

double calculateBlockingArtifact(const BMPFile& image) {
    int rows = image.bmp.rows();
    int cols = image.bmp.cols();
    int blockSize = 8;
    
    double totalBlocking = 0.0;
    int count = 0;
    
    for (int i = 0; i < rows; i++) {
        for (int j = blockSize; j < cols; j += blockSize) {
            if (j >= cols) continue;
            double diff = image.bmp(i, j) - image.bmp(i, j-1);
            totalBlocking += diff * diff;
            count++;
        }
    }
    
    for (int i = blockSize; i < rows; i += blockSize) {
        if (i >= rows) continue;
        for (int j = 0; j < cols; j++) {
            double diff = image.bmp(i, j) - image.bmp(i-1, j);
            totalBlocking += diff * diff;
            count++;
        }
    }
    
    if (count == 0) return 0.0;
    return totalBlocking / count;
}

double calculateBlockingArtifactsFormula(const BMPFile& image) {
    const int blockSize = 8;
    int width = image.infoHeader.width;
    int height = image.infoHeader.height;
    
    int N = (width + blockSize - 1) / blockSize;
    int M = (height + blockSize - 1) / blockSize;
    
    double totalBlocking = 0.0;
    int count = 0;
    
    for (int j = 1; j < N; j++) {
        int col = j * blockSize;
        if (col >= width) continue;
        
        for (int i = 0; i < height; i++) {
            if (col > 0) {
                double diff = image.bmp(i, col) - image.bmp(i, col - 1);
                totalBlocking += std::abs(diff);
                count++;
            }
        }
    }
    
    for (int i = 1; i < M; i++) {
        int row = i * blockSize;
        if (row >= height) continue;
        
        for (int j = 0; j < width; j++) {
            if (row > 0) {
                double diff = image.bmp(row, j) - image.bmp(row - 1, j);
                totalBlocking += std::abs(diff);
                count++;
            }
        }
    }
    
    if (count == 0) return 0.0;
    return totalBlocking / count;
}

void calculateMetrics(const std::string& originalPath, 
                     const std::string& compressedPath) {
    BMPFile original, compressed;
    
    if (!getBmpFile(originalPath, original)) {
        std::cerr << "Failed to load original image!" << std::endl;
        return;
    }
    
    if (!getBmpFile(compressedPath, compressed)) {
        std::cerr << "Failed to load compressed image!" << std::endl;
        return;
    }
    
    double psnr = calculatePSNR(original, compressed);
    std::cout << "PSNR: " << psnr << " dB" << std::endl;
    
    double entropyOriginal = calculateEntropy(original);
    double entropyCompressed = calculateEntropy(compressed);
    std::cout << "Original Entropy: " << entropyOriginal << " bits/pixel" << std::endl;
    std::cout << "Compressed Entropy: " << entropyCompressed << " bits/pixel" << std::endl;
    
    double blockingOriginal = calculateBlockingArtifactsFormula(original);
    double blockingCompressed = calculateBlockingArtifactsFormula(compressed);
    std::cout << "Original Blocking Measure: " << blockingOriginal << std::endl;
    std::cout << "Compressed Blocking Measure: " << blockingCompressed << std::endl;
    
    double blockingIncrease = blockingCompressed - blockingOriginal;
    std::cout << "Blocking Artifact Increase: " << blockingIncrease << std::endl;
}

int main() {

    std::string pathFull = "/home/risalor/Desktop/Multimedija/V2/slike BMP/Balloons.bmp";
    std::string pathcomp = "teh_new_test.bmp";

    compressAsBlocks(pathFull, "file.bin", 30, "hallo my name is kristjan how are you on this nice day??", 2);
    decompress("file.bin", "teh_new_test.bmp", 2, 30);

    calculateMetrics(pathFull, pathcomp);

    return 0;
}