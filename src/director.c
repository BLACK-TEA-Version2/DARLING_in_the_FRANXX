#include "director.h"
#include "game_data.h"
#include "lottery.h"
#include "reel.h"
#include "presentation.h"
#include "sdl_utils.h"
#include "normal.h"
#include "cz.h"
#include "at.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 内部状態定義 (Director専用) ---
typedef enum {
    DIR_STATE_IDLE,             // 通常の待機 (ループ動画再生中)
    DIR_STATE_SPINNING,         // リール回転中
    DIR_STATE_TRANSITION,       // 状態遷移演出中
    
    // AT高確率用
    DIR_STATE_AT_PRES_INTRO,    // 高確: 導入演出中
    DIR_STATE_AT_PRES_LOOP,     // 高確: 2回目レバー待ち
    
    // ジャッジ演出
    DIR_STATE_AT_JUDGE_PART1,   // Part1再生中 (逆回転あおり / ハズレ動画)
    DIR_STATE_AT_JUDGE_PART2,   // Part2再生中 (告知)
    DIR_STATE_AT_JUDGE_WAIT,    // Part2終了後 (静止画待機 -> 3回目レバー)

    // BB EX 初期枚数決定用
    DIR_STATE_BB_EX_ENTRY,      // EX: 導入(確定画面)
    DIR_STATE_BB_EX_WAIT,       // EX: 枚数告知待機 (ループ中)
    DIR_STATE_BB_EX_REVEAL,     // EX: 告知実行中 (逆回転)
    DIR_STATE_BB_EX_FINAL,      // EX: 最終確認 (リプレイ再生→静止画)

} DirectorState;

// --- 内部変数 ---
static DirectorState g_dir_state = DIR_STATE_IDLE;
static GameData g_game_data;
static YakuType g_current_yaku = YAKU_HAZURE;
static SDL_Renderer* g_renderer_ref = NULL;

// リール制御用
static int g_stop_order_counter = 1;
static bool g_reel_stop_flags[3] = {false, false, false};
static int g_actual_push_order[3] = {-1, -1, -1};

// 状態管理用
static AT_State g_current_logic_state = STATE_NORMAL; 
static AT_State g_current_media_state = STATE_NORMAL;
static bool g_is_first_game = true;

// BB EX 演出用
static int g_bb_ex_shown_payout = 0; // 現在告知済みの枚数

// --- ヘルパー関数プロトタイプ ---
static void HandleInput();
static void UpdateGameLogic(bool all_reels_stopped);
static void CheckAndPerformTransition();
static void StartSpin();
static void SelectPresentationPair(GameData* data);
static VideoType SelectJudgmentVideo(GameData* data);
static VideoType GetJudgePart2(VideoType part1);
static VideoType GetBBExShowVideo(int payout, bool is_intro);
static const char* GetLoopKeyForState(AT_State state);
static const char* GetTransitionKey(AT_State from, AT_State to);
static const char* GetVideoKey(VideoType type);
static bool PlayVideo(VideoType type, bool loop);
static bool PlayVideoByKey(const char* key, bool loop);
static void DrawDebugInfo();

// --- 公開関数 実装 ---

bool Director_Init(SDL_Renderer* renderer) {
    g_renderer_ref = renderer;
    memset(&g_game_data, 0, sizeof(GameData));
    g_game_data.current_state = STATE_NORMAL;
    g_current_logic_state = STATE_NORMAL;
    g_current_media_state = STATE_NORMAL;
    g_is_first_game = true;
    g_dir_state = DIR_STATE_IDLE;

    // デバックウィンドウの初期化
    if (!init_debug_window("Debug Info", 400, 600)) {
        fprintf(stderr, "Warning: Failed to init debug window.\n");
    }

    // 初期ループ動画再生
    const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(STATE_NORMAL));
    Presentation_Play(g_renderer_ref, loop_path, true);

    return true;
}

void Director_Cleanup() {
    // 特になし
}

