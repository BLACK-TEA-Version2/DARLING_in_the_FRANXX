#include "reel.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <math.h>

// ===================== 設定 =====================
#ifndef PAYLINE_ROW
#define PAYLINE_ROW 1 // 1=中段
#endif
#define MAX_SLIP 4
#define EPS 0.0001f
#define SYMBOL_NONE ((SymbolType)99)

// ===================== 内部状態 =====================
static SDL_Texture* gSymbolTextures[SYMBOL_COUNT];
static SDL_Texture* gReelBackgroundTexture = NULL; // (★) リール背景用テクスチャ
static float gReelPos[3]        = {0.0f, 0.0f, 0.0f};
static bool  gIsSpinning[3]     = {false, false, false};
static bool  gIsStopping[3]     = {false, false, false};
static bool  gIsSpinningBackward[3] = {false, false, false}; // (★追加) 逆回転フラグ
static int   gTargetStopGridM[3]= {0, 0, 0};
static float gTargetPos[3]      = {0.0f, 0.0f, 0.0f};
static YakuType gCurrentYaku = YAKU_HAZURE;
static int      gStopOrder[3] = {0, 0, 0};
static int gStoppedGridM[3] = {-1, -1, -1};

// (★追加) 強制停止パターンの定義 (グリッドインデックス)
// (注: グリッドインデックス m は「枠上」の図柄インデックスを指す)
// 中段 AKA_7 (インデックス 6) にするには、グリッド 5 (5+1=6) を指定
static const int PATTERN_RED7_MID_GRID[3] = { 5, 5, 5 }; 
// 左: AKA_7(6) -> グリッド 5
// 中: AKA_7(6) -> グリッド 5
// 右: UE(7)/NAKA(8) -> 枠上UE(7) -> グリッド 7
static const int PATTERN_FRANXX_BONUS_GRID[3] = { 5, 5, 7 };

// ===================== リール配列 =====================
SymbolType left_reel[SYMBOLS_PER_REEL] = {
    SYMBOL_AO, CHERRY, BELL, AKA_7, REPLAY, SYMBOL_AO, CHERRY,
    BELL, BAR, REPLAY, SYMBOL_AO, CHERRY, BELL, SYMBOL_PURPLE,
    REPLAY, SYMBOL_AO, NAKA, BELL, SYMBOL_PURPLE, REPLAY
};
SymbolType center_reel[SYMBOLS_PER_REEL] = {
    BELL, SYMBOL_PURPLE, CHERRY, AKA_7, REPLAY, BELL, SYMBOL_PURPLE,
    CHERRY, UE, REPLAY, BELL, SYMBOL_PURPLE, CHERRY, BAR,
    REPLAY, BELL, SYMBOL_PURPLE, CHERRY, NAKA, REPLAY
};
SymbolType right_reel[SYMBOLS_PER_REEL] = {
    SYMBOL_AO, BELL, REPLAY, AKA_7, CHERRY, SYMBOL_AO, BELL,
    REPLAY, BAR, CHERRY, SYMBOL_AO, BELL, REPLAY, UE,
    NAKA, SHITA, BELL, REPLAY, SYMBOL_PURPLE, CHERRY
};
SymbolType* all_reels[3] = { left_reel, center_reel, right_reel };

// ===================== ユーティリティ =====================
static inline float Wrap(float v, float length) {
    v = fmodf(v, length);
    if (v < 0.0f) v += length;
    return v;
}

static inline float NextGridPosForward(float pos, float reel_length) {
    float y_off = fmodf(pos, (float)SYMBOL_HEIGHT);
    if (y_off < 0.0f) y_off += (float)SYMBOL_HEIGHT;

    int m = (int)floorf(pos / (float)SYMBOL_HEIGHT);
    if (fabsf(y_off) <= EPS) {
        m = m - 1;
    }
    if (m < 0) m += SYMBOLS_PER_REEL;

    float target = (float)(m * SYMBOL_HEIGHT);
    return Wrap(target, reel_length);
}

