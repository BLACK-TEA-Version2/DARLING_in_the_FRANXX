/*
 * (★) 修正版 src/main.c (フェーズ1完了版)
 */

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

#include "presentation.h" 

#define SCREEN_WIDTH 838
#define SCREEN_HEIGHT 600
#define FONT_PATH "font.ttf" 
#define FONT_SIZE 24         
#define CONFIG_PATH "media.cfg" 

// --- メインループの状態定義 ---
typedef enum {
    STATE_WAIT_LEVER,           
    STATE_REELS_SPINNING,       
    STATE_PLAYING_TRANSITION,   
    STATE_AT_PRES_INTRO,        
    STATE_AT_PRES_LOOP,         
    STATE_AT_JUDGE,             
} MainLoopState;

// --- グローバル変数 ---
static GameData g_game_data;
static YakuType g_current_yaku = YAKU_HAZURE;
static int g_stop_order_counter = 1;
static bool g_reel_stop_flags[3] = {false, false, false};
static int g_actual_push_order[3] = {-1, -1, -1};

// --- 状態管理用グローバル変数 ---
static MainLoopState g_main_state;              
static AT_State g_current_logic_state = STATE_NORMAL; 
static AT_State g_current_media_state = STATE_NORMAL; 
static bool g_is_first_game = true;             


// --- 演出ヘルパー関数 ---

static const char* GetVideoKey(VideoType type) {
    switch(type) {
        case VIDEO_IDLE: return "LOOP_VIDEO_NORMAL";
        case VIDEO_SPIN: return "LOOP_VIDEO_NORMAL"; 
        case VIDEO_AT_PRES_A_INTRO: return "AT_PRES_A_INTRO";
        case VIDEO_AT_PRES_A_LOOP:  return "AT_PRES_A_LOOP";
        case VIDEO_AT_PRES_B_INTRO: return "AT_PRES_B_INTRO";
        case VIDEO_AT_PRES_B_LOOP:  return "AT_PRES_B_LOOP";
        case VIDEO_AT_PRES_C_INTRO: return "AT_PRES_C_INTRO";
        case VIDEO_AT_PRES_C_LOOP:  return "AT_PRES_C_LOOP";
        case VIDEO_AT_PRES_D_INTRO: return "AT_PRES_D_INTRO";
        case VIDEO_AT_PRES_D_LOOP:  return "AT_PRES_D_LOOP";
        case VIDEO_JUDGE_LOSE_1:    return "JUDGE_LOSE_1";
        case VIDEO_JUDGE_LOSE_2:    return "JUDGE_LOSE_2";
        case VIDEO_JUDGE_LOSE_3:    return "JUDGE_LOSE_3";
        case VIDEO_JUDGE_DARLING_1: return "JUDGE_DARLING_1";
        case VIDEO_JUDGE_DARLING_2: return "JUDGE_DARLING_2";
        case VIDEO_JUDGE_DARLING_3: return "JUDGE_DARLING_3";
        case VIDEO_JUDGE_FRANXX_1:  return "JUDGE_FRANXX_1";
        case VIDEO_JUDGE_FRANXX_2:  return "JUDGE_FRANXX_2";
        case VIDEO_JUDGE_FRANXX_3:  return "JUDGE_FRANXX_3";
        default: 
            fprintf(stderr, "GetVideoKey: 未定義のVideoType %d\n", type);
            return NULL;
    }
}

static bool PlayVideoByKey(const char* key, bool loop) {
    if (!key) return false;
    const char* path = MediaConfig_GetPath(key);
    if (path && Presentation_Play(gRenderer, path, loop)) {
        return true;
    }
    fprintf(stderr, "動画キー [%s] (パス: %s) の再生に失敗しました。\n", key, path ? path : "NULL");
    return false;
}

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
            return "LOOP_VIDEO_NORMAL"; 
    }
}

static const char* GetTransitionKey(AT_State from, AT_State to) {
    static char transition_key_buffer[256]; 
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
    snprintf(transition_key_buffer, sizeof(transition_key_buffer), "TRANSITION_%s_TO_%s", from_key, to_key);
    return transition_key_buffer;
}