void Director_Update() {
    // 1. 入力処理
    HandleInput();

    // 2. モジュール更新
    Reel_Update();
    Presentation_Update();

    // 3. 状態別更新ロジック
    switch (g_dir_state) {
        case DIR_STATE_IDLE:
            // 自動遷移チェックは行わない (レバーON時に行う)
            break;

        case DIR_STATE_SPINNING:
            if (!Reel_IsSpinning()) {
                UpdateGameLogic(true);
            }
            break;

        case DIR_STATE_TRANSITION:
            if (Presentation_IsFinished()) {
                // 遷移終了 -> 新しい状態のループへ
                g_current_media_state = g_current_logic_state;
                const char* path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
                Presentation_Play(g_renderer_ref, path, true);
                
                // ※ BB EX 突入時は特殊ステートへ
                if (g_current_logic_state == STATE_BB_EX) {
                    // まず導入動画を再生
                    if (PlayVideo(VIDEO_BB_EX_ENTRY_INTRO, false)) {
                        g_dir_state = DIR_STATE_BB_EX_ENTRY;
                        g_bb_ex_shown_payout = 0; // カウンタリセット
                    } else {
                        // 動画がない場合は即待機へ
                        g_dir_state = DIR_STATE_BB_EX_WAIT;
                    }
                } 
                else {
                    // 通常遷移後はリールが回っているので、回転中ステートへ移行
                    g_dir_state = DIR_STATE_SPINNING;
                }
            }
            break;

        // --- AT高確率 ---
        case DIR_STATE_AT_PRES_INTRO:
            if (Presentation_IsFinished()) {
                // 導入終了 -> ループへ
                PlayVideo(g_game_data.at_pres_loop_id, true);
                g_dir_state = DIR_STATE_AT_PRES_LOOP;
                g_game_data.at_step = AT_STEP_LOOP_VIDEO_MAIN;
            }
            break;

        case DIR_STATE_AT_JUDGE_PART1:
        {
            Uint32 elapsed = SDL_GetTicks() - g_game_data.at_judge_video_start_time;
            
            // 当選時は逆回転制御 (動画終了の5秒前)
            if (g_game_data.at_bonus_result != BONUS_AT_CONTINUE && g_game_data.at_bonus_result != BONUS_NONE) {
                const Uint32 REV_TIME = (g_game_data.at_judge_video_duration_ms > 2500) 
                                        ? (g_game_data.at_judge_video_duration_ms - 2500) : 0;

                if (!g_game_data.at_judge_timing_reverse_triggered && elapsed >= REV_TIME) {
                    Reel_StartSpinning_Reverse();
                    g_game_data.at_judge_timing_reverse_triggered = true;
                }
            }

            if (Presentation_IsFinished()) {
                // Part1 (or ハズレ動画) 終了
                if (g_game_data.at_bonus_result != BONUS_AT_CONTINUE && g_game_data.at_bonus_result != BONUS_NONE) {
                    // --- 当選時: 強制停止してPart2へ ---
                    if (g_game_data.at_bonus_result == BONUS_FRANXX) {
                        Reel_ForceStop(REEL_PATTERN_FRANXX_BONUS);
                    } else {
                        // DB, EX, EP は赤7揃い
                        Reel_ForceStop(REEL_PATTERN_RED7_MID);
                    }
                    
                    // Part2 再生
                    VideoType part1 = SelectJudgmentVideo(&g_game_data); // 現在のPart1を取得
                    VideoType part2 = GetJudgePart2(part1);
                    
                    PlayVideo(part2, false);
                    g_dir_state = DIR_STATE_AT_JUDGE_PART2;
                } else {
                    // --- ハズレ(継続)時: シームレスに次ゲームへ (2回目のレバーなし) ---
                    g_game_data.at_step = AT_STEP_WAIT_LEVER1;
                    
                    if (g_game_data.bonus_high_prob_games <= 0) {
                        // ゲーム数切れ -> AT終了へ (ここは停止して動画を見せる)
                        g_current_logic_state = STATE_AT_END;
                        g_game_data.current_state = STATE_AT_END;
                        g_dir_state = DIR_STATE_IDLE;
                        PlayVideoByKey(GetLoopKeyForState(STATE_AT_END), true);
                    } else {
                        // 継続 -> 動画をループに戻し、自動的に次ゲーム開始 (3回目レバー不要)
                        PlayVideoByKey(GetLoopKeyForState(STATE_BONUS_HIGH_PROB), true);
                        StartSpin();
                        // StartSpin内で g_dir_state = DIR_STATE_SPINNING になる
                    }
                }
            }
            break;
        }

        case DIR_STATE_AT_JUDGE_PART2:
            if (Presentation_IsFinished()) {
                // Part2終了 -> 静止画待機へ (当選時のみ)
                g_dir_state = DIR_STATE_AT_JUDGE_WAIT;
            }
            break;

        case DIR_STATE_AT_JUDGE_WAIT:
            // 入力待ち (3回目レバー)
            break;

        // --- BB EX 初期枚数決定 ---
        case DIR_STATE_BB_EX_ENTRY:
            if (Presentation_IsFinished()) {
                PlayVideo(VIDEO_BB_EX_ENTRY_LOOP, true);
                g_dir_state = DIR_STATE_BB_EX_WAIT;
            }
            break;

        case DIR_STATE_BB_EX_WAIT:
            // 入力待ち
            break;

        case DIR_STATE_BB_EX_REVEAL:
            if (Presentation_IsFinished()) {
                // 動画終了と同時に強制停止
                Reel_ForceStop(REEL_PATTERN_RED7_MID);
                
                // 現在の枚数に対応したループ動画を再生する
                VideoType loop_video = GetBBExShowVideo(g_bb_ex_shown_payout, false);
                PlayVideo(loop_video, true);
                
                g_dir_state = DIR_STATE_BB_EX_WAIT;
            }
            break;

        case DIR_STATE_BB_EX_FINAL:
            // 静止画待機
            break;
            
        default: break;
    }
}

