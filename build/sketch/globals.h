#line 1 "D:\\M5stick_proj\\self_workspace\\globals.h"
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5Unified.h>  
#include "config.h"

extern Config cfg;
extern Preferences prefs;
extern bool timeSynced;
extern unsigned long lastWifiAttempt;   //上次尝试连WIFI的时间戳
extern unsigned long lastNtpAttempt;    //上次尝试同步时间的时间戳

extern double balance;  //当前余额
extern bool   balanceValid;   //是否有有效值
extern bool   balanceExpired; //值是否过期
extern bool   balanceForce;   //强制立即查询
extern unsigned long lastBalanceFetch;   //上次查询时间戳

//显示模块 canvas画布
extern M5Canvas canvas; //双缓冲画布，所有绘制先进这里再推屏
extern M5Canvas peakSprite[2];  //峰谷大字预渲染
extern int page;    //当前页：0=主页，1=详情页
extern bool fullRedraw; //强制重绘标志（切页，配置变更）

struct  FrameSnap   
{   //帧内容快照
    char time[24];  //顶部时间串
    char countdown[16]; //倒计时串
    char    balance[32];//余额串
    uint8_t PeakImg;    //0未同步，1峰 2谷
    uint8_t page;       //页码
    uint8_t lowBalance;  //余额告警态
    uint8_t agentState;  //智能体状态：0 idle 1 thinking 2 await_approval（变化时触发重绘）
};

extern FrameSnap    prevSnap;   //上一帧快照
extern bool snapValid;  //快照是否有效
extern uint8_t agentState;
extern unsigned long approvedFlashUntil;  //KEY1 按下后的 OK! 反馈截止时间（web.cpp 写入）
extern unsigned long approvalDeadline;    //await_approval 批准截止时间戳（ms），倒计时用
extern char agentToolDesc[40];            //待批准命令简短概括

