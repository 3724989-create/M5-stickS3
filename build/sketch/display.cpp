#line 1 "D:\\M5stick_proj\\self_workspace\\display.cpp"
#include "display.h"
#include "pet.h"           // ← 新增：宠物渲染
#include "peak_font.h"
#include    <time.h>
#include    <WiFi.h>

// 颜色（峰谷黄/绿 + 余额红，红色是余额专属）
#define CLR_BG   M5.Display.color565(10, 12, 16)
#define CLR_FG   M5.Display.color565(230, 234, 240)
#define CLR_DIM  M5.Display.color565(120, 128, 140)
#define CLR_PEAK M5.Display.color565(255, 200, 50)
#define CLR_OFF  M5.Display.color565(64, 208, 120)
#define CLR_WARN M5.Display.color565(255, 70, 70)

void displayInit(){

    //主双缓冲画布（有限PSRAM)
    canvas.setPsram(true);
    if(!canvas.createSprite(135,240)){
        canvas.setPsram(false);
        canvas.createSprite(135,240);
    }
    //预渲染
    const struct PeakPixel *tables[2]={PEAK_WENFENG,PEAK_WENGU};
    const int counts[2]={PEAK_WENFENG_N,PEAK_WENGU_N};  //字符像素个数
    for(int i=0;i<2;i++){
        peakSprite[i].setPsram(true);   //有的话直接返回
        if (!peakSprite[i].createSprite(PEAK_W,PEAK_H)){
            //如果没有创建，则删除原区域重新创建
            peakSprite[i].setPsram(false);
            peakSprite[i].createSprite(PEAK_W,PEAK_H);
        }
        peakSprite[i].fillScreen(CLR_BG);   //背景色
        for(int j=0;j<counts[i];j++){
            //逐像素填入
            peakSprite[i].drawPixel(tables[i][j].x, tables[i][j].y, tables[i][j].c);
        }
    }
}

// 画一个进度条：x,y 左上角；w,h 大小；ratio 0.0~1.0；color 填充色
static void drawBar(int x, int y, int w, int h, float ratio, uint16_t color) {
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  canvas.drawRect(x, y, w, h, CLR_DIM);              // 背景边框
  int fw = (int)((w - 2) * ratio);                   // 按比例算填充宽
  if (fw > 0) canvas.fillRect(x + 1, y + 1, fw, h - 2, color);  // 实心填充
}

static void renderMainToCanvas(const FrameSnap &cur){
    // 主页布局：时间/倒计时在上，宠物居中，峰谷+余额在下
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(CLR_DIM);
    canvas.drawString(cur.time, canvas.width() / 2, 14);       // 时间
    canvas.setFont(&fonts::efontCN_16);
    canvas.setTextColor(CLR_FG);
    canvas.drawString(cur.countdown, canvas.width() / 2, 31);  // 倒计时

    // 宠物区 y 60~160：内容由 petRender() 叠上来，这里留空
    // （峰谷状态「梁文峰/梁文谷」已移到详情页）
    // ===== 宠物下方：血条（电量） + 法力条（余额） =====
    int batt = M5.Power.getBatteryLevel();
    if (batt < 0) batt = 0;
    float hpRatio = batt / 100.0f;                       // 电量 → 血量比例
    uint16_t hpColor = batt > 50 ? CLR_OFF : (batt > 20 ? CLR_PEAK : CLR_WARN);
    drawBar(24, 160, 44, 6, hpRatio, hpColor);           // 左：血条

    float mpRatio = 0;
    if (balanceValid && balance > 0) {
    mpRatio = min((float)(balance / 50.0), 1.0f);     // 余额 → 法力（50元满）
    }
    drawBar(70, 160, 44, 6, mpRatio, 0x4DB6);            // 右：法力条（蓝色）

        // ===== 智能体状态（最底部）=====
        canvas.setFont(&fonts::efontCN_24);   // ★ 放大到大号字体
        canvas.setTextDatum(middle_center);
        if (millis() < approvedFlashUntil) {
            // ★ 刚按过 KEY1（1.5s 内）→ 绿色 OK! 即时反馈，不等 PC 推状态
            canvas.setTextColor(CLR_OFF);
            canvas.drawString("OK!", canvas.width() / 2, 190);
        } else if (agentState == 1) {
            canvas.setTextColor(CLR_PEAK);                 // 黄
            canvas.drawString("thinking...", canvas.width() / 2, 190);
        } else if (agentState == 2) {
            canvas.setTextColor(CLR_PEAK);
            canvas.drawString("continue?", canvas.width() / 2, 190);
            // ★ 下方两行小字：倒计时（上）+ 命令概括（下）
            unsigned long now2 = millis();
            unsigned long remMs = (approvalDeadline > now2) ? (approvalDeadline - now2) : 0UL;
            int remSec = (int)((remMs + 999UL) / 1000UL);
            char lineCount[24];
            snprintf(lineCount, sizeof(lineCount), "剩余 %ds", remSec);
            canvas.setFont(&fonts::efontCN_12);
            canvas.setTextColor(CLR_DIM);
            canvas.drawString(lineCount, canvas.width() / 2, 212);
            if (agentToolDesc[0]) {
                canvas.drawString(agentToolDesc, canvas.width() / 2, 228);
            }
            canvas.setFont(&fonts::efontCN_24);   // 恢复大号字体
        } else {
            canvas.setTextColor(CLR_DIM);
            canvas.drawString("idle", canvas.width() / 2, 190);
        }
}

