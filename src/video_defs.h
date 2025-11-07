#ifndef VIDEO_DEFS_H
#define VIDEO_DEFS_H

typedef enum {
    VIDEO_NONE = -1, // 再生停止/非再生
    
    // (★) 待機中のループ動画
    VIDEO_IDLE,      
    
    // (★) ボーナス揃い演出動画
    VIDEO_ENSHUTSU_AKA7, 
    
    // (★) 通常回転中の動画
    VIDEO_SPIN,

    // ... 今後ここに追加 ...
    
    VIDEO_COUNT // 常に最後
} VideoType;

#endif // VIDEO_DEFS_H