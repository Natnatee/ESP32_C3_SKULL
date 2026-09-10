# ESP32C3_CHECK

โปรเจกต์ทดสอบ ESP32-C3 SuperMini: เชื่อมต่อ Wi-Fi, กระพริบ LED บนบอร์ด และพิมพ์สเปก/สถานะทุก 3 วินาที

## Features

- เชื่อมต่อ Wi-Fi และ reconnect อัตโนมัติ
- LED GPIO 8 กระพริบเร็วสลับช้า
- แสดง chip, revision, CPU, flash, heap, uptime, IP และ RSSI
- ทำงานแบบ non-blocking

## Hardware

- ESP32-C3 SuperMini
- สาย USB-C ที่ส่งข้อมูลได้

## Pin Map

มุมมองด้านบน โดยหันพอร์ต USB-C ลงด้านล่าง:

```text
          ESP32-C3 SuperMini
       ┌─────────────────────┐
  5V   │ ●                 ● │ GND
 GND   │ ●     ESP32-C3    ● │ 3V3
 IO10  │ ●                 ● │ IO0
 IO9   │ ●                 ● │ IO1
 IO8   │ ●  LED (Active-L) ● │ IO2
 IO7   │ ●                 ● │ IO3
 IO6   │ ●                 ● │ IO4
 IO5   │ ●                 ● │ IO20
 IO21  │ ●                 ● │ TBD
       │      ┌───────┐      │
       └──────┤ USB-C ├──────┘
              └───────┘
```

> ตำแหน่งขาบางล็อตอาจต่างกัน ให้ตรวจข้อความ silk screen บนบอร์ดก่อนต่อวงจรภายนอก โปรเจกต์นี้ใช้เฉพาะ LED GPIO 8

## Setup

ไฟล์จริง `include/secrets.h` ไม่ถูกติดตามโดย Git ตัวอย่างรูปแบบอยู่ใน `include/secrets.example.h`

## Build

```bash
pio run
```

## Upload และ Monitor

```bash
pio run -t upload
pio device monitor
```

Serial monitor ใช้ความเร็ว 115200 baud

## Status

Prototype — ต้องยืนยันการทำงานกับบอร์ดจริง
