#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>

#include "sdl_utils.h"
#include "reel.h"
#include "game_data.h"
#include "lottery.h"
#include "normal.h"
#include "cz.h"
#include "at.h"
#include "video.h"       // Video モジュールをインクルード
#include "video_defs.h"  // (★) 動画定義をインクルード

#define SCREEN_WIDTH 838
#define SCREEN_HEIGHT 600
#define FONT_PATH "font.ttf"
#define FONT_SIZE 24

// (★) 動画ファイルパスの定義は video.c に移動したため削除

typedef enum {
    MAIN_STATE_WAIT_LEVER,
    // (★) MAIN_STATE_ENSHUTSU_AKA7 は削除
    MAIN_STATE_REELS_SPINNING,
    MAIN_STATE_RESULT_SHOW
} MainLoopState;

// ... (グローバル変数 変更なし) ...
static GameData g_game_data;
static YakuType g_current_yaku = YAKU_HAZURE;
static int g_stop_order_counter = 1;
static bool g_reel_stop_flags[3] = {false, false, false};
static int g_actual_push_order[3] = {-1, -1, -1};


// (★) レバーオン時の処理 (通常のみ)
static void OnLeverOn(MainLoopState* next_main_state) { 
    
    // (★) 常に通常の抽選
    Video_Play(gRenderer, VIDEO_SPIN, true); // (★) 通常回転動画を再生
    
    if (g_game_data.current_state == STATE_CZ ||
        g_game_data.current_state == STATE_FRANXX_BONUS ||
        g_game_data.current_state == STATE_TSUREDASHI)
    {
        g_current_yaku = Lottery_GetResult_FranxxHighProb();
    }
    else
    {
        g_current_yaku = Lottery_GetResult_Normal();
    }

    // (★) TODO: ここで g_current_yaku や g_game_data.current_state に応じて
    // 1回再生の演出動画 (VIDEO_ENSHUTSU_AKA7 など) を Video_Play(..., false) で再生する
    if (g_current_yaku == YAKU_CHERRY) { // 例: チェリーなら演出
        // Video_Play(gRenderer, VIDEO_ENSHUTSU_CHERRY, false);
    }
    if (g_game_data.current_state == STATE_BB_INITIAL && g_game_data.current_bonus_payout == 0) {
        // 例: BB1ゲーム目なら赤7演出
        Video_Play(gRenderer, VIDEO_ENSHUTSU_AKA7, false);
    }


    Reel_SetYaku(g_current_yaku);
    snprintf(g_game_data.last_yaku_name, sizeof(g_game_data.last_yaku_name), "%s", GetYakuName(g_current_yaku));
    g_game_data.info_message[0] = '\0';

    Reel_StartSpinning(); // (★) 通常の順回転
    g_stop_order_counter = 1;
    g_reel_stop_flags[0] = false; // (★) 停止ボタンを有効化
    g_reel_stop_flags[1] = false;
    g_reel_stop_flags[2] = false;
    g_actual_push_order[0] = -1;
    g_actual_push_order[1] = -1;
    g_actual_push_order[2] = -1;
    
    *next_main_state = MAIN_STATE_REELS_SPINNING; // (★) 通常回転状態へ
}

// 全リール停止時の処理 (★ YAKU_FAKE_AKA_7 のチェックを削除)
static void OnReelsStopped() {
    bool oshijun_success = CheckOshijun(g_current_yaku, g_actual_push_order);

    if (g_game_data.current_state >= STATE_BB_INITIAL &&
        g_game_data.current_state < STATE_AT_END)
    {
        AT_ProcessStop(&g_game_data, g_current_yaku, oshijun_success);
    }
    else if (g_game_data.current_state == STATE_CZ)
    {
        Normal_Update(&g_game_data, g_current_yaku);
    }
    else // STATE_NORMAL
    {
        Normal_Update(&g_game_data, g_current_yaku);
    }

    if (!oshijun_success) {
         snprintf(g_game_data.info_message, sizeof(g_game_data.info_message), "押し順ミス！");
    }
}


