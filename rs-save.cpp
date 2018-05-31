// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

#include <iostream>
#include <vector>
// FFmpeg
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
}

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <opencv2/opencv.hpp>   // Include OpenCV API
#include <opencv2/highgui.hpp>

int main(int argc, char * argv[]) try
{
    FILE * pFile;

    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;

    // Declare RealSense pipeline, encapsulating the actual device and sensors
    rs2::pipeline pipe;
    // Start streaming with default recommended configuration
    pipe.start();

    using namespace cv;
    const auto window_name = "Display Image";
    namedWindow(window_name, WINDOW_AUTOSIZE);

#if 1
    int ret;
    if (argc < 2) {
        std::cout << "Usage: rs-save <outfile>" << std::endl;
        return 1;
    }
    const char* outfile = argv[1];

    pFile = fopen (outfile, "wb");
    if(pFile == NULL) {
        std::cout << "Can not open file for writing" << std::endl;
        return 2;
    }


    // initialize FFmpeg library
    av_register_all();

    // More
    //av_codec_av_register_all();
    avformat_network_init();


    // one time call to help init
    rs2::frameset data = pipe.wait_for_frames(); // Wait for next set of frames from the camera
    rs2::frame color_frame = data.get_color_frame();
    // Query frame size (width and height)
    const int dst_width = color_frame.as<rs2::video_frame>().get_width();
    const int dst_height = color_frame.as<rs2::video_frame>().get_height();
    const AVRational dst_fps = {30, 1};



    // create new video stream
    // Pixavi check outctx
    AVCodec *vcodec = 0;
    vcodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!vcodec)
    {
        std::cout << "Unable to find video encoder";
        // clean up resources
        return 1;
    }

    AVCodecContext  *encCdcCtx;

    //Context
    encCdcCtx = avcodec_alloc_context3(vcodec);
    if (!encCdcCtx)
    {
       std::cout << "Unable to allocate encoder context";
       //Cleanup memory
       return 2;
    }

    // Codec Parameters
    encCdcCtx->bit_rate = 500000;
    encCdcCtx->width = dst_width;
    encCdcCtx->height = dst_height;
    encCdcCtx->time_base.num = dst_fps.den;
    encCdcCtx->time_base.den = dst_fps.num;
    encCdcCtx->gop_size = (dst_fps.num/dst_fps.den)/2;
    encCdcCtx->max_b_frames=2;
    encCdcCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    encCdcCtx->qmax = 55; //Allow big degradation
    av_opt_set(encCdcCtx->priv_data, "preset", "ultrafast", AV_OPT_SEARCH_CHILDREN);
    av_opt_set(encCdcCtx->priv_data, "tune", "zerolatency", AV_OPT_SEARCH_CHILDREN);
    av_opt_set(encCdcCtx->priv_data, "profile", "main", AV_OPT_SEARCH_CHILDREN);

    std::cout
        << "outfile: " << outfile << "\n"
        << "vcodec:  " << vcodec->name << "\n"
        << "size:    " << dst_width << 'x' << dst_height << "\n"
        << "fps:     " << av_q2d(dst_fps) << "\n"
        << std::flush;


    ret = avcodec_open2(encCdcCtx, vcodec, NULL);
    if (ret < 0)
    {
        std::cout << "Could not open the encoder";
        //Cleanup memory
        return 3;
    }

    // initialize sample scaler
    SwsContext* swsctx = sws_getCachedContext(
        nullptr, dst_width, dst_height, AV_PIX_FMT_BGR24,
        dst_width, dst_height, encCdcCtx->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!swsctx) {
        std::cerr << "fail to sws_getCachedContext";
        return 4;
    }

    AVFrame* encFrame = av_frame_alloc();

    if (!encFrame)
    {
        std::cout << "Cannot allocate frame";
        return 5;
    }

    encFrame->width = dst_width;
    encFrame->height = dst_height;

    std::vector<uint8_t> framebuf(avpicture_get_size(encCdcCtx->pix_fmt, dst_width, dst_height));
    avpicture_fill(reinterpret_cast<AVPicture*>(encFrame), framebuf.data(), encCdcCtx->pix_fmt, dst_width, dst_height);
    encFrame->width = dst_width;
    encFrame->height = dst_height;
    encFrame->format = static_cast<int>(encCdcCtx->pix_fmt);

    int64_t frame_pts = 0;
    unsigned nb_frames = 0;
    bool end_of_stream = false;
    int got_pkt = 0;

#endif


    //while ((waitKey(1) < 0 && cvGetWindowHandle(window_name)))
    do
    {
        rs2::frameset data = pipe.wait_for_frames(); // Wait for next set of frames from the camera
        rs2::frame color_frame = data.get_color_frame();

        // Query frame size (width and height)
        const int w = color_frame.as<rs2::video_frame>().get_width();
        const int h = color_frame.as<rs2::video_frame>().get_height();

        // Create OpenCV matrix of size (w,h) from the colorized depth data
        Mat image(Size(w, h), CV_8UC3, (void*)color_frame.get_data(), Mat::AUTO_STEP);


#if 1

        if (!end_of_stream) {
            // Update the window with new data
            cv::cvtColor(image, image, cv::COLOR_RGB2BGR);
            if(cvGetWindowHandle(window_name))
                imshow(window_name, image);

            if (cv::waitKey(33) == 0x1b)
                end_of_stream = true;
        }

        if (!end_of_stream) {
            // convert cv::Mat(OpenCV) to AVFrame(FFmpeg)
            const int stride[] = { static_cast<int>(image.step[0]) };
            sws_scale(swsctx, &image.data, stride, 0, image.rows, encFrame->data, encFrame->linesize);
            encFrame->pts = frame_pts++;
        }

        // encode video frame
        AVPacket pkt;
        pkt.data = nullptr;
        pkt.size = 0;
        av_init_packet(&pkt);
        ret = avcodec_encode_video2(encCdcCtx, &pkt, end_of_stream ? nullptr : encFrame, &got_pkt);

        if (ret < 0) {
            std::cerr << "fail to avcodec_encode_video2: ret=" << ret << "\n";
            break;
        }


        if (got_pkt) {
            // rescale packet timestamp
            // write packet
            // copy pkt.size byte from pkt.data to a file
            fwrite (pkt.data , sizeof(char), pkt.size, pFile);

            // Other important fields which may be of interest
            // int64_tpts
            // int64_t 	dts
            // int 	stream_index
            // int 	flags
            // int64_t 	duration
            // int64_t 	pos

            std::cout << nb_frames << " , and time base is: "  << '\r' << std::flush;  // dump progress

            //std::cout << "data bytes: " << pkt.data[0] << ", " << pkt.data[1] << ", " << pkt.data[2] << ", " << pkt.data[3] << "\n";
            for (int i = 0; i < 7; ++i)
                std::cout << std::hex << std::setfill('0') << std::setw(2) << (int) pkt.data[i] << " ";
            std::cout << "\n";

            ++nb_frames;
        }
        av_free_packet(&pkt);
#endif
    //}
    } while (!end_of_stream || got_pkt);

    fclose (pFile);

    // End of Stream?

#if 1
    std::cout << (int) nb_frames << " frames encoded" << std::endl;

    av_frame_free(&encFrame);
    avcodec_close(encCdcCtx);

#endif

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}



