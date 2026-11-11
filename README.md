# 🚒 Hệ Thống IoT PCCC (Phòng Cháy Chữa Cháy) Chuẩn Hóa

Dự án Hệ thống IoT Phòng cháy chữa cháy (PCCC) được phát triển dựa trên chuẩn nghiệp vụ PCCC (TCVN 5738 & TCVN 7336). Hệ thống tích hợp đa cảm biến để phát hiện sớm các nguy cơ cháy nổ, phân loại ưu tiên xử lý sự cố, và điều khiển bơm nước chữa cháy tự động.

Điểm nổi bật của dự án là **Cơ chế liên động 2 kênh (Cross-zoning)** để chống báo giả và **Cơ chế trễ tắt bơm 2 giây** giúp dập tắt triệt để tàn tro.

---

## 🌟 Tính Năng & Nghiệp Vụ Cốt Lõi

Hệ thống hoạt động dựa trên 3 cấp độ ưu tiên (từ cao xuống thấp):

### 🔴 Ưu tiên 1: Chữa cháy tự động (Sprinkler) - Mức cao nhất
- **Điều kiện kích hoạt:** Phải có đồng thời Lửa (Cảm biến KY-026) **VÀ** Nhiệt độ tăng cao bất thường (DHT11).
- **Hành động:** 
  - Còi hú khẩn cấp dồn dập (1200Hz).
  - Đèn LED Đỏ sáng rực.
  - **Bật máy bơm** để dập lửa.
- **Tính năng đặc biệt (Trễ tắt bơm):** Khi lửa tắt hoặc nhiệt hạ, máy bơm không tắt ngay mà tiếp tục chạy thêm **2 giây** để dập tắt triệt để tàn tro, sau đó mới ngắt.

### 🟠 Ưu tiên 2: Báo cháy khẩn cấp bằng tay (Manual Call Point)
- **Điều kiện kích hoạt:** Có người nhấn nút khẩn cấp.
- **Hành động:** 
  - Còi hú báo động liên tục (1000Hz).
  - Đèn LED Đỏ sáng tĩnh.
  - **Máy bơm giữ tắt** (chỉ báo động sơ tán, chưa kích hoạt nước).

### 🟡 Ưu tiên 3: Cảnh báo sớm rò rỉ Khí Gas / Khói
- **Điều kiện kích hoạt:** Nồng độ khí Gas/Khói vượt ngưỡng an toàn (>= 400 trên cảm biến MQ-2).
- **Hành động:** 
  - Còi bíp ngắt quãng (900Hz).
  - Đèn LED Đỏ nhấp nháy cảnh báo sơ tán.
  - **Tuyệt đối tắt bơm:** Ngăn chặn nguy cơ tia lửa điện từ relay/máy bơm gây nổ khí gas.

### 🟢 Trạng thái Bình thường (An toàn)
- Đèn LED Xanh sáng.
- Màn hình LCD 16x2 liên tục cập nhật các thông số môi trường: Nhiệt độ, Độ ẩm, và Nồng độ khí Gas.

---

## 🛠️ Linh Kiện Phần Cứng

1. Mạch điều khiển trung tâm: Arduino UNO / Nano (hoặc tương đương).
2. Màn hình hiển thị: LCD 16x2 kèm module I2C.
3. Cảm biến:
   - Cảm biến khí gas/khói MQ-2.
   - Cảm biến nhiệt độ, độ ẩm DHT11.
   - Cảm biến ngọn lửa KY-026.
4. Cơ cấu chấp hành & Cảnh báo:
   - Module Relay 1 kênh (5V) điều khiển máy bơm (7-12V).
   - Nút nhấn nhả (Push button).
   - Còi Buzzer (Active hoặc Passive đều có thể dùng chung hàm `tone`).
   - Đèn LED (Đỏ, Xanh) kèm điện trở 220 Ohm.

---

## 🔌 Sơ Đồ Đấu Nối (Pinout)

| Linh Kiện | Chân trên Arduino | Ghi chú |
| :--- | :--- | :--- |
| **Nút nhấn (A2)** | `A2` | INPUT_PULLUP, đầu còn lại nối GND |
| **Cảm biến MQ-2 (AO)**| `A0` | Đọc giá trị Analog (Nồng độ khí/khói) |
| **Cảm biến DHT11** | `A1` | Chân Data của DHT11 |
| **Cảm biến Lửa KY-026**| `D6` | Chân DO, phát hiện lửa mức LOW |
| **Relay Máy bơm** | `D7` | Đang cấu hình HIGH là Bật, LOW là Tắt |
| **Còi Buzzer** | `D8` | Cảnh báo âm thanh |
| **LED Đỏ** | `D9` | Qua trở 220Ω - Cảnh báo sự cố |
| **LED Xanh** | `D10` | Qua trở 220Ω - Báo an toàn |
| **LCD 16x2 I2C** | `SDA -> A4`<br>`SCL -> A5`| Chân giao tiếp I2C mặc định trên Arduino UNO/Nano |