int main(int argc, char* args[]) {
    // --- 初期化 ---
    if (!init_sdl("Slot Simulator", SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !load_media(FONT_PATH, FONT_SIZE)) {
        close_sdl();
        return -1;
    }
    srand((unsigned int)time(NULL));

    // --- ゲームデータ初期化 ---
    memset(&g_game_data, 0, sizeof(GameData));
    AT_Init(&g_game_data); // (★) AT_Init はフラグをセットしなくなった

    if (!Reel_Init(gRenderer)) { // リール初期化
        fprintf(stderr, "リールの初期化に失敗\n");
        close_sdl();
        return -1;
    }

    // (★) Video モジュールを待機動画で初期化 (ループ再生: true)
    if (!Video_Play(gRenderer, VIDEO_IDLE, true)) {
        fprintf(stderr, "待機動画の初期化に失敗\n");
        Reel_Cleanup();
        close_sdl();
        return -1;
    }

    MainLoopState main_state = MAIN_STATE_WAIT_LEVER;

    // --- メインループ ---
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        // --- イベント処理 ---
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE) { // レバーオン
                    if ((main_state == MAIN_STATE_WAIT_LEVER || main_state == MAIN_STATE_RESULT_SHOW) &&
                        (g_game_data.current_state != STATE_AT_END))
                    {
                        // (★) OnLeverOn が main_state を変更する
                        OnLeverOn(&main_state); 
                    }
                }
                // (★) 通常の回転中のみ停止ボタンを受け付ける
                if (main_state == MAIN_STATE_REELS_SPINNING) { 
                    int reel_to_stop = -1;
                    if (e.key.keysym.sym == SDLK_z) reel_to_stop = 0;
                    if (e.key.keysym.sym == SDLK_x) reel_to_stop = 1;
                    if (e.key.keysym.sym == SDLK_c) reel_to_stop = 2;
                    
                    if (reel_to_stop != -1 && !g_reel_stop_flags[reel_to_stop])
                    {
                        Reel_RequestStop(reel_to_stop, g_stop_order_counter);
                        g_reel_stop_flags[reel_to_stop] = true;
                        g_actual_push_order[g_stop_order_counter - 1] = reel_to_stop;
                        g_stop_order_counter++;
                    }
                }
            }
        }

        // --- 更新処理 ---
        Reel_Update();
        Video_Update(); // 毎フレーム動画更新関数を呼ぶ

        // (★) 演出状態 (ENSHUTSU_AKA7) は削除

        // (★) 2. 通常の順回転が終了したら、結果表示に移行
        if (main_state == MAIN_STATE_REELS_SPINNING && !Reel_IsSpinning()) {
             main_state = MAIN_STATE_RESULT_SHOW;
             OnReelsStopped();
             // (★) 1回再生の演出動画が終わっていなければ、終わるまで待つ
             // (★) 終わっていたら、待機動画に戻す
             if (Video_IsFinished()) {
                Video_Play(gRenderer, VIDEO_IDLE, true);
             }
        }

        // (★) 3. 1回再生の動画が結果表示中に終わったら待機動画に戻す
        if (main_state == MAIN_STATE_RESULT_SHOW && Video_IsFinished()) {
             Video_Play(gRenderer, VIDEO_IDLE, true);
        }


        // --- 描画処理 ---
        SDL_SetRenderDrawColor(gRenderer, 0x1E, 0x1E, 0x1E, 0xFF);
        SDL_RenderClear(gRenderer);

        // (★) 描画順: 1.動画(リール上), 2.リール(背景+図柄), 3.テキスト
        Video_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);
        Reel_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);

        // テキストを描画
        char buffer[256];
        SDL_Color white = {255, 255, 255}, yellow = {255, 255, 0}, cyan = {0, 255, 255}, red = {255, 50, 50};
        snprintf(buffer, sizeof(buffer), "状態: %s", AT_GetStateName(g_game_data.current_state));
        draw_text(buffer, 50, 50, cyan);
        snprintf(buffer, sizeof(buffer), "成立役: %s", (main_state == MAIN_STATE_WAIT_LEVER) ? "---" : g_game_data.last_yaku_name);
        draw_text(buffer, 50, 90, white);
        draw_text(g_game_data.info_message, 50, 130, yellow);
        if (g_game_data.current_state >= STATE_BB_INITIAL && g_game_data.current_state < STATE_AT_END) {
             snprintf(buffer, sizeof(buffer), "ボーナス中: %d / %d 枚", g_game_data.current_bonus_payout, g_game_data.target_bonus_payout);
             draw_text(buffer, 50, 200, white);
        }
        snprintf(buffer, sizeof(buffer), "AT高確G数: %d G", g_game_data.bonus_high_prob_games);
        draw_text(buffer, 50, 240, white);
        if (g_game_data.hiyoku_is_active) {
            snprintf(buffer, sizeof(buffer), "比翼BEATS [LV%d]: %d ST %s",
                g_game_data.hiyoku_level, g_game_data.hiyoku_st_games,
                g_game_data.hiyoku_is_frozen ? "(FROZEN)" : "");
            draw_text(buffer, 50, 280, red);
        }
        snprintf(buffer, sizeof(buffer), "総獲得枚数: %lld 枚", g_game_data.total_payout_diff);
        draw_text(buffer, 600, 50, white);
        snprintf(buffer, sizeof(buffer), "通常ストック: %d 個", g_game_data.bonus_stock_count);
        draw_text(buffer, 600, 90, white);
        snprintf(buffer, sizeof(buffer), "BB EX 予約: %d 枚", g_game_data.queued_bb_ex_payout);
        draw_text(buffer, 600, 130, white);

        // 操作案内 (Y=320)
        if (g_game_data.current_state == STATE_AT_END) {
            draw_text("シミュレーション終了", 50, 320, red);
        } else if (main_state == MAIN_STATE_REELS_SPINNING) {
             draw_text("Z(左), X(中), C(右) でリール停止", 50, 320, yellow);
        } else if (main_state == MAIN_STATE_WAIT_LEVER || main_state == MAIN_STATE_RESULT_SHOW) {
            draw_text("スペースキーでレバーオン", 50, 320, yellow);
        }

        SDL_RenderPresent(gRenderer);
        SDL_Delay(16); // 60FPS
    }

    // --- 終了処理 ---
    Video_Cleanup(); // Video モジュール終了処理
    Reel_Cleanup();
    close_sdl();
    return 0;
}