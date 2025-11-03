#include "BitReader.hpp"
#include "BitWriter.hpp"
#include <bits/stdc++.h>

struct TableElement {
    uint32_t freq = 0;
    uint32_t upper = 0;
    uint32_t lower = 0;
    float probability = 0.f;
    uint8_t element = 0;
};

std::vector<TableElement> orderedElements;

std::array<TableElement, 256> makeTable(const std::vector<char>& buffer) {

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
    toReturnTable.reserve(counter);
    toReturnTable.resize(counter);

    for(uint8_t i = 0; i < 255; i++) {
        if(firstAppearance[i] != -1) {
            toReturnTable[firstAppearance[i]] = tempTable[i];
        }
    }

    orderedElements = toReturnTable;

    toReturnTable[0].probability = static_cast<double>(toReturnTable[0].freq) / static_cast<double>(buffer.size());
    toReturnTable[0].lower = 0;
    toReturnTable[0].upper = toReturnTable[0].lower + toReturnTable[0].freq;

    for(uint8_t i = 1; i < toReturnTable.size(); i++) {
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

    for(uint8_t i = 1; i < table.size(); i++) {
        table[i].probability = static_cast<double>(table[i].freq) / static_cast<double>(freq);
        table[i].lower = table[i - 1].upper;
        table[i].upper = table[i].lower + table[i].freq;
    }

    for(auto& it : table) {
        tempTable[it.element] = it;
    }

    return tempTable;
}

void encode(std::vector<char>& bts) {
    std::array<TableElement, 256> table = makeTable(bts);
    BitWriter writer;

    uint64_t comFreq = 0;

    writer.writeByte(static_cast<uint8_t>(orderedElements.size()));
    std::cout << "Size is: " << static_cast<uint8_t>(orderedElements.size()) << "\n";

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
                std::cout << "0";

                for(uint8_t i = 0; i < E3_counter; i++) {
                    writer.writeBit(1);
                    std::cout << "1";
                }

                E3_counter = 0;

            } else if (lowerBound >= secondQuarter) {
                lowerBound = 2 * (lowerBound - secondQuarter);
                upperBound = 2 * (upperBound - secondQuarter) + 1;
                writer.writeBit(1);
                std::cout << "1";

                for(uint8_t i = 0; i < E3_counter; i++) {
                    writer.writeBit(0);
                    std::cout << "0";
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

        std::cout << "01";

        for(uint8_t i = 0; i < E3_counter; i++) {
            writer.writeBit(1);
            std::cout << "1";
        }
    } else {
        writer.writeBit(1);
        writer.writeBit(0);

        std::cout << "10";

        for(uint8_t i = 0; i < E3_counter; i++) {
            writer.writeBit(0);
            std::cout << "0";
        }
    }

    std::cout << "\n";

    writer.writeBufferToFile("test.bin");
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

void decode(const std::string& filePath) {
    uint64_t comFreq = 0;
    uint64_t lowerBound = 0;
    uint64_t upperBound = std::pow(2, 64 - 1) - 1;

    uint64_t secondQuarter = std::floor((static_cast<double>(upperBound) + 1.f) / 2.f);
    uint64_t firstQuarter = std::floor(static_cast<double>(secondQuarter) / 2.f);
    uint64_t thirdQuarter = std::floor(static_cast<double>(firstQuarter) * 3.f);

    BitReader reader(filePath);

    uint8_t eleC = static_cast<uint8_t>(reader.readNextByte());

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

    for (uint8_t c : out) std::cout << (char)c;
    std::cout << '\n';
}

int main(int argc, char* argv[]) {
    std::vector<char> bts = { 'G', 'E', 'M', 'M', 'A' };
    std::array<TableElement, 256> a = makeTable(bts);

    encode(bts);
    decode("test.bin");

    /*for(auto& it : a) {
        std::cout << it.element << " f:" << it.freq << " p:" << it.probability << " l:" << it.lower << " u:" << it.upper << "\n";
    }*/
    
    return 0;
}