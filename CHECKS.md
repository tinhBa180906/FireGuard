# Kết quả kiểm tra — 20/10/2026

## Firmware

- Board: `esp32:esp32:esp32` / ESP32 Dev Module, ESP32-WROOM-32 cổ điển.
- Core Espressif Arduino-ESP32: **3.3.12**.
- Thư viện dùng khi biên dịch: DHT sensor library **1.4.6**, Adafruit Unified Sensor **1.1.15**, Adafruit GFX **1.12.6**, Adafruit SSD1306 **2.5.17**, Adafruit BusIO **1.17.4**.
- Biên dịch bằng Arduino CLI; kết quả bản thiết kế này: **PASS**.
- Flash: **979060 byte / 1310720 byte (74%)**; biến toàn cục: **49576 byte / 327680 byte (15%)**. Số RAM này là phân tích tĩnh, không bao gồm toàn bộ heap cấp phát động khi Wi-Fi/web chạy.
- Không nạp firmware lên ESP32, không thao tác cổng COM hoặc thay chương trình trên thiết bị của người dùng.

## Bộ máy trạng thái

Kiểm tra C++ độc lập với Arduino, dùng cùng `ControlLogic.h`: **PASS**.

- Bình thường: tải OFF.
- Nóng: quạt ON; khói sơ bộ khóa quạt.
- Khói + nóng hoặc lửa: chốt FIRE.
- ARM bật sau khi sự kiện bắt đầu không làm bơm chạy.
- ARM bật sẵn, lửa được xác nhận rồi cảm biến thứ hai xác nhận: một xung bơm.
- Hết 3000ms: bơm OFF; không lặp lại trong cùng sự kiện.
- Lỗi DHT, mở ARM hoặc mất tín hiệu lửa sơ bộ: ngắt bơm.
- Hết lỗi không tự khởi động lại xung đã dùng.
- ARM để ON ngay khi boot chưa mở khóa bơm.
- Đang warmup MQ vẫn phát hiện lửa, không chạy bơm.
- HELP chốt; reset bị từ chối khi chưa bình thường đủ 10 giây.
- Reset hợp lệ xóa chốt.
- Timer xác nhận xử lý được `millis()` tràn 32 bit.

Mã kiểm tra nằm trong `tests/control_logic_test.cpp`; có thể chạy bằng trình biên dịch C++17 trên máy tính. Đây là kiểm tra logic, không giả lập đặc tính điện, thời gian cảm biến hoặc độ trễ mạng của ESP32.

## Web và bản vẽ

- Web được mở bằng Edge headless với API mô phỏng cục bộ: **PASS** cho hiển thị số đo/trạng thái, sự kiện, bố cục điện thoại 390px và cảnh báo mất kết nối/dữ liệu cũ.
- Hai tờ sơ đồ PNG được xem trực tiếp và chỉnh bố cục; kèm SVG để phóng to.
- Thiết kế cuối: MQ-2 AO dùng chia áp; flame DO dùng BSS138; relay kích LOW được điều khiển qua NPN; motor 5V có nguồn riêng và diode.

## Chưa kiểm tra trên phần cứng thật

Chưa xác nhận model board ESP32, relay và các module cảm biến, chưa đo dòng khởi động bơm/quạt, ngưỡng MQ và mức DO thực tế. Chưa kiểm chứng nhiễu/sụt áp, nhiệt độ linh kiện, trạng thái relay khi reset, bố trí nước và hoạt động tải. Phải thực hiện bảng kiểm thử trên bàn trong README trước khi trình diễn. Bản vẽ là sơ đồ nguyên lý mức module, không phải thiết kế PCB được ERC/DRC kiểm tra.
