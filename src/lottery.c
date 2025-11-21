#include "lottery.h"
#include <stdlib.h> 
#include <stdio.h>

// 0〜65535 を偏りなく生成するための 8bit×2 合成
static inline unsigned rand_u16(void) {
    unsigned hi = (unsigned)(rand() % 256); // 0..255
    unsigned lo = (unsigned)(rand() % 256); // 0..255
    return (hi << 8) | lo;                  // 0..65535
}

// (★追加) 0〜999 (1000未満) の乱数を生成
static inline int rand_1000(void) {
    return rand() % 1000;
}

// --- 役情報 (払い出し) ---
int GetPayoutForYaku(YakuType yaku, bool oshijun_success) {
    switch (yaku) {
        case YAKU_OSHIJUN_BELL_LMR:
        case YAKU_OSHIJUN_BELL_LRM:
        case YAKU_OSHIJUN_BELL_MLR:
        case YAKU_OSHIJUN_BELL_MRL:
        case YAKU_OSHIJUN_BELL_RLM:
        case YAKU_OSHIJUN_BELL_RML:
            return oshijun_success ? 10 : 0; 

        case YAKU_REPLAY:         return 3;
        case YAKU_COMMON_BELL:    return 1;
        case YAKU_CHERRY:         return 3;
        case YAKU_CHANCE_ME:      return 3;
        case YAKU_FRANXX_ME:      return 3;
        case YAKU_STRELITZIA_ME:  return 3;
        
        case YAKU_HP_REVERSE_FRANXX:      return 3;
        case YAKU_HP_REVERSE_STRONG_FRANXX: return 3;
        case YAKU_HP_REVERSE_STRELITZIA:  return 3;
        
        case YAKU_FRANXX_SYMBOL:  return 15;

        case YAKU_HAZURE:
        default:
            return 0;
    }
}

// --- 役情報 (名称) ---
const char* GetYakuName(YakuType yaku) {
    switch (yaku) {
        case YAKU_OSHIJUN_BELL_LMR: return "押順ベル(左中右)";
        case YAKU_OSHIJUN_BELL_LRM: return "押順ベル(左右中)";
        case YAKU_OSHIJUN_BELL_MLR: return "押順ベル(中左右)";
        case YAKU_OSHIJUN_BELL_MRL: return "押順ベル(中右左)";
        case YAKU_OSHIJUN_BELL_RLM: return "押順ベル(右左中)";
        case YAKU_OSHIJUN_BELL_RML: return "押順ベル(右中左)";
        case YAKU_REPLAY:           return "リプレイ";
        case YAKU_COMMON_BELL:    return "共通ベル (1枚)";
        case YAKU_CHERRY:           return "チェリー";
        case YAKU_CHANCE_ME:      return "チャンス目";
        case YAKU_FRANXX_ME:      return "フランクス目";
        case YAKU_STRELITZIA_ME:  return "ストレリチア目";
        case YAKU_HP_REVERSE_FRANXX:      return "【高確】逆押しフランクス目";
        case YAKU_HP_REVERSE_STRONG_FRANXX: return "【高確】逆押し最強フランクス目";
        case YAKU_HP_REVERSE_STRELITZIA:  return "【高確】逆押しストレリチア目";
        case YAKU_HAZURE:           return "ハズレ";
        case YAKU_FRANXX_SYMBOL:  return "フランクス図柄(AT)";
        default:                    return "不明";
    }
}

// --- 役情報 (レア役判定) ---
bool IsRareYaku(YakuType yaku) {
    switch (yaku) {
        case YAKU_CHERRY:
        case YAKU_CHANCE_ME:
        case YAKU_FRANXX_ME:
        case YAKU_STRELITZIA_ME:
        case YAKU_HP_REVERSE_FRANXX:
        case YAKU_HP_REVERSE_STRONG_FRANXX:
        case YAKU_HP_REVERSE_STRELITZIA:
            return true;
        default:
            return false;
    }
}

