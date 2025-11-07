#include "presentation.h"
#include "video_internal.h" 
#include <SDL2/SDL_image.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// メディアタイプ
typedef enum {
    MEDIA_TYPE_NONE,
    MEDIA_TYPE_VIDEO, // FFMPEGで再生
    MEDIA_TYPE_IMAGE  // SDL_Textureとして表示
} MediaType;

// 現在再生中のメディア情報
static MediaType g_current_type = MEDIA_TYPE_NONE;
static bool g_is_looping = false;
static SDL_Texture* g_static_image = NULL; // 静止画用テクスチャ
static char g_current_filepath[1024] = {0}; // 再生中のパスを記録
static SDL_Renderer* g_cached_renderer = NULL; // (★) Stop時に使うため

// --- 内部ヘルパー関数 ---
static void cleanup_current_media() {
    if (g_current_type == MEDIA_TYPE_VIDEO) {
        // (★) 修正点: キャッシュした renderer を渡す
        Video_Internal_Stop(g_cached_renderer); 
    }
    if (g_static_image) {
        SDL_DestroyTexture(g_static_image);
        g_static_image = NULL;
    }
    g_current_type = MEDIA_TYPE_NONE;
    g_is_looping = false;
    g_current_filepath[0] = '\0';
}

// 拡張子比較（大文字小文字を無視）
static bool check_extension(const char* filepath, const char* ext) {
    const char* file_ext = strrchr(filepath, '.');
    if (!file_ext) return false;
    file_ext++; // ドットの次へ
    
    #ifdef _WIN32
        return _stricmp(file_ext, ext) == 0;
    #else
        return strcasecmp(file_ext, ext) == 0;
    #endif
}

// ファイルパスの拡張子で動画ファイルか判定
static bool is_video_file(const char* filepath) {
    if (check_extension(filepath, "mp4")) return true;
    if (check_extension(filepath, "avi")) return true;
    if (check_extension(filepath, "mkv")) return true;
    if (check_extension(filepath, "webm")) return true;
    return false;
}

// ファイルパスの拡張子で画像ファイルか判定
static bool is_image_file(const char* filepath) {
    if (check_extension(filepath, "png")) return true;
    if (check_extension(filepath, "jpg")) return true;
    if (check_extension(filepath, "jpeg")) return true;
    if (check_extension(filepath, "bmp")) return true;
    return false;
}


// --- 公開関数 ---

bool Presentation_Init(SDL_Renderer* renderer) {
    g_cached_renderer = renderer; // (★) レンダラをキャッシュ
    if (!Video_Internal_Init()) {
        fprintf(stderr, "Failed to init internal video module\n");
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        fprintf(stderr, "Failed to init SDL_image: %s\n", IMG_GetError());
        return false;
    }
    return true;
}

void Presentation_Cleanup() {
    cleanup_current_media();
    Video_Internal_Cleanup(); 
    IMG_Quit();
}

bool Presentation_Play(SDL_Renderer* renderer, const char* filepath, bool loop) {
    if (!filepath || strlen(filepath) == 0) {
        fprintf(stderr, "Presentation_Play: Invalid filepath provided.\n");
        return false;
    }
    
    g_cached_renderer = renderer; // (★) 常に最新のレンダラをキャッシュ

    // (★) もしリクエストされたパスが現在再生中のパスと同じ場合
    if (strcmp(g_current_filepath, filepath) == 0) {
        
        // (★) 1. ループ設定が異なる場合のみ、設定を変更する
        if (g_is_looping != loop && g_current_type == MEDIA_TYPE_VIDEO) {
             Video_Internal_SetLoop(loop); 
             g_is_looping = loop;
        }
        
        // (★) 2. 1回再生(loop=false)をリクエストされ、既に終了していた場合
        if (loop == false && Presentation_IsFinished()) {
             // (★) 終了済みの1回再生動画をもう一度再生するリクエスト
             // (★) -> このまま処理を続行し、cleanup -> open させる
             printf("Presentation_Play: 終了済みの動画 (%s) をリロードします。\n", filepath);
        } else {
             // (★) まだ再生中、またはループ動画の場合は、読み込み直さず true を返す
             return true; 
        }
    }


    // 現在再生中のものを停止
    cleanup_current_media();

    if (is_video_file(filepath)) {
        if (!Video_Internal_Open(renderer, filepath, loop)) {
            fprintf(stderr, "Failed to play video: %s\n", filepath);
            return false;
        }
        g_current_type = MEDIA_TYPE_VIDEO;
        g_is_looping = loop;

    } else if (is_image_file(filepath)) {
        g_static_image = IMG_LoadTexture(renderer, filepath);
        if (!g_static_image) {
            fprintf(stderr, "Failed to load image: %s, Error: %s\n", filepath, IMG_GetError());
            return false;
        }
        g_current_type = MEDIA_TYPE_IMAGE;
        g_is_looping = loop; 
    
    } else {
        fprintf(stderr, "Presentation_Play: Unknown file type: %s\n", filepath);
        return false;
    }

    // 再生中のパスを記録
    strncpy(g_current_filepath, filepath, sizeof(g_current_filepath) - 1);
    return true;
}

void Presentation_Stop() {
    cleanup_current_media();
}

void Presentation_Update() {
    if (g_current_type == MEDIA_TYPE_VIDEO) {
        Video_Internal_Update(); 
    }
}

void Presentation_Draw(SDL_Renderer* renderer, int screen_width, int screen_height) {
    SDL_Rect destRect = { 0, 0, screen_width, screen_height };

    if (g_current_type == MEDIA_TYPE_VIDEO) {
        Video_Internal_Draw(renderer, &destRect); 
    } 
    else if (g_current_type == MEDIA_TYPE_IMAGE && g_static_image) {
        SDL_RenderCopy(renderer, g_static_image, NULL, &destRect);
    }
}

bool Presentation_IsFinished() {
    if (g_is_looping || g_current_type == MEDIA_TYPE_IMAGE) {
        return false; 
    }
    if (g_current_type == MEDIA_TYPE_VIDEO) {
        return Video_Internal_IsFinished(); 
    }
    return true;
}