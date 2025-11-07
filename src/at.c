// at.c (修正版)

#include "at.h"
#include "lottery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- 内部ヘルパー関数 ---
// (...変更なし...)
static int get_st_games_from_level(HiyokuLevel level) {
    switch (level) {
        case HIYOKU_LV1: return 2;
        case HIYOKU_LV2: return 3;
        case HIYOKU_MAXX: return 4;
    }
    return 2;
}
static void Hiyoku_Init(GameData* data, bool is_from_ep_bonus) {
    data->hiyoku_is_active = true;
    data->hiyoku_is_frozen = false; 

    if (is_from_ep_bonus) {
        data->hiyoku_level = HIYOKU_MAXX;
    } else {
        int r = rand() % 1000; 
        if (r < 1) { 
            data->hiyoku_level = HIYOKU_MAXX;
        } else if (r < 216) { 
            data->hiyoku_level = HIYOKU_LV2;
        } else { 
            data->hiyoku_level = HIYOKU_LV1;
        }
    }
    data->hiyoku_st_games = get_st_games_from_level(data->hiyoku_level);
}
static void BB_EX_Init(GameData* data) {
    int continue_rate_percent = 50; 
    if ((rand() % 100) < 5) { 
        continue_rate_percent = 80;
    }

    int payout = PAYOUT_TARGET_BB_EX; 
    bool is_over_1000 = false; 

    while ((rand() % 100) < continue_rate_percent) {
        if (is_over_1000) {
            payout += 1000;
        } else {
            payout += 100;
            if (payout >= 1000) {
                is_over_1000 = true;
            }
        }
    }
    data->queued_bb_ex_payout += payout; 
}
static bool is_payout_reset_yaku(YakuType yaku) {
    switch (yaku) {
        case YAKU_FRANXX_ME:                
        case YAKU_HP_REVERSE_FRANXX:
        case YAKU_STRELITZIA_ME:
        case YAKU_HP_REVERSE_STRONG_FRANXX:
        case YAKU_HP_REVERSE_STRELITZIA:
            return true;
        default:
            return false;
    }
}
static int get_gcount_for_addon(YakuType yaku) {
    int r = rand() % 1000; 

    switch (yaku) {
        case YAKU_COMMON_BELL: 
            if (r < 967) return 1; 
            else if (r < 1000) return 3; 
            return 1; 
        case YAKU_CHERRY: 
            if (r < 997) return 1; 
            else if (r < 999) return 3; 
            else return 5; 
        case YAKU_CHANCE_ME: 
            if (r < 750) return 1; 
            else if (r < 982) return 2; 
            else if (r < 998) return 3; 
            else return 5; 
        case YAKU_FRANXX_ME: 
            if (r < 875) return 1; 
            else if (r < 996) return 2; 
            else if (r < 998) return 3; 
            else return 5; 
    }
    return 0;
}
static void perform_game_count_addon(GameData* data, YakuType yaku) {
    int added_games = 0;

    switch (yaku) {
        case YAKU_COMMON_BELL: 
            if ((rand() % 1000) < 61) { 
                added_games = get_gcount_for_addon(yaku);
            }
            break;
        case YAKU_CHERRY:
        case YAKU_CHANCE_ME:
        case YAKU_FRANXX_ME:
            added_games = get_gcount_for_addon(yaku); 
            break;
        case YAKU_HP_REVERSE_STRONG_FRANXX: 
            snprintf(data->info_message, sizeof(data->info_message), "最強フランクス目! EXストック+1");
            BB_EX_Init(data); 
            break;
        case YAKU_STRELITZIA_ME:
        case YAKU_HP_REVERSE_STRELITZIA: 
            snprintf(data->info_message, sizeof(data->info_message), "ストレリチア目! EXストック+1");
            BB_EX_Init(data); 
            break;
        default:
            break;
    }

    if (added_games > 0) {
        data->bonus_high_prob_games += added_games;
        snprintf(data->info_message, sizeof(data->info_message), "G数上乗せ +%dG！", added_games);
    }
}
static void perform_payout_addon(GameData* data, YakuType yaku) {
    int added_payout = 0;
    int r = rand() % 1000; 

    switch (yaku) {
        case YAKU_COMMON_BELL:
            if (r < 8) added_payout = 30;    
            else if (r < 12) added_payout = 50;   
            else if (r < 16) added_payout = 100;  
            break;
        case YAKU_CHERRY:
            if (r < 750) added_payout = 10;   
            else if (r < 992) added_payout = 30;   
            else if (r < 996) added_payout = 50;   
            else if (r < 1000) added_payout = 100;  
            break;
        case YAKU_CHANCE_ME:
            if (r < 750) added_payout = 30;   
            else if (r < 914) added_payout = 50;   
            else if (r < 992) added_payout = 100;  
            else if (r < 996) added_payout = 500;  
            else if (r < 1000) added_payout = 1000; 
            break;
        case YAKU_FRANXX_ME:
            if (r < 871) added_payout = 30;   
            else if (r < 996) added_payout = 50;   
            else if (r < 998) added_payout = 100;  
            else if (r < 999) added_payout = 500;  
            else if (r < 1000) added_payout = 1000; 
            break;
        case YAKU_HP_REVERSE_STRONG_FRANXX:
        case YAKU_STRELITZIA_ME:
        case YAKU_HP_REVERSE_STRELITZIA:
            if (r < 844) added_payout = 100;  
            else if (r < 922) added_payout = 500;  
            else if (r < 1000) added_payout = 1000; 
            break;
        default:
            break;
    }

    if (added_payout > 0) {
        data->target_bonus_payout += added_payout;
        snprintf(data->info_message, sizeof(data->info_message), "差枚数上乗せ +%d枚！", added_payout);
    }
}
static void transition_to_state(GameData* data, AT_State new_state) {
    data->current_state = new_state;
    data->current_bonus_payout = 0; 
    snprintf(data->info_message, sizeof(data->info_message), "%s へ遷移", AT_GetStateName(new_state));

    if (new_state != STATE_BB_EX && new_state != STATE_HIYOKU_BEATS) {
        data->hiyoku_is_active = false;
        data->hiyoku_is_frozen = false;
    }
    
    switch (new_state) {
        case STATE_BB_INITIAL:
            data->target_bonus_payout = PAYOUT_TARGET_BB_INITIAL;
            break;
        case STATE_BONUS_HIGH_PROB:
            data->target_bonus_payout = 0; 
            data->bonus_high_prob_games = GAMES_ON_BB_INITIAL_END;
            break;
        case STATE_BB_HIGH_PROB:
            data->target_bonus_payout = PAYOUT_TARGET_BB_HIGH_PROB;
            break;
        case STATE_FRANXX_BONUS:
            data->target_bonus_payout = PAYOUT_TARGET_FRANXX_BONUS;
            break;
        case STATE_BB_EX:
            data->target_bonus_payout = data->queued_bb_ex_payout > 0 ? data->queued_bb_ex_payout : PAYOUT_TARGET_BB_EX;
            data->queued_bb_ex_payout = 0; 
            break;
        case STATE_HIYOKU_BEATS:
            data->target_bonus_payout = 0; 
            break;
        case STATE_EPISODE_BONUS:
            data->target_bonus_payout = PAYOUT_TARGET_EP_BONUS;
            break;
        case STATE_TSUREDASHI:
            data->target_bonus_payout = PAYOUT_TARGET_TSUREDASHI;
            break;
        case STATE_AT_END:
            data->target_bonus_payout = 0;
            snprintf(data->info_message, sizeof(data->info_message), "AT終了。 総獲得: %lld枚", data->total_payout_diff);
            break;
        default:
             data->target_bonus_payout = 0;
             break;
    }
}
static bool Hiyoku_PerformBonusAllocation(GameData* data, YakuType yaku) {
    int r = rand() % 1000;
    
    switch (yaku) {
        case YAKU_CHANCE_ME: 
            if (r < 336) { data->bonus_stock_count++; return false; } 
            else if (r < 867) { data->bonus_stock_count++; return false; } 
            else { BB_EX_Init(data); return true; } 
        case YAKU_CHERRY: 
            if (r < 250) { data->bonus_stock_count++; return false; } 
            else if (r < 845) { data->bonus_stock_count++; return false; } 
            else { BB_EX_Init(data); return true; } 
        case YAKU_FRANXX_ME: 
            if (r < 336) { data->bonus_stock_count++; return false; } 
            else if (r < 859) { data->bonus_stock_count++; return false; } 
            else { BB_EX_Init(data); return true; } 
        case YAKU_STRELITZIA_ME: 
        case YAKU_HP_REVERSE_STRONG_FRANXX:
            BB_EX_Init(data);
            return true;
        default:
            break;
    }
    return false;
}
static void Hiyoku_PerformLevelUp(GameData* data) {
    int r = rand() % 1000;
    
    if (data->hiyoku_level == HIYOKU_LV1 && r < 215) { 
        data->hiyoku_level = HIYOKU_LV2;
        snprintf(data->info_message, sizeof(data->info_message), "レベル2へ昇格！");
    } 
    else if (data->hiyoku_level == HIYOKU_LV2 && r < 31) { 
        data->hiyoku_level = HIYOKU_MAXX;
        snprintf(data->info_message, sizeof(data->info_message), "レベルMAXXへ昇格！");
    }
}
static void Hiyoku_MainLogic(GameData* data, YakuType yaku) {
    if (!data->hiyoku_is_active) return;
    if (!data->hiyoku_is_frozen) {
        data->hiyoku_st_games--;
    }
    bool reset_st = false;
    int added_games = 0;
    switch (yaku) {
        case YAKU_OSHIJUN_BELL_LMR: case YAKU_OSHIJUN_BELL_LRM:
        case YAKU_OSHIJUN_BELL_MLR: case YAKU_OSHIJUN_BELL_MRL:
        case YAKU_OSHIJUN_BELL_RLM: case YAKU_OSHIJUN_BELL_RML:
            if ((rand() % 100) < 35) added_games = 1;
            break;
        case YAKU_COMMON_BELL: 
        case YAKU_REPLAY:
            added_games = 1;
            break;
        default:
            if (IsRareYaku(yaku)) {
                added_games = 1;
            }
            break;
    }
    if (added_games > 0) {
        data->bonus_high_prob_games += added_games;
        snprintf(data->info_message, sizeof(data->info_message), "G数上乗せ +%dG！", added_games);
        reset_st = true;
    }
    bool bonus_won = false;
    switch (yaku) {
        case YAKU_CHANCE_ME:
        case YAKU_STRELITZIA_ME:
        case YAKU_HP_REVERSE_STRONG_FRANXX: 
            bonus_won = true;
            break;
        case YAKU_FRANXX_ME:
            if (rand() % 2 == 0) bonus_won = true;
            break;
        case YAKU_CHERRY:
            if ((rand() % 1000) < 125) bonus_won = true;
            break;
        default:
            break;
    }
    if (bonus_won) {
        reset_st = true;
        bool is_ex_stock = Hiyoku_PerformBonusAllocation(data, yaku);
        snprintf(data->info_message, sizeof(data->info_message), "%s ストック！", is_ex_stock ? "BB EX" : "ボーナス");
        Hiyoku_PerformLevelUp(data);
    }
    if (reset_st) {
        data->hiyoku_st_games = get_st_games_from_level(data->hiyoku_level);
    } 
    else if (data->hiyoku_st_games <= 0 && !data->hiyoku_is_frozen) {
        data->hiyoku_is_active = false;
        
        if (data->current_state == STATE_HIYOKU_BEATS) {
            transition_to_state(data, STATE_BONUS_HIGH_PROB);
        } else {
            snprintf(data->info_message, sizeof(data->info_message), "比翼BEATS (並行) 終了");
        }
    }
}
static void handle_parallel_hiyoku(GameData* data, YakuType yaku) {
    Hiyoku_MainLogic(data, yaku);
}
static void handle_hiyoku_beats(GameData* data, YakuType yaku) {
    Hiyoku_MainLogic(data, yaku);
}
static void handle_payout_bonus(GameData* data, YakuType yaku, int diff) {
    data->current_bonus_payout += diff;
    bool did_transition = false;

    int progress = (diff > 0) ? diff : 0;
    if (data->hiyoku_is_frozen && data->franxx_bonus_part_remaining > 0) {
        data->franxx_bonus_part_remaining -= progress;
        if (data->franxx_bonus_part_remaining <= 0) {
            data->hiyoku_is_frozen = false; 
            snprintf(data->info_message, sizeof(data->info_message), "フランクスボーナス部 終了！ 比翼BEATS再開！");
        }
    }

    switch (data->current_state) {
        case STATE_BB_INITIAL: 
        case STATE_BB_HIGH_PROB: 
        case STATE_EPISODE_BONUS: 
            if (data->bonus_high_prob_games >= 30) {
                perform_payout_addon(data, yaku);
            } else {
                perform_game_count_addon(data, yaku);
            }
            break;
        case STATE_FRANXX_BONUS: 
            if (is_payout_reset_yaku(yaku)) {
                data->current_bonus_payout = 0; 
                snprintf(data->info_message, sizeof(data->info_message), "差枚リセット！");
            }
            switch (yaku) {
                case YAKU_CHERRY: 
                    if ((rand() % 1000) < 78) {
                        snprintf(data->info_message, sizeof(data->info_message), "連れ出し！比翼BEATS (ホールド)");
                        Hiyoku_Init(data, false); 
                        data->hiyoku_is_frozen = true;
                        data->franxx_bonus_part_remaining = PAYOUT_TARGET_FRANXX_BONUS - data->current_bonus_payout;
                        if (data->franxx_bonus_part_remaining < 0) data->franxx_bonus_part_remaining = 0;
                    }
                    break;
                case YAKU_CHANCE_ME:
                    if (rand() % 2 == 0) {
                        snprintf(data->info_message, sizeof(data->info_message), "連れ出し！比翼BEATS (ホールド)");
                        Hiyoku_Init(data, false); 
                        data->hiyoku_is_frozen = true;
                        data->franxx_bonus_part_remaining = PAYOUT_TARGET_FRANXX_BONUS - data->current_bonus_payout;
                        if (data->franxx_bonus_part_remaining < 0) data->franxx_bonus_part_remaining = 0;
                    }
                    break;
                case YAKU_FRANXX_ME:
                    if (rand() % 4 == 0) {
                        snprintf(data->info_message, sizeof(data->info_message), "連れ出し！比翼BEATS (ホールド)");
                        Hiyoku_Init(data, false); 
                        data->hiyoku_is_frozen = true;
                        data->franxx_bonus_part_remaining = PAYOUT_TARGET_FRANXX_BONUS - data->current_bonus_payout;
                        if (data->franxx_bonus_part_remaining < 0) data->franxx_bonus_part_remaining = 0;
                    }
                    break;
                case YAKU_STRELITZIA_ME:
                case YAKU_HP_REVERSE_STRELITZIA:
                case YAKU_HP_REVERSE_STRONG_FRANXX:
                    snprintf(data->info_message, sizeof(data->info_message), "連れ出し + BB EXへ！");
                    BB_EX_Init(data); 
                    Hiyoku_Init(data, false);
                    data->hiyoku_is_frozen = true;
                    data->franxx_bonus_part_remaining = PAYOUT_TARGET_FRANXX_BONUS - data->current_bonus_payout;
                    if (data->franxx_bonus_part_remaining < 0) data->franxx_bonus_part_remaining = 0;
                    transition_to_state(data, STATE_BB_EX); 
                    did_transition = true;
                    break;
                default: break;
            }
            perform_game_count_addon(data, yaku);
            break;
        case STATE_BB_EX: 
            if (data->bonus_high_prob_games >= 30) {
                perform_payout_addon(data, yaku);
            } else {
                perform_game_count_addon(data, yaku);
            }
            break;
        case STATE_TSUREDASHI: 
            if (is_payout_reset_yaku(yaku)) {
                data->current_bonus_payout = 0;
                snprintf(data->info_message, sizeof(data->info_message), "差枚リセット！");
            }
            break;
        default: break;
    }

    if (did_transition) return; 

    if (data->current_bonus_payout >= data->target_bonus_payout) {
        bool came_from_ep = (data->current_state == STATE_EPISODE_BONUS);
        
        if (data->hiyoku_is_active) {
            transition_to_state(data, STATE_HIYOKU_BEATS); 
            data->hiyoku_is_frozen = false; 
        } 
        else if (data->current_state == STATE_BB_HIGH_PROB || 
                 data->current_state == STATE_BB_EX ||        
                 data->current_state == STATE_EPISODE_BONUS)  
        {
            transition_to_state(data, STATE_HIYOKU_BEATS); 
            Hiyoku_Init(data, came_from_ep); 
        }
        else 
        {
            transition_to_state(data, STATE_BONUS_HIGH_PROB); 
        }
    }
}
static bool roll_high_prob_success(YakuType yaku) {
    int r = rand() % 1000;
    switch (yaku) {
        case YAKU_REPLAY:       return r < 318; // 31.8%
        case YAKU_COMMON_BELL:  return r < 379; // 37.9%
        default:
            if (IsRareYaku(yaku)) return true;
            return false;
    }
}
static bool perform_bonus_allocation(GameData* data, YakuType yaku) {
    if (!roll_high_prob_success(yaku)) {
        return false;
    }

    int r = rand() % 1000; 
    switch (yaku) {
        case YAKU_REPLAY: 
            if (r < 370) transition_to_state(data, STATE_FRANXX_BONUS); 
            else if (r < 870) transition_to_state(data, STATE_BB_HIGH_PROB); 
            else { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); }
            break;
        case YAKU_COMMON_BELL: 
            if (r < 380) transition_to_state(data, STATE_FRANXX_BONUS); 
            else if (r < 870) transition_to_state(data, STATE_BB_HIGH_PROB); 
            else { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); }
            break;
        case YAKU_CHANCE_ME: 
            if (r < 1) transition_to_state(data, STATE_EPISODE_BONUS); 
            else if (r < 201) { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); } 
            else transition_to_state(data, STATE_BB_HIGH_PROB); 
            break;
        case YAKU_CHERRY: 
            if (r < 500) transition_to_state(data, STATE_FRANXX_BONUS); 
            else if (r < 900) transition_to_state(data, STATE_BB_HIGH_PROB); 
            else { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); }
            break;
        case YAKU_FRANXX_ME: 
            if (r < 1) transition_to_state(data, STATE_EPISODE_BONUS); 
            else if (r < 376) { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); } 
            else if (r < 751) transition_to_state(data, STATE_BB_HIGH_PROB); 
            else transition_to_state(data, STATE_FRANXX_BONUS); 
            break;
        case YAKU_STRELITZIA_ME: 
            if (r < 4) transition_to_state(data, STATE_EPISODE_BONUS); 
            else { BB_EX_Init(data); transition_to_state(data, STATE_BB_EX); } 
            break;
        default:
            return false;
    }
    return true;
}
static void handle_bonus_high_prob(GameData* data, YakuType yaku) {
    data->bonus_high_prob_games--;

    bool bonus_won = false;
    if (yaku == YAKU_REPLAY || yaku == YAKU_COMMON_BELL || IsRareYaku(yaku)) {
        bonus_won = perform_bonus_allocation(data, yaku);
    }

    if (!bonus_won && data->bonus_high_prob_games <= 0) {
        transition_to_state(data, STATE_AT_END);
    }
}


