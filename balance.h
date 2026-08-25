// balance.h —— 余额模块：DeepSeek GET /user/balance 异步查询
#pragma once
#include "globals.h"

void manageBalance();    // loop 每轮调用
// void balanceAbort();     // 配置变更/重连时中断进行中的查询
