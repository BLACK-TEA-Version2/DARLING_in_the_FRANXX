#ifndef VIDEO_DEFS_H
#define VIDEO_DEFS_H

// (★) 動画の種類
typedef enum {
    VIDEO_NONE = -1,
    VIDEO_IDLE,            // 待機中
    VIDEO_ENSHUTSU_AKA7,   // 赤7そろい演出 (仮)
    VIDEO_SPIN,            // リール回転中

    // =================================================================
    // (★追加) 専用演出ペア (導入 + ループ) (全4ペア)
    // =================================================================
    VIDEO_AT_PRES_A_INTRO, // ペアA (期待度低)
    VIDEO_AT_PRES_A_LOOP,
    VIDEO_AT_PRES_B_INTRO, // ペアB (期待度中)
    VIDEO_AT_PRES_B_LOOP,
    VIDEO_AT_PRES_C_INTRO, // ペアC (期待度高)
    VIDEO_AT_PRES_C_LOOP,
    VIDEO_AT_PRES_D_INTRO, // ペアD (当選濃厚)
    VIDEO_AT_PRES_D_LOOP,

    // =================================================================
    // (★追加) 当落演出動画
    // =================================================================
    // 落選時 (3-4種)
    VIDEO_JUDGE_LOSE_1,
    VIDEO_JUDGE_LOSE_2,
    VIDEO_JUDGE_LOSE_3,
    // ダーリンボーナス当選時 (3-4種)
    VIDEO_JUDGE_DARLING_1,
    VIDEO_JUDGE_DARLING_2,
    VIDEO_JUDGE_DARLING_3,
    // フランクスボーナス当選時 (3-4種)
    VIDEO_JUDGE_FRANXX_1,
    VIDEO_JUDGE_FRANXX_2,
    VIDEO_JUDGE_FRANXX_3,

    VIDEO_COUNT            // (★) 常に末尾
} VideoType;

#endif // VIDEO_DEFS_H