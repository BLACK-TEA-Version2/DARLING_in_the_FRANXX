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

// (★) presentation.h をインクルード
#include "presentation.h" 

#define SCREEN_WIDTH 838
#define SCREEN_HEIGHT 600
#define FONT_PATH "font.ttf" // (★) 環境に合わせて要変更
#define FONT_SIZE 24         // (★) <<< 修正点: この行を追加
#define CONFIG_PATH "media.cfg" // (★) 設定ファイルのパス

// (★) --- メインループの状態定義 ---
typedef enum {
    STATE_WAIT_LEVER,           // 1. (入力可能) ループ動画再生中、レバー待ち
    STATE_REELS_SPINNING,       // 2. (入力可能) ループ動画再生中、リール停止待ち
    STATE_PLAYING_TRANSITION    // 3. (リール停止ブロック) 遷移動画再生中、リール回転
} MainLoopState;

// --- グローバル変数 ---
static GameData g_game_data;
static YakuType g_current_yaku = YAKU_HAZURE;
static int g_stop_order_counter = 1;
static bool g_reel_stop_flags[3] = {false, false, false};
static int g_actual_push_order[3] = {-1, -1, -1};

// (★) --- 状態管理用グローバル変数 (重要) ---
static MainLoopState g_main_state;              // 演出・操作の状態
static AT_State g_current_logic_state = STATE_NORMAL; // ロジック(内部)の状態
static AT_State g_current_media_state = STATE_NORMAL; // 再生中動画(演出)の状態
static bool g_is_first_game = true;             // 1G目 (強制AT) フラグ


// (★) --- 演出ヘルパー関数 ---

/**
 * @brief (★) AT_State (ゲームロジック) から「状態ループ」のキー名を取得
 */
static const char* GetLoopKeyForState(AT_State state) {
    switch (state) {
        case STATE_NORMAL:          return "LOOP_VIDEO_NORMAL";
        case STATE_CZ:              return "LOOP_VIDEO_CZ";
        case STATE_BB_INITIAL:      return "LOOP_VIDEO_BB_INITIAL";
        case STATE_BONUS_HIGH_PROB: return "LOOP_VIDEO_BONUS_HIGH_PROB";
        case STATE_BB_HIGH_PROB:    return "LOOP_VIDEO_BB_HIGH_PROB";
        case STATE_FRANXX_BONUS:    return "LOOP_VIDEO_FRANXX_BONUS";
        case STATE_BB_EX:           return "LOOP_VIDEO_BB_EX";
        case STATE_HIYOKU_BEATS:    return "LOOP_VIDEO_HIYOKU_BEATS";
        case STATE_EPISODE_BONUS:   return "LOOP_VIDEO_EPISODE_BONUS";
        case STATE_TSUREDASHI:      return "LOOP_VIDEO_TSUREDASHI";
        case STATE_AT_END:          return "LOOP_VIDEO_AT_END";
        default:
            return "LOOP_VIDEO_NORMAL"; // 不明な場合は NORMAL にフォールバック
    }
}

/**
 * @brief (★) 遷移「元」と「先」から「遷移演出」のキー名を作成
 */
static const char* GetTransitionKey(AT_State from, AT_State to) {
    static char transition_key_buffer[256]; // 静的バッファ
    
    // (★) AT_State の enum 値に基づいて手動でマッピングします
    const char* from_key = "NORMAL";
    const char* to_key = "NORMAL";

    switch(from) {
        case STATE_NORMAL: from_key = "NORMAL"; break;
        case STATE_CZ: from_key = "CZ"; break;
        case STATE_BB_INITIAL: from_key = "BB_INITIAL"; break;
        case STATE_BONUS_HIGH_PROB: from_key = "BONUS_HIGH_PROB"; break;
        case STATE_BB_HIGH_PROB: from_key = "BB_HIGH_PROB"; break;
        case STATE_FRANXX_BONUS: from_key = "FRANXX_BONUS"; break;
        case STATE_BB_EX: from_key = "BB_EX"; break;
        case STATE_HIYOKU_BEATS: from_key = "HIYOKU_BEATS"; break;
        case STATE_EPISODE_BONUS: from_key = "EPISODE_BONUS"; break;
        case STATE_TSUREDASHI: from_key = "TSUREDASHI"; break;
        case STATE_AT_END: from_key = "AT_END"; break;
        default: break;
    }

    switch(to) {
        case STATE_NORMAL: to_key = "NORMAL"; break;
        case STATE_CZ: to_key = "CZ"; break;
        case STATE_BB_INITIAL: to_key = "BB_INITIAL"; break;
        case STATE_BONUS_HIGH_PROB: to_key = "BONUS_HIGH_PROB"; break;
        case STATE_BB_HIGH_PROB: to_key = "BB_HIGH_PROB"; break;
        case STATE_FRANXX_BONUS: to_key = "FRANXX_BONUS"; break;
        case STATE_BB_EX: to_key = "BB_EX"; break;
        case STATE_HIYOKU_BEATS: to_key = "HIYOKU_BEATS"; break;
        case STATE_EPISODE_BONUS: to_key = "EPISODE_BONUS"; break;
        case STATE_TSUREDASHI: to_key = "TSUREDASHI"; break;
        case STATE_AT_END: to_key = "AT_END"; break;
        default: break;
    }

    // "TRANSITION_(FROM)_TO_(TO)" の文字列を生成
    snprintf(transition_key_buffer, sizeof(transition_key_buffer), "TRANSITION_%s_TO_%s", from_key, to_key);
    
    return transition_key_buffer;
}

