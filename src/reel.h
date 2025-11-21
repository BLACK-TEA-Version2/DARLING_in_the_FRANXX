#ifndef REEL_H
#define REEL_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "common.h" // (★) YakuType, SymbolType のためにインクルード
#include "game_data.h" // (★追加) ReelForceStopPattern のため

// --- 定義セクション ---
#define SYMBOLS_PER_REEL 20
#define SYMBOL_HEIGHT 83
#define REEL_WIDTH 157
#define REEL_SPACING 53
#define REEL_SPEED 48.718f
#define SYMBOL_COUNT 10

// --- 図柄の種類の定義 ---
typedef enum {
    REPLAY, BELL, CHERRY, SYMBOL_AO, SYMBOL_PURPLE, BAR, AKA_7, UE, NAKA,
    SHITA,
} SymbolType;

// --- 公開関数 ---

/**
 * @brief リールと図柄テクスチャを初期化します。
 */
bool Reel_Init(SDL_Renderer* renderer);

/**
 * @brief 3つのリールすべてを回転開始します。
 */
void Reel_StartSpinning();

/**
 * @brief (★新規) 3つのリールすべてを【逆回転】開始します。
 */
void Reel_StartSpinning_Reverse(void);

/**
 * @brief (★新規) 3つのリールすべてを【強制停止】させます。
 * @param pattern 停止させる図柄パターン (REEL_PATTERN_RED7_MID など)
 */
void Reel_ForceStop(ReelForceStopPattern pattern);

/**
 * @brief 特定のリールの停止を要求します。
 * @param reel_index 停止するリール (0:L, 1:C, 2:R)
 * @param stop_order 第何停止か (1, 2, 3)
 */
void Reel_RequestStop(int reel_index, int stop_order);

/**
 * @brief リールの位置を更新します（毎フレーム呼び出す）。
 */
void Reel_Update();

/**
 * @brief 現在のリール状態を描画します（毎フレーム呼び出す）。
 */
void Reel_Draw(SDL_Renderer* renderer, int screen_width, int screen_height);

/**
 * @brief いずれかのリールがまだ回転中か確認します。
 */
bool Reel_IsSpinning();

/**
 * @brief リールモジュールをクリーンアップします (テクスチャ解放)。
 */
void Reel_Cleanup();

/**
 * @brief (★新規) main.c から成立役をセットする関数
 */
void Reel_SetYaku(YakuType yaku);

/**
 * @brief (★新規) 停止したペイライン(中段)のインデックスを取得
 */
int Reel_GetPaylineIndex(int reel_index);

/**
 * @brief (★新規) 停止した3図柄(上/中/下)のインデックスを取得
 */
void Reel_GetStoppedSymbolIndices(int reel_index, int* out_idx_ue, int* out_idx_naka, int* out_idx_shita);

int Reel_GetSymbolIndex(int reel_index, SymbolType symbol);

#endif // REEL_H