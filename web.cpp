#include "web.h"
#include <WebServer.h>
#include <ArduinoJson.h>

uint8_t agentState;
static WebServer server(80);
static bool approved=false; // KEY1 批准标志（host GET 读走后清）
unsigned long approvedFlashUntil=0; // KEY1 按下后的屏幕 OK! 反馈截止时间（ms）
unsigned long approvalDeadline=0;   // await_approval 批准截止时间戳（ms），用于倒计时
char agentToolDesc[40]="";          // 待批准命令的简短概括（host 推送时带上）
static const int BEEP_VOLUME = 60; // BB 提示音音量 0-255（255 太吵，180 适中）
// BB 提示音状态机：0=空闲 1=第一声 2=间隔 3=第二声（continue? 出现时响两声）
static int  beepStage=0;
static unsigned long beepNextAt=0;

// POST /api/agent_status：host 推状态
static void handleAgentStatus() {        
    String body = server.arg("plain");   
    JsonDocument doc;
    Serial.print("[web] POST body: ");   // ★ 调试行
    Serial.println(body);                 // ★ 调试行
    if (deserializeJson(doc, body)) {//__把收到的 JSON 文本"翻译"成代码能直接用的数据__
        server.send(400, "application/json", "{\"ok\":false}");
        return;
    }else{
        const char *st=doc["state"]|"";
        if(strcmp(st,"thinking")==0)agentState=1;
        else if(strcmp(st,"await_approval")==0){
            agentState=2;
            approved=false;   // ★ 清掉旧的预批准标志：必须看到 continue? 后新按 KEY1 才生效
            beepStage=1;      // ★ 触发两次短 BB 提示音
            beepNextAt=millis();
            // ★ 解析倒计时秒数与命令摘要（host 推送时带上）
            int tmo = doc["timeout"] | 60;
            if(tmo<1)tmo=60;
            if(tmo>180)tmo=180;
            approvalDeadline = millis() + (unsigned long)tmo*1000UL;
            const char *tl = doc["tool"] | "";
            snprintf(agentToolDesc, sizeof(agentToolDesc), "%s", tl);
            fullRedraw=true;   // ★ 立即重绘（显示倒计时+命令摘要）
        }
        else { agentState=0; fullRedraw=true; }   // ★ 其它状态（idle/未知）重置回 0 并重绘
        server.send(200, "application/json", "{\"ok\":true}");
    }
}

// GET /api/agent_status：host 轮询（读状态 + 批准标志，读后清除）
static void handleGetStatus(){
    JsonDocument doc;
    doc["state"] = (agentState == 1) ? "thinking" :
                (agentState == 2) ? "await_approval" : "idle";
    doc["approved"]=approved;
    approved=false;
    String out;
    serializeJson(doc,out);
    server.send(200, "application/json", out);
}

void webInit() {
  M5.Speaker.setVolume(BEEP_VOLUME);   // ★ BB 提示音音量
  server.on("/api/agent_status", HTTP_POST, handleAgentStatus); //注册post
  server.on("/api/agent_status", HTTP_GET,  handleGetStatus);   //注册get
  server.begin();   //之后自动调用
}

void webHandle() { server.handleClient(); }   // 必须每轮 loop 调！

void webApprove() {
    approved = true;
    approvedFlashUntil = millis() + 1500;  // ★ 屏幕短暂显示 OK! 作按键即时反馈
    fullRedraw = true;                      // ★ 强制下一帧重绘（按键反馈不迟到）
}

// 蜂鸣提示音状态机：continue? 出现时响两声短 BB（非阻塞）
void webBeepTick(){
    unsigned long now=millis();
    if(beepStage==0||now<beepNextAt)return;
    if(beepStage==1||beepStage==3){
        M5.Speaker.tone(2000, 120, BEEP_VOLUME);   // 短音：2kHz、120ms
        beepStage++;
        beepNextAt=now+150;               // 两声响之间间隔 150ms
    }else{
        beepStage++;
        beepNextAt=now+20;
    }
    if(beepStage>3)beepStage=0;
}