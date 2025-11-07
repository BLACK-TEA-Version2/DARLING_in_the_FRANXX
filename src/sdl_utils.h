#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

extern SDL_Window* gWindow;
extern SDL_Renderer* gRenderer;
extern TTF_Font* gFont;

/**
 * @brief SDL (Window, Renderer) を初期化します。
 */
bool init_sdl(const char* title, int width, int height);

/**
 * @brief TTFフォントをロードします。
 */
bool load_media(const char* font_path, int font_size);

/**
 * @brief テキストを描画します。
 */
void draw_text(const char* text, int x, int y, SDL_Color color);

/**
 * @brief SDL関連のリソースを解放します。
 */
void close_sdl();


// (★) --- ここから追加 ---

/**
 * @brief media.cfg ファイルを読み込み、内部に設定を保持します。
 * @param config_path 設定ファイルのパス
 * @return 成功したら true
 */
bool MediaConfig_Load(const char* config_path);

/**
 * @brief 読み込んだ設定から、キーに対応するファイルパスを取得します。
 * @param key (例: "LOOP_VIDEO_NORMAL")
 * @return ファイルパスの文字列 (静的バッファ)。見つからない場合は NULL。
 */
const char* MediaConfig_GetPath(const char* key);

/**
 * @brief 読み込んだ設定をすべて解放します。
 */
void MediaConfig_Cleanup();


#endif // SDL_UTILS_H