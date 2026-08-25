#line 1 "D:\\M5stick_proj\\self_workspace\\balance.cpp"
#include "balance.h"
#include "certs.h"  //ROOT_CA
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

//常量
#define BALANCE_HOST        "api.deepseek.com"
#define BALANCE_PATH        "/user/balance"
#define BALANCE_INTERVAL_MS (5UL * 60UL * 1000UL)   // 5 分钟
#define BS_READ_SILENCE_MS  300    // 读响应静默超时
#define BS_READ_TOTAL_MS    5000   // 读响应整体超时

//模块私有状态
enum BalanceState{BS_IDLE,BS_CONNECTING,BS_SENDING,BS_READING,BS_PARSING};
static BalanceState bsState=BS_IDLE;
static WiFiClientSecure bsClient;   //跨状态激活
static String           bsResp;     //累积相应
static uint32_t         bsLastData;  //静默超时
static uint32_t         bsReadStart; //整体超时
static double           bsResult;   //解析结果



//余额查询收尾
static void finishBalance(bool ok){
    if(ok){
        balance=bsResult;
        balanceValid=true;
        balanceExpired=false;
    }else{
        balanceExpired=balanceValid;
    }
  lastBalanceFetch = millis();
  balanceForce = false;
  bsClient.stop();
  bsResp = "";
  bsState = BS_IDLE;
}

static bool parseBalanceResponse(){
    int split=bsResp.indexOf("\r\n\r\n");//头体分离，http协议自带的
    if(split<0)return false;
    String body=bsResp.substring(split+4);  // 分界线之后 = JSON 身体

    String head=bsResp.substring(0,split);// 分界线之前 = HTTP 头
    int sp=head.indexOf(' ');
    int status=(sp>0)?atoi(head.c_str()+sp+1):0;
    if(status!=200)return false;

    JsonDocument doc;
    if(deserializeJson(doc,body))return false;
    JsonArray infos=doc["balance_infos"].as<JsonArray>();
    if(infos.isNull()||infos.size()==0)return false;
    const char *tb=infos[0]["total_balance"]|"";
    if(!tb||!*tb)return false;
    bsResult=strtod(tb,nullptr);// 字符串 → double
    return true;
}

void manageBalance(){
    if(bsState==BS_IDLE){
        bool due=cfg.apikey.length()>0&&
            WiFi.status()==WL_CONNECTED&&
            (balanceForce||millis()-lastBalanceFetch>BALANCE_INTERVAL_MS);   // ★ 修复: balance→balanceForce
        if(!due) return;
        Serial.println("[余额] 开始查询...");     // ★ 探针1
        bsResp="";
        bsClient.setCACert(ROOT_CA);    //喂证书
        bsClient.setHandshakeTimeout(6);    //设置握手超时
        bsState=BS_CONNECTING;
        //直接落到下面switch执行CONNECTING
    }
    switch(bsState){
        case BS_CONNECTING:{//TLS握手，同步1-2s
            if(bsClient.connect(BALANCE_HOST, 443)){    // ★ 修复: . → ,
                Serial.println("[余额] TLS 连接成功");   // ★ 探针2a
                bsState=BS_SENDING;
            }else{
                Serial.println("[余额] TLS 连接失败");   // ★ 探针2b
                finishBalance(false);
            }
            break;
        }
        case BS_SENDING:{
            bsClient.print("GET " BALANCE_PATH " HTTP/1.1\r\n");        // ★ 修复: 补空格
            bsClient.print("Host: " BALANCE_HOST "\r\n");               // ★ 修复: 补空格
            bsClient.print("Authorization: Bearer ");                    // ★ 修复: 补空格
            bsClient.print(cfg.apikey);
            bsClient.print("\r\nConnection: close\r\n\r\n");
            Serial.println("[余额] 请求已发送");          // ★ 探针3
            bsState=BS_READING;
            bsReadStart=bsLastData=millis();
            break;                                      // ★ 修复: 补 break
        }
        case BS_READING:{
            while (bsClient.available()&&bsResp.length()<4096)
            {
                int c=bsClient.read();
                if(c<0)break;
                bsResp+=(char)c;    //读数据
                bsLastData=millis();
            }
            if(!bsClient.connected()&&!bsClient.available()){
                bsState=BS_PARSING; //parsing:解析
                Serial.print("[余额] 响应收完, 长度=");   // ★ 探针4a
                Serial.println(bsResp.length());
            }else if(millis()-bsLastData>BS_READ_SILENCE_MS){
                bsState=BS_PARSING;
                Serial.print("[余额] 静默超时, 长度=");   // ★ 探针4b
                Serial.println(bsResp.length());
            }else if(millis()-bsReadStart>BS_READ_TOTAL_MS){
                Serial.println("[余额] 整体超时");        // ★ 探针4c
                finishBalance(false);
            }
            break;
        }
        case BS_PARSING:{
            bool ok=parseBalanceResponse();
            if(ok){
                Serial.print("[余额] 查询成功: ");
                Serial.println(String(bsResult, 2));   // ★ 修复: 打 bsResult（刚解析的值）
                // Serial.print("[余额] 原始响应: ");      // ← 新增
                // Serial.println(bsResp.substring(bsResp.indexOf("\r\n\r\n") + 4));
            }else{
                Serial.println("[余额] 解析失败");         // ★ 探针5b
                // Serial.print("[余额] 原始响应: ");         // ★ 探针6: 打印服务器返回的前200字符
                // Serial.println(bsResp.substring(0, 200));
            }
            finishBalance(ok);
            break;
        }
    }
}