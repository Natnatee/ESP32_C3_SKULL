#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"

namespace {
constexpr uint8_t LED_PIN = 8;
constexpr uint8_t LED_ON = LOW;
constexpr uint8_t LED_OFF = HIGH;

constexpr unsigned long FAST_BLINK_INTERVAL_MS = 150;
constexpr unsigned long SLOW_BLINK_INTERVAL_MS = 600;
constexpr unsigned long BLINK_MODE_DURATION_MS = 3000;
constexpr unsigned long LOG_INTERVAL_MS = 3000;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

bool led_is_on = false;
bool fast_blink_mode = true;
unsigned long last_led_toggle_ms = 0;
unsigned long last_mode_change_ms = 0;
unsigned long last_log_ms = 0;
unsigned long last_wifi_attempt_ms = 0;

void start_wifi_connection() {
    last_wifi_attempt_ms = millis();
    Serial.printf("[WiFi] Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void update_wifi(unsigned long now) {
    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    if (now - last_wifi_attempt_ms >= WIFI_RETRY_INTERVAL_MS) {
        WiFi.disconnect();
        start_wifi_connection();
    }
}

void update_led(unsigned long now) {
    if (now - last_mode_change_ms >= BLINK_MODE_DURATION_MS) {
        last_mode_change_ms = now;
        fast_blink_mode = !fast_blink_mode;
        Serial.printf("[LED] Mode: %s\n", fast_blink_mode ? "FAST" : "SLOW");
    }

    const unsigned long interval_ms = fast_blink_mode
        ? FAST_BLINK_INTERVAL_MS
        : SLOW_BLINK_INTERVAL_MS;

    if (now - last_led_toggle_ms >= interval_ms) {
        last_led_toggle_ms = now;
        led_is_on = !led_is_on;
        digitalWrite(LED_PIN, led_is_on ? LED_ON : LED_OFF);
    }
}

void print_system_log(unsigned long now) {
    if (now - last_log_ms < LOG_INTERVAL_MS) {
        return;
    }
    last_log_ms = now;

    Serial.println("\n========== ESP32-C3 STATUS ==========");
    Serial.printf("Chip       : %s rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("CPU        : %u MHz, %u core(s)\n", ESP.getCpuFreqMHz(), ESP.getChipCores());
    Serial.printf("Flash      : %u bytes (%u MHz)\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed() / 1000000U);
    Serial.printf("Heap       : %u free / %u bytes\n", ESP.getFreeHeap(), ESP.getHeapSize());
    Serial.printf("Min heap   : %u bytes\n", ESP.getMinFreeHeap());
    Serial.printf("Sketch     : %u bytes, free %u bytes\n", ESP.getSketchSize(), ESP.getFreeSketchSpace());
    Serial.printf("Uptime     : %lu seconds\n", now / 1000UL);
    Serial.printf("LED mode   : %s\n", fast_blink_mode ? "FAST (150 ms)" : "SLOW (600 ms)");

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi       : CONNECTED to %s\n", WiFi.SSID().c_str());
        Serial.printf("IP         : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("RSSI       : %d dBm\n", WiFi.RSSI());
        Serial.printf("MAC        : %s\n", WiFi.macAddress().c_str());
    } else {
        Serial.printf("WiFi       : DISCONNECTED (status %d)\n", static_cast<int>(WiFi.status()));
    }
    Serial.println("=====================================\n");
}
}  // namespace

void setup() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);

    Serial.begin(115200);
    const unsigned long serial_wait_start_ms = millis();
    while (!Serial && millis() - serial_wait_start_ms < 2000) {
        yield();
    }

    Serial.println("\n[Boot] ESP32-C3 SuperMini board check");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    start_wifi_connection();
}

void loop() {
    const unsigned long now = millis();
    update_led(now);
    update_wifi(now);
    print_system_log(now);
}
