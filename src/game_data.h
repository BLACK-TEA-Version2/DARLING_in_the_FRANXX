#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "common.h" // AT_State, YakuType のため

// --- 比翼BEATSレベル ---
typedef enum {
    HIYOKU_LV1 = 1, // 2G
    HIYOKU_LV2 = 2, // 3G
    HIYOKU_MAXX = 3 // 4G
} HiyokuLevel;

// --- 全モジュール共通 ゲームデータ構造体 ---
typedef struct {
    // --- 1. 共通ステータス ---
    AT_State current_state;          // ★現在の「基本」状態
    long long total_payout_diff; // 総獲得枚数
    int bonus_stock_count;     // ボーナスストック_個数 (FB/DB)
    
    // --- 2. AT/CZ 関連データ ---
    int bonus_high_prob_games; // 2. ボーナス高確率_残りG数

    // --- 3. ボーナス差枚管理 ---
    int current_bonus_payout; // 現在のボーナスで獲得した差枚
    int target_bonus_payout;  // 現在のボーナスの目標差枚
    
    // --- 4. 並行処理用データ ---
    int queued_bb_ex_payout;  // BB EXの「予約」差枚数
    int franxx_bonus_part_remaining; // 連れ出し時のFB残り枚数

    // --- 5. 比翼BEATS 並行管理フラグ ---
    bool hiyoku_is_active;    // 比翼BEATSが(ホールド中含め)並行して作動中か
    bool hiyoku_is_frozen;    // 比翼BEATSのSTG数減算がストップしているか
    HiyokuLevel hiyoku_level;
    int hiyoku_st_games;

    // --- 6. 表示用データ ---
    char last_yaku_name[64];  // 最後に成立した役の名前
    char info_message[128]; // 画面に表示するメッセージ
    bool oshijun_success;     // そのゲームで押し順に成功したか

} GameData;

#endif // GAME_DATA_H