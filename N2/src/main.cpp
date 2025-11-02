#include "BitReader.hpp"
#include "BitWriter.hpp"
#include <bits/stdc++.h>

struct TableElement {
    uint64_t freq = 0;
    uint64_t upper = 0;
    uint64_t lower = 0;
    float probability = 0.f;
    uint8_t element = 0;
};

uint64_t lowerBound = 0;
uint64_t upperBound = 0;
uint64_t firstQuarter = 0;
uint64_t secondQuarter = 0;
uint64_t thirdQuarter = 0;
uint64_t comFreq = 0;

std::array<TableElement, 256> makeTable(const std::vector<char>& buffer) {
    std::array<TableElement, 256> tempTable;
    std::array<int16_t, 256> firstAppearance;
    firstAppearance.fill(-1);

    uint8_t counter = 0;
    for(const auto& byte : buffer) {
        tempTable[byte].freq++;
        comFreq++;
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

void encode(std::vector<char>& bts, std::array<TableElement, 256>& table) {
    BitWriter writer;
    uint8_t E3_counter = 0;
    for(const auto& bit : bts) {
        uint64_t step = (upperBound - lowerBound + 1) / comFreq;

        //std::cout << "Step: " << step << " = (" << upperBound << " - " << lowerBound << " + " << 1 << ") / " << comFreq << "\n";

        upperBound = lowerBound + step * table[bit].upper - 1;

        //std::cout << "Upper: " << upperBound << " = " << lowerBound << " + " << step << " * " << table[bit].upper << " - " << 1 << "\n";

        lowerBound = lowerBound + step * table[bit].lower;

        //std::cout << "Lower: " << lowerBound << " = " << lowerBound << " + " << step << " * " << table[bit].lower << "\n";

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

            } else if ((lowerBound >= firstQuarter) && (upperBound < thirdQuarter)) {
                lowerBound = 2 * (lowerBound - firstQuarter);
                upperBound = 2 * (upperBound - firstQuarter) + 1;
                E3_counter++;
            } else {
                break;
            }
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
}

int main(int argc, char* argv[]) {
    lowerBound = 0;
    upperBound = std::pow(2, 8 - 1) - 1;

    secondQuarter = std::floor((static_cast<double>(upperBound) + 1.f) / 2.f);
    firstQuarter = std::floor(static_cast<double>(secondQuarter) / 2.f);
    thirdQuarter = std::floor(static_cast<double>(firstQuarter) * 3.f);

    std::vector<char> bts = { 'G', 'E', 'M', 'M', 'A' };
    std::array<TableElement, 256> a = makeTable(bts);

    encode(bts, a);

    /*for(auto& it : a) {
        std::cout << it.element << " f:" << it.freq << " p:" << it.probability << " l:" << it.lower << " u:" << it.upper << "\n";
    }*/
    
    return 0;
}