#ifndef GAME_DATA_H
#define GAME_DATA_H

#include "common.h" 
#include "video_defs.h" 
#include <SDL2/SDL_stdinc.h> 

// --- 比翼BEATSレベル ---
typedef enum {
    HIYOKU_LV1 = 1, // 2G
    HIYOKU_LV2 = 2, // 3G
    HIYOKU_MAXX = 3 // 4G
} HiyokuLevel;

// =================================================================
// (★) AT高確率状態の内部ステップ定義
// =================================================================
typedef enum {
    AT_STEP_NONE,             // AT高確率状態ではない
    AT_STEP_WAIT_LEVER1,      // (1) 1回目レバーオン待ち
    AT_STEP_REEL_SPIN,        // (1) リール回転中
    AT_STEP_LOOP_VIDEO_INTRO, // (2) 専用演出（導入）再生中
    AT_STEP_LOOP_VIDEO_MAIN,  // (2-bis) 専用演出（ループ）再生中 (2回目レバー待ち)
    AT_STEP_JUDGE_VIDEO,      // (3) 当落演出 再生中
} AtStep;

// =================================================================
// (★修正) AT高確率状態のボーナス抽選結果
// =================================================================
typedef enum {
    BONUS_NONE,         // ハズレ (抽選対象外役)
    BONUS_AT_CONTINUE,  // 落選 (AT継続)
    BONUS_DARLING,      // 当選 (ダーリンインザボーナス)
    BONUS_FRANXX,       // 当選 (フランクスボーナス)
    BONUS_BB_EX,        // (★追加) BB EX
    BONUS_EPISODE       // (★追加) エピソードボーナス
} AT_BonusResultType;

// =================================================================
// (★修正) リール強制停止パターン定義
// =================================================================
typedef enum {
    REEL_PATTERN_NONE,
    REEL_PATTERN_RED7_MID,      // 中段 赤7揃い
    REEL_PATTERN_FRANXX_BONUS   // 左中:赤7, 右:SHITA (★修正: BARではなくSHITA)
} ReelForceStopPattern;


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

    // =================================================================
    // 7. AT高確率状態 演出制御データ
    // =================================================================
    AtStep at_step;                 // AT高確率状態の内部ステップ
    AT_BonusResultType at_bonus_result; // 1回目レバーオン時のボーナス抽選結果
    YakuType at_last_lottery_yaku;  // 1回目レバーオン時の成立役
    
    // 演出動画ペア
    VideoType at_pres_intro_id;
    VideoType at_pres_loop_id;
    
    // 当落演出用タイマー
    Uint32 at_judge_video_start_time;
    Uint32 at_judge_video_duration_ms; // 当落動画の再生時間
    bool at_judge_timing_reverse_triggered;
    bool at_judge_timing_stop_triggered;

} GameData;

#endif // GAME_DATA_H