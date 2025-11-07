#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

// --- AT状態 (全ファイルで共通) ---
typedef enum {
    STATE_NORMAL,          // 通常時
    STATE_CZ,              // CZ (チャンスゾーン)
    
    // --- AT States ---
    STATE_BB_INITIAL,      // 1. ダーリン・イン・ザ・ボーナス[初当り]
    STATE_BONUS_HIGH_PROB, // 2. ダーリン・イン・ザ・フランキス[ボーナス高確率]
    STATE_BB_HIGH_PROB,    // 3. ダーリン・イン・ザ・ボーナス[高確中]
    STATE_FRANXX_BONUS,    // 4. フランクスボーナス[高確中]
    STATE_BB_EX,           // 5. ダーリン・イン・ザ・ボーナスEX
    STATE_HIYOKU_BEATS,    // 6. 比翼BEATS[特化ゾーン]
    STATE_EPISODE_BONUS,   // 7. エピソードボーナス[高確中]
    STATE_TSUREDASHI,      // 8. 連れ出し[上位特化ゾーン]
    STATE_AT_END           // 10. AT終了
} AT_State;


// --- 成立役 (全ファイルで共通) ---
typedef enum {
    // --- 押し順ベル (6-way) ---
    YAKU_OSHIJUN_BELL_LMR, // (通常: 5,545 / CZ: 5,545)
    YAKU_OSHIJUN_BELL_LRM, // (通常: 5,545 / CZ: 5,545)
    YAKU_OSHIJUN_BELL_MLR, // (通常: 9,449 / CZ: 9,449)
    YAKU_OSHIJUN_BELL_MRL, // (通常: 9,449 / CZ: 9,449)
    YAKU_OSHIJUN_BELL_RLM, // (通常: 9,449 / CZ: 9,449)
    YAKU_OSHIJUN_BELL_RML, // (通常: 9,449 / CZ: 9,449)
    
    // --- 小役 ---
    YAKU_REPLAY,           // (通常: 8,402 / CZ: 3,421)
    YAKU_COMMON_BELL,      // (通常: 4,615 / CZ: 4,615) - 1枚
    YAKU_CHERRY,           // (通常: 1,280 / CZ: 1,280) - 3枚
    YAKU_CHANCE_ME,        // (通常: 200 / CZ: 200) - 3枚
    YAKU_FRANXX_ME,        // (通常: 368 / CZ: 368) - 3枚
    YAKU_STRELITZIA_ME,    // (通常: 28 / CZ: 28) - 3枚

    // --- CZ (フランクス高確) 専用 ---
    YAKU_HP_REVERSE_FRANXX,     // 【高確】逆押しフランクス目 (CZ: 4,965)
    YAKU_HP_REVERSE_STRONG_FRANXX, // 【高確】逆押し最強フランクス目 (CZ: 8)
    YAKU_HP_REVERSE_STRELITZIA,  // 【高確】逆押しストレリチア目 (CZ: 8)

    // --- ハズレ ---
    YAKU_HAZURE,           // (通常: 1,757 / CZ: 1,757)

    // --- AT/Bonus specific (テーブル外だが仕様上必要) ---
    YAKU_FRANXX_SYMBOL,    // フランクスボーナス中の図柄 (1/12)
    
    YAKU_COUNT
} YakuType;

#endif // COMMON_H