// --- AT高確率状態 ヘルパー ---
static void SelectPresentationPair(GameData* data) {
    int k = rand() % 100; 
    VideoType intro, loop;
    // (簡易実装)
    if (data->at_bonus_result == BONUS_AT_CONTINUE) {
        if (k < 40) { intro = VIDEO_AT_PRES_A_INTRO; loop = VIDEO_AT_PRES_A_LOOP; }
        else if (k < 70) { intro = VIDEO_AT_PRES_B_INTRO; loop = VIDEO_AT_PRES_B_LOOP; }
        else if (k < 90) { intro = VIDEO_AT_PRES_C_INTRO; loop = VIDEO_AT_PRES_C_LOOP; }
        else { intro = VIDEO_AT_PRES_D_INTRO; loop = VIDEO_AT_PRES_D_LOOP; }
    } else { 
        // 当選時は期待度高めの演出を選びやすくする
        if (k < 5) { intro = VIDEO_AT_PRES_A_INTRO; loop = VIDEO_AT_PRES_A_LOOP; }
        else if (k < 20) { intro = VIDEO_AT_PRES_B_INTRO; loop = VIDEO_AT_PRES_B_LOOP; }
        else if (k < 50) { intro = VIDEO_AT_PRES_C_INTRO; loop = VIDEO_AT_PRES_C_LOOP; }
        else { intro = VIDEO_AT_PRES_D_INTRO; loop = VIDEO_AT_PRES_D_LOOP; }
    }
    data->at_pres_intro_id = intro;
    data->at_pres_loop_id = loop;
}

static VideoType SelectJudgmentVideo(GameData* data) {
    int k = rand() % 3; 
    data->at_judge_video_duration_ms = 5000; 
    
    if (data->at_bonus_result == BONUS_AT_CONTINUE) {
        const VideoType videos[] = {VIDEO_JUDGE_LOSE_1, VIDEO_JUDGE_LOSE_2, VIDEO_JUDGE_LOSE_3};
        return videos[k];
    } else if (data->at_bonus_result == BONUS_FRANXX) {
        const VideoType videos[] = {VIDEO_JUDGE_FRANXX_1, VIDEO_JUDGE_FRANXX_2, VIDEO_JUDGE_FRANXX_3};
        return videos[k];
    } else { 
        // DARLING, BB_EX, EPISODE は共通演出 (ダーリンボーナス当選演出)
        const VideoType videos[] = {VIDEO_JUDGE_DARLING_1, VIDEO_JUDGE_DARLING_2, VIDEO_JUDGE_DARLING_3};
        return videos[k];
    }
}