void Director_Draw(SDL_Renderer* renderer, int screen_width, int screen_height) {
    // 1. メインウィンドウ描画
    SDL_SetRenderDrawColor(renderer, 0x1E, 0x1E, 0x1E, 0xFF);
    SDL_RenderClear(renderer);

    // 1. 動画
    Presentation_Draw(renderer, screen_width, screen_height);
    
    // 2. リール
    Reel_Draw(renderer, screen_width, screen_height);

    // 3. AT情報 (オーバーレイ)
    if (g_current_logic_state >= STATE_BB_INITIAL && g_current_logic_state < STATE_AT_END) {
        AT_Draw(renderer, screen_width, screen_height);
    }
    
    SDL_RenderPresent(renderer);

    // 2. デバックウィンドウ描画
    DrawDebugInfo();
}

// --- デバック情報を別ウィンドウに描画 ---
static void DrawDebugInfo() {
    if (!gDebugRenderer) return;

    // 背景クリア (黒)
    SDL_SetRenderDrawColor(gDebugRenderer, 0, 0, 0, 255);
    SDL_RenderClear(gDebugRenderer);

    SDL_Color white = {255, 255, 255};
    SDL_Color yellow = {255, 255, 0};
    SDL_Color cyan = {0, 255, 255};
    SDL_Color green = {0, 255, 0};
    SDL_Color red = {255, 50, 50};

    char buffer[256];
    int y = 10;
    int h = 25;

    // --- 基本情報 ---
    draw_text(gDebugRenderer, "=== DEBUG INFO ===", 10, y, white); y += h + 5;

    snprintf(buffer, sizeof(buffer), "Logic State: %s", AT_GetStateName(g_current_logic_state));
    draw_text(gDebugRenderer, buffer, 10, y, cyan); y += h;

    snprintf(buffer, sizeof(buffer), "Media State: %s", AT_GetStateName(g_current_media_state));
    draw_text(gDebugRenderer, buffer, 10, y, cyan); y += h;

    snprintf(buffer, sizeof(buffer), "Dir State: %d", g_dir_state);
    draw_text(gDebugRenderer, buffer, 10, y, white); y += h;

    y += 10; // スペーサー

    // --- 役情報 ---
    snprintf(buffer, sizeof(buffer), "Last Yaku: %s", g_game_data.last_yaku_name);
    draw_text(gDebugRenderer, buffer, 10, y, yellow); y += h;

    // --- AT/ボーナス情報 ---
    if (g_game_data.current_state >= STATE_BB_INITIAL && g_game_data.current_state < STATE_AT_END) {
        snprintf(buffer, sizeof(buffer), "Payout: %d / %d", g_game_data.current_bonus_payout, g_game_data.target_bonus_payout);
        draw_text(gDebugRenderer, buffer, 10, y, green); y += h;
        
        snprintf(buffer, sizeof(buffer), "Total Payout: %lld", g_game_data.total_payout_diff);
        draw_text(gDebugRenderer, buffer, 10, y, green); y += h;
    }

    // --- AT高確率情報 ---
    if (g_current_logic_state == STATE_BONUS_HIGH_PROB) {
        y += 10;
        snprintf(buffer, sizeof(buffer), "--- AT High Prob ---");
        draw_text(gDebugRenderer, buffer, 10, y, white); y += h;

        snprintf(buffer, sizeof(buffer), "Games Left: %d", g_game_data.bonus_high_prob_games);
        draw_text(gDebugRenderer, buffer, 10, y, red); y += h;

        snprintf(buffer, sizeof(buffer), "AT Result: %d", g_game_data.at_bonus_result);
        draw_text(gDebugRenderer, buffer, 10, y, red); y += h;
        
        snprintf(buffer, sizeof(buffer), "Lottery Yaku: %s", GetYakuName(g_game_data.at_last_lottery_yaku));
        draw_text(gDebugRenderer, buffer, 10, y, red); y += h;
    }

    // --- BB EX情報 ---
    if (g_current_logic_state == STATE_BB_EX) {
        y += 10;
        snprintf(buffer, sizeof(buffer), "--- BB EX ---");
        draw_text(gDebugRenderer, buffer, 10, y, white); y += h;

        snprintf(buffer, sizeof(buffer), "Total Queued: %d", g_game_data.queued_bb_ex_payout);
        draw_text(gDebugRenderer, buffer, 10, y, yellow); y += h;

        snprintf(buffer, sizeof(buffer), "Shown: %d", g_bb_ex_shown_payout);
        draw_text(gDebugRenderer, buffer, 10, y, yellow); y += h;
    }

    // --- その他 ---
    y += 10;
    snprintf(buffer, sizeof(buffer), "Bonus Stock: %d", g_game_data.bonus_stock_count);
    draw_text(gDebugRenderer, buffer, 10, y, white); y += h;

    draw_text(gDebugRenderer, g_game_data.info_message, 10, y, white); y += h;

    SDL_RenderPresent(gDebugRenderer);
}


