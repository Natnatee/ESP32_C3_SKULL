#include <Arduino.h>

namespace {
constexpr uint8_t LEFT_EYE_PIN = 0;
constexpr uint8_t RIGHT_EYE_PIN = 1;
constexpr uint8_t PIR_PIN = 10;
constexpr uint8_t SERVO_PIN = 4;
constexpr unsigned long WARMUP_MS = 30000;
constexpr unsigned long SHOW_MS = 8000;
constexpr unsigned long COOLDOWN_MS = 10000;
unsigned long boot_ms = 0;
unsigned long show_ms = 0;
unsigned long cooldown_ms = 0;
unsigned long log_ms = 0;
bool showing = false;
bool cooling = false;
bool armed = false;
int last_pir = -1;

void eyes(bool left, bool right) {
    digitalWrite(LEFT_EYE_PIN, left ? HIGH : LOW);
    digitalWrite(RIGHT_EYE_PIN, right ? HIGH : LOW);
}
}  // namespace

void setup() {
    digitalWrite(SERVO_PIN, LOW);
    pinMode(SERVO_PIN, OUTPUT);  // No PWM; disconnect servo power for this test.
    digitalWrite(LEFT_EYE_PIN, LOW);
    digitalWrite(RIGHT_EYE_PIN, LOW);
    pinMode(LEFT_EYE_PIN, OUTPUT);
    pinMode(RIGHT_EYE_PIN, OUTPUT);
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    Serial.begin(115200);
    boot_ms = millis();
}

void loop() {
    const unsigned long now = millis();
    const unsigned long elapsed = now - boot_ms;
    const int pir = digitalRead(PIR_PIN);
    if (pir != last_pir || now - log_ms >= 2000) {
        log_ms = now;
        last_pir = pir;
        Serial.printf("[PIR TEST] uptime=%lus GPIO10=%s warmup=%s armed=%d show=%d cooldown=%d\n",
                      elapsed / 1000UL, pir == HIGH ? "HIGH" : "LOW",
                      elapsed < WARMUP_MS ? "YES" : "NO", armed, showing, cooling);
    }
    // Visible wiring test independent of PIR: left, right, both, then off.
    if (elapsed < 3000) {
        eyes(elapsed < 1000 || elapsed >= 2000, elapsed >= 1000);
        return;
    }
    if (elapsed < WARMUP_MS) {
        eyes(false, false);
        return;
    }
    if (showing) {
        const unsigned long duration = now - show_ms;
        if (duration >= SHOW_MS) {
            showing = false;
            cooling = true;
            cooldown_ms = now;
            eyes(false, false);
            Serial.println("[END] 10-second cooldown.");
        } else {
            const unsigned long mode = duration / 2000;
            const bool phase = (duration / (mode == 3 ? 80 : mode == 2 ? 500 : 200)) % 2 == 0;
            if (mode == 0) {
                eyes(true, true);
            } else {
                eyes(phase, mode == 1 ? !phase : phase);
            }
        }
        return;
    }
    if (cooling) {
        if (now - cooldown_ms >= COOLDOWN_MS) {
            cooling = false;
            armed = false;
            Serial.println("[READY] Wait for LOW then motion.");
        }
        return;
    }
    if (pir == LOW) {
        armed = true;
    } else if (armed) {
        armed = false;
        showing = true;
        show_ms = now;
        eyes(true, true);
        Serial.println("[TRIGGER] PIR detected: 8-second eye pattern test.");
    }
}
