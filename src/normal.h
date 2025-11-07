#ifndef NORMAL_H
#define NORMAL_H

#include "game_data.h" 

/**
 * @brief 通常時のゲームロジックを更新します。
 * (レア役でのAT/CZ抽選など)
 */
void Normal_Update(GameData* data, YakuType yaku);

#endif // NORMAL_H