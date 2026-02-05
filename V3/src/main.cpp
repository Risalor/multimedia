#include <iostream>
#include <vector>
#include <string>
#include <bits/stdc++.h>
#include <cstring>
#include "BitWriter.hpp"
#include "BitReader.hpp"

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

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

struct VideoInfo {
    double fps;
    uint32_t w;
    uint32_t h;
};

struct MotionVector {
    int32_t dx, dy, x, y;
    float mse;
    
    MotionVector(int32_t dx = 0, int32_t dy = 0, int32_t x = 0, int32_t y = 0, float mse = 0.0f) : dx(dx), dy(dy), x(x), y(y), mse(mse) {}
};

enum FrameType {
    I_FRAME,
    P_FRAME,
    B_FRAME
};

struct FrameData {
    FrameType type = B_FRAME;
    Eigen::MatrixXf frame;
    std::vector<MotionVector> motionVectorsPrev;
    std::vector<MotionVector> motionVectorsNext;
    uint32_t prev = 0;
    uint32_t next = 0;
};

std::pair<int, int> getPaddedDimensions(int width, int height, int block_size = 16) {
    int padded_width = ((width + block_size - 1) / block_size) * block_size;
    int padded_height = ((height + block_size - 1) / block_size) * block_size;
    return {padded_width, padded_height};
}

FrameData padFrame(const FrameData& frame, int block_size = 16) {
    int original_height = frame.frame.rows();
    int original_width = frame.frame.cols();
    
    auto [padded_width, padded_height] = getPaddedDimensions(original_width, original_height, block_size);
    
    Eigen::MatrixXf padded_frame = Eigen::MatrixXf::Zero(padded_height, padded_width);
    padded_frame.block(0, 0, original_height, original_width) = frame.frame;
    
    FrameData dat = frame;
    dat.frame = padded_frame;

    return dat;
}

std::vector<FrameData> extractVideoFrames(const std::string& filename, double& outFramerate, int maxFrames = -1) {
    std::vector<FrameData> frames;
    
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, filename.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Error: Could not open video file: " << filename << std::endl;
        return frames;
    }
    
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Error: Could not find stream information" << std::endl;
        avformat_close_input(&formatContext);
        return frames;
    }
    
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    
    if (videoStreamIndex == -1) {
        std::cerr << "Error: Could not find video stream" << std::endl;
        avformat_close_input(&formatContext);
        return frames;
    }
    
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
    if (!codec) {
        std::cerr << "Error: Unsupported codec" << std::endl;
        avformat_close_input(&formatContext);
        return frames;
    }
    
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecContext, codecParameters);
    
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Error: Could not open codec" << std::endl;
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return frames;
    }
    
    SwsContext* swsContext = sws_getContext(
        codecContext->width, codecContext->height, codecContext->pix_fmt,
        codecContext->width, codecContext->height, AV_PIX_FMT_GRAY8,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    
    if (!swsContext) {
        std::cerr << "Error: Could not create scaling context" << std::endl;
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return frames;
    }
    
    AVFrame* frame = av_frame_alloc();
    AVFrame* grayFrame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    
    int grayBufferSize = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, codecContext->width, codecContext->height, 1);
    uint8_t* grayBuffer = (uint8_t*)av_malloc(grayBufferSize);
    av_image_fill_arrays(grayFrame->data, grayFrame->linesize, grayBuffer, AV_PIX_FMT_GRAY8, codecContext->width, codecContext->height, 1);
    
    int frameCount = 0;
    int width = codecContext->width;
    int height = codecContext->height;
    
    std::cout << "Video info: " << width << "x" << height << " pixels" << std::endl;

    AVStream* videoStream = formatContext->streams[videoStreamIndex];

    AVRational avgFrameRate = videoStream->avg_frame_rate;
    double fps = av_q2d(avgFrameRate);

    std::cout << "Video framerate: " << fps << " fps" << std::endl;

    outFramerate = fps;
    
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex) {
            if (avcodec_send_packet(codecContext, packet) < 0) {
                break;
            }
            
            while (avcodec_receive_frame(codecContext, frame) >= 0) {
                sws_scale(swsContext, frame->data, frame->linesize,
                          0, height, grayFrame->data, grayFrame->linesize);
                
                Eigen::MatrixXf eigenFrame(height, width);
                uint8_t* data = grayFrame->data[0];
                int stride = grayFrame->linesize[0];
                
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        eigenFrame(y, x) = static_cast<float>(data[y * stride + x]);
                    }
                }
                
                FrameData fram;
                fram.frame = eigenFrame;
                frames.push_back(fram);
                frameCount++;
                
                if (maxFrames > 0 && frameCount >= maxFrames) {
                    av_packet_unref(packet);
                    goto cleanup;
                }
            }
        }
        av_packet_unref(packet);
    }
    
