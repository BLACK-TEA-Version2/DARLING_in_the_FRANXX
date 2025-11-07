#include "sdl_utils.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <locale.h>

// グローバル変数の実体
SDL_Window* gWindow = NULL;
SDL_Renderer* gRenderer = NULL;
TTF_Font* gFont = NULL;

bool init_sdl(const char* title, int width, int height) {
    #ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    #endif
    setlocale(LC_ALL, ".UTF8");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }
    
    gWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (gWindow == NULL) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }
    
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
    if (gRenderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return false;
    }
    
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        return false;
    }
    
    if (!(IMG_Init(IMG_INIT_PNG))) {
        printf("IMG_Init Error: %s\n", IMG_GetError());
        return false;
    }
    return true;
}

bool load_media(const char* font_path, int font_size) {
    gFont = TTF_OpenFont(font_path, font_size);
    if (gFont == NULL) {
        // (パスが "font.ttf" の場合、"../font.ttf" も試す)
        char alt_path[256];
        snprintf(alt_path, sizeof(alt_path), "../%s", font_path);
        gFont = TTF_OpenFont(alt_path, font_size);
         if (gFont == NULL) {
            fprintf(stderr, "フォントファイルが見つかりません: %s, Error: %s\n", font_path, TTF_GetError());
            return false;
         }
    }
    return true;
}

void close_sdl() {
    TTF_CloseFont(gFont);
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
    gFont = NULL; gRenderer = NULL; gWindow = NULL;
    
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void draw_text(const char* text, int x, int y, SDL_Color color) {
    if (gFont == NULL || text == NULL || strlen(text) == 0) return;
    
    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(gFont, text, color);
    if (textSurface == NULL) {
        fprintf(stderr, "TTF_RenderUTF8_Blended Error: %s\n", TTF_GetError());
        return;
    }
    
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(gRenderer, textSurface);
    if (textTexture == NULL) { 
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        SDL_FreeSurface(textSurface); 
        return; 
    }
    
    SDL_Rect renderQuad = { x, y, textSurface->w, textSurface->h };
    SDL_RenderCopy(gRenderer, textTexture, NULL, &renderQuad);
    
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}