// --- 内部実装 ---

static void HandleInput() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) exit(0); 

        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_SPACE) {
                // --- レバーON処理 ---
                
                // 1. 通常/AT待機中
                if (g_dir_state == DIR_STATE_IDLE) {
                    
                    // 状態不一致なら遷移演出へ (ここでのみ遷移を許可)
                    if (g_current_logic_state != g_current_media_state) {
                        CheckAndPerformTransition();
                    }
                    // 一致なら通常回転開始
                    else {
                        StartSpin();
                    }
                }
                // 2. AT高確 2回目レバー (当選時のみここに来る)
                else if (g_dir_state == DIR_STATE_AT_PRES_LOOP) {
                    VideoType judgePart1 = SelectJudgmentVideo(&g_game_data);
                    if (PlayVideo(judgePart1, false)) {
                        g_dir_state = DIR_STATE_AT_JUDGE_PART1; // Part1開始
                        g_game_data.at_step = AT_STEP_JUDGE_VIDEO;
                        g_game_data.at_judge_video_start_time = SDL_GetTicks();
                        g_game_data.at_judge_timing_reverse_triggered = false;
                        g_game_data.at_judge_timing_stop_triggered = false;
                    }
                }
                // 3. AT高確 3回目レバー (当選時の告知後)
                else if (g_dir_state == DIR_STATE_AT_JUDGE_WAIT) {
                    switch (g_game_data.at_bonus_result) {
                        case BONUS_DARLING:
                            g_current_logic_state = STATE_BB_HIGH_PROB;
                            g_game_data.current_state = STATE_BB_HIGH_PROB;
                            g_dir_state = DIR_STATE_IDLE; // 遷移
                            break;
                        case BONUS_FRANXX:
                            g_current_logic_state = STATE_FRANXX_BONUS;
                            g_game_data.current_state = STATE_FRANXX_BONUS;
                            g_dir_state = DIR_STATE_IDLE; // 遷移
                            break;
                        case BONUS_BB_EX:
                            g_current_logic_state = STATE_BB_EX;
                            g_game_data.current_state = STATE_BB_EX;
                            if(g_game_data.queued_bb_ex_payout <= 0) g_game_data.queued_bb_ex_payout = 100;
                            g_dir_state = DIR_STATE_IDLE; // 遷移
                            break;
                        case BONUS_EPISODE:
                            g_current_logic_state = STATE_EPISODE_BONUS;
                            g_game_data.current_state = STATE_EPISODE_BONUS;
                            g_dir_state = DIR_STATE_IDLE; // 遷移
                            break;
                        default:
                            // ハズレ時など通常はここに来ないが、念のため
                            g_dir_state = DIR_STATE_IDLE;
                            break;
                    }
                }
                
                // 4. BB EX 枚数告知待機
                else if (g_dir_state == DIR_STATE_BB_EX_WAIT) {
                    int remaining = g_game_data.queued_bb_ex_payout - g_bb_ex_shown_payout;
                    
                    if (remaining <= 0) {
                        // 完了 -> 最終確認へ (リプレイ再生)
                        VideoType intro = GetBBExShowVideo(g_bb_ex_shown_payout, true);
                        PlayVideo(intro, false);
                        g_dir_state = DIR_STATE_BB_EX_FINAL;
                    } 
                    else {
                        // 次の加算枚数を決定
                        int next_add = 0;
                        if (g_bb_ex_shown_payout == 0) {
                            next_add = 200; // 初回200
                        } else if (g_bb_ex_shown_payout < 1000) {
                            next_add = 100; // 1000未満は +100
                        } else {
                            next_add = 1000; // 1000以上は +1000
                        }
                        
                        // 上限クリップ
                        if (g_bb_ex_shown_payout + next_add > g_game_data.queued_bb_ex_payout) {
                            next_add = g_game_data.queued_bb_ex_payout - g_bb_ex_shown_payout;
                        }

                        g_bb_ex_shown_payout += next_add;
                        
                        // 告知開始 (即逆回転)
                        Reel_StartSpinning_Reverse();
                        PlayVideo(GetBBExShowVideo(g_bb_ex_shown_payout, true), false);
                        g_dir_state = DIR_STATE_BB_EX_REVEAL;
                    }
                }
                // 5. BB EX 最終確認 (静止画待機)
                else if (g_dir_state == DIR_STATE_BB_EX_FINAL) {
                    if (Presentation_IsFinished()) { 
                        // ゲーム開始
                        const char* loop_path = MediaConfig_GetPath(GetLoopKeyForState(STATE_BB_EX));
                        Presentation_Play(g_renderer_ref, loop_path, true);
                        g_dir_state = DIR_STATE_IDLE;
                        Reel_ForceStop(REEL_PATTERN_NONE); 
                    }
                }
            }
            
            // --- リール停止 ---
            if (g_dir_state == DIR_STATE_SPINNING) {
                int stop_idx = -1;
                if (e.key.keysym.sym == SDLK_z) stop_idx = 0;
                if (e.key.keysym.sym == SDLK_x) stop_idx = 1;
                if (e.key.keysym.sym == SDLK_c) stop_idx = 2;
                
                if (stop_idx != -1 && !g_reel_stop_flags[stop_idx]) {
                    Reel_RequestStop(stop_idx, g_stop_order_counter);
                    g_reel_stop_flags[stop_idx] = true;
                    g_actual_push_order[g_stop_order_counter - 1] = stop_idx;
                    g_stop_order_counter++;
                }
            }
        }
    }
}

