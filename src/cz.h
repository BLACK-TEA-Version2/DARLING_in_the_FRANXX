#ifndef CZ_H
#define CZ_H

#include "game_data.h"

/**
 * @brief CZ中のゲームロジックを更新します。
 */
void CZ_Update(GameData* data, YakuType yaku);

/**
 * @brief CZ突入時の初期化処理
 */
void CZ_Init(GameData* data);

#endif // CZ_H