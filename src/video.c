#include "video.h"
#include <stdio.h>

// --- FFmpeg ライブラリのヘッダー ---
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
#include "video_defs.h" // (★) video_defs.h をインクルード

// (★) REEL_DRAW_START_Y 定義を削除

// (★) 動画IDとファイルパスのマッピング
const char* g_video_filepaths[VIDEO_COUNT] = {
    [VIDEO_IDLE]            = "../videos/test.mp4", // (★) 待機用動画
    [VIDEO_ENSHUTSU_AKA7]   = "../videos/ScreenRecording_10-29-2025 18-04-16_1.mp4",
    [VIDEO_SPIN]            = "../videos/test.mp4"  // (★) 回転中用動画 (今は同じ)
};

// --- 内部状態 ---
static AVFormatContext* pFormatCtx = NULL;
static AVCodecContext* pCodecCtx = NULL;
static struct SwsContext* sws_ctx = NULL;
static int videoStreamIndex = -1;

static SDL_Texture* pFrameTexture = NULL;
static SDL_Renderer* gRendererRef = NULL;
static int videoWidth = 0;
static int videoHeight = 0;

static AVFrame* pFrame = NULL;
static AVFrame* pFrameRGB = NULL;
static uint8_t* rgb_buffer = NULL;

static bool g_looping = false;
static bool g_video_finished = false;
static VideoType g_current_video_type = VIDEO_NONE;


/**
 * @brief (★内部関数化) 動画再生モジュールを初期化
 */
