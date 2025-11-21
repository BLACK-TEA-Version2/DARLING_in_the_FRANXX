#include "sdl_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

SDL_Window* gWindow = NULL;
SDL_Renderer* gRenderer = NULL;
SDL_Window* gDebugWindow = NULL;     // 追加
SDL_Renderer* gDebugRenderer = NULL; // 追加
TTF_Font* gFont = NULL;

bool init_sdl(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    gWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (gWindow == NULL) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (gRenderer == NULL) {
        fprintf(stderr, "Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        return false;
    }
    return true;
}

// デバックウィンドウの作成
bool init_debug_window(const char* title, int width, int height) {
    gDebugWindow = SDL_CreateWindow(title, 
                                    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                    width, height, SDL_WINDOW_SHOWN);
    if (gDebugWindow == NULL) {
        fprintf(stderr, "Debug Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return false;
    }
    gDebugRenderer = SDL_CreateRenderer(gDebugWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (gDebugRenderer == NULL) {
        fprintf(stderr, "Debug Renderer could not be created! SDL Error: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

bool load_media(const char* font_path, int font_size) {
    gFont = TTF_OpenFont(font_path, font_size);
    if (gFont == NULL) {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
        return false;
    }
    return true;
}

void draw_text(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    // レンダラーがNULLの場合やテキストが空の場合は何もしない
    if (!renderer || !text || strlen(text) == 0) return;
    
    // フォントがロードされていない場合の安全策
    if (!gFont) return;

    SDL_Surface* textSurface = TTF_RenderUTF8_Blended(gFont, text, color);
    if (textSurface == NULL) {
        fprintf(stderr, "Unable to render text surface! TTF_Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (textTexture == NULL) {
        fprintf(stderr, "Unable to create texture from rendered text! SDL Error: %s\n", SDL_GetError());
    } else {
        SDL_Rect renderQuad = { x, y, textSurface->w, textSurface->h };
        SDL_RenderCopy(renderer, textTexture, NULL, &renderQuad);
        SDL_DestroyTexture(textTexture);
    }
    SDL_FreeSurface(textSurface);
}

void close_sdl() {
    if (gFont) {
        TTF_CloseFont(gFont);
        gFont = NULL;
    }
    
    // デバックウィンドウの破棄
    if (gDebugRenderer) {
        SDL_DestroyRenderer(gDebugRenderer);
        gDebugRenderer = NULL;
    }
    if (gDebugWindow) {
        SDL_DestroyWindow(gDebugWindow);
        gDebugWindow = NULL;
    }

    // メインウィンドウの破棄
    if (gRenderer) {
        SDL_DestroyRenderer(gRenderer);
        gRenderer = NULL;
    }
    if (gWindow) {
        SDL_DestroyWindow(gWindow);
        gWindow = NULL;
    }
    
    TTF_Quit();
    SDL_Quit();
}

// --- Config Loader (変更なし) ---
#define MAX_CONFIG_LINES 256
#define MAX_LINE_LEN 1024

typedef struct {
    char key[MAX_LINE_LEN];
    char value[MAX_LINE_LEN];
} ConfigEntry;

static ConfigEntry g_config_cache[MAX_CONFIG_LINES];
static int g_config_count = 0;

static char* trim_whitespace(char* str) {
    char* end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = 0;
    return str;
}

bool MediaConfig_Load(const char* config_path) {
    FILE* file = fopen(config_path, "r");
    if (!file) {
        fprintf(stderr, "Failed to open config file: %s\n", config_path);
        return false;
    }

    char line[MAX_LINE_LEN];
    g_config_count = 0;
    bool in_media_section = false;

    while (fgets(line, sizeof(line), file) && g_config_count < MAX_CONFIG_LINES) {
        line[strcspn(line, "\r\n")] = 0;
        char* trimmed_line = trim_whitespace(line);

        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') {
            continue;
        }

        if (trimmed_line[0] == '[' && trimmed_line[strlen(trimmed_line) - 1] == ']') {
            if (strcmp(trimmed_line, "[Media]") == 0) {
                in_media_section = true;
            } else {
                in_media_section = false;
            }
            continue;
        }

        if (in_media_section) {
            char* equals = strchr(trimmed_line, '=');
            if (equals) {
                *equals = '\0';
                char* key = trim_whitespace(trimmed_line);
                char* value = trim_whitespace(equals + 1);

                if (strlen(key) > 0 && strlen(value) > 0) {
                    strncpy(g_config_cache[g_config_count].key, key, MAX_LINE_LEN - 1);
                    strncpy(g_config_cache[g_config_count].value, value, MAX_LINE_LEN - 1);
                    g_config_count++;
                }
            }
        }
    }

    fclose(file);
    printf("MediaConfig: Loaded %d entries.\n", g_config_count);
    return g_config_count > 0;
}

const char* MediaConfig_GetPath(const char* key) {
    for (int i = 0; i < g_config_count; i++) {
        if (strcmp(g_config_cache[i].key, key) == 0) {
            return g_config_cache[i].value;
        }
    }
    fprintf(stderr, "MediaConfig: Key not found: %s\n", key);
    return NULL;
}

void MediaConfig_Cleanup() {
    g_config_count = 0;
}