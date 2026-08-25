#line 1 "D:\\M5stick_proj\\self_workspace\\pet.h"
#pragma once
#include "globals.h"

void petInit();              // setup：挂载 LittleFS + 读 manifest + 建宠物画布
void petSetState(uint8_t s); // 切换状态（0=idle, 1=sleep, 2=dizzy, 3=busy）
void petTick();              // loop 每轮：推进动画帧（内部解码到 petSprite）
bool petIsDirty();           // 宠物帧是否变了（updateDisplay 用）
void petRender();            // 把宠物画布叠到主 canvas
void petClearDirty();        // 清脏标记（updateDisplay 推屏后调）