cleanup:
    av_free(grayBuffer);
    av_frame_free(&frame);
    av_frame_free(&grayFrame);
    av_packet_free(&packet);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    
    std::cout << "Extracted " << frames.size() << " frames" << std::endl;
    return frames;
}

double MSE(const std::vector<FrameData>& frames, size_t frame1_idx, size_t frame2_idx, int x1, int y1, int x2, int y2, int width, int height) {
    __m128 sum_vec = _mm_setzero_ps();
    float sum_scalar = 0.0f;
    
    const Eigen::MatrixXf& mat1 = frames[frame1_idx].frame;
    const Eigen::MatrixXf& mat2 = frames[frame2_idx].frame;
    
    int cols1 = mat1.cols();
    int cols2 = mat2.cols();
    
    for (int dy = 0; dy < height; dy++) {
        const float* row1 = mat1.data() + (y1 + dy) * cols1 + x1;
        const float* row2 = mat2.data() + (y2 + dy) * cols2 + x2;
        
        int dx = 0;
        for (; dx + 4 <= width; dx += 4) {
            __m128 a = _mm_loadu_ps(row1 + dx);
            __m128 b = _mm_loadu_ps(row2 + dx);
            __m128 diff = _mm_sub_ps(a, b);
            __m128 sq = _mm_mul_ps(diff, diff);
            sum_vec = _mm_add_ps(sum_vec, sq);
        }
        
        for (; dx < width; dx++) {
            float diff = row1[dx] - row2[dx];
            sum_scalar += diff * diff;
        }
    }
    
    alignas(16) float sum_array[4];
    _mm_store_ps(sum_array, sum_vec);
    
    float total_sum = sum_scalar + sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];
    
    return total_sum / (width * height);
}

