#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

// グローバル変数 (main.c と sdl_utils.c で共有)
extern SDL_Window* gWindow;
extern SDL_Renderer* gRenderer;
extern TTF_Font* gFont;

// 関数プロトタイプ
bool init_sdl(const char* title, int width, int height);
bool load_media(const char* font_path, int font_size);
void close_sdl();
void draw_text(const char* text, int x, int y, SDL_Color color);

#endif // SDL_UTILS_H