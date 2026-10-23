/* IOT102 FireGuard - classroom demonstrator, ESP32-WROOM-32 DevKit.
   GPIO HIGH -> external NPN ON -> active-LOW relay input -> relay ON.
   Do NOT omit the transistor interface, MQ divider or flame level shifter.
   See README.md and schematic sheets before connecting any power. */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "ControlLogic.h"

constexpr uint8_t MQ_PIN=34, FLAME_PIN=33, DHT_PIN=4;
constexpr uint8_t PUMP_PIN=26, FAN_PIN=27, BUZZ_PIN=25;
constexpr uint8_t LED_RED=18, LED_GREEN=19;
constexpr uint8_t HELP_PIN=13, RESET_PIN=14, ARM_PIN=32;
constexpr uint8_t SDA_PIN=21, SCL_PIN=22;
constexpr bool FLAME_ACTIVE_LOW=true; // Verify your particular module.
constexpr uint32_t MQ_WARMUP_MS=180000; // NOT initial 48-hour conditioning!
// Example thresholds at GPIO34 AFTER the 10k/15k divider, not ppm.
// Measure in clean air and controlled tests, then tune these constants.
constexpr int SMOKE_ON_MV=1200, SMOKE_OFF_MV=1000;
constexpr float TEMP_ON_C=40.0f, TEMP_OFF_C=37.0f;
const char* AP_SSID="IOT102-FireGuard";
const char* AP_PASSWORD="demo102esp32"; // Change for your own demo.

DHT dht(DHT_PIN,DHT11);
Adafruit_SSD1306 oled(128,64,&Wire,-1);
WebServer server(80);
ControlLogic logic;
HoldSignal smokeHold,flameHold;
Inputs input;
bool oledOK=false, dhtOK=false, hot=false, smokeRaw=false;
bool flameRaw=false, smoke=false, flame=false, ready=false;
float temp=NAN, humidity=NAN;
int mqMV=0;
uint8_t dhtFailures=0;
uint32_t bootTime=0, lastSample=0, lastDHT=0, lastGoodDHT=0;
uint32_t lastDisplay=0, lastSerial=0;
bool firstSample=true;
String state="BOOT", previousState="";

struct Button {
  uint8_t pin;
  bool previousRaw=false, stable=false;
  uint32_t changedAt=0;
  bool pressed(uint32_t now) {
    bool raw=digitalRead(pin)==LOW;
    if(raw!=previousRaw) { previousRaw=raw; changedAt=now; }
    if(raw!=stable && uint32_t(now-changedAt)>=40) {
      stable=raw;
      return stable;
    }
    return false;
  }
};
Button helpButton{HELP_PIN}, resetButton{RESET_PIN};

struct Event { uint32_t sec=0; String message; };
Event events[20];
uint8_t nextEvent=0, eventCount=0;
void logEvent(const String& message) {
  events[nextEvent].sec=uint32_t(millis()-bootTime)/1000;
  events[nextEvent].message=message;
  nextEvent=(nextEvent+1)%20;
  if(eventCount<20) ++eventCount;
  Serial.printf("[%lu s] %s\n",(unsigned long)((millis()-bootTime)/1000),message.c_str());
}

String jsonNumber(float n) { return isnan(n)?String("null"):String(n,1); }
String yesNo(bool b) { return b?"true":"false"; }

String makeJSON() {
  String j; j.reserve(2200);
  j="{\"state\":\""+state+"\",\"uptime\":"+String((millis()-bootTime)/1000);
  j+=",\"temperature\":"+jsonNumber(dhtOK?temp:NAN);
  j+=",\"humidity\":"+jsonNumber(dhtOK?humidity:NAN);
  j+=",\"mq_mV\":"+String(mqMV)+",\"ready\":"+yesNo(ready);
  j+=",\"smoke\":"+yesNo(smoke)+",\"flame\":"+yesNo(flame);
  j+=",\"hot\":"+yesNo(hot)+",\"dht_ok\":"+yesNo(dhtOK);
  j+=",\"oled_ok\":"+yesNo(oledOK);
  j+=",\"armed\":"+yesNo(input.armSwitch && logic.armOffSeen);
  j+=",\"pump\":"+yesNo(logic.pump)+",\"fan\":"+yesNo(logic.fan);
  j+=",\"fire_latched\":"+yesNo(logic.fireLatched);
  j+=",\"help_latched\":"+yesNo(logic.helpLatched)+",\"events\":[";
  for(uint8_t k=0;k<eventCount;++k) {
    uint8_t i=(nextEvent+20-1-k)%20;
    if(k) j+=",";
    // Messages are fixed internal ASCII strings, never user supplied.
    j+="{\"sec\":"+String(events[i].sec)+",\"message\":\""+events[i].message+"\"}";
  }
  j+="]}";
  return j;
}

