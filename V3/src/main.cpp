#include <iostream>
#include <vector>
#include <string>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

#include <Dense>

struct MotionVector {
    int16_t dx, dy;
    float mse;
    
    MotionVector(int32_t dx = 0, int32_t dy = 0, float mse = 0.0f) : dx(dx), dy(dy), mse(mse) {}
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

std::vector<FrameData> extractVideoFrames(const std::string& filename, int maxFrames = -1) {
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
                        eigenFrame(y, x) = static_cast<float>(data[y * stride + x]) / 255.0f;
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
    double mse_sum = 0.0;
    size_t total_pixels = static_cast<size_t>(width) * height;
    
    for (int dy = 0; dy < height; ++dy) {
        for (int dx = 0; dx < width; ++dx) {
            float val1 = frames[frame1_idx].frame(y1 + dy, x1 + dx);
            float val2 = frames[frame2_idx].frame(y2 + dy, x2 + dx);
            float diff = val1 - val2;
            mse_sum += diff * diff;
        }
    }
    
    return mse_sum / total_pixels;
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

    block_w = std::min(block_w, static_cast<int>(ref_frame.cols()  - block_x));
    block_h = std::min(block_h, static_cast<int>(ref_frame.rows()  - block_y));

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

    return MotionVector(motion_dx, motion_dy, best_mse);
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
    for(int i = 0; i < frames.size(); i++) {
        if(i % N == 0) {
            frames[i].type = I_FRAME;
        } else if(i > 0) {
            double mse = MSE(frames, i, i - 1, 0, 0, 0, 0, frames[0].frame.cols(), frames[0].frame.rows());
            if(mse > 0.75) {
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

std::vector<std::vector<MotionVector>> calculateMotionVectors(std::vector<FrameData>& frames, uint8_t N) {
    uint8_t PInterval = N/3;

    markFrameTypes(frames, N);

    size_t prev_i = 0;
    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].type == P_FRAME) {
            std::vector<MotionVector> vecs = getMotionVectors(frames, prev_i, i);
            frames[i].motionVectorsPrev = vecs;
        } else if(frames[i].type == I_FRAME) {
            size_t prev_i = i;
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
            frames[i].motionVectorsNext = getMotionVectors(frames, next, i);

        } else if(frames[i].type == I_FRAME || frames[i].type == P_FRAME) {
            prev = i;
        }
    }

    std::cout << frames[1].motionVectorsPrev.size() << "\n";

    for(auto f : frames[1].motionVectorsPrev) {
        std::cout << f.mse << "\n";
    }

    return std::vector<std::vector<MotionVector>>();
}

/*void testMotionEstimation() {
    int width = 320;
    int height = 240;
    
    Eigen::MatrixXf frame1 = Eigen::MatrixXf::Random(height, width);
    Eigen::MatrixXf frame2 = Eigen::MatrixXf::Zero(height, width);
    
    frame2.block(0, 10, height, width - 10) = frame1.block(0, 0, height, width - 10);
    
    std::vector<Eigen::MatrixXf> frames = {frame1, frame2};
    
    MotionVector mv = getMotionVector(frames, 0, 1, 100, 100, 16);
    
    std::cout << "Expected dx: -10, dy: 0  (content moved right → MV points left)\n";
    std::cout << "Got dx: " << mv.dx << ", dy: " << mv.dy << ", MSE: " << mv.mse << std::endl;
}*/

int main(int argc, char* argv[]) {
    //testMotionEstimation();

    //return 0;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.mkv>" << std::endl;
        return 1;
    }

    uint8_t M = 10, N = 9;
    
    std::vector<FrameData> frames = extractVideoFrames(argv[1]);

    for(auto& frame : frames) {
        frame = padFrame(frame);
    }
    
    //std::cout << "MSE: " << MSE(frames, 0, 19, 100, 100, 106, 106) << "\n";
    calculateMotionVectors(frames, N);

    if (!frames.empty()) {
        std::cout << "\nFirst frame statistics:" << std::endl;
        std::cout << "  Size: " << frames[0].frame.rows() << "x" << frames[0].frame.cols() << std::endl;
        std::cout << "  Mean: " << frames[0].frame.mean() << std::endl;
        std::cout << "  Min: " << frames[0].frame.minCoeff() << std::endl;
        std::cout << "  Max: " << frames[0].frame.maxCoeff() << std::endl;
    }
    
    return 0;
}