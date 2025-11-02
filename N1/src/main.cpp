#include "BitReader.hpp"
#include "BitWriter.hpp"
#include <bits/stdc++.h>

std::vector<uint32_t> findPatternsBruteForce(BitReader& reader, std::vector<bool>& pattern) {
    std::vector<uint32_t> patternLocations;
    uint32_t location = 0;

    reader.resetToStart();
    
    while(!reader.isEnd()) {
        reader.setSavePoint();
        bool match = true;
        
        uint32_t newLocation = location;

        for(uint32_t i = 0; i < pattern.size(); i++) {
            if(reader.isEnd()) {
                match = false;
                break;
            }
            
            bool bit = reader.readNextBit();
            if(bit != pattern[i]) {
                match = false;
                break;
            }
        }
        
        if(match) {
            patternLocations.push_back(location);
            location += pattern.size();
        } else {
            reader.loadFromSavePoint();
            if(!reader.isEnd()) {
                reader.readNextBit();
                location++;
            }
        }
    }
    
    return patternLocations;
}

std::vector<uint32_t> findPatternsOptimized(BitReader& reader, std::vector<bool>& pattern) {
    std::vector<uint32_t> patternLocations;

    if (pattern.empty() || pattern.size() > 64) {
        throw std::runtime_error("Empty pattern or pattern longer than 64 bits");
    }

    reader.resetToStart();

    const size_t patternSize = pattern.size();
    uint64_t patternMask = 0;
    uint64_t bitMask = (1ULL << patternSize) - 1;

    for (size_t i = 0; i < patternSize; i++) {
        if (pattern[i]) {
            patternMask |= (1ULL << (patternSize - 1 - i));
        }
    }

    uint64_t window = 0;
    for (size_t i = 0; i < patternSize && !reader.isEnd(); i++) {
        bool bit = reader.readNextBit();
        window = ((window << 1) | (bit ? 1 : 0)) & bitMask;
    }

    uint32_t location = 0;

    while (!reader.isEnd()) {
        if (window == patternMask) {
            patternLocations.push_back(location);
            
            window = 0;
            
            for (size_t i = 0; i < patternSize && !reader.isEnd(); i++) {
                bool bit = reader.readNextBit();
                window = ((window << 1) | (bit ? 1 : 0)) & bitMask;
            }
            
            location += patternSize;
            continue;
        }

        bool newBit = reader.readNextBit();
        window = ((window << 1) | (newBit ? 1 : 0)) & bitMask;
        location++;
    }

    return patternLocations;
}

std::vector<bool> stringToBitVector(const std::string& bitString) {
    std::vector<bool> bits;
    for (char c : bitString) {
        if (c == '0') {
            bits.push_back(false);
        } else if (c == '1') {
            bits.push_back(true);
        } else {
            throw std::invalid_argument("Invalid bit string - must contain only 0 and 1");
        }
    }
    return bits;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " <input_file> <option> <data1> [data2]\n";
    std::cout << "Options:\n";
    std::cout << "  f   - find bit pattern from <data1>\n";
    std::cout << "  fr  - replace bit pattern from <data1> with bits from <data2>\n";
    std::cout << "Examples:\n";
    std::cout << "  " << programName << " test.bin f 0110100001100101011011000110110001101111\n";
    std::cout << "  " << programName << " test.bin fr 0000000 1111\n";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string inputFile = argv[1];
    std::string option = argv[2];
    
    if (option != "f" && option != "fr") {
        std::cerr << "Error: Invalid option. Use 'f' for find or 'fr' for find and replace.\n";
        printUsage(argv[0]);
        return 1;
    }

    if ((option == "f" && argc != 4) || (option == "fr" && argc != 5)) {
        std::cerr << "Error: Incorrect number of arguments for option '" << option << "'\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        BitReader reader(inputFile);
        std::vector<bool> pattern1 = stringToBitVector(argv[3]);
        
        if (option == "f") {
            std::vector<uint32_t> locations = findPatternsOptimized(reader, pattern1);
            
            std::cout << "Pattern found at locations: ";
            for (size_t i = 0; i < locations.size(); i++) {
                std::cout << locations[i];
                if (i < locations.size() - 1) {
                    std::cout << ", ";
                }
            }
            if (locations.empty()) {
                std::cout << "No matches found";
            }
            std::cout << std::endl;
            
        } else if (option == "fr") {
            std::vector<bool> pattern2 = stringToBitVector(argv[4]);
            
            std::vector<uint32_t> locations = findPatternsOptimized(reader, pattern1);
            
            reader.resetToStart();
            
            std::string outputFile = inputFile;
            size_t dotPos = outputFile.find_last_of('.');
            if (dotPos != std::string::npos) {
                outputFile = outputFile.substr(0, dotPos) + "_modified" + outputFile.substr(dotPos);
            } else {
                outputFile += "_modified";
            }
            
            BitWriter writer;
            uint32_t index = 0;
            
            while(!reader.isEnd()) {
                auto ite = std::find(locations.begin(), locations.end(), index);
                
                if(ite == locations.end()) {
                    writer.writeBit(reader.readNextBit());
                    index++;
                } else {
                    for(auto bit : pattern2) {
                        writer.writeBit(bit);
                    }
                    
                    for(uint32_t i = 0; i < pattern1.size(); i++) {
                        reader.readNextBit();
                        index++;
                    }
                }
            }
            
            writer.writeBufferToFile(outputFile);
            std::cout << "File processed successfully. Modified file saved as: " << outputFile << std::endl;
            std::cout << "Pattern found and replaced at " << locations.size() << " locations" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}