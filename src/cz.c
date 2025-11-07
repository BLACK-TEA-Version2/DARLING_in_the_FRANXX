#include "cz.h"
#include "lottery.h"
#include "at.h" // AT_Init() のため
#include <stdio.h>

void CZ_Init(GameData* data) {
    // (将来的に実装する)
    snprintf(data->info_message, sizeof(data->info_message), "CZ突入！ (未実装)");
}

void CZ_Update(GameData* data, YakuType yaku) {
    // (現在はATから開始するため、この関数は呼ばれない)
    // (将来的に実装する)
    
    // (仮: レア役でAT突入)
    if (IsRareYaku(yaku)) {
        snprintf(data->info_message, sizeof(data->info_message), "CZ中レア役！ AT当選！");
        AT_Init(data);
        return;
    }
}