static void CheckAndPerformTransition() {
    const char* trans_key = GetTransitionKey(g_current_media_state, g_current_logic_state);
    
    if (PlayVideoByKey(trans_key, false)) {
        StartSpin(); 
        g_dir_state = DIR_STATE_TRANSITION;
    } else {
        // 動画がない -> 即切り替え
        g_current_media_state = g_current_logic_state;
        const char* path = MediaConfig_GetPath(GetLoopKeyForState(g_current_media_state));
        Presentation_Play(g_renderer_ref, path, true);
        
        StartSpin();
    }
}

static void StartSpin() {
    // 1. 抽選
    if (g_is_first_game) {
        AT_Init(&g_game_data);
        g_current_yaku = YAKU_HAZURE;
        g_is_first_game = false;
        g_current_logic_state = g_game_data.current_state;
    } else {
        if (g_game_data.current_state == STATE_BONUS_HIGH_PROB) {
            // AT高確
            g_game_data.bonus_high_prob_games--;
            g_current_yaku = Lottery_GetResult_AT();
            g_game_data.at_bonus_result = Lottery_CheckBonus_AT(g_current_yaku);
            g_game_data.at_last_lottery_yaku = g_current_yaku;
            g_game_data.at_step = AT_STEP_REEL_SPIN;
        }
        else if (g_game_data.current_state == STATE_CZ || 
                 g_game_data.current_state == STATE_FRANXX_BONUS) {
            g_current_yaku = Lottery_GetResult_FranxxHighProb();
        }
        else {
            g_current_yaku = Lottery_GetResult_Normal();
        }
    }

    // 2. リール始動
    Reel_SetYaku(g_current_yaku);
    snprintf(g_game_data.last_yaku_name, sizeof(g_game_data.last_yaku_name), "%s", GetYakuName(g_current_yaku));
    Reel_StartSpinning();
    
    g_stop_order_counter = 1;
    g_reel_stop_flags[0] = false;
    g_reel_stop_flags[1] = false;
    g_reel_stop_flags[2] = false;
    g_actual_push_order[0] = -1;
    g_actual_push_order[1] = -1;
    g_actual_push_order[2] = -1;
    
    if (g_dir_state != DIR_STATE_TRANSITION) {
        g_dir_state = DIR_STATE_SPINNING;
    }
}