const char PAGE[] PROGMEM=R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IOT102 FireGuard</title><style>
body{font:16px system-ui;margin:0;background:#101b27;color:#eaf0f5}main{max-width:850px;margin:auto;padding:24px}
h1{margin-bottom:4px}p{color:#b6c9d9}#state{font-size:28px;color:#ffce6e}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}
.card{background:#1c3042;padding:18px;border-radius:12px}.value{font-size:25px;margin-top:8px}
li{margin:8px 0}#connection{color:#7ce2bd}small{color:#b6c9d9}
</style><main><h1>IOT102 / FireGuard</h1><p>Mô hình giám sát khói, lửa và nhiệt độ qua Wi-Fi nội bộ.</p>
<p id="connection">Đang kết nối…</p><div id="state">--</div><p id="status"></p>
<div class="grid"><div class="card">Nhiệt độ<div class="value" id="temp">--</div></div>
<div class="card">Độ ẩm<div class="value" id="rh">--</div></div>
<div class="card">MQ-2 tại ADC<div class="value" id="mq">--</div><small>mV sau chia áp; không phải ppm</small></div>
<div class="card">Bơm / Quạt<div class="value" id="loads">--</div></div></div>
<p id="flags"></p><p>Nút HELP, RESET và công tắc ARM thao tác trực tiếp trên mô hình. Bơm tối đa 3 giây mỗi sự kiện.</p>
<h2>20 sự kiện gần nhất</h2><small>Thời gian tính từ lúc khởi động; lịch sử chỉ lưu trong RAM.</small><ol id="events"></ol>
<p>Thiết bị phục vụ học tập; không thay thế hệ thống báo cháy chuyên dụng.</p></main><script>
const names={NORMAL:'Bình thường',WARMUP:'Đang làm nóng MQ-2',FIRE:'Cảnh báo cháy (đã chốt)',
HELP:'Yêu cầu trợ giúp (đã chốt)',SMOKE:'Cảnh báo khói / khí',HOT:'Nhiệt độ cao',FAULT:'Lỗi DHT11'};
const el=id=>document.getElementById(id),on=b=>b?'BẬT':'TẮT';
async function poll(){try{let r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(2500)});
if(!r.ok)throw Error('HTTP');let d=await r.json();el('connection').textContent='Trực tuyến · '+d.uptime+' giây';
el('state').textContent=names[d.state]||d.state;
el('temp').textContent=d.temperature===null?'Lỗi / chưa có dữ liệu':d.temperature+' °C';
el('rh').textContent=d.humidity===null?'--':d.humidity+' %';el('mq').textContent=d.mq_mV+' mV';
el('loads').textContent=on(d.pump)+' / '+on(d.fan);
el('status').textContent=(d.ready?'MQ-2 sẵn sàng':'MQ-2 chờ ổn định 180 giây')+' · Cho phép bơm: '+(d.armed?'CÓ':'KHÔNG');
el('flags').textContent='Khói: '+d.smoke+' | Lửa: '+d.flame+' | Nóng: '+d.hot+' | DHT: '+d.dht_ok+' | OLED: '+d.oled_ok;
el('events').replaceChildren(...d.events.map(e=>{let n=document.createElement('li');n.textContent=e.sec+'s — '+e.message;return n;}));
}catch(e){el('connection').textContent='Mất kết nối — dữ liệu hiển thị có thể đã cũ';}finally{setTimeout(poll,1000);}}poll();
</script></html>)HTML";

void sample(uint32_t now) {
  ready=uint32_t(now-bootTime)>=MQ_WARMUP_MS;
  uint32_t sum=0;
  for(int i=0;i<16;++i) sum+=analogReadMilliVolts(MQ_PIN);
  int measured=sum/16;
  mqMV=firstSample?measured:(mqMV*3+measured)/4;
  firstSample=false;
  if(!ready) smokeRaw=false;
  else if(mqMV>=SMOKE_ON_MV) smokeRaw=true;
  else if(mqMV<=SMOKE_OFF_MV) smokeRaw=false;
  flameRaw=(digitalRead(FLAME_PIN)==(FLAME_ACTIVE_LOW?LOW:HIGH)); // Via BSS138, non-inverting.
  smoke=smokeHold.update(ready && smokeRaw,now,1500);
  flame=flameHold.update(flameRaw,now,800);

  if(uint32_t(now-lastDHT)>=2500) {
    lastDHT=now;
    float t=dht.readTemperature(),h=dht.readHumidity();
    if(!isnan(t) && !isnan(h)) {
      temp=t; humidity=h; dhtFailures=0; lastGoodDHT=now; dhtOK=true;
    } else if(++dhtFailures>=3) dhtOK=false;
  }
  if(!dhtOK || uint32_t(now-lastGoodDHT)>10000) {dhtOK=false;hot=false;}
  else if(temp>=TEMP_ON_C) hot=true;
  else if(temp<=TEMP_OFF_C) hot=false;
}

