#include "video_internal.h"
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <SDL2/SDL_thread.h>
#include <stdio.h>

// (★) ビデオフレームキュー
typedef struct VideoFrame {
    AVFrame* frame;
    double pts;
    struct VideoFrame* next;
} VideoFrame;

typedef struct {
    VideoFrame *first;
    VideoFrame *last;
    int nb_frames;
    SDL_mutex *mutex;
    SDL_cond *cond;
} VideoFrameQueue;

// (★) 動画再生に必要なすべてのコンテキスト
typedef struct VideoState {
    AVFormatContext *format_ctx;
    AVCodecContext  *video_codec_ctx;
    struct SwsContext *sws_ctx;

    int video_stream_index;
    AVStream *video_stream; 

    VideoFrameQueue video_queue;
    double          video_clock; 

    SDL_Texture     *texture;
    SDL_Thread      *decode_thread;
    SDL_mutex       *context_mutex; // (★) このコンテキスト専用のミューテックス

    bool quit_thread;
    bool is_looping;
    bool is_finished;
    bool thread_started;
} VideoState;

// (★) グローバルコンテキスト
static VideoState *g_video_state = NULL;
// (★) --- 修正点: グローバルポインタ(g_video_state)自体を保護するミューテックス ---
static SDL_mutex *g_state_mutex = NULL;


// --- (★) フレームキュー関連 ---
static void frame_queue_init(VideoFrameQueue *q) {
    memset(q, 0, sizeof(VideoFrameQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

static void frame_queue_put(VideoFrameQueue *q, AVFrame *frame, double pts) {
    VideoFrame *vf = (VideoFrame*)av_malloc(sizeof(VideoFrame));
    if (!vf) { av_frame_free(&frame); return; }
    vf->frame = frame;
    vf->pts = pts;
    vf->next = NULL;

    SDL_LockMutex(q->mutex);
    if (!q->last) {
        q->first = vf;
    } else {
        q->last->next = vf;
    }
    q->last = vf;
    q->nb_frames++;
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);
}

// (★) 修正: 停止要求を正しくハンドリング
static AVFrame* frame_queue_get_timed(VideoFrameQueue *q, double *pts, Uint32 timeout, VideoState *state) {
    VideoFrame *vf;
    AVFrame *frame = NULL;

    SDL_LockMutex(q->mutex);
    while (!q->first) {
        // (★) スレッド停止要求が来たら待機を中断
        SDL_LockMutex(state->context_mutex);
        bool should_quit = state->quit_thread;
        SDL_UnlockMutex(state->context_mutex);
        if (should_quit) {
             SDL_UnlockMutex(q->mutex);
             return NULL;
        }

        if (SDL_CondWaitTimeout(q->cond, q->mutex, timeout) == SDL_MUTEX_TIMEDOUT) {
            SDL_UnlockMutex(q->mutex);
            return NULL; // タイムアウト
        }
    }
    
    vf = q->first;
    q->first = vf->next;
    if (!q->first) {
        q->last = NULL;
    }
    q->nb_frames--;
    SDL_UnlockMutex(q->mutex);

    frame = vf->frame;
    *pts = vf->pts;
    av_free(vf);
    return frame;
}

static void frame_queue_flush(VideoFrameQueue *q) {
    VideoFrame *vf, *vf_next;
    SDL_LockMutex(q->mutex);
    for (vf = q->first; vf; vf = vf_next) {
        vf_next = vf->next;
        av_frame_free(&vf->frame);
        av_free(vf);
    }
    q->first = q->last = NULL;
    q->nb_frames = 0;
    SDL_UnlockMutex(q->mutex);
}

static void frame_queue_destroy(VideoFrameQueue *q) {
    frame_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}


// --- (★) デコードスレッド (修正) ---
static int run_decode_thread(void *arg) {
    VideoState *state = (VideoState*)arg;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    double pts;
    int ret;
    
    if (!packet || !frame) {
         fprintf(stderr, "FFMPEG: Failed to allocate packet/frame in thread\n");
         goto end;
    }
    
    printf("Decode Thread: [%p] 開始。\n", (void*)state);
    state->thread_started = true;

    while (true) {
        SDL_LockMutex(state->context_mutex);
        bool should_quit = state->quit_thread;
        SDL_UnlockMutex(state->context_mutex);
        if (should_quit) break;

        if (state->video_queue.nb_frames > 30) {
            SDL_Delay(10);
            continue;
        }

        ret = av_read_frame(state->format_ctx, packet);
        
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                SDL_LockMutex(state->context_mutex);
                bool looping = state->is_looping;
                SDL_UnlockMutex(state->context_mutex);

                if (looping) {
                    av_seek_frame(state->format_ctx, state->video_stream_index, 0, AVSEEK_FLAG_BACKWARD);
                    avcodec_flush_buffers(state->video_codec_ctx);
                    frame_queue_flush(&state->video_queue);
                    continue;
                } else {
                    SDL_LockMutex(state->context_mutex);
                    state->is_finished = true;
                    SDL_UnlockMutex(state->context_mutex);
                    break; 
                }
            } else {
                 // (★) 停止要求による終了とエラーを区別
                 if (!should_quit) {
                    fprintf(stderr, "FFMPEG: av_read_frame error %d\n", ret);
                 }
                 break; 
            }
        }

        if (packet->stream_index == state->video_stream_index) {
            if (avcodec_send_packet(state->video_codec_ctx, packet) == 0) {
                while (avcodec_receive_frame(state->video_codec_ctx, frame) == 0) {
                    AVFrame *cloned_frame = av_frame_clone(frame);
                    pts = (frame->pts == AV_NOPTS_VALUE) ? 0 : frame->pts * av_q2d(state->video_stream->time_base);
                    frame_queue_put(&state->video_queue, cloned_frame, pts);
                }
            }
        } 
        
        av_packet_unref(packet);
    }

end:
    av_frame_free(&frame);
    av_packet_free(&packet);

    SDL_LockMutex(state->context_mutex);
    state->is_finished = true;
    SDL_UnlockMutex(state->context_mutex);
    
    printf("Decode Thread: [%p] 終了。\n", (void*)state);
    return 0;
}