static void UpdateGameLogic(bool all_reels_stopped) {
    bool oshijun_success = CheckOshijun(g_current_yaku, g_actual_push_order);
    int payout = GetPayoutForYaku(g_current_yaku, oshijun_success);
    int diff = payout - 3; 
    g_game_data.total_payout_diff += diff;

    // AT高確率時の1回目停止
    if (g_current_logic_state == STATE_BONUS_HIGH_PROB && g_game_data.at_step == AT_STEP_REEL_SPIN) {
        if (g_game_data.at_bonus_result == BONUS_NONE) {
            // ハズレ (ボーナス抽選なし) -> 即次ゲームへ
            g_dir_state = DIR_STATE_IDLE;
            g_game_data.at_step = AT_STEP_WAIT_LEVER1;
            if (g_game_data.bonus_high_prob_games <= 0) {
                g_current_logic_state = STATE_AT_END;
                g_game_data.current_state = STATE_AT_END;
            }
        } else {
            // 当選 -> 導入演出へ
            SelectPresentationPair(&g_game_data);
            PlayVideo(g_game_data.at_pres_intro_id, false);
            g_dir_state = DIR_STATE_AT_PRES_INTRO;
            g_game_data.at_step = AT_STEP_LOOP_VIDEO_INTRO;
        }
    } else {
        // 通常停止
        if (g_current_logic_state == STATE_CZ) CZ_Update(&g_game_data, g_current_yaku);
        else if (g_current_logic_state == STATE_NORMAL) Normal_Update(&g_game_data, g_current_yaku);
        
        // ATロジック (毎フレーム更新関数だが、停止時処理として呼び出す)
        if (g_current_logic_state >= STATE_BB_INITIAL && g_current_logic_state < STATE_AT_END) {
            AT_Update(&g_game_data, g_current_yaku, diff, false, true);
            g_current_logic_state = g_game_data.current_state;
        }
        
        g_dir_state = DIR_STATE_IDLE;
    }
}

// --- Helper wrappers ---
static bool PlayVideo(VideoType type, bool loop) {
    const char* key = GetVideoKey(type);
    return PlayVideoByKey(key, loop);
}

static bool PlayVideoByKey(const char* key, bool loop) {
    if (!key) return false;
    const char* path = MediaConfig_GetPath(key);
    if (path && Presentation_Play(g_renderer_ref, path, loop)) {
        return true;
    }
    return false;
}

