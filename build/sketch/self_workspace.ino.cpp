#include <Arduino.h>
#line 1 "D:\\M5stick_proj\\self_workspace\\self_workspace.ino"
#include <M5Unified.h>
#include "serial.h"
#include "globals.h"
#include "wifi_mgr.h"
#include "balance.h"
#include "display.h"
#include "pet.h"
#include "web.h"

Config cfg;
Preferences prefs;
bool timeSynced =false;
unsigned long lastWifiAttempt=0;   //上次尝试连WIFI的时间戳
unsigned long lastNtpAttempt=0;    //上次尝试同步时间的时间戳

double balance = 0.0;
bool   balanceValid = false;
bool   balanceExpired = false;
bool   balanceForce = true;      // 开机强制查一次
unsigned long lastBalanceFetch = 0;

M5Canvas canvas(&M5.Display);   //绑定同一块屏幕M                                         5 &M5.Display是全局屏幕对象的地址
M5Canvas peakSprite[2] = { M5Canvas(&M5.Display), M5Canvas(&M5.Display) };
int  page = 0;
bool fullRedraw = true;
FrameSnap prevSnap;
bool snapValid = false;


#line 30 "D:\\M5stick_proj\\self_workspace\\self_workspace.ino"
void setup();
#line 46 "D:\\M5stick_proj\\self_workspace\\self_workspace.ino"
void loop();
#line 30 "D:\\M5stick_proj\\self_workspace\\self_workspace.ino"
void setup()
{
	auto m5cfg=M5.config();
    M5.begin(m5cfg);
    M5.Display.setRotation(0);
    Serial.begin(115200);   // StickS3 原生 USB CDC
    delay(300);             // 等 USB 枚举稳定，否则上电瞬间的 hello 会丢

    cfgLoad(cfg,prefs);
    networkInit();
    displayInit();
    petInit();          // ★ 新增：挂载 LittleFS + 读 manifest + 建宠物画布
    webInit();
    sendHello();
}

void loop()
{
	M5.update();   //调用按键等
    webHandle();
    webBeepTick();  // ★ continue? 出现时响两声 BB 提示
    if (M5.BtnA.wasClicked()) {
        if (page == 0) {                    // 主页：KEY1 任何状态都武装批准
            webApprove();                   // 置 approved=true（host 轮询读走即清）
            // 不本地回 idle：保持 continue?/thinking 显示，等 PC 推下一条状态（PostToolUse -> thinking）
        } else {                            // 详情页：刷新余额
            if (cfg.apikey.length()) {
                balanceForce = true;
                balanceValid = false;
            }
        }
    }
    // ★ KEY2（BtnB）：切换页面
    if (M5.BtnB.wasClicked()) {
        page = 1 - page;
        fullRedraw = true;
    }
    
    pump_Serial(); //发送的数据要有回车
    manageNtp();
    manageWiFi();
    manageBalance();
    petTick();          // ★ 新增：推进宠物动画
    updateDisplay();
}

