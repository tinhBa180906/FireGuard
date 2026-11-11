/*
  =============================================================================
  HỆ THỐNG IOT PCCC (PHÒNG CHÁY CHỮA CHÁY) - BẢN CHUẨN HÓA NGHIỆP VỤ PCCC
  Chuyên biệt: Liên động 2 kênh (Lửa + Nhiệt cao), Trễ tắt bơm 2s & Nguồn Pin 2S
  =============================================================================
  * THỨ TỰ ƯU TIÊN & 3 NGHIỆP VỤ CỐT LÕI (CHUẨN TCVN 5738 & TCVN 7336):
    - ƯU TIÊN 1 (Cao nhất) - Nghiệp vụ 3 (Hệ thống chữa cháy tự động Sprinkler):
      + Điều kiện kích hoạt (Cross-zoning): Phát hiện Lửa (KY-026) VÀ Nhiệt độ tăng cao (DHT11).
      + Hành động: Còi hú khẩn cấp (1200Hz), LED Đỏ sáng rực, BẬT MÁY BƠM dập lửa.
      + Cơ chế trễ tắt bơm: Khi ngọn lửa tắt / nhiệt hạ, máy bơm tiếp tục duy trì
        chạy thêm 2 giây (2000ms) để dập tắt triệt để tàn tro trước khi ngắt Relay.
    - ƯU TIÊN 2 - Nghiệp vụ 1 (Nút nhấn báo cháy khẩn cấp bằng tay - Manual Call Point):
      + Điều kiện: Con người chủ động nhấn nút khẩn cấp (A2).
      + Hành động: Còi hú liên tục (1000Hz), LED Đỏ sáng tĩnh, Máy bơm giữ TẮT.
    - ƯU TIÊN 3 - Nghiệp vụ 2 (Cảnh báo sớm rò rỉ Khí Gas / Khói - MQ-2):
      + Điều kiện: Nồng độ khí Gas/Khói vượt ngưỡng an toàn (>= 400).
      + Hành động: Còi bíp ngắt quãng (900Hz), LED Đỏ nhấp nháy, TUYỆT ĐỐI TẮT BƠM
        để ngăn ngừa nguy cơ chập điện gây nổ khí gas.
  -----------------------------------------------------------------------------
  * SƠ ĐỒ ĐẤU CHÂN:
    - Nút nhấn (Push Button):       A2 (INPUT_PULLUP, đầu kia nối GND)
    - Cảm biến MQ-2 (AO):           A0
    - Cảm biến DHT11 (DATA):        A1
    - Cảm biến Lửa KY-026 (DO):     D6
    - Relay kích Máy bơm 7-12V:    D7 (Active LOW)
    - Còi Buzzer:                   D8
    - Đèn LED Đỏ (Cảnh báo):        D9 (qua trở 220 Ohm)
    - Đèn LED Xanh (An toàn):       D10 (qua trở 220 Ohm)
    - LCD 16x2 I2C:                 SDA -> A4, SCL -> A5
  =============================================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// Khởi tạo LCD I2C (Địa chỉ mặc định 0x27 hoặc 0x3F, màn hình 16 cột 2 dòng)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Khởi tạo cảm biến nhiệt độ & độ ẩm DHT11
#define DHTPIN  A1
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ======================= ĐỊNH NGHĨA CHÂN KẾT NỐI =======================
const int PIN_MQ2        = A0; // Cảm biến khói/gas MQ-2 (Analog)
const int PIN_DHT        = A1; // Cảm biến nhiệt độ & độ ẩm DHT11
const int PIN_BTN        = A2; // Nút nhấn khẩn cấp bằng tay (Nghiệp vụ 1)
const int PIN_FLAME      = 6;  // Cảm biến ngọn lửa KY-026 (Nghiệp vụ 3)
const int PIN_RELAY_PUMP = 7;  // Relay điều khiển máy bơm 7-12V (Nghiệp vụ 3)
const int PIN_BUZZER     = 8;  // Còi báo động Buzzer
const int PIN_LED_RED    = 9;  // Đèn LED Đỏ (Cảnh báo sự cố)
const int PIN_LED_GREEN  = 10; // Đèn LED Xanh (Hệ thống an toàn)

// ================= CẤU HÌNH LOGIC & NGƯỠNG CẢM BIẾN =================
const int RELAY_ON     = HIGH;  // Relay 5V kích mức LOW
const int RELAY_OFF    = LOW; // Mức HIGH ngắt Relay
const int FLAME_ACTIVE = LOW;  // Cảm biến KY-026 phát hiện lửa trả về mức LOW

// Ngưỡng khói/gas MQ-2
const int MQ_THRESHOLD = 400;

// Ngưỡng nhiệt độ tăng cao (Kết hợp Nhiệt cố định & Nhiệt gia tăng theo chuẩn PCCC)
const float TEMP_THRESHOLD  = 34.0; // Ngưỡng nhiệt độ cao tuyệt đối (°C)
const float TEMP_RISE_DELTA = 2.0;  // Hoặc nhiệt độ tăng thêm >= 2.0°C so với lúc khởi động

// Thời gian duy trì máy bơm chạy thêm sau khi tắt lửa (2000ms = 2 giây)
const unsigned long PUMP_OFF_DELAY = 2000;

// ======================== BIẾN TOÀN CỤC & ĐA NHIỆM ========================
// Nghiệp vụ 1: Biến lưu trạng thái nút nhấn khẩn cấp
bool manualFireAlarm = false;
bool lastBtnState    = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 250; // Chống dội phím 250ms

// Nghiệp vụ 3: Biến quản lý trạng thái bơm và thời gian giữ trễ tắt bơm 2s
bool pumpActive = false;
unsigned long lastFireTriggerTime = 0;

// Đa nhiệm: Thời gian đọc DHT11 và chu kỳ chớp đèn/còi
unsigned long lastDhtRead = 0;
unsigned long lastBlink   = 0;
bool blinkState           = false;

float currentTemp = 28.0;
float currentHum  = 60.0;
float baseTemp    = 28.0; // Nhiệt độ môi trường ban đầu khi khởi động hệ thống

void setup() {
  Serial.begin(9600);

  // Cấu hình chân nút nhấn và cảm biến lửa (dùng trở kéo lên nội)
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_FLAME, INPUT_PULLUP);

  // ĐẢM BẢO RELAY BƠM TẮT NGAY TỪ ĐẦU (Tránh tình trạng giật bơm lúc mới bật nguồn)
  digitalWrite(PIN_RELAY_PUMP, RELAY_OFF);
  pinMode(PIN_RELAY_PUMP, OUTPUT);

  // Cấu hình các chân ngõ ra cho đèn và còi
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);

  // Khởi động DHT và LCD
  dht.begin();
  lcd.init();
  lcd.backlight();

  // Đọc nhiệt độ phòng ban đầu làm mốc tham chiếu Nhiệt gia tăng (Rate-of-Rise)
  float initTemp = dht.readTemperature();
  float initHum  = dht.readHumidity();
  if (!isnan(initTemp)) {
    currentTemp = initTemp;
    baseTemp    = initTemp;
  }
  if (!isnan(initHum)) {
    currentHum = initHum;
  }

  // Màn hình chào mừng khi khởi động
  lcd.setCursor(0, 0);
  lcd.print(" HE THONG PCCC  ");
  lcd.setCursor(0, 1);
  lcd.print(" SAN SANG DEMO! ");
  delay(1500);
  lcd.clear();
}

void loop() {
  unsigned long now = millis();

  // -------------------------------------------------------------------------
  // 1. XỬ LÝ NÚT NHẤN KHẨN CẤP (NGHIỆP VỤ 1: MANUAL CALL POINT)
  // -------------------------------------------------------------------------
  bool readingBtn = digitalRead(PIN_BTN);
  if (readingBtn == LOW && lastBtnState == HIGH && (now - lastDebounceTime > DEBOUNCE_DELAY)) {
    manualFireAlarm = !manualFireAlarm; // Bấm lần 1: Bật, Bấm lần 2: Tắt
    lastDebounceTime = now;
    lcd.clear();
  }
  lastBtnState = readingBtn;

  // -------------------------------------------------------------------------
  // 2. ĐỌC CÁC CẢM BIẾN (LỬA, KHÓI/GAS MQ-2, NHIỆT ĐỘ & ĐỘ ẨM DHT11)
  // -------------------------------------------------------------------------
  int mqValue   = analogRead(PIN_MQ2);
  bool hasFlame = (digitalRead(PIN_FLAME) == FLAME_ACTIVE);
  bool hasGas   = (mqValue >= MQ_THRESHOLD);

  // Đọc DHT11 định kỳ mỗi 1 giây (1000ms) để cập nhật nhanh nhiệt độ khi có lửa
  if (now - lastDhtRead >= 300) {
    lastDhtRead = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (!isnan(t)) currentTemp = t;
    if (!isnan(h)) currentHum  = h;

    // Gửi log về Serial Monitor để theo dõi trực tiếp
    Serial.print("Temp: ");     Serial.print(currentTemp, 1);
    Serial.print("C (Goc: ");   Serial.print(baseTemp, 1);
    Serial.print("C) | Lua: "); Serial.print(hasFlame ? "CO" : "KHONG");
    Serial.print(" | MQ-2: ");  Serial.print(mqValue);
    Serial.print(" | Bom: ");   Serial.println(pumpActive ? "CHAY" : "TAT");
  }

  // Kiểm tra điều kiện nhiệt độ tăng cao (Vượt ngưỡng 34°C hoặc tăng >= 2°C so với ban đầu)
  bool isHighTemp = (currentTemp >= TEMP_THRESHOLD) || ((currentTemp - baseTemp) >= TEMP_RISE_DELTA);

  // Điều kiện liên động 2 kênh (Cross-zoning): PHẢI CÓ LỬA + NHIỆT ĐỘ TĂNG CAO
  bool fireConfirmed = (hasFlame && isHighTemp);

  // -------------------------------------------------------------------------
  // 3. QUẢN LÝ TRẠNG THÁI BƠM & DELAY TẮT BƠM 2 GIÂY (2000ms)
  // -------------------------------------------------------------------------
  if (fireConfirmed) {
    pumpActive = true;
    lastFireTriggerTime = now; // Liên tục cập nhật mốc thời gian khi còn đám cháy
  } else if (pumpActive && (now - lastFireTriggerTime >= PUMP_OFF_DELAY)) {
    pumpActive = false;        // Đã hết lửa/nhiệt cao và giữ đủ 2 giây -> Tắt bơm
    lcd.clear();
  }

  // -------------------------------------------------------------------------
  // 4. THỰC THI 3 NGHIỆP VỤ THEO THỨ TỰ ƯU TIÊN ĐÃ CHUẨN HÓA
  // -------------------------------------------------------------------------

  // === ƯU TIÊN 1 (CAO NHẤT) - NGHIỆP VỤ 3: CHÁY THẬT (LỬA + NHIỆT CAO) & DUY TRÌ BƠM 2S ===
  // Hành động: Còi kêu dồn dập (1200Hz), LED đỏ sáng rực, BẬT MÁY BƠM (giữ thêm 2s trước khi tắt)
  if (pumpActive) {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_BUZZER, 1200);                  // Còi hú khẩn cấp 1200Hz
    digitalWrite(PIN_RELAY_PUMP, RELAY_ON);  // ĐÓNG RELAY -> MÁY BƠM CHẠY

    lcd.setCursor(0, 0);
    if (fireConfirmed) {
      lcd.print("CHAY: LUA+NHIET!");
    } else {
      lcd.print("DUY TRI BOM 2S..");         // Hiển thị rõ giai đoạn trễ tắt bơm 2s
    }
    lcd.setCursor(0, 1);
    lcd.print("BOM:BAT  T:");
    lcd.print((int)currentTemp);
    lcd.print("C  ");
  }

  // === ƯU TIÊN 2 - NGHIỆP VỤ 1: NÚT NHẤN BÁO CHÁY KHẨN CẤP BẰNG TAY (MANUAL CALL POINT) ===
  // Hành động: Con người chủ động phát lệnh báo cháy -> Hú còi 1000Hz, sáng LED đỏ, Bơm giữ tắt
  else if (manualFireAlarm) {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    tone(PIN_BUZZER, 1000);                  // Tần số còi 1000Hz liên tục
    digitalWrite(PIN_RELAY_PUMP, RELAY_OFF); // MÁY BƠM GIỮ TẮT

    lcd.setCursor(0, 0);
    lcd.print("BAO CHAY KHAN!  ");
    lcd.setCursor(0, 1);
    lcd.print("BAM NUT DE TAT  ");
  }

  // === ƯU TIÊN 3 - NGHIỆP VỤ 2: PHÁT HIỆN RÒ RỈ GAS / KHÓI (MQ-2) ===
  // Hành động: Cảnh báo sơ tán sớm bằng nhấp nháy đèn và còi ngắt quãng.
  // QUAN TRỌNG: TUYỆT ĐỐI TẮT BƠM khi chưa có lửa để tránh chập điện nổ gas.
  else if (hasGas) {
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_RELAY_PUMP, RELAY_OFF); // MÁY BƠM GIỮ TẮT

    // Chu kỳ chớp nháy còi và LED đỏ mỗi 300ms
    if (now - lastBlink >= 300) {
      lastBlink = now;
      blinkState = !blinkState;
      digitalWrite(PIN_LED_RED, blinkState ? HIGH : LOW);
      if (blinkState) tone(PIN_BUZZER, 900);
      else            noTone(PIN_BUZZER);
    }

    lcd.setCursor(0, 0);
    lcd.print("CANH BAO GAS!   ");
    lcd.setCursor(0, 1);
    lcd.print("MQ:");
    lcd.print(mqValue);
    lcd.print("  SO TAN! ");
  }

  // === TRẠNG THÁI BÌNH THƯỜNG: HỆ THỐNG AN TOÀN (TRỰC BAN) ===
  else {
    digitalWrite(PIN_LED_GREEN, HIGH);       // LED Xanh sáng báo bình thường
    digitalWrite(PIN_LED_RED, LOW);          // LED Đỏ tắt
    noTone(PIN_BUZZER);                      // Còi tắt
    digitalWrite(PIN_RELAY_PUMP, RELAY_OFF); // Máy bơm tắt

    // Hiển thị thông số giám sát môi trường liên tục lên LCD 16x2
    lcd.setCursor(0, 0);
    lcd.print("AN TOAN  T:");
    lcd.print((int)currentTemp);
    lcd.print("C   ");

    lcd.setCursor(0, 1);
    lcd.print("MQ:");
    lcd.print(mqValue);
    lcd.print("    H:");
    lcd.print((int)currentHum);
    lcd.print("%   ");
  }
}
