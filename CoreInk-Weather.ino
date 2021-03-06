#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <M5CoreInk.h>

#include "Battery.h"
#include "DateTimeUtil.h"
#include "images/background.h"
#include "images/cloudy.h"
#include "images/rainy.h"
#include "images/rainyandcloudy.h"
#include "images/snow.h"
#include "images/sunny.h"
#include "images/sunnyandcloudy.h"
#include "images/lowbattery.h"
#include "time.h"

#define SHOW_LAST_UPDATED true // 天気を更新した時刻を表示するかどうか
#define SHOW_BATTERY_CAPACITY true // 電池残量を表示するかどうか

#define LOW_BATTERY_THRETHOLD 20 // バッテリー残量低下と判断するバッテリー残量 (0 - 100)

// この時刻より前は今日の時刻を表示し、この時刻以降は明日の時刻を表示する
const int8_t boundaryOfDate = 18;

// WiFi接続情報を指定する場合は有効にする 無効の場合は前回接続したWiFiの情報を使用
// #define WIFI_SSID "****"
// #define WIFI_PASS "****"

// NTPによる時刻補正を常に行う場合は有効にする 無効の場合でもEXTボタンを押しながら起動することで時刻補正を行う
// #define ADJUST_RTC_NTP

const char* endpoint = "https://www.drk7.jp/weather/json/13.js";
const char* region = "東京地方";

const char* NTP_SERVER = "ntp.nict.jp";
const char* TZ_INFO    = "JST-9";

/*
// --------------
*/

Ink_Sprite dateSprite(&M5.M5Ink);
Ink_Sprite weatherSprite(&M5.M5Ink);
Ink_Sprite temperatureSprite(&M5.M5Ink);
Ink_Sprite rainfallChanceSprite(&M5.M5Ink);

DynamicJsonDocument weatherInfo(20000);
 
void setup() {
    M5.begin();
    Wire.begin();
    Serial.begin(115200);

    dateSprite.creatSprite(0,0,200,200); // typo of "create"
    weatherSprite.creatSprite(0,0,200,200);
    temperatureSprite.creatSprite(0,0,200,200);
    rainfallChanceSprite.creatSprite(0,0,200,200);

    // バッテリー残量が低下している場合はそのことを表示し、自動更新はしない
    if(getBatCapacity() < LOW_BATTERY_THRETHOLD) {
        drawLowbattery();
        delay(1000);
        digitalWrite(LED_EXT_PIN,LOW);
        M5.shutdown();
        return;
    }

#ifdef WIFI_SSID
    WiFi.begin(WIFI_SSID,WIFI_PASS);
#else
    WiFi.begin();
#endif
     
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    Serial.println("Connected to the WiFi network");

    // EXTが押されている時は時刻合わせをする
    if(M5.BtnEXT.isPressed()) adjustRTC();
    else {
#ifdef ADJUST_RTC_NTP
        adjustRTC();
#endif
    }

    weatherInfo = getJson();
    WiFi.disconnect();

    // 18時以降は明日の天気を表示 それ以外は今日の天気を表示
    RTC_DateTypeDef RtcDate;
    RTC_TimeTypeDef RtcTime;
    M5.rtc.GetTime(&RtcTime);
    M5.rtc.GetData(&RtcDate); // typo of "Date"
    if(RtcTime.Hours >= boundaryOfDate) drawWeatherOfNextDay(RtcDate);
    else drawWeatherOfDay(RtcDate);
}
 
void loop() {
    delay(1000);
    digitalWrite(LED_EXT_PIN,LOW);
    M5.shutdown(10800);
}

DynamicJsonDocument getJson() {
    DynamicJsonDocument doc(20000);
  
    if ((WiFi.status() == WL_CONNECTED)) {
        HTTPClient http;
        http.begin(endpoint);
        int httpCode = http.GET();
        if (httpCode > 0) {
            //jsonオブジェクトの作成
            String jsonString = createJson(http.getString());
            deserializeJson(doc, jsonString);
        } else {
            Serial.println("Error on HTTP request");
        }
        http.end(); //リソースを解放
    }
    return doc;
}

// JSONP形式からJSON形式に変える
String createJson(String jsonString){
    jsonString.replace("drk7jpweather.callback(","");
    return jsonString.substring(0,jsonString.length()-2);
}

void drawWeatherOfDay(RTC_Date date) {
    JsonArray weatherArray = weatherInfo["pref"]["area"][region]["info"];

    // Dateと合致する日の天気がJson内にあればそれを描画
    for(JsonVariant v : weatherArray) {
        if(v["date"] == dateToString(date)) {
            drawWeather(v.as<String>());
            break;
        }
    }
}

// RTC_Dateにおいて次の日の日付を計算するのが難しいためこの方法を使用
void drawWeatherOfNextDay(RTC_Date date) {
    JsonArray weatherArray = weatherInfo["pref"]["area"][region]["info"];

    // Dateと合致する日の天気がJson内にあればそれの次の要素を描画
    for(int i = 0; i < weatherArray.size(); i++) {
        JsonVariant v = weatherArray[i];
        if(v["date"] == dateToString(date) && (i+1) < weatherArray.size()) {
            v = weatherArray[i+1];
            drawWeather(v.as<String>());
            break;
        }
    }
}

