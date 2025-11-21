#ifndef DIRECTOR_H
#define DIRECTOR_H

#include <SDL2/SDL.h>
#include <stdbool.h>

/**
 * @brief 演出制御モジュール(Director)を初期化します。
 * @return 成功したら true
 */
bool Director_Init(SDL_Renderer* renderer);

/**
 * @brief Directorを終了し、リソースを解放します。
 */
void Director_Cleanup();

/**
 * @brief 毎フレーム呼び出すメイン更新処理。
 * 入力処理、状態遷移、動画再生、リール制御、ロジック更新をすべて統括します。
 */
void Director_Update();

/**
 * @brief 毎フレーム呼び出す描画処理。
 * 動画、リール、UIなどを適切な順序で描画します。
 */
void Director_Draw(SDL_Renderer* renderer, int screen_width, int screen_height);

#endif // DIRECTOR_H