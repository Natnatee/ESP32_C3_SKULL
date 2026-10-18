# Hardware and Libraries

## Hardware
- ESP32-C3 SuperMini: logic 3.3V, Native USB CDC
- MG995 180°: ใช้ทดสอบขยับกราม
- ปุ่ม momentary normally-open 2 ปุ่ม
- ถ่านอัลคาไล 4 ก้อนประมาณ 6V
- HT7333 ลดไฟเป็น 3.3V ให้ ESP32-C3
- C 1000 µF คร่อมไฟใกล้เซอร์โว ตรวจขั้วและพิกัดแรงดัน

## Pin map
| Function | GPIO | Direction | Notes |
|---|---:|---|---|
| Servo signal | 4 | Output | PWM 50 Hz, 1000–2000 µs |
| Increase | 5 | Input pull-up | Active Low, ปุ่มต่อ GND |
| Decrease | 6 | Input pull-up | Active Low, ปุ่มต่อ GND |

ไม่ใช้ GPIO8/9 ซึ่งมีหน้าที่ boot/LED และไม่ใช้ GPIO18/19 ของ Native USB ตรวจ silk screen ของบอร์ดจริงก่อนต่อ

## Framework / Libraries
- PlatformIO `espressif32`, Arduino framework
- Board ID เดิม `lolin_c3_mini`, เปิด USB CDC ตอน boot
- `Arduino.h` และ `driver/ledc.h` ที่มากับ framework
- ไม่ใช้ third-party library, Wi-Fi หรือ secrets

## Power
Servo รับไฟแบตเตอรี่โดยตรง ESP32 รับผ่าน HT7333 ต่อ GND ร่วม ใช้สายไฟเซอร์โวสั้นและเหมาะกับกระแส หลีกเลี่ยงให้กระแสมอเตอร์ไหลผ่านสาย GND เล็กของ ESP32

ตรวจ datasheet HT7333 รุ่นจริงสำหรับแรงดันสูงสุด กระแส ความร้อน และ capacitor ที่จำเป็น ถ่านอัลคาไลอาจแรงดันตกเมื่อ MG995 ออกแรง C 1000 µF ไม่สามารถชดเชยกระแสต่อเนื่องได้ อ่านข้อควรระวัง USB และขั้นตอนตั้งศูนย์ใน README.md