void drawWeather(String infoWeather) {
    M5.M5Ink.clear();
    M5.M5Ink.drawBuff((uint8_t *)image_background);
    weatherSprite.clear();
    
    DynamicJsonDocument doc(20000);
    deserializeJson(doc, infoWeather);
    String weather = doc["weather"];
    if (weather.indexOf("雨") != -1) {
        if (weather.indexOf("くもり") != -1) {
            weatherSprite.drawBuff(46,36,108,96,image_rainyandcloudy);
        } else {
            weatherSprite.drawBuff(46,36,108,96,image_rainy);
        }
    } else if (weather.indexOf("晴") != -1) {
        if (weather.indexOf("くもり") != -1) {
            weatherSprite.drawBuff(46,36,108,96,image_sunnyandcloudy);
        } else {
            weatherSprite.drawBuff(46,36,108,96,image_sunny);
        }
    } else if (weather.indexOf("雪") != -1) {
            weatherSprite.drawBuff(46,36,108,96,image_snow);
    } else if (weather.indexOf("くもり") != -1) {
            weatherSprite.drawBuff(46,36,108,96,image_cloudy);
    }
    weatherSprite.pushSprite();
 
   String maxTemperature = doc["temperature"]["range"][0]["content"];
   String minTemperature = doc["temperature"]["range"][1]["content"];
   drawTemperature(maxTemperature, minTemperature);
 
    int rainfallChances[] = {doc["rainfallchance"]["period"][0]["content"].as<int>(),
        doc["rainfallchance"]["period"][1]["content"].as<int>(),
        doc["rainfallchance"]["period"][2]["content"].as<int>(),
        doc["rainfallchance"]["period"][3]["content"].as<int>()};

    int maxRainfallChance = -255;
    int minRainfallChance = 255;

    for(int item : rainfallChances) {
        if(item > maxRainfallChance) maxRainfallChance = item;
        if(item < minRainfallChance) minRainfallChance = item;
    }
    
   drawRainfallChance(String(maxRainfallChance),String(minRainfallChance));
 
   drawDate(doc["date"]);
}

void drawTemperature(String maxTemperature, String minTemperature) {
    temperatureSprite.clear();
    temperatureSprite.drawString(63,145,maxTemperature.c_str(),&AsciiFont8x16);
    temperatureSprite.drawString(63,168,minTemperature.c_str(),&AsciiFont8x16);
    temperatureSprite.pushSprite();
}

void drawRainfallChance(String maxRainfallChance,String minRainfallChance) {
    rainfallChanceSprite.clear();
    rainfallChanceSprite.drawString(142,145,maxRainfallChance.c_str(),&AsciiFont8x16);
    rainfallChanceSprite.drawString(142,168,minRainfallChance.c_str(),&AsciiFont8x16);
    rainfallChanceSprite.pushSprite();
}

void drawDate(String date) {
    dateSprite.clear();
    dateSprite.drawString(60,16,date.c_str(),&AsciiFont8x16);
    
    // Draw last updated time
    if(SHOW_LAST_UPDATED){
        RTC_DateTypeDef RtcDate;
        RTC_TimeTypeDef RtcTime;
        M5.rtc.GetTime(&RtcTime);
        M5.rtc.GetData(&RtcDate); // typo of "Date"
        String nowString = String("Updated: ") + dateTimeToString(RtcDate,RtcTime);
        dateSprite.drawString(0,184,nowString.c_str(),&AsciiFont8x16);
    }

    //Draw battery capacity
    if(SHOW_BATTERY_CAPACITY) dateSprite.drawString(176,184,(String(getBatCapacity()) + String("%")).c_str(),&AsciiFont8x16);

    dateSprite.pushSprite();
}

void drawLowbattery(){
    M5.M5Ink.clear();
    M5.M5Ink.drawBuff((uint8_t *)image_background);
    weatherSprite.clear();
    weatherSprite.drawString(56,16,"Low Battery",&AsciiFont8x16);
    weatherSprite.drawBuff(46,36,108,96,image_lowbattery);
    weatherSprite.pushSprite();
}

void adjustRTC() {
    Serial.println("RTC adjusting...");

    tm timeinfo;
    time_t now;
    RTC_TimeTypeDef RtcTime;
    RTC_DateTypeDef RtcDate;

    configTime(0, 0, NTP_SERVER);
    setenv("TZ", TZ_INFO, 1);
    delay(1500); // wait to adjust time by NTP server 
    time(&now);
    localtime_r(&now, &timeinfo);
    // print adjusted time to serial
    char time_output[30];
    strftime(time_output, 30, "NTP: %a  %d-%m-%y %T", localtime(&now));
    Serial.println(time_output);
    
    // save to RTC
    convertTimeToRTC(&RtcTime, timeinfo);
    convertDateToRTC(&RtcDate, timeinfo);
    M5.rtc.SetTime(&RtcTime);
    M5.rtc.SetData(&RtcDate);
}