MotionVector getMotionVector(const std::vector<FrameData>& frames, uint32_t reference_frame_idx, uint32_t current_frame_idx, int32_t block_x, int32_t block_y, int search_range) {
    const Eigen::MatrixXf& ref_frame  = frames[reference_frame_idx].frame;
    const Eigen::MatrixXf& curr_frame = frames[current_frame_idx].frame;

    constexpr int NOMINAL_BLOCK_SIZE = 16;
    int block_w = NOMINAL_BLOCK_SIZE;
    int block_h = NOMINAL_BLOCK_SIZE;

    if (block_x + block_w > curr_frame.cols()) {
        block_w = curr_frame.cols() - block_x;
    }
    if (block_y + block_h > curr_frame.rows()) {
        block_h = curr_frame.rows() - block_y;
    }

    block_w = std::min(block_w, static_cast<int>(ref_frame.cols() - block_x));
    block_h = std::min(block_h, static_cast<int>(ref_frame.rows() - block_y));

    if (block_w <= 0 || block_h <= 0) {
        return MotionVector(0, 0, std::numeric_limits<float>::max());
    }

    int best_x = block_x;
    int best_y = block_y;
    float best_mse = std::numeric_limits<float>::max();

    best_mse = MSE(frames, current_frame_idx, reference_frame_idx, block_x, block_y, block_x, block_y, block_w, block_h);

    constexpr int LDSP[5][2] = {
        { 0,  0},
        { 0, -2},
        { 2,  0},
        { 0,  2},
        {-2,  0}
    };

    bool improved = true;
    while (improved) {
        improved = false;
        float min_mse = best_mse;
        int best_dx = 0;
        int best_dy = 0;

        for (int i = 1; i < 5; ++i) {
            int cand_x = best_x + LDSP[i][0];
            int cand_y = best_y + LDSP[i][1];

            if (cand_x < 0 || cand_x + block_w > ref_frame.cols() ||
                cand_y < 0 || cand_y + block_h > ref_frame.rows()) {
                continue;
            }

            float cost = MSE(frames, current_frame_idx, reference_frame_idx, block_x, block_y, cand_x, cand_y, block_w, block_h);

            if (cost < min_mse) {
                min_mse  = cost;
                best_dx  = LDSP[i][0];
                best_dy  = LDSP[i][1];
                improved = true;
            }
        }

        if (improved) {
            best_x += best_dx;
            best_y += best_dy;
            best_mse = min_mse;
        }
    }

    int step = search_range;
    while (step >= 1) {
        bool improved = false;
        float best_cost = best_mse;
        int best_dx = 0, best_dy = 0;

        for (int dy = -step; dy <= step; dy += step) {
            for (int dx = -step; dx <= step; dx += step) {
                if (dx == 0 && dy == 0) continue;
                int cand_x = best_x + dx;
                int cand_y = best_y + dy;
                if (cand_x < 0 || cand_x + block_w > ref_frame.cols() ||
                    cand_y < 0 || cand_y + block_h > ref_frame.rows()) continue;

                float cost = MSE(frames, current_frame_idx, reference_frame_idx, block_x, block_y, cand_x, cand_y, block_w, block_h);

                if (cost < best_cost) {
                    best_cost = cost;
                    best_dx = dx;
                    best_dy = dy;
                    improved = true;
                }
            }
        }

        if (improved) {
            best_x += best_dx;
            best_y += best_dy;
            best_mse = best_cost;
        }

        step /= 2;
    }

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;

            int cand_x = best_x + dx;
            int cand_y = best_y + dy;

            if (cand_x < 0 || cand_x + block_w > ref_frame.cols() ||
                cand_y < 0 || cand_y + block_h > ref_frame.rows()) {
                continue;
            }

            float cost = MSE(frames, current_frame_idx, reference_frame_idx, block_x, block_y, cand_x, cand_y, block_w, block_h);

            if (cost < best_mse) {
                best_mse = cost;
                best_x = cand_x;
                best_y = cand_y;
            }
        }
    }

    int motion_dx = best_x - block_x;
    int motion_dy = best_y - block_y;

    return MotionVector(motion_dx, motion_dy, block_x, block_y, best_mse);
}

std::vector<MotionVector> getMotionVectors(const std::vector<FrameData>& frames, uint32_t reference_frame_idx, uint32_t current_frame_idx) {
    if (frames.empty()) return {};

    const Eigen::MatrixXf& frame = frames[0].frame;
    int frame_height = frame.rows();
    int frame_width = frame.cols();
    int block_size = 16;

    int blocks_y = (frame_height + block_size - 1) / block_size;
    int blocks_x = (frame_width + block_size - 1) / block_size;
    int total_blocks = blocks_y * blocks_x;

    std::vector<MotionVector> motionVectors(total_blocks);

    for (int block_y = 0; block_y < blocks_y; block_y++) {
        for (int block_x = 0; block_x < blocks_x; block_x++) {
            int actual_block_y = block_y * block_size;
            int actual_block_x = block_x * block_size;

            int idx = block_y * blocks_x + block_x;
            motionVectors[idx] = getMotionVector(frames, reference_frame_idx, current_frame_idx, actual_block_x, actual_block_y, 16);
        }
    }

    return motionVectors;
}