// --- 押し順判定 ---
bool CheckOshijun(YakuType yaku, int push_order[3]) {
    switch (yaku) {
        case YAKU_OSHIJUN_BELL_LMR: return (push_order[0] == 0 && push_order[1] == 1 && push_order[2] == 2);
        case YAKU_OSHIJUN_BELL_LRM: return (push_order[0] == 0 && push_order[1] == 2 && push_order[2] == 1);
        case YAKU_OSHIJUN_BELL_MLR: return (push_order[0] == 1 && push_order[1] == 0 && push_order[2] == 2);
        case YAKU_OSHIJUN_BELL_MRL: return (push_order[0] == 1 && push_order[1] == 2 && push_order[2] == 0);
        case YAKU_OSHIJUN_BELL_RLM: return (push_order[0] == 2 && push_order[1] == 0 && push_order[2] == 1);
        case YAKU_OSHIJUN_BELL_RML: return (push_order[0] == 2 && push_order[1] == 1 && push_order[2] == 0);
        default: return true; 
    }
}

// --- 公開抽選関数 (通常時) ---
YakuType Lottery_GetResult_Normal() {
    int r = (int)rand_u16(); 
    int cumulative_prob = 0;

    cumulative_prob += 5545; // YAKU_OSHIJUN_BELL_LMR
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_LMR;
    cumulative_prob += 5545; // YAKU_OSHIJUN_BELL_LRM
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_LRM;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_MLR
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_MLR;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_MRL
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_MRL;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_RLM
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_RLM;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_RML
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_RML;
    cumulative_prob += 8402; // YAKU_REPLAY
    if (r < cumulative_prob) return YAKU_REPLAY;
    cumulative_prob += 4615; // YAKU_COMMON_BELL
    if (r < cumulative_prob) return YAKU_COMMON_BELL;
    cumulative_prob += 1280; // YAKU_CHERRY
    if (r < cumulative_prob) return YAKU_CHERRY;
    cumulative_prob += 200; // YAKU_CHANCE_ME
    if (r < cumulative_prob) return YAKU_CHANCE_ME;
    cumulative_prob += 368; // YAKU_FRANXX_ME
    if (r < cumulative_prob) return YAKU_FRANXX_ME;
    cumulative_prob += 28; // YAKU_STRELITZIA_ME
    if (r < cumulative_prob) return YAKU_STRELITZIA_ME;

    return YAKU_HAZURE;
}

// --- 公開抽選関数 (フランクス高確率) ---
YakuType Lottery_GetResult_FranxxHighProb() {
    int r = (int)rand_u16(); 
    int cumulative_prob = 0;

    cumulative_prob += 4965; // YAKU_HP_REVERSE_FRANXX
    if (r < cumulative_prob) return YAKU_HP_REVERSE_FRANXX;
    cumulative_prob += 8; // YAKU_HP_REVERSE_STRONG_FRANXX
    if (r < cumulative_prob) return YAKU_HP_REVERSE_STRONG_FRANXX;
    cumulative_prob += 8; // YAKU_HP_REVERSE_STRELITZIA
    if (r < cumulative_prob) return YAKU_HP_REVERSE_STRELITZIA;
    cumulative_prob += 3421; // YAKU_REPLAY
    if (r < cumulative_prob) return YAKU_REPLAY;
    cumulative_prob += 5545; // YAKU_OSHIJUN_BELL_LMR
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_LMR;
    cumulative_prob += 5545; // YAKU_OSHIJUN_BELL_LRM
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_LRM;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_MLR
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_MLR;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_MRL
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_MRL;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_RLM
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_RLM;
    cumulative_prob += 9449; // YAKU_OSHIJUN_BELL_RML
    if (r < cumulative_prob) return YAKU_OSHIJUN_BELL_RML;
    cumulative_prob += 4615; // YAKU_COMMON_BELL
    if (r < cumulative_prob) return YAKU_COMMON_BELL;
    cumulative_prob += 1280; // YAKU_CHERRY
    if (r < cumulative_prob) return YAKU_CHERRY;
    cumulative_prob += 200; // YAKU_CHANCE_ME
    if (r < cumulative_prob) return YAKU_CHANCE_ME;
    cumulative_prob += 368; // YAKU_FRANXX_ME
    if (r < cumulative_prob) return YAKU_FRANXX_ME;
    cumulative_prob += 28; // YAKU_STRELITZIA_ME
    if (r < cumulative_prob) return YAKU_STRELITZIA_ME;

    return YAKU_HAZURE;
}


