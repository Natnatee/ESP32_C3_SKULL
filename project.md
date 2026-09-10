# ESP32C3_CHECK

## Overview

เฟิร์มแวร์ตรวจสอบพื้นฐานของบอร์ด ESP32-C3 SuperMini ด้วย PlatformIO และ Arduino Framework

## Goals

- ตรวจสอบ LED บนบอร์ดด้วยจังหวะเร็วสลับช้า
- ตรวจสอบการเชื่อมต่อ Wi-Fi
- แสดงสเปกบอร์ดและสถานะระบบทุก 3 วินาทีผ่าน Serial

## Hardware

- ESP32-C3 SuperMini
- LED บนบอร์ด GPIO 8 (Active Low)
- USB-C สำหรับจ่ายไฟ, upload และ Serial CDC

## Pin Map

| Function | Pin | Device | Direction | Notes |
|---|---:|---|---|---|
| Status LED | GPIO 8 | Onboard LED | Output | Active Low |
| USB Serial | Native USB | ESP32-C3 USB CDC | I/O | 115200 baud |

## Electrical / Safety Notes

- ลอจิก GPIO เป็น 3.3V
- GPIO 8 มี LED บนบอร์ดพ่วงอยู่

## Firmware Architecture

- LED toggle แบบ non-blocking: เร็ว 150 ms และช้า 600 ms
- สลับความเร็วทุก 3 วินาที
- Wi-Fi reconnect แบบ non-blocking
- System log ทุก 3 วินาที

## Libraries

ใช้เฉพาะ Arduino ESP32 core: `WiFi.h`, `ESP.h`

## Build & Upload

```bash
pio run
pio run -t upload
pio device monitor
```

## Current State

สร้างเฟิร์มแวร์ทดสอบเริ่มต้นแล้ว ยังไม่ได้ยืนยันกับบอร์ดจริง

## Open Questions

- พอร์ต COM ของบอร์ดจะตรวจเมื่อ upload/monitor

## Issue Log

- ยังไม่มี