void markFrameTypes(std::vector<FrameData>& frames, uint8_t N) {
    double maxmse = frames[0].frame.rows() * frames[0].frame.cols();
    for(int i = 0; i < frames.size(); i++) {
        if(i % N == 0) {
            frames[i].type = I_FRAME;
        } else if(i > 0) {
            double mse = MSE(frames, i, i - 1, 0, 0, 0, 0, frames[0].frame.cols(), frames[0].frame.rows());
            if(false) {
                std::cout << "NEW I FRAME";
                frames[i].type = I_FRAME;
            }
        }
    }

    for(int i = 0; i < frames.size(); i++) {
        if(frames[i].type == I_FRAME) {
            for(int j = N/3; j < N; j += N/3) {
                if (frames[i+j].type != I_FRAME) {
                    frames[i+j].type = P_FRAME;
                }
            }
        }
    }
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

std::vector<uint8_t> compressAsBlocks(Eigen::MatrixXf& input_matrix, uint8_t M, uint8_t q) {
    uint32_t init_size = input_matrix.size();

    Eigen::MatrixXf H = hMat();
    
    uint32_t blocks_x = (input_matrix.cols() + 7) / 8;
    uint32_t blocks_y = (input_matrix.rows() + 7) / 8;
    uint32_t total_blocks = blocks_x * blocks_y;
    
    uint32_t size_DC = total_blocks;
    uint32_t size_AC = total_blocks * 63;
    
    std::vector<int16_t> flattened_DC(size_DC, 0);
    std::vector<int16_t> flattened_AC(size_AC, 0);
    
    //std::cout << "Blocks: " << blocks_x << "x" << blocks_y << " = " << total_blocks << std::endl;
    //std::cout << "DC size: " << flattened_DC.size() << ", AC size: " << flattened_AC.size() << std::endl;
    
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
    
    std::vector<std::vector<int>> q_table = quantization_table(q);
    
    uint32_t block_index = 0;
    
    for(uint32_t i = 0; i < input_matrix.rows(); i += 8) {
        for(uint32_t j = 0; j < input_matrix.cols(); j += 8) {
            Eigen::MatrixXf block(8, 8);
            
            uint32_t copy_height = std::min(8u, static_cast<uint32_t>(input_matrix.rows() - i));
            uint32_t copy_width = std::min(8u, static_cast<uint32_t>(input_matrix.cols() - j));
            
            if(copy_height < 8 || copy_width < 8) {
                block.setZero();
                block.topLeftCorner(copy_height, copy_width) = 
                    input_matrix.block(i, j, copy_height, copy_width);
            } else {
                block = input_matrix.block(i, j, 8, 8);
            }
            
            Eigen::MatrixXf temp = H.transpose() * block * H;
            
            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    float coeff = temp(x, y);
                    int q_value = q_table[x][y];
                    
                    if(x == 0 && y == 0) {
                        flattened_DC[block_index] = static_cast<int16_t>(std::round(coeff / q_value));
                    } else {
                        int zigzag_idx = zigzag[x][y];
                        int ac_index = block_index * 63 + (zigzag_idx - 1);
                        
                        if(std::abs(coeff) < M || q_value == 0) {
                            flattened_AC[ac_index] = 0;
                        } else {
                            flattened_AC[ac_index] = static_cast<int16_t>(std::round(coeff / q_value));
                        }
                    }
                }
            }
            
            block_index++;
        }
    }
    
    if(block_index != total_blocks) {
        std::cerr << "ERROR: Processed " << block_index << " blocks, expected " << total_blocks << "\n";
    }
    
    std::vector<int16_t> DC_pred(size_DC, 0);
    DC_pred[0] = flattened_DC[0];
    
    for(uint32_t i = 1; i < size_DC; i++) {
        DC_pred[i] = flattened_DC[i-1] - flattened_DC[i];
    }
    
    std::vector<uint8_t> tem = mergeAndSplitVectors(DC_pred, flattened_AC);
    return tem;
}

