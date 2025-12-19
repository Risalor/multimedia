#include <iostream>
#include <fstream>
#include <cstdint>
#include <Dense>
#include <vector>

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
            bmpFile.bmp(i, j) = static_cast<float>(byte);
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

void compressAsBlocks(BMPFile& bmpFile, uint8_t thc) {
    Eigen::MatrixXf H = hMat();
    
    uint32_t padded_width = bmpFile.infoHeader.width + (8 - bmpFile.infoHeader.width % 8) % 8;
    uint32_t padded_height = bmpFile.infoHeader.height + (8 - bmpFile.infoHeader.height % 8) % 8;
    
    uint32_t num_blocks = (padded_width / 8) * (padded_height / 8);
    uint32_t total_coeffs = num_blocks * 64;
    
    std::vector<float> flattened(total_coeffs, 0.0f);
    
    int zigzag[64] = {
        0, 1, 5, 6,14,15,27,28,
        2, 4, 7,13,16,26,29,42,
        3, 8,12,17,25,30,41,43,
        9,11,18,24,31,40,44,53,
        10,19,23,32,39,45,52,54,
        20,22,33,38,46,51,55,60,
        21,34,37,47,50,56,59,61,
        35,36,48,49,57,58,62,63
    };

    uint32_t blocks_x = padded_width / 8;
    uint32_t blocks_y = padded_height / 8;
    
    std::cout << "Processing " << blocks_y << "x" << blocks_x << " blocks (" << num_blocks << " total)" << std::endl;

    for(uint32_t block_y = 0; block_y < blocks_y; block_y++) {
        for(uint32_t block_x = 0; block_x < blocks_x; block_x++) {
            uint32_t img_y = block_y * 8;
            uint32_t img_x = block_x * 8;
            
            Eigen::MatrixXf block(8, 8);
            if(img_y + 8 <= bmpFile.infoHeader.height && img_x + 8 <= bmpFile.infoHeader.width) {
                block = bmpFile.bmp.block(img_y, img_x, 8, 8);
            } else {
                block.setZero();
                uint32_t copy_height = std::min(8u, bmpFile.infoHeader.height - img_y);
                uint32_t copy_width = std::min(8u, bmpFile.infoHeader.width - img_x);
                
                block.topLeftCorner(copy_height, copy_width) = bmpFile.bmp.block(img_y, img_x, copy_height, copy_width);
            }
            
            Eigen::MatrixXf transformed = H * block * H.transpose();
            
            uint32_t block_index = (block_y * blocks_x + block_x) * 64;
            
            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    float coeff = transformed(x, y);
                    int zigzag_index = zigzag[x * 8 + y];
                    
                    if(std::abs(coeff) < thc) {
                        flattened[block_index + zigzag_index] = 0.0f;
                    } else {
                        flattened[block_index + zigzag_index] = coeff;
                    }
                }
            }
        }
    }
}

int main() {
    BMPFile bmpFile;
    if (getBmpDimensions("/home/risalor/Desktop/Multimedija/DN2/slike BMP/Sun.bmp", bmpFile)) {
        std::cout << "Width: " << bmpFile.infoHeader.width << ", Height: " << bmpFile.infoHeader.height << std::endl;
    }

    compressAsBlocks(bmpFile, 90);

    return 0;
}