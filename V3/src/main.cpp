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

std::vector<Eigen::MatrixXf> extractVideoFrames(const std::string& filename, int maxFrames = -1) {
    std::vector<Eigen::MatrixXf> frames;
    
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
    
    int grayBufferSize = av_image_get_buffer_size(AV_PIX_FMT_GRAY8, 
                                                 codecContext->width, 
                                                 codecContext->height, 1);
    uint8_t* grayBuffer = (uint8_t*)av_malloc(grayBufferSize);
    av_image_fill_arrays(grayFrame->data, grayFrame->linesize, grayBuffer,
                        AV_PIX_FMT_GRAY8, codecContext->width, 
                        codecContext->height, 1);
    
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
                
                frames.push_back(eigenFrame);
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.mkv>" << std::endl;
        return 1;
    }
    
    auto frames = extractVideoFrames(argv[1], 10);
    
    if (!frames.empty()) {
        std::cout << "\nFirst frame statistics:" << std::endl;
        std::cout << "  Size: " << frames[0].rows() << "x" << frames[0].cols() << std::endl;
        std::cout << "  Mean: " << frames[0].mean() << std::endl;
        std::cout << "  Min: " << frames[0].minCoeff() << std::endl;
        std::cout << "  Max: " << frames[0].maxCoeff() << std::endl;
    }
    
    return 0;
}