Eigen::MatrixXf decompressToBMP(const std::vector<int16_t>& flattened_DC, const std::vector<int16_t>& flattened_AC, const std::vector<std::vector<int>>& q_table, uint32_t w, uint32_t h) {
    
    Eigen::MatrixXf H = hMat();
    
    uint32_t blocks_x = (w + 7) / 8;
    uint32_t blocks_y = (h + 7) / 8;
    uint32_t total_blocks = blocks_x * blocks_y;
    
    if(flattened_DC.size() != total_blocks) {
        throw std::runtime_error("ERROR: DC size mismatch!");
    }
    
    if(flattened_AC.size() != total_blocks * 63) {
        std::cout << "AC: " << flattened_AC.size() << "\n";
        throw std::runtime_error("ERROR: AC size mismatch!");
    }
    
    Eigen::MatrixXf bmp(h, w);
    bmp.setZero();
    
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
            
            float dc_value = static_cast<float>(flattened_DC[block_index]) * q_table[0][0];
            coeff_matrix(0, 0) = dc_value;
            
            for(uint8_t x = 0; x < 8; x++) {
                for(uint8_t y = 0; y < 8; y++) {
                    if(x == 0 && y == 0) continue;
                    
                    int zigzag_idx = zigzag[x][y];
                    int ac_index = block_index * 63 + (zigzag_idx - 1);
                    
                    if(ac_index < flattened_AC.size()) {
                        int16_t ac_val = flattened_AC[ac_index];
                        
                        if(ac_val != 0) {
                            float ac_value = static_cast<float>(ac_val) * q_table[x][y];
                            coeff_matrix(x, y) = ac_value;
                        }
                    } else {
                        std::cerr << "ERROR: AC index out of bounds!" << std::endl;
                    }
                }
            }
            
            Eigen::MatrixXf block_reconstructed = H * coeff_matrix * H.transpose();
            
            uint32_t copy_height = std::min(8u, h - img_i);
            uint32_t copy_width = std::min(8u, w - img_j);
            
            bmp.block(img_i, img_j, copy_height, copy_width) = block_reconstructed.topLeftCorner(copy_height, copy_width);
            
            block_index++;
        }
    }

    return bmp;
}

Eigen::MatrixXf decompress(std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    uint32_t blocks_x = (w + 7) / 8;
    uint32_t blocks_y = (h + 7) / 8;
    uint32_t total_blocks = blocks_x * blocks_y;

    std::vector<int16_t> all = splitAndReconstructVectors(data);

    std::vector<int16_t> DC_pred(total_blocks, 0);
    DC_pred = std::vector<int16_t>(all.begin(), all.begin() + total_blocks);
    DC_pred = reverseDCPrediction(DC_pred);
    std::vector<int16_t> flattened_AC = std::vector<int16_t>(all.begin() + DC_pred.size(), all.end());

    std::vector<std::vector<int>> q_table = quantization_table(0);

    return decompressToBMP(DC_pred, flattened_AC, q_table, w, h);
}

std::vector<FrameData> reorderFramesForCompression(const std::vector<FrameData>& frames) {
    std::vector<FrameData> compressedOrder;
    std::unordered_set<size_t> processed;
    
    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].type == I_FRAME && processed.find(i) == processed.end()) {
            compressedOrder.push_back(frames[i]);
            processed.insert(i);
            
            for(size_t j = i + 1; j < frames.size(); j++) {
                if(frames[j].type == P_FRAME && 
                    frames[j].prev == i &&
                    processed.find(j) == processed.end()) {
                    compressedOrder.push_back(frames[j]);
                    processed.insert(j);
                }
            }
            
            for(size_t j = i + 1; j < frames.size(); j++) {
                if(frames[j].type == B_FRAME && 
                    frames[j].prev == i &&
                    frames[j].next < frames.size() &&
                    processed.find(frames[j].next) != processed.end() &&
                    processed.find(j) == processed.end()) { compressedOrder.push_back(frames[j]);
                    processed.insert(j);
                }
            }
        }
    }
    
    return compressedOrder;
}

