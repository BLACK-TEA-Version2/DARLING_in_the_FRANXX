#ifndef AT_H
#define AT_H

#include "game_data.h" 

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
 * @brief (★変更) AT中の【全リール停止時】のゲームロジックを更新します。
 * @param data ゲームデータ
 * @param yaku 【レバーオン時】に成立した役
 * @param oshijun_success 押し順が正解だったか
 */
void AT_ProcessStop(GameData* data, YakuType yaku, bool oshijun_success);

/**
 * @brief AT状態の名称を取得
 */
const char* AT_GetStateName(AT_State state);

#endif // AT_H