#ifndef VIDEO_INTERNAL_H
#define VIDEO_INTERNAL_H

#include <SDL2/SDL.h>
#include <stdbool.h>

// FFMPEGのグローバル初期化
bool Video_Internal_Init();
// FFMPEGのグローバル終了処理
void Video_Internal_Cleanup();

// 動画ファイルを開く
bool Video_Internal_Open(SDL_Renderer* renderer, const char* filepath, bool loop);

// (★) <<< 修正点: 引数を (SDL_Renderer* renderer) に変更
// 動画を停止・閉じる
void Video_Internal_Stop(SDL_Renderer* renderer);

// 毎フレーム更新
void Video_Internal_Update();
// 描画
void Video_Internal_Draw(SDL_Renderer* renderer, const SDL_Rect* destRect);

// 終了したか確認
bool Video_Internal_IsFinished();
// ループ設定を変更
void Video_Internal_SetLoop(bool loop);

#endif // VIDEO_INTERNAL_H