std::vector<std::vector<MotionVector>> compressVideo(std::vector<FrameData>& frames, uint8_t N, uint8_t M, VideoInfo& videoInfo) {
    uint8_t PInterval = N/3;

    markFrameTypes(frames, N);

    size_t prev_i = 0;
    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].type == P_FRAME) {
            std::vector<MotionVector> vecs = getMotionVectors(frames, prev_i, i);
            frames[i].motionVectorsPrev = vecs;
        } else if(frames[i].type == I_FRAME) {
            prev_i = i;
        }
    }

    size_t prev = 0, next = 0;

    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].type == B_FRAME) {
            for(size_t j = i + 1; j < N && j + i < frames.size(); j++) {
                if(frames[i+j].type == I_FRAME || frames[i+j].type == P_FRAME) {
                    next = j + i;
                    break;
                }
            }

            frames[i].motionVectorsPrev = getMotionVectors(frames, prev, i);
            frames[i].prev = prev;
            frames[i].motionVectorsNext = getMotionVectors(frames, next, i);
            frames[i].next = next;

        } else if(frames[i].type == I_FRAME || frames[i].type == P_FRAME) {
            prev = i;
        }
    }

    std::cout << frames[1].motionVectorsPrev.size() << "\n";

    /*for(auto& fr : frames) {
        if(fr.type = P_FRAME) {
            for(auto f : fr.motionVectorsPrev) {
                if(f.mse < 0.3) continue;
                std::cout << f.mse << "\n";
            }
        }
    }*/

    frames = reorderFramesForCompression(frames);

    BitWriter writer;

    writer.writeUint32(videoInfo.w);
    writer.writeUint32(videoInfo.h);
    writer.writeUint32((uint32_t)videoInfo.fps);
    writer.writeByte(N);
    writer.writeByte(M);

    for(auto& fr : frames) {
        if(fr.type == I_FRAME) {
            std::cout << "I\n";
            writer.writeBit(0);
            std::vector<uint8_t> comp = compressAsBlocks(fr.frame, M, 0);
            for(auto& val : comp) {
                writer.writeByte(val);
            }
        } else if(fr.type == P_FRAME) {
            std::cout << "P\n";
            writer.writeBit(1);
            writer.writeBit(0);
            for(auto& mv : fr.motionVectorsPrev) {
                if(mv.mse < 0.75) {
                    writer.writeBit(0);
                    writer.writeInt32(mv.dx);
                    writer.writeInt32(mv.dy);
                    //std::cout << "MSE: " << mv.mse << "\n";
                } else {
                    writer.writeBit(1);
                    Eigen::MatrixXf temp = fr.frame.block(mv.y, mv.x, 16, 16);
                    std::vector<uint8_t> comp = compressAsBlocks(temp, M, 0);
                    //std::cout << "MSE: " << mv.mse << "\n";
                    for(auto& it : comp) {
                        writer.writeByte(it);
                    }
                }
            }
        } else if(fr.type == B_FRAME) {
            std::cout << "B\n";
            writer.writeBit(1);
            writer.writeBit(1);
            for(size_t i = 0; i < fr.motionVectorsNext.size(); i++) {
                if(fr.motionVectorsNext[i].mse < fr.motionVectorsPrev[i].mse) {
                    writer.writeBit(0);
                    writer.writeInt32(fr.motionVectorsNext[i].dx);
                    writer.writeInt32(fr.motionVectorsNext[i].dy);
                } else {
                    writer.writeBit(1);
                    writer.writeInt32(fr.motionVectorsPrev[i].dx);
                    writer.writeInt32(fr.motionVectorsPrev[i].dy);
                }
            }
        }
    }

    std::vector<uint8_t> fin = writer.getBuffer();
    writer = encode(fin);

    //std::cout << "FinalSize: " << (double)writer.getBuffer().size()/(double)1048576 << "\n";
    writer.flush();
    writer.writeBufferToFile("test.bin");

    return std::vector<std::vector<MotionVector>>();
}

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t file_type{0x4D42};
    uint32_t file_size{0};
    uint16_t reserved1{0};
    uint16_t reserved2{0};
    uint32_t offset_data{0};
    
    uint32_t dib_header_size{40};
    int32_t width{0};
    int32_t height{0};
    uint16_t planes{1};
    uint16_t bits_per_pixel{24};
    uint32_t compression{0};
    uint32_t image_size{0};
    int32_t x_pixels_per_meter{0};
    int32_t y_pixels_per_meter{0};
    uint32_t colors_used{0}; 
    uint32_t important_colors{0};
};
#pragma pack(pop)