static inline void GetSymbolIndices(int grid_m, int* ue, int* naka, int* shita) {
    *ue   = (grid_m + 0 + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;
    *naka = (grid_m + 1 + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;
    *shita = (grid_m + 2 + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;
}

static inline SymbolType GetSymbol(int reel_index, int symbol_index) {
    return all_reels[reel_index][symbol_index];
}

static inline int IndexAtPayline(int grid_m) {
    int idx = grid_m + PAYLINE_ROW;
    idx %= SYMBOLS_PER_REEL;
    if (idx < 0) idx += SYMBOLS_PER_REEL;
    return idx;
}

static bool GetStoppedSymbols(int reel_index, SymbolType* out_ue, SymbolType* out_naka, SymbolType* out_shita) {
    int m = gStoppedGridM[reel_index];
    if (m == -1) return false;

    int ue_idx, naka_idx, shita_idx;
    GetSymbolIndices(m, &ue_idx, &naka_idx, &shita_idx);
    *out_ue   = GetSymbol(reel_index, ue_idx);
    *out_naka = GetSymbol(reel_index, naka_idx);
    *out_shita = GetSymbol(reel_index, shita_idx);
    return true;
}

// ===================== リール制御の心臓部 =====================
static bool CheckForKoyakuCompletion(int reel_index, int target_grid_m,
                                     bool rL_stopped, SymbolType rL_ue, SymbolType rL_naka, SymbolType rL_shita,
                                     bool rC_stopped, SymbolType rC_ue, SymbolType rC_naka, SymbolType rC_shita,
                                     bool rR_stopped, SymbolType rR_ue, SymbolType rR_naka, SymbolType rR_shita)
{
    int ue_idx, naka_idx, shita_idx;
    GetSymbolIndices(target_grid_m, &ue_idx, &naka_idx, &shita_idx);
    SymbolType ue   = GetSymbol(reel_index, ue_idx);
    SymbolType naka = GetSymbol(reel_index, naka_idx);
    SymbolType shita = GetSymbol(reel_index, shita_idx);

    SymbolType L_u = (reel_index == 0) ? ue    : (rL_stopped ? rL_ue   : SYMBOL_NONE);
    SymbolType L_n = (reel_index == 0) ? naka  : (rL_stopped ? rL_naka : SYMBOL_NONE);
    SymbolType L_s = (reel_index == 0) ? shita : (rL_stopped ? rL_shita : SYMBOL_NONE);
    SymbolType C_u = (reel_index == 1) ? ue    : (rC_stopped ? rC_ue   : SYMBOL_NONE);
    SymbolType C_n = (reel_index == 1) ? naka  : (rC_stopped ? rC_naka : SYMBOL_NONE);
    SymbolType C_s = (reel_index == 1) ? shita : (rC_stopped ? rC_shita : SYMBOL_NONE);
    SymbolType R_u = (reel_index == 2) ? ue    : (rR_stopped ? rR_ue   : SYMBOL_NONE);
    SymbolType R_n = (reel_index == 2) ? naka  : (rR_stopped ? rR_naka : SYMBOL_NONE);
    SymbolType R_s = (reel_index == 2) ? shita : (rR_stopped ? rR_shita : SYMBOL_NONE);

    if (L_n == REPLAY && C_n == REPLAY && R_n == REPLAY) return true;
    if (L_s == REPLAY && C_s == REPLAY && R_s == REPLAY) return true;
    if (L_s == REPLAY && C_n == REPLAY && R_u == REPLAY) return true;
    if (L_u == BELL && C_u == BELL && R_u == BELL) return true;
    if (L_n == BELL && C_n == BELL && R_n == BELL) return true;
    if ((L_u == CHERRY || L_u == NAKA || L_s == CHERRY || L_s == NAKA) && (R_n == BELL)) return true;

    return false;
}

static inline bool CheckBellPullIn(SymbolType naka_sym) {
    return (naka_sym == BELL);
}

static bool CheckYakuMatch(int reel_index, int stop_order, int target_grid_m) {

    int ue_idx, naka_idx, shita_idx;
    GetSymbolIndices(target_grid_m, &ue_idx, &naka_idx, &shita_idx);
    SymbolType ue   = GetSymbol(reel_index, ue_idx);
    SymbolType naka = GetSymbol(reel_index, naka_idx);
    SymbolType shita = GetSymbol(reel_index, shita_idx);

    SymbolType rL_ue, rL_naka, rL_shita, rC_ue, rC_naka, rC_shita, rR_ue, rR_naka, rR_shita;
    bool rL_stopped = GetStoppedSymbols(0, &rL_ue, &rL_naka, &rL_shita);
    bool rC_stopped = GetStoppedSymbols(1, &rC_ue, &rC_naka, &rC_shita);
    bool rR_stopped = GetStoppedSymbols(2, &rR_ue, &rR_naka, &rR_shita);

    switch (gCurrentYaku) {
        // (★) YAKU_FAKE_AKA_7 削除
        case YAKU_HP_REVERSE_FRANXX:
        case YAKU_HP_REVERSE_STRONG_FRANXX:
        case YAKU_HP_REVERSE_STRELITZIA:
        {
            if (stop_order == 1 && reel_index != 2) {
                goto case_hazure;
            }
            if (gCurrentYaku == YAKU_HP_REVERSE_FRANXX) {
                if (reel_index == 0) return (ue == CHERRY || shita == CHERRY);
                if (reel_index == 1) return true;
                if (reel_index == 2) {
                    bool pat1 = (ue == NAKA && naka == SHITA);
                    bool pat2 = (naka == UE && shita == NAKA);
                    return (pat1 || pat2);
                }
            }
            else if (gCurrentYaku == YAKU_HP_REVERSE_STRONG_FRANXX) {
                if (reel_index == 0) return (naka == CHERRY);
                if (reel_index == 1) return true;
                if (reel_index == 2) {
                    bool pat1 = (ue == NAKA && naka == SHITA);
                    bool pat2 = (naka == UE && shita == NAKA);
                    return (pat1 || pat2);
                }
            }
            else if (gCurrentYaku == YAKU_HP_REVERSE_STRELITZIA) {
                if (reel_index == 0) return (ue == CHERRY || shita == CHERRY);
                if (reel_index == 1) return true;
                if (reel_index == 2) return (ue == UE && naka == NAKA && shita == SHITA);
            }
            goto case_hazure;
        }
        case YAKU_CHERRY:
            if (reel_index == 0) return (ue == CHERRY || ue == NAKA || shita == CHERRY || shita == NAKA);
            if (reel_index == 1) return true;
            if (reel_index == 2) return (naka == BELL);
            break;
        case YAKU_COMMON_BELL:
            return (ue == BELL);
        case YAKU_OSHIJUN_BELL_LMR:
            if (stop_order == 1) return (reel_index == 0) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 1) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 2) && CheckBellPullIn(naka);
            break;
        case YAKU_OSHIJUN_BELL_LRM:
            if (stop_order == 1) return (reel_index == 0) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 2) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 1) && CheckBellPullIn(naka);
            break;
        case YAKU_OSHIJUN_BELL_MLR:
            if (stop_order == 1) return (reel_index == 1) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 0) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 2) && CheckBellPullIn(naka);
            break;
        case YAKU_OSHIJUN_BELL_MRL:
            if (stop_order == 1) return (reel_index == 1) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 2) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 0) && CheckBellPullIn(naka);
            break;
        case YAKU_OSHIJUN_BELL_RLM:
            if (stop_order == 1) return (reel_index == 2) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 0) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 1) && CheckBellPullIn(naka);
            break;
        case YAKU_OSHIJUN_BELL_RML:
            if (stop_order == 1) return (reel_index == 2) && CheckBellPullIn(naka);
            if (stop_order == 2) return (reel_index == 1) && CheckBellPullIn(naka);
            if (stop_order == 3) return (reel_index == 0) && CheckBellPullIn(naka);
            break;
        case YAKU_REPLAY: {
            bool can_naka = (naka == REPLAY);
            if (rL_stopped && rL_naka != REPLAY) can_naka = false;
            if (rC_stopped && rC_naka != REPLAY) can_naka = false;
            if (rR_stopped && rR_naka != REPLAY) can_naka = false;
            if (can_naka) return true;
            bool can_shita = (shita == REPLAY);
            if (rL_stopped && rL_shita != REPLAY) can_shita = false;
            if (rC_stopped && rC_shita != REPLAY) can_shita = false;
            if (rR_stopped && rR_shita != REPLAY) can_shita = false;
            if (can_shita) return true;
            bool can_migiagari = true;
            if (reel_index == 0 && shita != REPLAY) can_migiagari = false;
            if (reel_index == 1 && naka != REPLAY)  can_migiagari = false;
            if (reel_index == 2 && ue != REPLAY)    can_migiagari = false;
            if (rL_stopped && rL_shita != REPLAY) can_migiagari = false;
            if (rC_stopped && rC_naka  != REPLAY) can_migiagari = false;
            if (rR_stopped && rR_ue    != REPLAY) can_migiagari = false;
            if (can_migiagari) return true;
            return false;
        }
        case YAKU_FRANXX_ME:
            if (reel_index == 0) return (ue == CHERRY || shita == CHERRY);
            if (reel_index == 1) return true;
            if (reel_index == 2) {
                bool pat1 = (ue == NAKA && naka == SHITA);
                bool pat2 = (naka == UE && shita == NAKA);
                return (pat1 || pat2);
            }
            break;
        case YAKU_CHANCE_ME: {
            bool can_pull_in = true;
            if (reel_index == 0 && naka != REPLAY) can_pull_in = false;
            if (rL_stopped && rL_naka != REPLAY)   can_pull_in = false;
            if (reel_index == 1 && naka != REPLAY) can_pull_in = false;
            if (rC_stopped && rC_naka != REPLAY)   can_pull_in = false;
            if (reel_index == 2 && naka != BELL)   can_pull_in = false;
            if (rR_stopped && rR_naka != BELL)     can_pull_in = false;
            return can_pull_in;
        }
        case YAKU_STRELITZIA_ME:
             if (reel_index == 0) return (ue == CHERRY || shita == CHERRY);
             if (reel_index == 1) return true;
             if (reel_index == 2) return (ue == UE && naka == NAKA && shita == SHITA);
             break;
        case YAKU_HAZURE:
        case_hazure:
        {
            if (CheckForKoyakuCompletion(reel_index, target_grid_m,
                                         rL_stopped, rL_ue, rL_naka, rL_shita,
                                         rC_stopped, rC_ue, rC_naka, rC_shita,
                                         rR_stopped, rR_ue, rR_naka, rR_shita)) {
                return false;
            }
            if (reel_index == 2) {
                int count = 0;
                if (ue == UE || ue == NAKA || ue == SHITA) count++;
                if (naka == UE || naka == NAKA || naka == SHITA) count++;
                if (shita == UE || shita == NAKA || shita == SHITA) count++;
                if (count >= 2) {
                    return false;
                }
            }
            if (reel_index == 0) {
                if (ue == CHERRY || ue == NAKA ||
                    naka == CHERRY || naka == NAKA ||
                    shita == CHERRY || shita == NAKA)
                {
                    return false;
                }
            }
            return true;
        }
        default:
            goto case_hazure;
    }
    return false;
}