// --- 公開関数 (★修正済み) ---

void AT_Init(GameData* data) {
    transition_to_state(data, STATE_BB_INITIAL);
    // (★) 演出フラグのセットを削除
}

/**
 * @brief (★変更) AT中の【全リール停止時】のゲームロジックを更新
 * @param yaku 【レバーオン時】に成立した役
 * @param oshijun_success 押し順が正解だったか
 */
void AT_ProcessStop(GameData* data, YakuType yaku, bool oshijun_success) {
    snprintf(data->last_yaku_name, sizeof(data->last_yaku_name), "%s", GetYakuName(yaku));
    data->info_message[0] = '\0';
    data->oshijun_success = oshijun_success;
    
    // (★) 演出用のダミー役チェックを削除

    int payout = GetPayoutForYaku(yaku, data->oshijun_success);
    int diff = payout - BET_COUNT;
    data->total_payout_diff += diff;

    // (★FIX) 比翼ビーツ中は「並行比翼」の処理を呼ばない（Hiyoku_MainLogic の二重実行防止）
    bool run_parallel_hiyoku = (data->hiyoku_is_active && data->current_state != STATE_HIYOKU_BEATS);
    if (run_parallel_hiyoku) {
        handle_parallel_hiyoku(data, yaku);
    }

    // (↓) これ以降のロジックは、引数 'yaku' と計算済み 'diff' を使っており変更なし
    switch (data->current_state) {
        case STATE_BB_INITIAL:
        case STATE_BB_HIGH_PROB:
        case STATE_FRANXX_BONUS:
        case STATE_BB_EX:
        case STATE_EPISODE_BONUS:
        case STATE_TSUREDASHI:
            handle_payout_bonus(data, yaku, diff);
            break;
        case STATE_BONUS_HIGH_PROB:
            handle_bonus_high_prob(data, yaku);
            break;
        case STATE_HIYOKU_BEATS:
            handle_hiyoku_beats(data, yaku); 
            break;
        default:
            break;
    }
}

const char* AT_GetStateName(AT_State state) {
    const char* names[] = {
        "通常時",
        "CZ",
        "BB[初当り]",
        "ボーナス高確率",
        "BB[高確中]",
        "フランクスボーナス",
        "BB EX",
        "比翼BEATS",
        "EPボーナス",
        "連れ出し",
        "AT終了"
    };
    if (state >= STATE_NORMAL && state <= STATE_AT_END) {
        return names[state];
    }
    return "不明な状態";
}