// =================================================================
// (★) AT高確率状態用 抽選
// =================================================================

/**
 * @brief 【AT高確率状態】の小役抽選
 * 通常時のテーブル(Normal)をそのまま使用します。
 */
YakuType Lottery_GetResult_AT(void) {
    return Lottery_GetResult_Normal();
}

/**
 * @brief 【AT高確率状態】のボーナス当否・種別抽選 (完全実装版)
 */
AT_BonusResultType Lottery_CheckBonus_AT(YakuType yaku) {
    // 1. 当否判定 (当選率)
    int r_success = rand_1000();
    bool is_success = false;

    switch (yaku) {
        case YAKU_REPLAY:       is_success = (r_success < 318); break; // 31.8%
        case YAKU_COMMON_BELL:  is_success = (r_success < 379); break; // 37.9%
        case YAKU_HAZURE:       is_success = false; break;
        default:
            // レア役・確定役は 100% 当選
            if (IsRareYaku(yaku)) is_success = true;
            break;
    }

    if (!is_success) {
        // 抽選対象役 (リプ/ベル) なら演出用継続、それ以外はハズレ
        if (yaku == YAKU_REPLAY || yaku == YAKU_COMMON_BELL) {
            return BONUS_AT_CONTINUE;
        }
        return BONUS_NONE;
    }

    // 2. 種別振り分け (0.1%単位、0-999)
    int r_type = rand_1000(); 

    /*
     * FB: フランクスボーナス
     * DB: ダーリンボーナス
     * EX: BB EX
     * EP: エピソードボーナス
     */

    switch (yaku) {
        case YAKU_REPLAY: 
        case YAKU_COMMON_BELL: 
            // FB 37%, DB 50%, EX 13%, EP 0%
            if (r_type < 370) return BONUS_FRANXX;      // 0-369
            if (r_type < 870) return BONUS_DARLING;     // 370-869 (370+500)
            return BONUS_BB_EX;                         // 870-999 (残り130)
        
        case YAKU_CHANCE_ME: 
            // FB 0%, DB 80%, EX 19%, EP 1%
            if (r_type < 800) return BONUS_DARLING;     // 0-799
            if (r_type < 990) return BONUS_BB_EX;       // 800-989 (800+190)
            return BONUS_EPISODE;                       // 990-999 (残り10)
        
        case YAKU_CHERRY: 
            // FB 50%, DB 40%, EX 9%, EP 1%
            if (r_type < 500) return BONUS_FRANXX;      // 0-499
            if (r_type < 900) return BONUS_DARLING;     // 500-899 (500+400)
            if (r_type < 990) return BONUS_BB_EX;       // 900-989 (900+90)
            return BONUS_EPISODE;                       // 990-999 (残り10)
        
        case YAKU_FRANXX_ME: 
            // FB 25%, DB 37.5%, EX 36.5%, EP 1%
            // (37.5% -> 375, 36.5% -> 365)
            if (r_type < 250) return BONUS_FRANXX;      // 0-249
            if (r_type < 625) return BONUS_DARLING;     // 250-624 (250+375)
            if (r_type < 990) return BONUS_BB_EX;       // 625-989 (625+365)
            return BONUS_EPISODE;                       // 990-999 (残り10)

        case YAKU_STRELITZIA_ME: 
        case YAKU_HP_REVERSE_STRONG_FRANXX:
        case YAKU_HP_REVERSE_STRELITZIA:
            // FB 0%, DB 0%, EX 95%, EP 5%
            if (r_type < 950) return BONUS_BB_EX;       // 0-949
            return BONUS_EPISODE;                       // 950-999

        default:
            return BONUS_AT_CONTINUE; 
    }
}