// ===================== 公開関数 =====================

bool Reel_Init(SDL_Renderer* renderer) {
    char filename[256];
    // 図柄テクスチャの読み込み
    for (int i = 0; i < SYMBOL_COUNT; i++) {
        sprintf(filename, "../images/zugara_%d.png", i + 1);
        gSymbolTextures[i] = IMG_LoadTexture(renderer, filename);
        if (gSymbolTextures[i] == NULL) {
            fprintf(stderr, "Failed to load texture: %s, Error: %s\n", filename, IMG_GetError());
            // (★) 読み込み失敗時の後処理を追加
            for(int j = 0; j < i; j++) { // 既に読み込んだテクスチャを解放
                 SDL_DestroyTexture(gSymbolTextures[j]);
                 gSymbolTextures[j] = NULL;
            }
            return false;
        }
    }

    // (★) 背景画像の読み込み
    sprintf(filename, "../images/reel_background.png");
    gReelBackgroundTexture = IMG_LoadTexture(renderer, filename);
    if (gReelBackgroundTexture == NULL) {
        fprintf(stderr, "Failed to load background texture: %s, Error: %s\n", filename, IMG_GetError());
        // (★) 読み込み失敗時の後処理を追加 (図柄テクスチャも解放)
        for(int i = 0; i < SYMBOL_COUNT; i++) {
             if (gSymbolTextures[i]) {
                 SDL_DestroyTexture(gSymbolTextures[i]);
                 gSymbolTextures[i] = NULL;
             }
        }
        return false;
    }

    return true;
}