/**
 * @brief (★) 通常のレバーオン処理（ロジック部）
 */
static void DoLeverLogic() {
    // (★) 1G目（強制AT）の処理
    if (g_is_first_game) {
        printf("1G目です。AT_Init() を呼び出し、強制的にATに突入させます。\n");
        AT_Init(&g_game_data); // (★) これで g_game_data.current_state が BB_INITIAL になる
        g_current_yaku = YAKU_HAZURE; // 1G目は抽選しない（仮）
        g_is_first_game = false;
    } else {
    // (★) 2G目以降の通常の抽選
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
    }

    Reel_SetYaku(g_current_yaku);
    snprintf(g_game_data.last_yaku_name, sizeof(g_game_data.last_yaku_name), "%s", GetYakuName(g_current_yaku));
    g_game_data.info_message[0] = '\0';

    Reel_StartSpinning(); 
    g_stop_order_counter = 1;
    g_reel_stop_flags[0] = false;
    g_reel_stop_flags[1] = false;
    g_reel_stop_flags[2] = false;
    g_actual_push_order[0] = -1;
    g_actual_push_order[1] = -1;
    g_actual_push_order[2] = -1;
}

/**
 * @brief (★) 全リール停止時の処理（ロジック更新）
 */
static void OnReelsStopped() {
    bool oshijun_success = CheckOshijun(g_current_yaku, g_actual_push_order);

    // (★) 1G目の場合は AT_Init() が既に呼ばれているので、ProcessStopから呼ぶ
    if (g_game_data.current_state >= STATE_BB_INITIAL &&
        g_game_data.current_state < STATE_AT_END)
    {
        AT_ProcessStop(&g_game_data, g_current_yaku, oshijun_success);
    }
    else if (g_game_data.current_state == STATE_CZ)
    {
        CZ_Update(&g_game_data, g_current_yaku);
    }
    else // STATE_NORMAL
    {
        Normal_Update(&g_game_data, g_current_yaku);
    }

    if (!oshijun_success) {
         snprintf(g_game_data.info_message, sizeof(g_game_data.info_message), "押し順ミス！");
    }

    // (★) 最も重要：ロジックの内部状態を更新
    g_current_logic_state = g_game_data.current_state;
}