bool saveMatrixAsBMP(const Eigen::MatrixXf& matrix, const std::string& filename, bool normalize = true, float min_val = 0.0f, float max_val = 255.0f) {
    
    if (matrix.rows() == 0 || matrix.cols() == 0) {
        std::cerr << "ERROR: Empty matrix!" << std::endl;
        return false;
    }
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "ERROR: Cannot open file: " << filename << std::endl;
        return false;
    }
    
    int width = matrix.cols();
    int height = matrix.rows();
    
    float actual_min = min_val;
    float actual_max = max_val;
    
    if (normalize) {
        actual_min = matrix.minCoeff();
        actual_max = matrix.maxCoeff();
        
        if (actual_max - actual_min < 1e-6f) {
            actual_max = actual_min + 1.0f;
        }
    }
    
    int row_padding = (4 - (width * 3) % 4) % 4;
    
    BMPHeader header;
    header.width = width;
    header.height = height;
    header.image_size = (width * 3 + row_padding) * height;
    header.file_size = sizeof(BMPHeader) + header.image_size;
    header.offset_data = sizeof(BMPHeader);
    
    file.write(reinterpret_cast<char*>(&header), sizeof(header));
    
    for (int i = height - 1; i >= 0; --i) {
        for (int j = 0; j < width; ++j) {
            float value = matrix(i, j);
            
            if (normalize) {
                value = 255.0f * (value - actual_min) / (actual_max - actual_min);
            } else {
                if (value < 0.0f) value = 0.0f;
                if (value > 255.0f) value = 255.0f;
            }
            
            uint8_t pixel = static_cast<uint8_t>(value);
            
            file.put(pixel);
            file.put(pixel);
            file.put(pixel);
        }
        
        for (int p = 0; p < row_padding; ++p) {
            file.put(0);
        }
    }
    
    file.close();
    std::cout << "Saved BMP image: " << filename << " (" << width << "x" << height << ")" << std::endl;
    return true;
}

Eigen::MatrixXf regenPFrame(BitReader& reader, uint32_t blocks_x, uint32_t blocks_y, Eigen::MatrixXf& prevI) {
    std::cout << "P\n";
    uint32_t mb_x = blocks_x / 2;
    uint32_t mb_y = blocks_y / 2;

    Eigen::MatrixXf mat(mb_y * 16, mb_x * 16);

    for (uint32_t i = 0; i < mb_y; i++) {
        for (uint32_t j = 0; j < mb_x; j++) {
            bool bit = reader.readNextBit();
            if(bit) {
                std::vector<uint8_t> block;
                for(uint32_t l = 0; l < 16 * 16 * 2; l++) {
                    block.push_back(reader.readNextByte());
                }

                auto t = decompress(block, 16, 16);
                mat.block(i * 16, j * 16, 16, 16) = t;
            } else {
                int32_t x = reader.readInt32();
                int32_t y = reader.readInt32();

                mat.block(i * 16, j * 16, 16, 16) = prevI.block((i * 16) + y, (j * 16) + x, 16, 16);
            }
        }
    }

    return mat;
}