void Reel_SetYaku(YakuType yaku) {
    gCurrentYaku = yaku;
}

void Reel_StartSpinning() {
    // gIsSpinningBackward = false; // (★) 削除
    for (int i = 0; i < 3; i++) {
        gIsSpinning[i] = true;
        gIsStopping[i] = false;
        gIsSpinningBackward[i] = false; // (★追加)
        gStoppedGridM[i] = -1;
        gStopOrder[i] = 0;
    }
}

// (★新規) 逆回転開始
void Reel_StartSpinning_Reverse(void) {
    for (int i = 0; i < 3; i++) {
        gIsSpinning[i] = false;
        gIsStopping[i] = false;
        gIsSpinningBackward[i] = true; // (★追加)
        gStoppedGridM[i] = -1;
        gStopOrder[i] = 0;
    }
}

// (★新規) 強制停止
void Reel_ForceStop(ReelForceStopPattern pattern) {
    const int* target_grid_m = NULL;

    if (pattern == REEL_PATTERN_RED7_MID) {
        target_grid_m = PATTERN_RED7_MID_GRID;
    } else if (pattern == REEL_PATTERN_FRANXX_BONUS) {
        target_grid_m = PATTERN_FRANXX_BONUS_GRID;
    } else {
        return; // 不明なパターン
    }
    
    const float REEL_LENGTH = (float)(SYMBOLS_PER_REEL * SYMBOL_HEIGHT);

    for (int i = 0; i < 3; i++) {
        gIsSpinning[i] = false;
        gIsStopping[i] = false;
        gIsSpinningBackward[i] = false; // (★追加)

        int grid_m = target_grid_m[i];
        gStoppedGridM[i] = grid_m;
        gTargetStopGridM[i] = grid_m; // (念のため)
        
        // グリッドインデックスからY座標を計算
        gReelPos[i] = Wrap((float)(grid_m * SYMBOL_HEIGHT), REEL_LENGTH);
        gTargetPos[i] = gReelPos[i];
    }
}


