#include "normal.h"
#include "lottery.h"
#include "at.h" // AT_Init() のため
#include <stdio.h>
#include <stdlib.h> 

/**
 * @brief 通常時のゲームロジックを更新
 */
void Normal_Update(GameData* data, YakuType yaku) {
    // (現在はATから開始するため、この関数は呼ばれない)
    // (将来的に実装する)
    
    // (仮: レア役でAT突入テスト)
    if (IsRareYaku(yaku)) {
        if (yaku == YAKU_STRELITZIA_ME) {
             snprintf(data->info_message, sizeof(data->info_message), "ストレリチア目！ AT当選！");
             AT_Init(data); // ATモジュールを初期化
             return;
        }
    }
}