// --- (★) 公開関数 (インターフェース実装) ---

bool Video_Internal_Init() {
    avformat_network_init();
    if (!g_state_mutex) {
        g_state_mutex = SDL_CreateMutex();
    }
    return true;
}

void Video_Internal_Cleanup() {
    Video_Internal_Stop(NULL); 
    avformat_network_deinit();
    if (g_state_mutex) {
        SDL_DestroyMutex(g_state_mutex);
        g_state_mutex = NULL;
    }
}

// (★) 修正: グローバルロックの範囲を変更
bool Video_Internal_Open(SDL_Renderer* renderer, const char* filepath, bool loop) {
    
    SDL_LockMutex(g_state_mutex);
    if (g_video_state) {
         SDL_UnlockMutex(g_state_mutex);
         fprintf(stderr, "Video_Internal_Open: g_video_state が NULL でないのに Open が呼ばれました。\n");
         return false;
    }

    g_video_state = (VideoState*)calloc(1, sizeof(VideoState));
    if (!g_video_state) {
        SDL_UnlockMutex(g_state_mutex);
        return false;
    }
    
    VideoState *state = g_video_state;
    SDL_UnlockMutex(g_state_mutex);

    state->is_looping = loop;
    state->is_finished = false;
    state->thread_started = false;
    state->video_stream_index = -1;
    
    printf("Video_Internal_Open: [%p] 開始 (File: %s)\n", (void*)state, filepath);

    if (avformat_open_input(&state->format_ctx, filepath, NULL, NULL) != 0) {
        fprintf(stderr, "FFMPEG: Failed to open file: %s\n", filepath);
        goto fail;
    }
    if (avformat_find_stream_info(state->format_ctx, NULL) < 0) {
        fprintf(stderr, "FFMPEG: Failed to find stream info\n");
        goto fail;
    }

    for (unsigned int i = 0; i < state->format_ctx->nb_streams; i++) {
        if (state->format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            state->video_stream_index = i;
            state->video_stream = state->format_ctx->streams[i];
            break;
        }
    }
    if (state->video_stream_index == -1) {
        fprintf(stderr, "FFMPEG: No video stream found\n");
        goto fail;
    }

    const AVCodec *video_codec = avcodec_find_decoder(state->video_stream->codecpar->codec_id);
    if (!video_codec) {
        fprintf(stderr, "FFMPEG: Video codec not found\n");
        goto fail;
    }
    state->video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(state->video_codec_ctx, state->video_stream->codecpar);
    if (avcodec_open2(state->video_codec_ctx, video_codec, NULL) < 0) {
        fprintf(stderr, "FFMPEG: Failed to open video codec\n");
        goto fail;
    }
    
    state->texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_YV12, 
        SDL_TEXTUREACCESS_STREAMING,
        state->video_codec_ctx->width,
        state->video_codec_ctx->height
    );
    if (!state->texture) {
        fprintf(stderr, "SDL: Failed to create texture: %s\n", SDL_GetError());
        goto fail;
    }

    state->sws_ctx = sws_getContext(
        state->video_codec_ctx->width, state->video_codec_ctx->height, state->video_codec_ctx->pix_fmt,
        state->video_codec_ctx->width, state->video_codec_ctx->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    frame_queue_init(&state->video_queue);
    state->context_mutex = SDL_CreateMutex();

    state->quit_thread = false;
    state->decode_thread = SDL_CreateThread(run_decode_thread, "video_decode", (void*)state);
    if (!state->decode_thread) {
        fprintf(stderr, "SDL: Failed to create decode thread\n");
        goto fail;
    }

    return true;

fail:
    SDL_LockMutex(g_state_mutex);
    g_video_state = NULL; 
    SDL_UnlockMutex(g_state_mutex);
    
    frame_queue_destroy(&state->video_queue);
    if (state->context_mutex) SDL_DestroyMutex(state->context_mutex);
    if (state->texture) SDL_DestroyTexture(state->texture);
    if (state->sws_ctx) sws_freeContext(state->sws_ctx);
    if (state->video_codec_ctx) avcodec_free_context(&state->video_codec_ctx);
    if (state->format_ctx) avformat_close_input(&state->format_ctx);
    free(state);
    
    return false;
}