// (★) Reel_StartSpinning_Enshutsu_Backward 関数を削除

void Reel_RequestStop(int reel_index, int stop_order) {
    if (reel_index < 0 || reel_index > 2) return;
    
    // (★修正) 順回転中、または逆回転中のみ停止を受け付ける
    if (!gIsSpinning[reel_index] && !gIsSpinningBackward[reel_index]) return;

    // (★追加) 逆回転中だった場合は、ここで止める
    gIsSpinningBackward[reel_index] = false;

    const float REEL_LENGTH = (float)(SYMBOLS_PER_REEL * SYMBOL_HEIGHT);
    gStopOrder[reel_index] = stop_order;

    float base_grid_pos = NextGridPosForward(gReelPos[reel_index], REEL_LENGTH);
    int base_grid_m = (int)roundf(base_grid_pos / (float)SYMBOL_HEIGHT);
    base_grid_m %= SYMBOLS_PER_REEL;
    if (base_grid_m < 0) base_grid_m += SYMBOLS_PER_REEL;

    int best_slip_koma = 0;
    bool found_target = false;

    for (int k = 0; k <= MAX_SLIP; k++) {
        int slip_grid_m = (base_grid_m - k + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;

        if (CheckYakuMatch(reel_index, stop_order, slip_grid_m)) {
            best_slip_koma = k;
            found_target = true;
            break;
        }
    }

    if (!found_target) {
        best_slip_koma = 0;
    }

    int final_grid_m = (base_grid_m - best_slip_koma + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;
    float final_target_pos = (float)(final_grid_m * SYMBOL_HEIGHT);

    gTargetPos[reel_index]       = final_target_pos;
    gTargetStopGridM[reel_index] = final_grid_m;

    gIsSpinning[reel_index] = false;
    gIsStopping[reel_index] = true;
}


void Reel_Update() {
    const float REEL_LENGTH = (float)(SYMBOLS_PER_REEL * SYMBOL_HEIGHT);
    // (★) 順回転 (-REEL_SPEED)
    const float speed_forward = -REEL_SPEED;
    const float speed_backward = REEL_SPEED; // (★追加) 逆回転（プラス方向）

    for (int i = 0; i < 3; i++) {
        if (gIsSpinning[i]) {
            gReelPos[i] += speed_forward; 
            gReelPos[i]  = Wrap(gReelPos[i], REEL_LENGTH);

        } 
        else if (gIsSpinningBackward[i]) {
             // (★追加) 逆回転
            gReelPos[i] += speed_backward; 
            gReelPos[i]  = Wrap(gReelPos[i], REEL_LENGTH);
        }
        else if (gIsStopping[i]) {
            // (★) 順回転の残り距離計算のみ
            float remaining = gReelPos[i] - gTargetPos[i];
            if (remaining < 0.0f) remaining += REEL_LENGTH;

            // (★) 速度の絶対値で比較
            if (remaining <= fabsf(speed_forward) + EPS) {
                gReelPos[i]  = gTargetPos[i];
                gIsStopping[i] = false;
                gStoppedGridM[i] = gTargetStopGridM[i];
            } else {
                gReelPos[i] += speed_forward; 
                gReelPos[i]  = Wrap(gReelPos[i], REEL_LENGTH);
            }
        }
    }
}

void Reel_Draw(SDL_Renderer* renderer, int screen_width, int screen_height) {
    const int total_reels_width = (REEL_WIDTH * 3) + (REEL_SPACING * 2);
    // (★) X座標の中心を3ピクセル左にずらす
    const int start_x_base = ((screen_width - total_reels_width) / 2) - 3; // (★) ずらす前の中心X

    // (★) リール窓の上端 Y 座標を、 `355` に固定
    const int start_y = 355; 

    // (★) 背景画像の描画領域を計算 (枠線を描画する領域に合わせる)
    SDL_Rect backgroundRect = {
        start_x_base - 5, // (★) 左に5pxずらす
        start_y - 2,      // (★) 上に2pxずらす
        total_reels_width + 8, // (★) 幅を8px広げる
        (SYMBOL_HEIGHT * 3) + 4 // (★) 高さを4px広げる
    };

    // (★) 1. 背景画像を描画
    if (gReelBackgroundTexture) {
         SDL_RenderCopy(renderer, gReelBackgroundTexture, NULL, &backgroundRect);
    } else {
         // (フォールバック: もし画像読み込み失敗なら白で塗る)
         SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
         SDL_RenderFillRect(renderer, &backgroundRect);
    }


    // --- (★) ここから図柄描画 ---
    // (★) クリップ領域を計算 (背景画像の描画領域と同じ)
    SDL_Rect clip_rect = backgroundRect; // (★) 背景描画領域をクリップ領域とする

    for (int r = 0; r < 3; r++) {
        int base_index = (int)floorf(gReelPos[r] / SYMBOL_HEIGHT);
        float y_offset = fmodf(gReelPos[r], (float)SYMBOL_HEIGHT);
        if (y_offset < 0.0f) y_offset += SYMBOL_HEIGHT;

        // (★) リールごとのX座標オフセットを追加
        int x_offset = 0;
        if (r == 0) {      // 左リール
            x_offset = 10;  // 右に2pxずらす
        } else if (r == 2) { // 右リール
            x_offset = -10; // 左に2pxずらす
        }
        // 中央(r=1)は x_offset = 0

        int current_start_x = start_x_base + r * (REEL_WIDTH + REEL_SPACING) + x_offset; // (★) 各リールの開始X座標

        // (★) 3コマ (+α) のみ描画
        for (int i = 0; i < 4; i++) { // 0:上, 1:中, 2:下, 3:下の下(一部)
            int current_index = (base_index + i + SYMBOLS_PER_REEL) % SYMBOLS_PER_REEL;
            SymbolType symbol_to_draw = all_reels[r][current_index];

            SDL_Rect destRect = {
                current_start_x, // (★) オフセット適用後のX座標を使う
                (int)(start_y + (i * SYMBOL_HEIGHT) - y_offset),
                REEL_WIDTH,
                SYMBOL_HEIGHT
            };

            // (★) クリップ領域を設定
            SDL_RenderSetClipRect(renderer, &clip_rect);

            SDL_RenderCopy(renderer, gSymbolTextures[symbol_to_draw], NULL, &destRect);

            SDL_RenderSetClipRect(renderer, NULL); // クリップ解除
        }
    }
    // --- (★) 図柄描画ここまで ---

    // (★) 枠線の描画は削除済み

}

bool Reel_IsSpinning() {
    for (int i = 0; i < 3; i++) {
        // (★修正) 逆回転中も「回転中」とみなす
        if (gIsSpinning[i] || gIsStopping[i] || gIsSpinningBackward[i]) return true;
    }
    return false;
}

void Reel_Cleanup() {
    // 図柄テクスチャの解放
    for (int i = 0; i < SYMBOL_COUNT; i++) {
        if (gSymbolTextures[i]) {
            SDL_DestroyTexture(gSymbolTextures[i]);
            gSymbolTextures[i] = NULL;
        }
    }
    // (★) 背景テクスチャの解放
    if (gReelBackgroundTexture) {
        SDL_DestroyTexture(gReelBackgroundTexture);
        gReelBackgroundTexture = NULL;
    }
}

int Reel_GetPaylineIndex(int reel_index) {
     if (reel_index < 0 || reel_index > 2) return -1;
     int m = gStoppedGridM[reel_index];
     if (m == -1) return -1;
     return IndexAtPayline(m);
}

void Reel_GetStoppedSymbolIndices(int reel_index, int* out_idx_ue, int* out_idx_naka, int* out_idx_shita) {
    if (reel_index < 0 || reel_index > 2 || gStoppedGridM[reel_index] == -1) {
        *out_idx_ue = -1; *out_idx_naka = -1; *out_idx_shita = -1;
        return;
    }
    GetSymbolIndices(gStoppedGridM[reel_index], out_idx_ue, out_idx_naka, out_idx_shita);
}