static void renderInfoToCanvas(const FrameSnap &cur){
    // 详情页：峰谷大字 + 余额 + 状态信息
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(CLR_DIM);
    canvas.drawString("峰谷状态", canvas.width() / 2, 10);

    // 峰谷大字（黄/绿，详情页主角）
    canvas.setFont(&fonts::efontCN_24);
    if (cur.PeakImg == 1) {
        canvas.setTextColor(CLR_PEAK);
        canvas.drawString("梁文峰", canvas.width() / 2, 40);
        canvas.setFont(&fonts::efontCN_12);
        canvas.setTextColor(CLR_DIM);
        canvas.drawString("高峰期", canvas.width() / 2, 62);
    } else if (cur.PeakImg == 2) {
        canvas.setTextColor(CLR_OFF);
        canvas.drawString("梁文谷", canvas.width() / 2, 40);
        canvas.setFont(&fonts::efontCN_12);
        canvas.setTextColor(CLR_DIM);
        canvas.drawString("非高峰期", canvas.width() / 2, 62);
    } else {
        canvas.setTextColor(CLR_DIM);
        canvas.drawString("未同步", canvas.width() / 2, 40);
    }

    // 余额
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextColor(CLR_DIM);
    canvas.drawString("余额", canvas.width() / 2, 90);
    canvas.setFont(&fonts::efontCN_16);
    canvas.setTextColor(cur.lowBalance ? CLR_WARN : CLR_FG);
    canvas.drawString(cur.balance, canvas.width() / 2, 106);

    // 状态信息
    canvas.setFont(&fonts::efontCN_12);
    canvas.setTextDatum(middle_left);
    canvas.setTextColor(CLR_DIM);
    int y = 132;
    char line[64];
    snprintf(line, sizeof(line), "时段  %s", cfg.peakcount > 0 ? "自定义" : "官方默认");
    canvas.drawString(line, 10, y); y += 18;
    snprintf(line, sizeof(line), "WiFi  %s", cfg.haswifi() ? cfg.ssid.c_str() : "未配置");
    canvas.drawString(line, 10, y); y += 18;
    snprintf(line, sizeof(line), "IP    %s", WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-");
    canvas.drawString(line, 10, y); y += 18;
    snprintf(line, sizeof(line), "NTP   %s", cfg.ntp.c_str());
    canvas.drawString(line, 10, y); y += 18;
    int batt = M5.Power.getBatteryLevel();
    snprintf(line, sizeof(line), "电量  %d%%", batt < 0 ? 0 : batt);
    canvas.drawString(line, 10, y); y += 18;

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(CLR_DIM);
    canvas.drawString("KEY2 返回", canvas.width() / 2, 228);
}

static const char *weekdayCn(int wday) {
  static const char *map[7] = {"周日","周一","周二","周三","周四","周五","周六"};
  return map[wday];
}

void updateDisplay(){
    time_t now=time(nullptr);
    bool synced=now>1600000000L; // 刚算的：现在时间同步了吗？
    if(synced!=timeSynced){  // 两者不一样 = 状态"刚翻转"（刚同步上/刚丢失）
        timeSynced=synced;   // 更新全局标志
        fullRedraw=true;     // 强制重绘
    }
    
    struct tm tmv;
    localtime_r(&now,&tmv);
    int md=tmv.tm_hour*60+tmv.tm_min;
    bool peak=synced&&inPeakWindow(cfg,md);

    //算这一帧的内容快照
    FrameSnap cur;
    memset(&cur,0,sizeof(cur));
    if(synced){
        snprintf(cur.time,sizeof(cur.time),"%02d:%02d:%02d %02d-%02d %s",
        tmv.tm_hour,tmv.tm_min,tmv.tm_sec,tmv.tm_mon+1,tmv.tm_mday,
        weekdayCn(tmv.tm_wday));
        long secs=secondsToNextSwitch(cfg, now, md);
        if(secs<0)secs=0;
        snprintf(cur.countdown, sizeof(cur.countdown), "%02ld:%02ld:%02ld",
            secs / 3600, (secs / 60) % 60, secs % 60);
        cur.PeakImg=peak?1:2;
    }else{
        snprintf(cur.time,sizeof(cur.time), "--:--:-- 等待同步");
        snprintf(cur.countdown, sizeof(cur.countdown), "--:--:--");
        cur.PeakImg=0;
    }
    if(balanceValid){
        snprintf(cur.balance,sizeof(cur.balance),"%.2f元",balance);
        cur.lowBalance=(balance<(double)cfg.balance_warn);
    }else{
        snprintf(cur.balance, sizeof(cur.balance), "--");
    }
    cur.page=(uint8_t)page;
    cur.agentState=agentState;   // ★ agent 状态进快照，变化才触发重绘

    //diff 内容没强制
    bool changed=fullRedraw||!snapValid||
        memcmp(&cur,&prevSnap,sizeof(FrameSnap))!=0 ||
        petIsDirty();                 // ★ 宠物帧变了也要重绘
    if(!changed)return;

    //重绘到画布
    canvas.fillScreen(CLR_BG);
    if (page == 0) { renderMainToCanvas(cur); petRender(); }   // 主页 + 宠物
    else          renderInfoToCanvas(cur);    // 详情页
    canvas.pushSprite(0, 0);
    petClearDirty();                   // ★ 推屏后清宠物脏标记

    prevSnap = cur;
    snapValid = true;
    fullRedraw = false;
}