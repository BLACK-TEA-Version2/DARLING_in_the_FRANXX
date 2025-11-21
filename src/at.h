#ifndef AT_H
#define AT_H

#include "game_data.h" 
#include <SDL2/SDL.h> // (★追加) SDL_Renderer のため

// --- 定義 (AT内部でのみ使う定数) ---
#define BET_COUNT 3
#define PAYOUT_TARGET_BB_INITIAL 100      // 1. BB[初当り]
#define PAYOUT_TARGET_BB_HIGH_PROB 100    // 3. BB[高確中]
#define PAYOUT_TARGET_FRANXX_BONUS 60     // 4. フランクスボーナス
#define PAYOUT_TARGET_BB_EX 100           // 5. BB EX (最低枚数)
#define PAYOUT_TARGET_EP_BONUS 200        // 7. EPボーナス
#define PAYOUT_TARGET_TSUREDASHI 60       // 8. 連れ出し
#define GAMES_ON_BB_INITIAL_END 10        // 1. BB[初当り] 終了時のG数

// --- 公開関数プロトタイプ ---

/**
 * @brief AT突入時の初期化処理
 */
void AT_Init(GameData* data);

/**
 * @brief (★廃止) AT_ProcessStop は AT_Update に統合されます。
 */
// void AT_ProcessStop(GameData* data, YakuType yaku, bool oshijun_success);

/**
 * @brief (★新規) AT中のメイン更新処理 (毎フレーム呼び出す)
 *
 * @param data ゲームデータ
 * @param yaku 【レバーオン時】に成立した役 (全停止までは保持される)
 * @param diff 【全停止時】に計算された差枚 (全停止時以外は 0)
 * @param lever_on このフレームでレバーが押されたか
 * @param all_reels_stopped このフレームで全リールが停止したか
 */
void AT_Update(GameData* data, YakuType yaku, int diff, bool lever_on, bool all_reels_stopped);

/**
 * @brief (★新規) AT中の描画処理 (毎フレーム呼び出す)
 *
 * @param renderer レンダラー
 * @param screen_width 画面幅
 * @param screen_height 画面高さ
 */
void AT_Draw(SDL_Renderer* renderer, int screen_width, int screen_height);

/**
 * @brief (★ T19 修正) AT状態を強制的に遷移させ、状態に応じた初期化を行います。
 * (main.c から呼び出すために static を解除)
 */
void transition_to_state(GameData* data, AT_State new_state);


/**
 * @brief AT状態の名称を取得
 */
const char* AT_GetStateName(AT_State state);

#endif // AT_H