Eigen::MatrixXf regenBFrame(BitReader& reader, uint32_t blocks_x, uint32_t blocks_y, Eigen::MatrixXf& prev, Eigen::MatrixXf& next) {
    std::cout << "B\n";
    uint32_t mb_x = blocks_x / 2;
    uint32_t mb_y = blocks_y / 2;

    Eigen::MatrixXf mat(mb_y * 16, mb_x * 16);

    for (uint32_t i = 0; i < mb_y; i++) {
        for (uint32_t j = 0; j < mb_x; j++) {
            bool bit = reader.readNextBit();
            if(bit) {
                int32_t x = reader.readInt32();
                int32_t y = reader.readInt32();

                mat.block(i * 16, j * 16, 16, 16) = prev.block((i * 16) + y, (j * 16) + x, 16, 16);
            } else {
                int32_t x = reader.readInt32();
                int32_t y = reader.readInt32();

                mat.block(i * 16, j * 16, 16, 16) = next.block((i * 16) + y, (j * 16) + x, 16, 16);
            }
        }
    }

    return mat;
}

void decompressVideo() {
    BitReader reader("test.bin");
    std::vector<uint8_t> temp = decode(reader);
    reader = BitReader(temp);

    VideoInfo videoInfo;
    uint8_t N, M;
    
    videoInfo.w = reader.readUint32();
    videoInfo.h = reader.readUint32();
    videoInfo.fps = reader.readUint32();
    N = reader.readNextByte();
    M = reader.readNextByte();

    auto dims = getPaddedDimensions(videoInfo.w, videoInfo.h);

    uint32_t blocks_x = (dims.first + 7) / 8;
    uint32_t blocks_y = (dims.second + 7) / 8;
    uint32_t full_frame_size_bytes = blocks_x * blocks_y * 64 * 2;

    Eigen::MatrixXf next;
    Eigen::MatrixXf prev;

    bool bit = reader.readNextBit();
    std::vector<uint8_t> temp1(full_frame_size_bytes);
    for(size_t i = 0; i < full_frame_size_bytes; i++) {
        temp1[i] = reader.readNextByte();
    }
    prev = decompress(temp1, dims.first, dims.second);
    next = prev;

    std::vector<FrameType> frameTypes;
    std::vector<Eigen::MatrixXf> frames;

    frames.push_back(prev);

    uint32_t il = 0;

    while(il < 100) {
        il++;
        bit = reader.readNextBit();
        if(bit) {
            bit = reader.readNextBit();
            if(bit) {
                //B_FRAME
                frames.push_back(regenBFrame(reader, blocks_x, blocks_y, prev, next));
                frameTypes.push_back(B_FRAME);
                saveMatrixAsBMP(frames.back() ,"aaaaB.bmp");
            } else {
                //P_FRAME
                prev = next;
                next = regenPFrame(reader, blocks_x, blocks_y, prev);
                frames.push_back(next);
                frameTypes.push_back(P_FRAME);
                //std::cout << "It's b time!\n";
                //saveMatrixAsBMP(next ,"aaaa2.bmp");
                //break;
            }
        } else {
            //First I_FRAME
            std::cout << "I\n";
            std::vector<uint8_t> temp1(full_frame_size_bytes);
            for(size_t i = 0; i < full_frame_size_bytes; i++) {
                temp1[i] = reader.readNextByte();
            }
            prev = next;
            next = decompress(temp1, dims.first, dims.second);
            frames.push_back(next);
            frameTypes.push_back(I_FRAME);
            //saveMatrixAsBMP(prev ,"aaaa.bmp");
            //break;
        }
    }
}

int main(int argc, char* argv[]) {

    //return 0;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.mkv>" << std::endl;
        return 1;
    }

    uint8_t M = 10, N = 9;
    
    VideoInfo videoInfo;
    std::vector<FrameData> frames = extractVideoFrames(argv[1], videoInfo.fps);
    videoInfo.h = frames[0].frame.rows();
    videoInfo.w = frames[0].frame.cols();

    for(auto& frame : frames) {
        frame = padFrame(frame);
    }
    
    compressVideo(frames, N, M, videoInfo);
    //decompressVideo();
    
    return 0;
}