// (★) 修正: グローバルロック
void Video_Internal_Stop(SDL_Renderer* renderer) {
    SDL_LockMutex(g_state_mutex);
    if (!g_video_state) {
        SDL_UnlockMutex(g_state_mutex);
        return; 
    }
    
    VideoState *state = g_video_state; 
    g_video_state = NULL; 
    SDL_UnlockMutex(g_state_mutex); 
    
    printf("Video_Internal_Stop: [%p] 開始。\n", (void*)state);

    if (state->decode_thread) {
        SDL_LockMutex(state->context_mutex);
        state->quit_thread = true;
        SDL_UnlockMutex(state->context_mutex);
        
        SDL_CondSignal(state->video_queue.cond);
        SDL_WaitThread(state->decode_thread, NULL);
    }
    
    frame_queue_destroy(&state->video_queue);
    
    if (state->texture) {
        SDL_DestroyTexture(state->texture);
        state->texture = NULL; 
    }
    if (renderer) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
    }

    if (state->context_mutex) SDL_DestroyMutex(state->context_mutex);
    if (state->sws_ctx) sws_freeContext(state->sws_ctx);
    if (state->video_codec_ctx) avcodec_free_context(&state->video_codec_ctx);
    if (state->format_ctx) avformat_close_input(&state->format_ctx);

    free(state);
    printf("Video_Internal_Stop: 完了。\n");
}


void Video_Internal_Update() {
    // (★) 修正: グローバルロックで state を取得し、そのままロック
    SDL_LockMutex(g_state_mutex);
    VideoState *state = g_video_state;
    if (!state || !state->thread_started) {
        SDL_UnlockMutex(g_state_mutex);
        return;
    }
    // (★) Update が終わるまで g_video_state が変わらないようロックし続ける

    if (Video_Internal_IsFinished()) {
        SDL_UnlockMutex(g_state_mutex);
        return;
    }

    double pts;
    AVFrame *frame = frame_queue_get_timed(&state->video_queue, &pts, 1, state); 
    if (!frame) {
        SDL_UnlockMutex(g_state_mutex);
        return; 
    }

    SDL_LockMutex(state->context_mutex);
    if (state->texture) {
        AVFrame* yuvFrame = av_frame_alloc();
        yuvFrame->format = AV_PIX_FMT_YUV420P;
        yuvFrame->width = state->video_codec_ctx->width;
        yuvFrame->height = state->video_codec_ctx->height;
        av_image_alloc(yuvFrame->data, yuvFrame->linesize, yuvFrame->width, yuvFrame->height, yuvFrame->format, 1);

        sws_scale(state->sws_ctx, (const uint8_t * const *)frame->data, frame->linesize, 0, frame->height, yuvFrame->data, yuvFrame->linesize);

        SDL_UpdateYUVTexture(
            state->texture,
            NULL,
            yuvFrame->data[0], yuvFrame->linesize[0],
            yuvFrame->data[1], yuvFrame->linesize[1],
            yuvFrame->data[2], yuvFrame->linesize[2]
        );
        
        av_freep(&yuvFrame->data[0]);
        av_frame_free(&yuvFrame);
        state->video_clock = pts;
    }
    SDL_UnlockMutex(state->context_mutex);
    
    av_frame_free(&frame);
    
    SDL_UnlockMutex(g_state_mutex); // (★) Update 終了
}

void Video_Internal_Draw(SDL_Renderer* renderer, const SDL_Rect* destRect) {
    // (★) 修正: グローバルロックで state を取得し、そのままロック
    SDL_LockMutex(g_state_mutex);
    VideoState *state = g_video_state;
    if (!state) {
        SDL_UnlockMutex(g_state_mutex);
        return;
    }
    // (★) Draw が終わるまで g_video_state が変わらないようロックし続ける

    SDL_LockMutex(state->context_mutex);
    if (state->texture) {
        SDL_RenderCopy(renderer, state->texture, NULL, destRect);
    }
    SDL_UnlockMutex(state->context_mutex);
    
    SDL_UnlockMutex(g_state_mutex); // (★) Draw 終了
}

bool Video_Internal_IsFinished() {
    // (★) 修正: グローバルロック
    SDL_LockMutex(g_state_mutex);
    VideoState *state = g_video_state;
    if (!state) {
        SDL_UnlockMutex(g_state_mutex);
        return true; 
    }

    SDL_LockMutex(state->context_mutex);
    bool finished = state->is_finished;
    SDL_UnlockMutex(state->context_mutex);
    
    SDL_UnlockMutex(g_state_mutex);
    return finished;
}

void Video_Internal_SetLoop(bool loop) {
    // (★) 修正: グローバルロック
    SDL_LockMutex(g_state_mutex);
    VideoState *state = g_video_state;
    if (!state) {
        SDL_UnlockMutex(g_state_mutex);
        return;
    }

    SDL_LockMutex(state->context_mutex);
    state->is_looping = loop;
    SDL_UnlockMutex(state->context_mutex);
    
    SDL_UnlockMutex(g_state_mutex);
}