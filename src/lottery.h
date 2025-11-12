#ifndef LOTTERY_H
#define LOTTERY_H

#include "common.h" 
#include "game_data.h" // (★追加) AT_BonusResultType のため

/**
 * @brief 【通常時】の確率テーブルに基づいて小役を抽選します。
 * @return 当選した YakuType
 */
YakuType Lottery_GetResult_Normal();

/**
 * @brief 【フランクス高確率】の確率テーブルに基づいて小役を抽選します。
 * @return 当選した YakuType
 */
YakuType Lottery_GetResult_FranxxHighProb();

/**
 * @brief (★新規) 【AT高確率状態】の確率テーブルに基づいて小役を抽選します。
 * (現在は通常時と同じテーブルを流用します)
 * @return 当選した YakuType
 */
YakuType Lottery_GetResult_AT(void);

/**
 * @brief (★新規) 【AT高確率状態】で、成立役に基づきボーナス当否を抽選します。
 * @param yaku 成立役
 * @return 抽選結果 (AT_BonusResultType)
 */
AT_BonusResultType Lottery_CheckBonus_AT(YakuType yaku);


/**
 * @brief 役の日本語名を取得します。
 */
const char* GetYakuName(YakuType yaku);

/**
 * @brief 役の払い出し枚数を取得します。
 */
int GetPayoutForYaku(YakuType yaku, bool oshijun_success);

/**
 * @brief 役がレア役かどうかを判定します。
 */
bool IsRareYaku(YakuType yaku);

/**
 * @brief (★新規) 押し順が成立役と合っているか判定します。
 * @param yaku 成立役
 * @param push_order 実際に押されたリールインデックス (L=0, C=1, R=2) の配列
 * @return true: 押し順正解 (または押し順不問の役), false: 押し順ミス
 */
bool CheckOshijun(YakuType yaku, int push_order[3]);

#endif // LOTTERY_H