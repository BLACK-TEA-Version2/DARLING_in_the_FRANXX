#ifndef VIDEO_H
#define VIDEO_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "video_defs.h" // (★) 動画タイプ定義をインクルード

/**
 * @brief (★廃止・Video_Playに統合)
 */

/**
 * @brief 動画再生モジュールを終了処理します。
 */
void Video_Cleanup();

/**
 * @brief 動画の次のフレームを準備します。
 */
void Video_Update();

/**
 * @brief 現在の動画フレームを画面の【リール以外の領域】に描画します。
 */
void Video_Draw(SDL_Renderer* renderer, int screen_width, int screen_height);

/**
 * @brief 動画が再生終了したか確認します (ループ再生時は常にfalse)。
 */
bool Video_IsFinished();

/**
 * @brief (★新規) 指定された種類の動画再生を開始する
 * @param type 再生したい動画の種類 (VideoType)
 * @param loop ループ再生するかどうか
 * @return 初期化・再生開始に成功したら true
 */
bool Video_Play(SDL_Renderer* renderer, VideoType type, bool loop);

#endif // VIDEO_H