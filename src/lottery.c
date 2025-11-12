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
    // (at.c の既存のロジックに合わせる)
    return rand() % 1000;
}

// --- 役情報 (払い出し) ---
int GetPayoutForYaku(YakuType yaku, bool oshijun_success) {
    switch (yaku) {
        // 押し順ベル (★) oshijun_success が false なら 0 を返す
        case YAKU_OSHIJUN_BELL_LMR:
        case YAKU_OSHIJUN_BELL_LRM:
        case YAKU_OSHIJUN_BELL_MLR:
        case YAKU_OSHIJUN_BELL_MRL:
        case YAKU_OSHIJUN_BELL_RLM:
        case YAKU_OSHIJUN_BELL_RML:
            return oshijun_success ? 10 : 0; 

        // 小役
        case YAKU_REPLAY:         return 3;
        case YAKU_COMMON_BELL:    return 1;
        case YAKU_CHERRY:         return 3;
        case YAKU_CHANCE_ME:      return 3;
        case YAKU_FRANXX_ME:      return 3;
        case YAKU_STRELITZIA_ME:  return 3;
        
        // CZ専用役
        case YAKU_HP_REVERSE_FRANXX:      return 3;
        case YAKU_HP_REVERSE_STRONG_FRANXX: return 3;
        case YAKU_HP_REVERSE_STRELITZIA:  return 3;
        
        // AT中 (仮)
        case YAKU_FRANXX_SYMBOL:  return 15;

        // (★) 演出役は削除

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
        
        // (★) 演出役は削除

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

/**
 * @brief (★新規) 押し順が成立役と合っているか判定します。
 */
bool CheckOshijun(YakuType yaku, int push_order[3]) {
    switch (yaku) {
        // (L=0, C=1, R=2)
        case YAKU_OSHIJUN_BELL_LMR: // 0 -> 1 -> 2
            return (push_order[0] == 0 && push_order[1] == 1 && push_order[2] == 2);
        case YAKU_OSHIJUN_BELL_LRM: // 0 -> 2 -> 1
            return (push_order[0] == 0 && push_order[1] == 2 && push_order[2] == 1);
        case YAKU_OSHIJUN_BELL_MLR: // 1 -> 0 -> 2
            return (push_order[0] == 1 && push_order[1] == 0 && push_order[2] == 2);
        case YAKU_OSHIJUN_BELL_MRL: // 1 -> 2 -> 0
            return (push_order[0] == 1 && push_order[1] == 2 && push_order[2] == 0);
        case YAKU_OSHIJUN_BELL_RLM: // 2 -> 0 -> 1
            return (push_order[0] == 2 && push_order[1] == 0 && push_order[2] == 1);
        case YAKU_OSHIJUN_BELL_RML: // 2 -> 1 -> 0
            return (push_order[0] == 2 && push_order[1] == 1 && push_order[2] == 0);
        
        // (★) 演出役は削除

        default:
            // 押し順ベル以外の役は、常に「正解」扱い
            return true; 
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
// (★新規) AT高確率状態用
// =================================================================

/**
 * @brief (★新規) 【AT高確率状態】の確率テーブルに基づいて小役を抽選します。
 * (注: 現状、適切なAT中テーブルがないため、通常時テーブルを流用します)
 */
YakuType Lottery_GetResult_AT(void) {
    // TODO: AT専用の抽選テーブルを実装する
    //
    // 仕様 4: 「完全なハズレ（ボーナス当否の抽選を一切行わない子役）」
    // を実装するため、ここではダミーとして 50% で YAKU_HAZURE (抽選対象外) を返す
    if (rand_1000() < 500) {
        return YAKU_HAZURE;
    }
    
    // 残り 50% で通常時の抽選を行う (YAKU_HAZURE 以外)
    YakuType yaku;
    do {
        yaku = Lottery_GetResult_Normal();
    } while (yaku == YAKU_HAZURE);
    
    return yaku;
}

/**
 * @brief (★新規) AT高確率状態でのボーナス当否判定 (at.c からロジック移植)
 */
AT_BonusResultType Lottery_CheckBonus_AT(YakuType yaku) {
    // (at.c の roll_high_prob_success 相当)
    int r_success = rand_1000();
    bool is_success = false;
    switch (yaku) {
        case YAKU_REPLAY:       is_success = (r_success < 318); break; // 31.8%
        case YAKU_COMMON_BELL:  is_success = (r_success < 379); break; // 37.9%
        default:
            if (IsRareYaku(yaku)) is_success = true; // レア役は 100% 当選
            break;
    }

    // 仕様 4: 「抽選対象役」かどうか
    // = 当否抽選の対象になったか？
    if (!is_success) {
        // (YAKU_HAZURE や、抽選に漏れたリプ/ベル)
        
        // (★) 抽選対象役 (リプ/ベル) だったが抽選に漏れた場合
        if (yaku == YAKU_REPLAY || yaku == YAKU_COMMON_BELL) {
            return BONUS_AT_CONTINUE; // 演出対象
        }
        
        // (★) そもそも抽選対象外 (ハズレ、押し順ベルなど)
        return BONUS_NONE;
    }

    // --- 当選確定 ---
    // (at.c の perform_bonus_allocation 相当の種別振り分け)
    int r_type = rand_1000(); 
    
    switch (yaku) {
        case YAKU_REPLAY: 
        case YAKU_COMMON_BELL: 
            if (r_type < 370) return BONUS_FRANXX; 
            else return BONUS_DARLING; // (BB_EXはダーリンボーナスとして扱う)
        
        case YAKU_CHANCE_ME: 
            return BONUS_DARLING; // (EPボーナス/BB_EXはダーリンボーナスとして扱う)
        
        case YAKU_CHERRY: 
            if (r_type < 500) return BONUS_FRANXX; 
            else return BONUS_DARLING; // (BB_EXはダーリンボーナスとして扱う)
        
        case YAKU_FRANXX_ME: 
            if (r_type < 751) return BONUS_DARLING; // (EP/BB_EXはダーリンボーナスとして扱う)
            else return BONUS_FRANXX; 
        
        case YAKU_STRELITZIA_ME: 
        case YAKU_HP_REVERSE_STRONG_FRANXX: // (AT中には来ないが念のため)
        case YAKU_HP_REVERSE_STRELITZIA:    // (AT中には来ないが念のため)
            return BONUS_DARLING; // (EP/BB_EXはダーリンボーナスとして扱う)
        
        default:
            // (抽選対象役だが、振り分けにない = 落選)
            return BONUS_AT_CONTINUE; 
    }
}