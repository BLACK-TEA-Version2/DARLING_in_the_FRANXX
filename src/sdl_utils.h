#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

extern SDL_Window* gWindow;
extern SDL_Renderer* gRenderer;
extern SDL_Window* gDebugWindow;     // デバックウィンドウ
extern SDL_Renderer* gDebugRenderer; // デバックレンダラー
extern TTF_Font* gFont;

// メインウィンドウ初期化
bool init_sdl(const char* title, int width, int height);

// デバックウィンドウ初期化
bool init_debug_window(const char* title, int width, int height);

// フォントロード
bool load_media(const char* font_path, int font_size);

// テキスト描画 (レンダラー指定可能版)
void draw_text(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color);

// 終了処理
void close_sdl();

// 設定ファイル読み込み
bool MediaConfig_Load(const char* config_path);
const char* MediaConfig_GetPath(const char* key);
void MediaConfig_Cleanup();

#endif // SDL_UTILS_H