static void DoLeverLogic() {
    
    if (g_is_first_game) {
        printf("1G目です。AT_Init() を呼び出し、強制的にATに突入させます。\n");
        AT_Init(&g_game_data); 
        g_current_yaku = YAKU_HAZURE; 
        g_is_first_game = false;
        g_current_logic_state = g_game_data.current_state; 
    } else {
        if (g_game_data.current_state == STATE_BONUS_HIGH_PROB) {
            fprintf(stderr, "エラー: DoLeverLogic が AT高確率状態 (1回目レバー) で呼ばれました。\n");
            g_current_yaku = Lottery_GetResult_Normal();
        }
        else if (g_game_data.current_state == STATE_CZ ||
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

static void OnReelsStopped(bool oshijun_success) { 

    if (g_game_data.current_state == STATE_CZ)
    {
        CZ_Update(&g_game_data, g_current_yaku);
    }
    else if (g_game_data.current_state == STATE_NORMAL) 
    {
        Normal_Update(&g_game_data, g_current_yaku);
    }

    if (!oshijun_success) {
         snprintf(g_game_data.info_message, sizeof(g_game_data.info_message), "押し順ミス！");
    }

    g_current_logic_state = g_game_data.current_state;
}


int SDL_main(int argc, char* args[]) {
    if (!init_sdl("Slot Simulator", SCREEN_WIDTH, SCREEN_HEIGHT) ||
        !load_media(FONT_PATH, FONT_SIZE)) { 
        close_sdl();
        return -1;
    }
    srand((unsigned int)time(NULL));
    if (!MediaConfig_Load(CONFIG_PATH)) {
        fprintf(stderr, "media.cfg の読み込みに失敗。実行を終了します。\n");
        close_sdl();
        return -1;
    }
    memset(&g_game_data, 0, sizeof(GameData));
    g_game_data.current_state = STATE_NORMAL; 
    g_game_data.at_step = AT_STEP_NONE; 
    g_current_logic_state = STATE_NORMAL;
    g_current_media_state = STATE_NORMAL;
    g_is_first_game = true; 
    if (!Reel_Init(gRenderer)) { 
        fprintf(stderr, "リールの初期化に失敗\n");
        MediaConfig_Cleanup();
        close_sdl();
        return -1;
    }
    if (!Presentation_Init(gRenderer)) {
        fprintf(stderr, "演出モジュールの初期化に失敗\n");
        Reel_Cleanup();
        MediaConfig_Cleanup();
        close_sdl();
        return -1;
    }
    const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
    Presentation_Play(gRenderer, loop_path, true);
    g_main_state = STATE_WAIT_LEVER;

    bool quit = false;
    SDL_Event e;
    while (!quit) {
        
        bool lever_on_this_frame = false;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_SPACE) {
                    if (g_current_logic_state == STATE_AT_END) continue;

                    // (★) AT高確率状態の【2回目レバーオン】
                    if (g_current_logic_state == STATE_BONUS_HIGH_PROB && 
                        g_game_data.at_step == AT_STEP_LOOP_VIDEO_MAIN && 
                        g_main_state == STATE_AT_PRES_LOOP)
                    {
                        printf("AT高確: 2回目レバーオン\n");
                        lever_on_this_frame = true; 
                        
                        VideoType judgeVideoId = SelectJudgmentVideo(&g_game_data);
                        const char* video_key = GetVideoKey(judgeVideoId);
                        
                        if (PlayVideoByKey(video_key, false)) { 
                            g_game_data.at_step = AT_STEP_JUDGE_VIDEO;
                            g_main_state = STATE_AT_JUDGE;
                            g_game_data.at_judge_video_start_time = SDL_GetTicks();
                            g_game_data.at_judge_timing_reverse_triggered = false;
                            g_game_data.at_judge_timing_stop_triggered = false;
                        } else {
                            // 動画再生失敗時のフォールバック
                            g_main_state = STATE_WAIT_LEVER; // 強制的に次に進める
                        }
                    }
                    else if (g_main_state == STATE_WAIT_LEVER) 
                    {
                        // 状態遷移判定
                        if (g_current_logic_state != g_current_media_state) {
                            
                            printf("通常レバーオン (状態遷移)\n");
                            lever_on_this_frame = true; 
                            const char* trans_key = GetTransitionKey(g_current_media_state, g_current_logic_state);
                            
                            if (PlayVideoByKey(trans_key, false)) { 
                                printf("遷移演出 [%s] を再生します。\n", trans_key);
                                DoLeverLogic(); 
                                g_main_state = STATE_PLAYING_TRANSITION; 
                            } else {
                                fprintf(stderr, "遷移演出 [%s] が未定義です。即時ループを切り替えます。\n", trans_key);
                                g_current_media_state = g_current_logic_state;
                                const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
                                Presentation_Play(gRenderer, loop_path, true);
                                
                                if (g_current_logic_state == STATE_BONUS_HIGH_PROB && g_game_data.at_step == AT_STEP_WAIT_LEVER1) {
                                    // スキップ時のAT高確1回目レバー処理
                                    printf("AT高確: 1回目レバーオン (スキップ)\n");
                                    g_game_data.bonus_high_prob_games--;
                                    snprintf(g_game_data.info_message, sizeof(g_game_data.info_message), "AT 残り %dG", g_game_data.bonus_high_prob_games);
                                    g_current_yaku = Lottery_GetResult_AT();
                                    g_game_data.at_bonus_result = Lottery_CheckBonus_AT(g_current_yaku);
                                    g_game_data.at_last_lottery_yaku = g_current_yaku; 
                                    Reel_SetYaku(g_current_yaku);
                                    snprintf(g_game_data.last_yaku_name, sizeof(g_game_data.last_yaku_name), "%s", GetYakuName(g_current_yaku));
                                    Reel_StartSpinning(); 
                                    g_stop_order_counter = 1;
                                    g_reel_stop_flags[0] = g_reel_stop_flags[1] = g_reel_stop_flags[2] = false;
                                    g_actual_push_order[0] = g_actual_push_order[1] = g_actual_push_order[2] = -1;
                                    g_game_data.at_step = AT_STEP_REEL_SPIN;
                                    g_main_state = STATE_REELS_SPINNING;
                                } else {
                                    printf("通常レバーオン (スキップ)\n");
                                    DoLeverLogic();
                                    g_main_state = STATE_REELS_SPINNING;
                                }
                            }
                        }
                        else {
                            lever_on_this_frame = true; 
                            
                            // (★) AT高確率状態の【1回目レバーオン】
                            if (g_current_logic_state == STATE_BONUS_HIGH_PROB && 
                                g_game_data.at_step == AT_STEP_WAIT_LEVER1) 
                            {
                                printf("AT高確: 1回目レバーオン\n");
                                g_game_data.bonus_high_prob_games--;
                                snprintf(g_game_data.info_message, sizeof(g_game_data.info_message), "AT 残り %dG", g_game_data.bonus_high_prob_games);
                                g_current_yaku = Lottery_GetResult_AT();
                                g_game_data.at_bonus_result = Lottery_CheckBonus_AT(g_current_yaku);
                                g_game_data.at_last_lottery_yaku = g_current_yaku; 
                                Reel_SetYaku(g_current_yaku);
                                snprintf(g_game_data.last_yaku_name, sizeof(g_game_data.last_yaku_name), "%s", GetYakuName(g_current_yaku));
                                Reel_StartSpinning(); 
                                g_stop_order_counter = 1;
                                g_reel_stop_flags[0] = g_reel_stop_flags[1] = g_reel_stop_flags[2] = false;
                                g_actual_push_order[0] = g_actual_push_order[1] = g_actual_push_order[2] = -1;
                                g_game_data.at_step = AT_STEP_REEL_SPIN;
                                g_main_state = STATE_REELS_SPINNING;
                            }
                            else 
                            {
                                printf("通常レバーオン (状態一致)\n");
                                DoLeverLogic();
                                g_main_state = STATE_REELS_SPINNING;
                            }
                        }
                    }
                } 

                if (e.key.keysym.sym == SDLK_z || e.key.keysym.sym == SDLK_x || e.key.keysym.sym == SDLK_c) {
                    
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

        Reel_Update();
        Presentation_Update(); 

        bool all_reels_stopped_this_frame = false;
        int diff_this_frame = 0;
        bool oshijun_success_this_frame = false;

        switch (g_main_state) {
            
            case STATE_WAIT_LEVER:
                break;

            case STATE_PLAYING_TRANSITION:
                if (Presentation_IsFinished()) {
                    printf("遷移演出 終了。\n");
                    g_current_media_state = g_current_logic_state;
                    const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
                    Presentation_Play(gRenderer, loop_path, true);
                    
                    g_main_state = STATE_REELS_SPINNING;
                }
                break;
            
            case STATE_REELS_SPINNING:
                if (!Reel_IsSpinning()) {
                    all_reels_stopped_this_frame = true; 
                    oshijun_success_this_frame = CheckOshijun(g_current_yaku, g_actual_push_order);
                    int payout = GetPayoutForYaku(g_current_yaku, oshijun_success_this_frame);
                    diff_this_frame = payout - BET_COUNT; 
                    g_game_data.total_payout_diff += diff_this_frame;
                    
                    // AT高確 1回目リール停止
                    if (g_current_logic_state == STATE_BONUS_HIGH_PROB && 
                        g_game_data.at_step == AT_STEP_REEL_SPIN)
                    {
                        printf("AT高確: 1回目リール停止\n");
                        
                        OnReelsStopped(oshijun_success_this_frame); 
                        
                        if (g_game_data.at_bonus_result == BONUS_NONE) {
                            g_main_state = STATE_WAIT_LEVER;
                            g_game_data.at_step = AT_STEP_WAIT_LEVER1;
                            if (g_game_data.bonus_high_prob_games <= 0) {
                                g_current_logic_state = STATE_AT_END;
                                g_game_data.current_state = STATE_AT_END; 
                            }
                        } else {
                            SelectPresentationPair(&g_game_data);
                            const char* video_key = GetVideoKey(g_game_data.at_pres_intro_id);
                            
                            if (PlayVideoByKey(video_key, false)) { 
                                g_main_state = STATE_AT_PRES_INTRO;
                                g_game_data.at_step = AT_STEP_LOOP_VIDEO_INTRO;
                            } else {
                                const char* loop_key = GetVideoKey(g_game_data.at_pres_loop_id);
                                PlayVideoByKey(loop_key, true); 
                                g_main_state = STATE_AT_PRES_LOOP;
                                g_game_data.at_step = AT_STEP_LOOP_VIDEO_MAIN;
                            }
                        }
                    }
                    else 
                    {
                        OnReelsStopped(oshijun_success_this_frame); 
                        g_main_state = STATE_WAIT_LEVER; 
                    }
                }
                break;
                
            case STATE_AT_PRES_INTRO: 
                if (Presentation_IsFinished()) {
                    const char* loop_key = GetVideoKey(g_game_data.at_pres_loop_id);
                    PlayVideoByKey(loop_key, true); 
                    
                    g_main_state = STATE_AT_PRES_LOOP;
                    g_game_data.at_step = AT_STEP_LOOP_VIDEO_MAIN;
                }
                break;
                
            case STATE_AT_PRES_LOOP: 
                break;
                
            case STATE_AT_JUDGE: 
            {
                Uint32 elapsed = SDL_GetTicks() - g_game_data.at_judge_video_start_time;

                // 逆回転・強制停止の対象か？
                if (g_game_data.at_bonus_result != BONUS_AT_CONTINUE && g_game_data.at_bonus_result != BONUS_NONE) {
                    
                    const Uint32 REVERSE_START_TIME = g_game_data.at_judge_video_duration_ms - 3000;
                    const Uint32 STOP_TIME = g_game_data.at_judge_video_duration_ms - 2000;

                    if (!g_game_data.at_judge_timing_reverse_triggered && elapsed >= REVERSE_START_TIME && elapsed < STOP_TIME) { 
                        Reel_StartSpinning_Reverse();
                        g_game_data.at_judge_timing_reverse_triggered = true;
                    }
                    if (!g_game_data.at_judge_timing_stop_triggered && elapsed >= STOP_TIME) {
                        
                        // (★修正) 当選内容に応じた停止形
                        if (g_game_data.at_bonus_result == BONUS_FRANXX) {
                            Reel_ForceStop(REEL_PATTERN_FRANXX_BONUS);
                        } else {
                            // DARLING, BB_EX, EPISODE は全て赤7揃い
                            Reel_ForceStop(REEL_PATTERN_RED7_MID);
                        }
                        g_game_data.at_judge_timing_stop_triggered = true;
                    }
                }
                
                if (Presentation_IsFinished()) {
                    printf("AT高確: 当落演出 終了\n");
                    
                    // (★修正) 演出終了後の状態遷移 (EX/EPを追加)
                    switch (g_game_data.at_bonus_result) {
                        case BONUS_AT_CONTINUE:
                            g_game_data.at_step = AT_STEP_WAIT_LEVER1;
                            g_main_state = STATE_WAIT_LEVER;
                            if (g_game_data.bonus_high_prob_games <= 0) {
                                 g_current_logic_state = STATE_AT_END;
                                 g_game_data.current_state = STATE_AT_END; 
                            }
                            break;
                            
                        case BONUS_DARLING:
                            g_current_logic_state = STATE_BB_HIGH_PROB; 
                            g_game_data.current_state = STATE_BB_HIGH_PROB; 
                            g_main_state = STATE_WAIT_LEVER;
                            break;
                            
                        case BONUS_FRANXX:
                            g_current_logic_state = STATE_FRANXX_BONUS;
                            g_game_data.current_state = STATE_FRANXX_BONUS; 
                            g_main_state = STATE_WAIT_LEVER;
                            break;

                        case BONUS_BB_EX: // (★追加)
                            g_current_logic_state = STATE_BB_EX;
                            g_game_data.current_state = STATE_BB_EX;
                            if(g_game_data.queued_bb_ex_payout <= 0) g_game_data.queued_bb_ex_payout = 100;
                            g_main_state = STATE_WAIT_LEVER;
                            break;

                        case BONUS_EPISODE: // (★追加)
                            g_current_logic_state = STATE_EPISODE_BONUS;
                            g_game_data.current_state = STATE_EPISODE_BONUS;
                            g_main_state = STATE_WAIT_LEVER;
                            break;
                            
                        default:
                            g_main_state = STATE_WAIT_LEVER;
                            break;
                    }
                    
                    if (g_game_data.at_bonus_result == BONUS_AT_CONTINUE) {
                         const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(STATE_BONUS_HIGH_PROB));
                         Presentation_Play(gRenderer, loop_path, true);
                    }
                }
                break;
            }
        }

        if (g_current_logic_state >= STATE_BB_INITIAL &&
            g_current_logic_state < STATE_AT_END)
        {
            AT_Update(&g_game_data, g_current_yaku, diff_this_frame, lever_on_this_frame, all_reels_stopped_this_frame);
            g_current_logic_state = g_game_data.current_state;
        }


        SDL_SetRenderDrawColor(gRenderer, 0x1E, 0x1E, 0x1E, 0xFF);
        SDL_RenderClear(gRenderer);

        Presentation_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT); 
        Reel_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);

        if (g_current_logic_state >= STATE_BB_INITIAL &&
            g_current_logic_state < STATE_AT_END)
        {
            AT_Draw(gRenderer, SCREEN_WIDTH, SCREEN_HEIGHT);
        }

        char buffer[256];
        SDL_Color white = {255, 255, 255}, yellow = {255, 255, 0}, cyan = {0, 255, 255}, red = {255, 50, 50}, green = {50, 255, 50};
        
        snprintf(buffer, sizeof(buffer), "ロジック状態: %s", AT_GetStateName(g_current_logic_state));
        draw_text(buffer, 50, 50, cyan);
        snprintf(buffer, sizeof(buffer), "メディア状態: %s", AT_GetStateName(g_current_media_state));
        draw_text(buffer, 50, 80, cyan);

        if (g_current_logic_state == STATE_BONUS_HIGH_PROB) {
            snprintf(buffer, sizeof(buffer), "AT高確Step: %d | MainState: %d", g_game_data.at_step, g_main_state);
            draw_text(buffer, 50, 110, green);
            snprintf(buffer, sizeof(buffer), "AT抽選結果: %d (%s)", g_game_data.at_bonus_result, GetYakuName(g_game_data.at_last_lottery_yaku));
            draw_text(buffer, 50, 140, green);
        }

        snprintf(buffer, sizeof(buffer), "成立役: %s", (g_main_state == STATE_WAIT_LEVER || g_main_state == STATE_AT_PRES_LOOP) ? "---" : g_game_data.last_yaku_name);
        draw_text(buffer, 50, 180, white);
        draw_text(g_game_data.info_message, 50, 210, yellow);
        
        if (g_game_data.current_state >= STATE_BB_INITIAL && g_game_data.current_state < STATE_AT_END) {
             snprintf(buffer, sizeof(buffer), "ボーナス中: %d / %d 枚", g_game_data.current_bonus_payout, g_game_data.target_bonus_payout);
             draw_text(buffer, 50, 250, white);
        }
        snprintf(buffer, sizeof(buffer), "AT高確G数: %d G", g_game_data.bonus_high_prob_games);
        draw_text(buffer, 50, 280, white);
        if (g_game_data.hiyoku_is_active) {
            snprintf(buffer, sizeof(buffer), "比翼BEATS [LV%d]: %d ST %s",
                g_game_data.hiyoku_level, g_game_data.hiyoku_st_games,
                g_game_data.hiyoku_is_frozen ? "(FROZEN)" : "");
            draw_text(buffer, 50, 310, red);
        }
        snprintf(buffer, sizeof(buffer), "総獲得枚数: %lld 枚", g_game_data.total_payout_diff);
        draw_text(buffer, 600, 50, white);
        snprintf(buffer, sizeof(buffer), "通常ストック: %d 個", g_game_data.bonus_stock_count);
        draw_text(buffer, 600, 90, white);
        snprintf(buffer, sizeof(buffer), "BB EX 予約: %d 枚", g_game_data.queued_bb_ex_payout);
        draw_text(buffer, 600, 130, white);

        if (g_current_logic_state == STATE_AT_END) {
            draw_text("シミュレーション終了", 50, 350, red);
        } else if (g_main_state == STATE_REELS_SPINNING) {
            draw_text("Z(左), X(中), C(右) でリール停止", 50, 350, yellow);
        } else if (g_main_state == STATE_WAIT_LEVER) {
            draw_text("スペースキーでレバーオン (1回目)", 50, 350, yellow);
        } else if (g_main_state == STATE_PLAYING_TRANSITION) {
            draw_text("遷移演出中 (リール停止ブロック)", 50, 350, red);
        } else if (g_main_state == STATE_AT_PRES_INTRO) {
            draw_text("AT高確: 専用演出(導入) 再生中", 50, 350, green);
        } else if (g_main_state == STATE_AT_PRES_LOOP) {
            draw_text("AT高確: スペースキーでレバーオン (2回目)", 50, 350, green);
        } else if (g_main_state == STATE_AT_JUDGE) {
            draw_text("AT高確: 当落演出 再生中", 50, 350, green);
        }

        SDL_RenderPresent(gRenderer);
        SDL_Delay(16); 
    }

    Presentation_Cleanup(); 
    Reel_Cleanup();
    MediaConfig_Cleanup(); 
    close_sdl();
    return 0;
}