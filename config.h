#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>   // 提供 Preferences 类 ← 这行

//设置默认值
#define CFG_NS "lzcfg"   // NVS命名空间
#define MAX_PEAK_RANGES 4   //表示最多有几段
#define DEFULT_NTP "ntp.aliyun.com"
#define DEFULT_BALANCE_WARN 10
//在原先模板里再添加一个开心狐狸和伤心狐狸？

//一个高峰时间段的起止
struct PeakRange{
    int startMin=0;   //起始分钟数,0~1439
    int endMin=0;     //结束分钟数,0~1439
};

struct Config{
    //整数成员
    String ssid,password;
    String ntp="ntp.aliyun.com";   // 默认值,之后替换为自己的api网站
    String apikey;
    float balance_warn=0;   //余额警告
    PeakRange peak[MAX_PEAK_RANGES];   //峰值时间段
    int peakcount=0;   //峰值计数
    //成员函数
    bool haswifi() const{return ssid.length()>0;}//这个花括号内就是has_wifi函数的内容
};
/*
程序需要知道"配没配 WiFi"
        │
        ▼
  cfg.hasWifi()          ← 这个"提问"动作，只要代码执行到这一行就会发生
        │
        ▼
  函数体执行：ssid.length() > 0   ← 根据 ssid 的当前内容，现场算出答案
        │
        ▼
  返回 true / false       ← 这就是答案

*/
//NVS读写函数
inline void cfgsave(const Config &c,Preferences &prefs){
    prefs.begin(CFG_NS,false);   //flase=可写
    
    prefs.putString("ssid",c.ssid);
    prefs.putString("password",c.password); 
    prefs.putString("ntp",c.ntp);
    prefs.putString("apikey",c.apikey);
    prefs.putFloat("balance_warn",c.balance_warn);

    //峰谷区间
    if(c.peakcount>0){
        String ranges;
        for(int i=0;i<c.peakcount;i++){
            if(i>0)ranges+=","; //段之间加逗号
            ranges+=String(c.peak[i].startMin)+","+String(c.peak[i].endMin);
        }
        prefs.putString("peak_ranges",ranges);
    }else{
        prefs.remove("peak_ranges");   //没有峰值区间就删除这个键
    }
    prefs.end();
}

inline void cfgLoad(Config &c,Preferences &prefs){
    prefs.begin(CFG_NS,true);   //true=只读
    c.ssid=prefs.getString("ssid","");
    c.password=prefs.getString("password","");
    c.ntp=prefs.getString("ntp","ntp.aliyun.com");
    c.apikey=prefs.getString("apikey","");
    c.balance_warn=prefs.getFloat("balance_warn",0);

    String ranges=prefs.getString("peak_ranges","");
    if(ranges.length()>0){
        int idx=0;
        int count=0;
        while(idx<ranges.length()&&count<MAX_PEAK_RANGES){
            int comma1=ranges.indexOf(',',idx);//从idx开始找到ranges里面‘，’第一次出现的位置
            if(comma1==-1)break;    //没有逗号
            int comma2=ranges.indexOf(',',comma1+1);//从comma_1+1开始找到ranges里面‘，’第一次出现的位置
            if(comma2==-1)comma2=ranges.length();//最后一段没逗号
            String startStr=ranges.substring(idx,comma1);//左闭右开切出字符串
            String endStr=ranges.substring(comma1+1,comma2);
            c.peak[count].startMin=startStr.toInt();    //把字符串转换为int
            c.peak[count].endMin=endStr.toInt();
            count++;
            idx=comma2+1;   //把光标移动到下一段开头
        }
        c.peakcount=count;
    }else{
        c.peakcount=0;
    }
    prefs.end();//
}

inline bool inPeakWindow(const Config &c,int minuteOfDay){
    //没配置
    if(c.peakcount<=0){
        return(minuteOfDay>=540&&minuteOfDay<720)||
            (minuteOfDay>=840&&minuteOfDay<1080);
    }
    for(int i=0;i<c.peakcount;i++){
        if(minuteOfDay>=c.peak[i].startMin&&minuteOfDay<c.peak[i].endMin){
            return true;
        }
    }   
    return false;
}

inline long secondsToNextSwitch(const Config &c,time_t now,int minuteOfDay){
    if(now<=0)return-1;
    time_t  dayStart= now - ((now + 8 * 3600) % 86400);  // 北京当日 0 点（epoch】
    long best = 86400L;                            // 最坏情况：一天
    int n=c.peakcount>0?c.peakcount:2;
    for(int i=0;i<n;i++){
        PeakRange   r;
        if(c.peakcount>0){
            r=c.peak[i];
        }else{
            static const PeakRange official[2]={ {540,720}, {840,1080} };
            r=official[i];
        }
        long b1 = dayStart + r.startMin * 60L;       // 高峰开始时刻(epoch)
        long b2 = dayStart + r.endMin * 60L;         // 高峰结束时刻(epoch)
        long d1 = b1 - now;
        long d2 = b2 - now;
        if (d1 <= 0) d1 += 86400;                    // 已过 → 算明天那次
        if (d2 <= 0) d2 += 86400;
        if (d1 < best) best = d1;
        if (d2 < best) best = d2;
    }
    return best;
}