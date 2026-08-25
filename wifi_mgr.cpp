#include "wifi_mgr.h"
#include <WiFi.h>        // WiFi API（ESP32 板包自带）
#include <time.h>        // time_t / time()（时间同步用）

void networkInit(){
    if(cfg.haswifi()){
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.ssid.c_str(),      // String → C 字符串
            cfg.password.length() ? cfg.password.c_str() : NULL);
    }else{
        WiFi.mode(WIFI_OFF);
    }
    configTime(8*3600,0,cfg.ntp.c_str());   // 北京时间 = UTC+8
    lastWifiAttempt=lastNtpAttempt=millis();
}

void manageWiFi(){
    if(!cfg.haswifi())return;       //没配置，退出

    static bool lastConnected=false;    //记住上一次状态
    bool nowConnected=(WiFi.status()==WL_CONNECTED);
    if(nowConnected&&!lastConnected){
        Serial.print("[WiFi] 已连接, IP=");
        Serial.println(WiFi.localIP().toString());
    }
    if (!nowConnected && lastConnected) {       // 刚刚断开，只在变化的时候发送串口命令
        Serial.println("[WiFi] 已断开");
    }

    lastConnected = nowConnected;

    if(WiFi.status()!= WL_CONNECTED &&
       millis()-lastWifiAttempt>30000UL ){  //断线超过30s
        lastWifiAttempt=millis();   //更新上次尝试的时间
        WiFi.begin(cfg.ssid.c_str(),      // String → C 字符串
        cfg.password.length() ? cfg.password.c_str() : NULL);
    }
}

#define NTP_RESYNC_MS (6UL*3600UL*1000UL)   //六小时校准一次时间

void manageNtp(){
    if(WiFi.status()!=WL_CONNECTED)return;//没网直接退出

    time_t now=time(nullptr);   //nullptr表示不需要返回值
    if(!timeSynced&&now>1600000000L){
        timeSynced=true;    // epoch 超过 2020 年 → 同步成功
        Serial.println("[NTP] 时间已同步");   // ← 新增
    }

    unsigned long interval=timeSynced?NTP_RESYNC_MS:30000UL;
    if(millis()-lastNtpAttempt>interval){
        lastNtpAttempt=millis();
        configTime(8*3600,0,cfg.ntp.c_str());   // 北京时间 = UTC+8
    }
}