static bool Video_Init(SDL_Renderer* renderer, const char* video_filepath, bool loop) {
    if (pFormatCtx) {
        Video_Cleanup(); // 既に開いていたら閉じる
    }

    gRendererRef = renderer;
    g_looping = loop;
    g_video_finished = false;

    if (avformat_open_input(&pFormatCtx, video_filepath, NULL, NULL) != 0) {
        fprintf(stderr, "Error: Couldn't open video file %s\n", video_filepath);
        return false;
    }
    
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        fprintf(stderr, "Error: Couldn't find stream information\n");
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    videoStreamIndex = -1;
    AVCodecParameters* pCodecParams = NULL;
    for (unsigned int i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            pCodecParams = pFormatCtx->streams[i]->codecpar;
            break;
        }
    }
    if (videoStreamIndex == -1 || pCodecParams == NULL) {
        fprintf(stderr, "Error: Didn't find a video stream\n");
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    const AVCodec* pCodec = avcodec_find_decoder(pCodecParams->codec_id);
    if (pCodec == NULL) {
        fprintf(stderr, "Error: Unsupported codec!\n");
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (!pCodecCtx) {
        fprintf(stderr, "Error: Failed to allocate codec context\n");
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    if (avcodec_parameters_to_context(pCodecCtx, pCodecParams) < 0) {
        fprintf(stderr, "Error: Failed to copy codec parameters to context\n");
        avcodec_free_context(&pCodecCtx); pCodecCtx = NULL;
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        fprintf(stderr, "Error: Could not open codec\n");
        avcodec_free_context(&pCodecCtx); pCodecCtx = NULL;
        avformat_close_input(&pFormatCtx); pFormatCtx = NULL;
        return false;
    }
    videoWidth = pCodecCtx->width;
    videoHeight = pCodecCtx->height;
    pFrameTexture = SDL_CreateTexture(renderer,
                                      SDL_PIXELFORMAT_RGB24,
                                      SDL_TEXTUREACCESS_STREAMING,
                                      videoWidth,
                                      videoHeight);
    if (!pFrameTexture) {
         fprintf(stderr, "Error: Failed to create SDL texture: %s\n", SDL_GetError());
         Video_Cleanup();
         return false;
    }
    sws_ctx = sws_getContext(videoWidth, videoHeight, pCodecCtx->pix_fmt,
                             videoWidth, videoHeight, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
         fprintf(stderr, "Error: Failed to create SwsContext\n");
         Video_Cleanup();
         return false;
    }
    pFrame = av_frame_alloc();
    pFrameRGB = av_frame_alloc();
    if (!pFrame || !pFrameRGB) {
        fprintf(stderr, "Error: Failed to allocate AVFrame\n");
        Video_Cleanup();
        return false;
    }
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
    rgb_buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    if (!rgb_buffer) {
        fprintf(stderr, "Error: Failed to allocate RGB buffer\n");
        Video_Cleanup();
        return false;
    }
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, rgb_buffer,
                         AV_PIX_FMT_RGB24, videoWidth, videoHeight, 1);
    
    printf("Video Init OK: %s (%dx%d), Loop: %s\n", video_filepath, videoWidth, videoHeight, loop ? "true" : "false");
    return true;
}


void Video_Cleanup() {
    if (rgb_buffer) {
        av_free(rgb_buffer);
        rgb_buffer = NULL;
    }
    if (pFrameRGB) {
        av_frame_free(&pFrameRGB);
    }
    if (pFrame) {
        av_frame_free(&pFrame);
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
    if (pFrameTexture) {
        SDL_DestroyTexture(pFrameTexture);
        pFrameTexture = NULL;
    }
    if (pCodecCtx) {
        avcodec_free_context(&pCodecCtx);
    }
    if (pFormatCtx) {
        avformat_close_input(&pFormatCtx);
        pFormatCtx = NULL;
    }
    g_looping = false;
    g_video_finished = false;
    g_current_video_type = VIDEO_NONE;
}

// (Video_Update ... 変更なし)
void Video_Update() {
    if (g_video_finished || !pFormatCtx || !pCodecCtx || !pFrame || !pFrameRGB || !sws_ctx || !pFrameTexture) {
        return;
    }
    AVPacket packet;
    int ret;
    while (1) {
        ret = avcodec_receive_frame(pCodecCtx, pFrame);
        if (ret == 0) {
            break;
        }
        else if (ret == AVERROR_EOF) {
             if (!g_looping) {
                 g_video_finished = true;
                 printf("Video Finished (receive EOF)\n");
                 return;
             }
        }
        else if (ret == AVERROR(EAGAIN)) {
            // パケットが必要
        }
        else {
            char errbuf[64];
            av_strerror(ret, errbuf, sizeof(errbuf));
            fprintf(stderr, "avcodec_receive_frame error: %s\n", errbuf);
            g_video_finished = true;
            return;
        }
        ret = av_read_frame(pFormatCtx, &packet);
        if (ret < 0) {
            av_packet_unref(&packet);
            if (g_looping) {
                av_seek_frame(pFormatCtx, videoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(pCodecCtx);
                // printf("Video Loop Seek\n");
                continue;
            } else {
                g_video_finished = true;
                printf("Video Finished (read EOF)\n");
                avcodec_send_packet(pCodecCtx, NULL);
                return;
            }
        }
        if (packet.stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(pCodecCtx, &packet);
            if (ret < 0) {
                 char errbuf[64];
                 av_strerror(ret, errbuf, sizeof(errbuf));
                 fprintf(stderr, "avcodec_send_packet error: %s\n", errbuf);
            }
        }
        av_packet_unref(&packet);
    }
    sws_scale(sws_ctx,
              (uint8_t const* const*)pFrame->data,
              pFrame->linesize,
              0,
              videoHeight,
              pFrameRGB->data,
              pFrameRGB->linesize);
    SDL_UpdateTexture(pFrameTexture,
                      NULL,
                      pFrameRGB->data[0],
                      pFrameRGB->linesize[0]);
}

/**
 * @brief (★修正) 動画を画面全体に描画するように戻す
 */
void Video_Draw(SDL_Renderer* renderer, int screen_width, int screen_height) {
    if (!pFrameTexture) {
        return; // (★) 再生する動画がない(pFrameTextureがNULL)なら何も描画しない
    }
    
    // (★) 描画先の矩形を画面全体に設定
    SDL_Rect destRect = { 0, 0, screen_width, screen_height };

    // (★) 動画テクスチャを画面全体に描画 (引き伸ばされる)
    SDL_RenderCopy(renderer, pFrameTexture, NULL, &destRect);
}

bool Video_IsFinished() {
    return g_video_finished;
}

/**
 * @brief (★新規実装) 指定された種類の動画再生を開始する
 */
bool Video_Play(SDL_Renderer* renderer, VideoType type, bool loop) {
    // (★) 停止が要求された場合
    if (type == VIDEO_NONE) {
        if (g_current_video_type != VIDEO_NONE) {
            Video_Cleanup();
            printf("Video Play STOP\n");
        }
        return true;
    }

    if (type < 0 || type >= VIDEO_COUNT) {
        fprintf(stderr, "Error: Invalid VideoType %d\n", type);
        return false;
    }

    // (★) 同じ動画を再生しようとしている場合は何もしない (ループ設定が同じなら)
    if (type == g_current_video_type && loop == g_looping && !g_video_finished) {
        return true;
    }

    // (★) 違う動画なら、現在の動画をクリーンアップ
    Video_Cleanup();

    const char* filepath = g_video_filepaths[type];
    
    // (★) 新しい動画を Init する
    if (Video_Init(renderer, filepath, loop)) {
        g_current_video_type = type; // (★) 現在の動画タイプを設定
        return true;
    } else {
        g_current_video_type = VIDEO_NONE;
        return false;
    }
}