static VideoType GetBBExShowVideo(int payout, bool is_intro) {
    VideoType base = VIDEO_NONE;
    switch (payout) {
        case 200: base = VIDEO_BB_EX_SHOW_200_INTRO; break;
        case 300: base = VIDEO_BB_EX_SHOW_300_INTRO; break;
        case 400: base = VIDEO_BB_EX_SHOW_400_INTRO; break;
        case 500: base = VIDEO_BB_EX_SHOW_500_INTRO; break;
        case 600: base = VIDEO_BB_EX_SHOW_600_INTRO; break;
        case 700: base = VIDEO_BB_EX_SHOW_700_INTRO; break;
        case 800: base = VIDEO_BB_EX_SHOW_800_INTRO; break;
        case 900: base = VIDEO_BB_EX_SHOW_900_INTRO; break;
        case 1000: base = VIDEO_BB_EX_SHOW_1000_INTRO; break;
        case 2000: base = VIDEO_BB_EX_SHOW_2000_INTRO; break;
        case 3000: base = VIDEO_BB_EX_SHOW_3000_INTRO; break;
        default: base = VIDEO_BB_EX_SHOW_200_INTRO; break;
    }
    return is_intro ? base : (VideoType)(base + 1);
}

// Helper: Part1のIDからPart2のIDを取得
static VideoType GetJudgePart2(VideoType part1) {
    // PART1の次は定義上 PART2 であることを利用
    return (VideoType)(part1 + 1);
}

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
        
        case VIDEO_JUDGE_DARLING_1_PART1: return "JUDGE_DARLING_1_PART1";
        case VIDEO_JUDGE_DARLING_1_PART2: return "JUDGE_DARLING_1_PART2";
        case VIDEO_JUDGE_DARLING_2_PART1: return "JUDGE_DARLING_2_PART1";
        case VIDEO_JUDGE_DARLING_2_PART2: return "JUDGE_DARLING_2_PART2";
        case VIDEO_JUDGE_DARLING_3_PART1: return "JUDGE_DARLING_3_PART1";
        case VIDEO_JUDGE_DARLING_3_PART2: return "JUDGE_DARLING_3_PART2";

        case VIDEO_JUDGE_FRANXX_1_PART1:  return "JUDGE_FRANXX_1_PART1";
        case VIDEO_JUDGE_FRANXX_1_PART2:  return "JUDGE_FRANXX_1_PART2";
        case VIDEO_JUDGE_FRANXX_2_PART1:  return "JUDGE_FRANXX_2_PART1";
        case VIDEO_JUDGE_FRANXX_2_PART2:  return "JUDGE_FRANXX_2_PART2";
        case VIDEO_JUDGE_FRANXX_3_PART1:  return "JUDGE_FRANXX_3_PART1";
        case VIDEO_JUDGE_FRANXX_3_PART2:  return "JUDGE_FRANXX_3_PART2";
        
        case VIDEO_BB_EX_ENTRY_INTRO: return "BB_EX_ENTRY_INTRO";
        case VIDEO_BB_EX_ENTRY_LOOP:  return "BB_EX_ENTRY_LOOP";
        
        case VIDEO_BB_EX_SHOW_200_INTRO: return "BB_EX_SHOW_200_INTRO";
        case VIDEO_BB_EX_SHOW_200_LOOP:  return "BB_EX_SHOW_200_LOOP";
        case VIDEO_BB_EX_SHOW_300_INTRO: return "BB_EX_SHOW_300_INTRO";
        case VIDEO_BB_EX_SHOW_300_LOOP:  return "BB_EX_SHOW_300_LOOP";
        case VIDEO_BB_EX_SHOW_400_INTRO: return "BB_EX_SHOW_400_INTRO";
        case VIDEO_BB_EX_SHOW_400_LOOP:  return "BB_EX_SHOW_400_LOOP";
        case VIDEO_BB_EX_SHOW_500_INTRO: return "BB_EX_SHOW_500_INTRO";
        case VIDEO_BB_EX_SHOW_500_LOOP:  return "BB_EX_SHOW_500_LOOP";
        case VIDEO_BB_EX_SHOW_600_INTRO: return "BB_EX_SHOW_600_INTRO";
        case VIDEO_BB_EX_SHOW_600_LOOP:  return "BB_EX_SHOW_600_LOOP";
        case VIDEO_BB_EX_SHOW_700_INTRO: return "BB_EX_SHOW_700_INTRO";
        case VIDEO_BB_EX_SHOW_700_LOOP:  return "BB_EX_SHOW_700_LOOP";
        case VIDEO_BB_EX_SHOW_800_INTRO: return "BB_EX_SHOW_800_INTRO";
        case VIDEO_BB_EX_SHOW_800_LOOP:  return "BB_EX_SHOW_800_LOOP";
        case VIDEO_BB_EX_SHOW_900_INTRO: return "BB_EX_SHOW_900_INTRO";
        case VIDEO_BB_EX_SHOW_900_LOOP:  return "BB_EX_SHOW_900_LOOP";
        case VIDEO_BB_EX_SHOW_1000_INTRO: return "BB_EX_SHOW_1000_INTRO";
        case VIDEO_BB_EX_SHOW_1000_LOOP:  return "BB_EX_SHOW_1000_LOOP";
        case VIDEO_BB_EX_SHOW_2000_INTRO: return "BB_EX_SHOW_2000_INTRO";
        case VIDEO_BB_EX_SHOW_2000_LOOP:  return "BB_EX_SHOW_2000_LOOP";
        case VIDEO_BB_EX_SHOW_3000_INTRO: return "BB_EX_SHOW_3000_INTRO";
        case VIDEO_BB_EX_SHOW_3000_LOOP:  return "BB_EX_SHOW_3000_LOOP";

        default: return NULL;
    }
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
        default: return "LOOP_VIDEO_NORMAL"; 
    }
}