// (★) main関数は SDL_main にリネーム (Windows, -mwindows でのリンクのため)
int SDL_main(int argc, char* args[]) {
    // --- 1. 初期化 ---
    if (!init_sdl("Slot Simulator", SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !load_media(FONT_PATH, FONT_SIZE)) { // (★) FONT_SIZE が使われる
        close_sdl();
        return -1;
    }
    srand((unsigned int)time(NULL));

    // (★) メディア設定をファイルからロード
    if (!MediaConfig_Load(CONFIG_PATH)) {
        fprintf(stderr, "media.cfg の読み込みに失敗。実行を終了します。\n");
        close_sdl();
        return -1;
    }

    // (★) ゲームデータ初期化
    memset(&g_game_data, 0, sizeof(GameData));
    g_game_data.current_state = STATE_NORMAL; // (★) AT_Init() はまだ呼ばない
    g_current_logic_state = STATE_NORMAL;
    g_current_media_state = STATE_NORMAL;
    g_is_first_game = true;

    if (!Reel_Init(gRenderer)) { 
        fprintf(stderr, "リールの初期化に失敗\n");
        MediaConfig_Cleanup();
        close_sdl();
        return -1;
    }

    // (★) Presentation モジュールを初期化
    if (!Presentation_Init(gRenderer)) {
        fprintf(stderr, "演出モジュールの初期化に失敗\n");
        Reel_Cleanup();
        MediaConfig_Cleanup();
        close_sdl();
        return -1;
    }

    // (★) 起動時のメディアを再生 (NORMALのループ)
    const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
    Presentation_Play(gRenderer, loop_path, true);
    g_main_state = STATE_WAIT_LEVER;

    // --- 2. メインループ ---
    bool quit = false;
    SDL_Event e;
    while (!quit) {
        
        // --- 3. イベント処理 ---
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                // (★) レバーオン
                if (e.key.keysym.sym == SDLK_SPACE) {
                    if (g_main_state == STATE_WAIT_LEVER && g_current_logic_state != STATE_AT_END) {
                        
                        // (★) 状態不一致チェック
                        if (g_current_logic_state != g_current_media_state) {
                            // (★) 遷移演出を再生
                            const char* trans_key = GetTransitionKey(g_current_media_state, g_current_logic_state);
                            const char* path = MediaConfig_GetPath(trans_key);
                            
                            if (path && Presentation_Play(gRenderer, path, false)) { // ループなし
                                printf("遷移演出 [%s] を再生します。\n", trans_key);
                                DoLeverLogic(); // (★) 抽選とリール回転は行う
                                g_main_state = STATE_PLAYING_TRANSITION; // (★) リール停止ブロック状態へ
                            } else {
                                // (★) 遷移動画が定義されていない
                                fprintf(stderr, "遷移演出 [%s] が未定義です。即時ループを切り替えます。\n", trans_key);
                                // (★) 遷移先のループ動画に即時切り替え
                                g_current_media_state = g_current_logic_state;
                                const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
                                Presentation_Play(gRenderer, loop_path, true);
                                // (★) 通常の回転へ
                                DoLeverLogic();
                                g_main_state = STATE_REELS_SPINNING;
                            }
                        } else {
                            // (★) 状態一致 (通常のゲーム)
                            DoLeverLogic();
                            g_main_state = STATE_REELS_SPINNING;
                        }
                    }
                }

                // (★) リール停止
                if (e.key.keysym.sym == SDLK_z || e.key.keysym.sym == SDLK_x || e.key.keysym.sym == SDLK_c) {
                    
                    // (★) 遷移演出中はリール停止をブロック
                    if (g_main_state == STATE_PLAYING_TRANSITION) {
                        printf("リール停止ブロック中\n");
                        continue; // (★) ブロック
                    }

                    if (g_main_state == STATE_REELS_SPINNING) {
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
        }

        // --- 4. 更新処理 ---
        Reel_Update();
        Presentation_Update(); 

        // (★) メインループの状態遷移
        switch (g_main_state) {
            
            case STATE_WAIT_LEVER:
                // (★) 何もしない (入力待ち)
                break;

            case STATE_PLAYING_TRANSITION:
                if (Presentation_IsFinished()) {
                    // (★) 遷移演出が終了した
                    printf("遷移演出 終了。\n");
                    // (★) 遷移先のループ動画に切り替え
                    g_current_media_state = g_current_logic_state;
                    const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
                    Presentation_Play(gRenderer, loop_path, true);
                    
                    // (★) リール停止ブロックを解除
                    g_main_state = STATE_REELS_SPINNING;
                    printf("リール停止ブロックを解除。\n");
                }
                break;
            
            case STATE_REELS_SPINNING:
                if (!Reel_IsSpinning()) {
                    // (★) 全リール停止した
                    OnReelsStopped(); // (★) この中で g_current_logic_state が更新される
                    g_main_state = STATE_WAIT_LEVER; // (★) レバー待ちに戻る
                }
                break;
        }


        // --- 5. 描画処理 ---
        SDL_SetRenderDrawColor(gRenderer, 0x1E, 0x1E, 0x1E, 0xFF);
        SDL_RenderClear(gRenderer);

        // (★) 描画順: 1.動画/画像, 2.リール, 3.テキスト
        Presentation_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT); // (★)
        Reel_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);

        // (★) テキスト描画 (デバッグ表示を追加)
        char buffer[256];
        SDL_Color white = {255, 255, 255}, yellow = {255, 255, 0}, cyan = {0, 255, 255}, red = {255, 50, 50};
        
        snprintf(buffer, sizeof(buffer), "ロジック状態: %s", AT_GetStateName(g_current_logic_state));
        draw_text(buffer, 50, 50, cyan);
        snprintf(buffer, sizeof(buffer), "メディア状態: %s", AT_GetStateName(g_current_media_state));
        draw_text(buffer, 50, 80, cyan);

        snprintf(buffer, sizeof(buffer), "成立役: %s", (g_main_state == STATE_WAIT_LEVER) ? "---" : g_game_data.last_yaku_name);
        draw_text(buffer, 50, 120, white);
        draw_text(g_game_data.info_message, 50, 150, yellow);
        
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


        // (★) 操作案内 (Y=320)
        if (g_current_logic_state == STATE_AT_END) {
            draw_text("シミュレーション終了", 50, 320, red);
        } else if (g_main_state == STATE_REELS_SPINNING) {
             draw_text("Z(左), X(中), C(右) でリール停止", 50, 320, yellow);
        } else if (g_main_state == STATE_WAIT_LEVER) {
            draw_text("スペースキーでレバーオン", 50, 320, yellow);
        } else if (g_main_state == STATE_PLAYING_TRANSITION) {
            draw_text("遷移演出中 (リール停止ブロック)", 50, 320, red);
        }

        SDL_RenderPresent(gRenderer);
        SDL_Delay(16); // 60FPS
    }

    // --- 6. 終了処理 ---
    Presentation_Cleanup(); // (★)
    Reel_Cleanup();
    MediaConfig_Cleanup(); // (★)
    close_sdl();
    return 0;
}