void updateControl(uint32_t now) {
  input.ready=ready;input.smoke=smoke;input.flame=flame;input.hot=hot;
  input.smokeRaw=smokeRaw;input.flameRaw=flameRaw;input.fault=!dhtOK;
  input.helpPressed=helpButton.pressed(now);input.resetPressed=resetButton.pressed(now);
  input.armSwitch=digitalRead(ARM_PIN)==LOW;
  bool oldPump=logic.pump,oldFan=logic.fan;
  logic.step(input,now);
  digitalWrite(PUMP_PIN,logic.pump?HIGH:LOW);
  digitalWrite(FAN_PIN,logic.fan?HIGH:LOW);
  if(oldPump!=logic.pump) logEvent(logic.pump?"PUMP_ON":"PUMP_OFF");
  if(oldFan!=logic.fan) logEvent(logic.fan?"FAN_ON":"FAN_OFF");
  if(logic.resetAccepted) logEvent("RESET_ACCEPTED");
  if(logic.resetRejected) logEvent("RESET_REJECTED: safe 10s and ARM OFF required");

  if(logic.fireLatched) state="FIRE";
  else if(logic.helpLatched) state="HELP";
  else if(smoke) state="SMOKE";
  else if(hot) state="HOT";
  else if(!dhtOK) state="FAULT";
  else if(!ready) state="WARMUP";
  else state="NORMAL";
  if(state!=previousState) {logEvent("STATE_"+state);previousState=state;}
  bool urgent=logic.fireLatched || logic.helpLatched;
  bool warning=smoke || hot;
  bool beep=urgent ? now%500<250 : (warning ? now%2000<200 : (!dhtOK && now%5000<100));
  digitalWrite(BUZZ_PIN,beep?HIGH:LOW); // Active buzzer, no tone()/LEDC dependency.
  digitalWrite(LED_RED,(urgent || warning || !dhtOK)?HIGH:LOW);
  digitalWrite(LED_GREEN,state=="NORMAL"?HIGH:(state=="WARMUP" && now%1000<500?HIGH:LOW));
}

void drawDisplay() {
  if(!oledOK)return;
  oled.clearDisplay();oled.setCursor(0,0);
  oled.println("IOT102 FireGuard");oled.println(state);
  if(dhtOK) {oled.print(temp,1);oled.print(" C  ");oled.print(humidity,0);oled.println(" %");}
  else oled.println("DHT11 ERROR");
  oled.print("MQ:");oled.print(mqMV);oled.print("mV ");oled.println(ready?"OK":"WAIT");
  oled.print("P:");oled.print(logic.pump);oled.print(" F:");oled.print(logic.fan);
  oled.print(" A:");oled.println(input.armSwitch && logic.armOffSeen);
  oled.println("192.168.4.1");oled.display();
}

void setup() {
  // External 100k base pulldowns keep transistor drivers OFF before firmware starts.
  for(uint8_t p:{PUMP_PIN,FAN_PIN,BUZZ_PIN,LED_RED,LED_GREEN}) {
    digitalWrite(p,LOW);pinMode(p,OUTPUT);
  }
  Serial.begin(115200);bootTime=millis();
  pinMode(FLAME_PIN,INPUT);pinMode(MQ_PIN,INPUT);
  pinMode(HELP_PIN,INPUT_PULLUP);pinMode(RESET_PIN,INPUT_PULLUP);pinMode(ARM_PIN,INPUT_PULLUP);
  analogReadResolution(12);analogSetPinAttenuation(MQ_PIN,ADC_11db);
  dht.begin();
  Wire.begin(SDA_PIN,SCL_PIN);Wire.setTimeOut(50);
  // Probe before allocating the display. A missing OLED must not stop monitoring.
  Wire.beginTransmission(0x3C);
  if(Wire.endTransmission()==0) oledOK=oled.begin(SSD1306_SWITCHCAPVCC,0x3C);
  if(oledOK){oled.setTextSize(1);oled.setTextColor(SSD1306_WHITE);}
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
  bool apOK=WiFi.softAP(AP_SSID,AP_PASSWORD);
  logEvent(apOK?"WIFI_AP_STARTED":"WIFI_AP_FAILED: local monitoring remains active");
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PAGE);});
  server.on("/api/status",HTTP_GET,[]{server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",makeJSON());});
  server.onNotFound([]{server.send(404,"text/plain","Not found");});
  server.begin();
  logEvent("BOOT: ARM must be OFF at least once before enabling pump");
}

void loop() {
  uint32_t now=millis();
  if(uint32_t(now-lastSample)>=100){lastSample=now;sample(now);}
  updateControl(now);
  server.handleClient();
  if(uint32_t(now-lastDisplay)>=500){lastDisplay=now;drawDisplay();}
  if(uint32_t(now-lastSerial)>=2000){lastSerial=now;Serial.println(makeJSON());}
  delay(2);
}