static const char* GetTransitionKey(AT_State from, AT_State to) {
    static char buffer[256];
    const char *f = "NORMAL", *t = "NORMAL";
    
    switch(from) {
        case STATE_NORMAL: f = "NORMAL"; break;
        case STATE_CZ: f = "CZ"; break;
        case STATE_BB_INITIAL: f = "BB_INITIAL"; break;
        case STATE_BONUS_HIGH_PROB: f = "BONUS_HIGH_PROB"; break;
        case STATE_BB_HIGH_PROB: f = "BB_HIGH_PROB"; break;
        case STATE_FRANXX_BONUS: f = "FRANXX_BONUS"; break;
        case STATE_BB_EX: f = "BB_EX"; break;
        case STATE_HIYOKU_BEATS: f = "HIYOKU_BEATS"; break;
        case STATE_EPISODE_BONUS: f = "EPISODE_BONUS"; break;
        case STATE_TSUREDASHI: f = "TSUREDASHI"; break;
        case STATE_AT_END: f = "AT_END"; break;
        default: break;
    }
    switch(to) {
        case STATE_NORMAL: t = "NORMAL"; break;
        case STATE_CZ: t = "CZ"; break;
        case STATE_BB_INITIAL: t = "BB_INITIAL"; break;
        case STATE_BONUS_HIGH_PROB: t = "BONUS_HIGH_PROB"; break;
        case STATE_BB_HIGH_PROB: t = "BB_HIGH_PROB"; break;
        case STATE_FRANXX_BONUS: t = "FRANXX_BONUS"; break;
        case STATE_BB_EX: t = "BB_EX"; break;
        case STATE_HIYOKU_BEATS: t = "HIYOKU_BEATS"; break;
        case STATE_EPISODE_BONUS: t = "EPISODE_BONUS"; break;
        case STATE_TSUREDASHI: t = "TSUREDASHI"; break;
        case STATE_AT_END: t = "AT_END"; break;
        default: break;
    }
    
    snprintf(buffer, sizeof(buffer), "TRANSITION_%s_TO_%s", f, t);
    return buffer;
}

static void SelectPresentationPair(GameData* data) {
    int k = rand() % 100; 
    VideoType intro, loop;
    if (data->at_bonus_result == BONUS_AT_CONTINUE) {
        if (k < 40) { intro = VIDEO_AT_PRES_A_INTRO; loop = VIDEO_AT_PRES_A_LOOP; }
        else if (k < 70) { intro = VIDEO_AT_PRES_B_INTRO; loop = VIDEO_AT_PRES_B_LOOP; }
        else if (k < 90) { intro = VIDEO_AT_PRES_C_INTRO; loop = VIDEO_AT_PRES_C_LOOP; }
        else { intro = VIDEO_AT_PRES_D_INTRO; loop = VIDEO_AT_PRES_D_LOOP; }
    } else { 
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
        return (VideoType)(VIDEO_JUDGE_LOSE_1 + k);
    } else if (data->at_bonus_result == BONUS_FRANXX) {
        return (VideoType)(VIDEO_JUDGE_FRANXX_1_PART1 + (k*2)); 
    } else { 
        return (VideoType)(VIDEO_JUDGE_DARLING_1_PART1 + (k*2)); 
    }
}