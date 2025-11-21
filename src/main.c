/*
 * src/main.c (フェーズ2完了版: Director導入)
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "sdl_utils.h"
#include "director.h"
#include "reel.h"
#include "presentation.h"

#define SCREEN_WIDTH 838
#define SCREEN_HEIGHT 600
#define FONT_PATH "font.ttf" 
#define FONT_SIZE 24         
#define CONFIG_PATH "media.cfg" 

int SDL_main(int argc, char* args[]) {
    // 1. システム初期化
    if (!init_sdl("Slot Simulator", SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !load_media(FONT_PATH, FONT_SIZE)) { 
        close_sdl();
        return -1;
    }
    srand((unsigned int)time(NULL));
    
    if (!MediaConfig_Load(CONFIG_PATH)) {
        fprintf(stderr, "media.cfg の読み込みに失敗しました。\n");
        close_sdl();
        return -1;
    }

    // 2. サブシステム初期化
    // (Reel, PresentationはDirector内部でも使うが、初期化順序の依存のためここで呼ぶ)
    if (!Reel_Init(gRenderer)) { 
        fprintf(stderr, "Reel Init Failed\n");
        close_sdl();
        return -1;
    }
    if (!Presentation_Init(gRenderer)) {
        fprintf(stderr, "Presentation Init Failed\n");
        close_sdl();
        return -1;
    }

    // 3. Director (現場監督) の初期化
    if (!Director_Init(gRenderer)) {
        fprintf(stderr, "Director Init Failed\n");
        close_sdl();
        return -1;
    }

    // 4. メインループ
    bool quit = false;
    while (!quit) {
        // Directorが一括して入力・更新を行う
        // (※SDL_QUITイベントだけはmainで捕捉してループを抜ける設計にする場合もあるが、
        //  ここでは Director_Update 内で exit(0) している簡易実装を採用)
        
        Director_Update();
        Director_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);

        SDL_Delay(16); // 60FPS
    }

    // 5. 終了処理
    Director_Cleanup();
    Presentation_Cleanup(); 
    Reel_Cleanup();
    MediaConfig_Cleanup(); 
    close_sdl();
    return 0;
}