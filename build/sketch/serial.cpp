#line 1 "D:\\M5stick_proj\\self_workspace\\serial.cpp"
#include "serial.h"
#include <WiFi.h>   
#include <M5Unified.h>

static char serialBuffer[512];
static size_t serial_len=0; 
/**
 * size_t是c/c++中定义的无符号整数类型，通常用于表示对象的大小或数组的索引。
 * 它的大小取决于平台和编译器，但通常与指针大小相同。在32位系统上，
 * size_t通常是32位无符号整数，而在64位系统上，它通常是64位无符号整数。 
 */

 /**
  * 上位机发 {"type":"ping"}
        │
        ▼
  ┌─ 模式A：pumpSerial()  ──┐
  │  把串口字节攒成"整行"      │   ← 负责"收"
  └──────────┬──────────────┘
             ▼ 一行完整的 JSON 字符串
  ┌─ 模式C：handleLine()  ──┐
  │  解析 JSON、按 type 分发   │   ← 负责"判断收到的是什么"
  └──────────┬──────────────┘
             ▼ 判断出是 ping，于是要"回话"
  ┌─ 模式B：sendHello()  ──┐
  │  组装 JSON、serialize、换行 │   ← 负责"发"
  └──────────┬──────────────┘
             ▼
    固件发出 {"type":"hello",...}
  */

static int hhmmToMin(const char *s){
    if(strlen(s)!=5||s[2]!=':')return -1;   //格式不对
    int h=atoi(s);
    int m=atoi(s+3);    //atoi解析开头数字部分
    if(h<0||h>23||m<0||m>59)return -1;  //数值非法
    return h*60+m;    
}

static void handleConfig(JsonDocument &doc){
    Serial.println("[1] enter");
    JsonObject wifi=doc["wifi"];
    if(!wifi.isNull()){
        cfg.ssid=wifi["ssid"]|"";
        cfg.password=wifi["password"]|"";
    }
    Serial.println("[2] wifi done");
    cfg.ntp=doc["ntp"]|DEFULT_NTP;
    cfg.apikey=doc["api_key"]|"";
    cfg.balance_warn=doc["balance_warn"]|DEFULT_BALANCE_WARN;
    Serial.println("[3] top done");
    JsonArray ranges=doc["peak_ranges"];
    if(ranges.isNull()){ cfg.peakcount=0; }
    else{
        int n=0;
        for(JsonVariant v:ranges){
            if(n>=MAX_PEAK_RANGES)break;
            const char *s=v["start"]|"";
            const char *e=v["end"]|"";
            int sm=hhmmToMin(s); int em=hhmmToMin(e);
            if(sm<0||em<0)continue;
            cfg.peak[n].startMin=sm; cfg.peak[n].endMin=em;
            n++;
        }
        cfg.peakcount=n;
    }
    Serial.println("[4] ranges done");
    cfgsave(cfg,prefs);
    Serial.println("[5] nvs saved");
    sendAck(true,"config applied");
    Serial.println("[6] ack sent");
}

static void send_json(JsonDocument &doc)
{
    serializeJson(doc,Serial);  //把 JSON 文档序列化到串口
    Serial.println();  //发送换行符，表示一行结束
}
void sendHello()
{
    JsonDocument doc;
    doc["type"]="hello";
    doc["version"]=FW_VERESION;
    doc["model"]=MODEL_NAME;
    doc["configured"]=cfg.haswifi();
    send_json(doc);
}

void sendAck(bool ok ,const char *msg)
{
    JsonDocument doc;
    doc["type"]="ack";
    doc["ok"]=ok;
    doc["msg"]=msg;
    send_json(doc);
}

void sendState(){
    time_t now=time(nullptr);   //当前epoch秒
    struct tm tmv;
    localtime_r(&now,&tmv);
    char tbuf[32];
    snprintf(tbuf,sizeof(tbuf),"%04d-%02d-%02d %02d:%02d:%02d",
    tmv.tm_year+1900,tmv.tm_mon+1,tmv.tm_mday,
    tmv.tm_hour,tmv.tm_min,tmv.tm_sec);//格式化时间

    JsonDocument doc;
    doc["type"]="state";
    doc["time"]=tbuf;

    int md=tmv.tm_hour*60+tmv.tm_min;   //当前分钟数
    if(timeSynced){
        doc["phrase"]=inPeakWindow(cfg,md)?"peak":"offpeak";
    }else{
        doc["phrase"]="unKnown";
    }
    doc["wifi"]=(WiFi.status()==WL_CONNECTED);  //返回布尔值
    doc["ssid"]=cfg.ssid;
    doc["ip"]=WiFi.localIP().toString();
    doc["battery"]=M5.Power.getBatteryLevel();
    doc["fw"]=FW_VERESION;
    send_json(doc);
}

static void handleLine(char *line){
    JsonDocument doc;
    if(deserializeJson(doc,line)){
        sendAck(false,"json parse error");
        return;
    }
    const char *type=doc["type"]|"";//取type字段,如果没有用空字符串代替
    if (strcmp(type,"ping")==0)
    {
        sendHello();
    }else if(strcmp(type,"config")==0){
        handleConfig(doc);
    }else if(strcmp(type,"state")==0){
        sendState();
    }
}

void pump_Serial()
{
    while(Serial.available()){
        char ch=Serial.read();

        // Serial.print("<");
        // Serial.print((int)(unsigned char)ch, HEX);
        // Serial.print(">");

        if(ch=='\n'){   //一行结束标记
            serialBuffer[serial_len]=0; //加字符串结尾
            if(serial_len>0){
                handleLine(serialBuffer);   //有内容才处理
            }
            serial_len=0; //处理完毕，清空缓冲区
        }else if(ch!='\r'&&serial_len<sizeof(serialBuffer)-1){
        serialBuffer[serial_len++]=ch;  //其余字符入队，跳过\r
        /**
         * '/r': 回车符，ASCII码为13，表示光标回到当前行的开头,不换行。
         * '/n': 换行符，ASCII码为10，表示光标移动到下一行的开头。
         */
        }
    }
}