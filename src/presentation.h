#ifndef PRESENTATION_H
#define PRESENTATION_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/**
 * @brief 演出モジュールを初期化します。
 * (内部で video モジュールと SDL_image を初期化します)
 */
bool Presentation_Init(SDL_Renderer* renderer);

/**
 * @brief 演出モジュールを終了します。
 */
void Presentation_Cleanup();

/**
 * @brief メディアファイル（動画 or 静止画）を再生（表示）します。
 * * 拡張子で自動判別します (.mp4, .avi, .mkv -> 動画 / .png, .jpg, .bmp -> 静止画)
 * @param renderer SDLレンダラ
 * @param filepath メディアファイルのパス
 * @param loop ループ再生するか (静止画の場合は無視されます)
 * @return 成功した場合は true
 */
bool Presentation_Play(SDL_Renderer* renderer, const char* filepath, bool loop);

/**
 * @brief 現在のメディアを停止します (待機状態に戻します)。
 */
void Presentation_Stop();

/**
 * @brief 毎フレームの更新処理。
 * (主に動画のデコードフレームを進めるために使います)
 */
void Presentation_Update();

/**
 * @brief 画面に現在のメディアフレームを描画します。
 */
void Presentation_Draw(SDL_Renderer* renderer, int screen_width, int screen_height);

/**
 * @brief 1回再生のメディアが終了したかを返します。
 * (ループ再生中、または静止画表示中は常に false を返します)
 */
bool Presentation_IsFinished();


#endif // PRESENTATION_H