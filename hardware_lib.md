# Hardware and Libraries

## ESP32-C3 SuperMini

- MCU: ESP32-C3, RISC-V single core
- Clock: สูงสุด 160 MHz
- Flash: โดยทั่วไป 4 MB
- SRAM: 400 KB internal
- Logic voltage: 3.3V
- Onboard LED: GPIO 8, Active Low
- USB: Native USB CDC

## Pin Usage

```text
GPIO 8 ──> ตัวต้านทาน/LED บนบอร์ด ──> Active Low
           LOW  = LED ติด
           HIGH = LED ดับ
```

## Framework / Libraries

- PlatformIO
- Arduino Framework for ESP32
- `WiFi.h` จาก Arduino ESP32 core
- ไม่ใช้ third-party library

## Board Configuration

ใช้ PlatformIO board ID `lolin_c3_mini` ตามข้อมูลในถังความจำ และเปิด USB CDC ตอน boot ผ่าน build flags

## Notes

- GPIO 8 เป็นขาพ่วง LED ไม่ควรใช้กับอุปกรณ์อื่นในโปรเจกต์ทดสอบนี้
- ข้อมูลยังไม่ได้ยืนยันกับบอร์ดจริงในรอบนี้
