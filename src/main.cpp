#include <Arduino.h>
#include <driver/ledc.h>
#include <esp_system.h>

namespace {
constexpr uint8_t SERVO_PIN = 4;
constexpr uint32_t SERVO_FREQUENCY_HZ = 50;
constexpr uint32_t PWM_MAX = (1UL << 14) - 1;
constexpr uint32_t JAW_OPEN_PULSE_US = 1200;
constexpr uint32_t CENTER_PULSE_US = 1500;
constexpr uint32_t JAW_CLOSED_PULSE_US = 1600;

constexpr uint8_t INCREASE_PIN = 5;
constexpr uint8_t DECREASE_PIN = 6;
constexpr unsigned long DEBOUNCE_MS = 30;
constexpr int STEP_US = 50;

struct Button {
    uint8_t pin;
    bool raw_pressed = false;
    bool pressed = false;
    unsigned long changed_ms = 0;

    bool update(unsigned long now) {
        const bool reading = digitalRead(pin) == LOW;
        if (reading != raw_pressed) {
            raw_pressed = reading;
            changed_ms = now;
        }
        if (pressed != raw_pressed && now - changed_ms >= DEBOUNCE_MS) {
            pressed = raw_pressed;
            return pressed;
        }
        return false;
    }
};

Button increase_button{INCREASE_PIN};
Button decrease_button{DECREASE_PIN};

uint32_t pulse_us = CENTER_PULSE_US;
bool pwm_ready = false;


bool wait_for_release = true;

bool write_pulse() {
    const uint32_t duty = (pulse_us * PWM_MAX + 10000UL) / 20000UL;
    const esp_err_t set_result = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    const esp_err_t update_result = set_result == ESP_OK
        ? ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0) : set_result;
    if (update_result != ESP_OK) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        pwm_ready = false;
        Serial.printf("[ERROR] PWM failed: %d. Restart to retry.\n", update_result);
        return false;
    }

    Serial.printf("[SERVO] Commanded angle (estimated, not measured): %d deg | %lu us\n", 60 + static_cast<int>(pulse_us - JAW_OPEN_PULSE_US) / 10,
                  static_cast<unsigned long>(pulse_us));
    return true;
}
}  // namespace

void setup() {
    pinMode(SERVO_PIN, OUTPUT);
    digitalWrite(SERVO_PIN, LOW);

    pinMode(INCREASE_PIN, INPUT_PULLUP);
    pinMode(DECREASE_PIN, INPUT_PULLUP);
    Serial.begin(115200);

    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = SERVO_FREQUENCY_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;

    ledc_channel_config_t channel = {};
    channel.gpio_num = SERVO_PIN;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = (CENTER_PULSE_US * PWM_MAX + 10000UL) / 20000UL;
    channel.hpoint = 0;

    const esp_err_t timer_result = ledc_timer_config(&timer);
    const esp_err_t channel_result = timer_result == ESP_OK
        ? ledc_channel_config(&channel) : timer_result;
    pwm_ready = channel_result == ESP_OK;
    if (!pwm_ready) {
        Serial.printf("[ERROR] PWM setup failed: %d\n", channel_result);
    }
    if (pwm_ready) {
        Serial.println("[START] Center command active: approximately 90 deg (1500 us). Initial move may be fast.");
        write_pulse();
    }
    Serial.printf("[BOOT] Reset reason: %d\n", static_cast<int>(esp_reset_reason()));
}

void loop() {
    if (!pwm_ready) {
        return;
    }
    const unsigned long now = millis();
    const bool increase_edge = increase_button.update(now);
    const bool decrease_edge = decrease_button.update(now);

    if (increase_button.raw_pressed && decrease_button.raw_pressed) {
        wait_for_release = true;
    }
    if (wait_for_release) {
        if (!increase_button.raw_pressed && !decrease_button.raw_pressed
            && !increase_button.pressed && !decrease_button.pressed) {
            wait_for_release = false;
        }
        return;
    }
    if (increase_edge == decrease_edge) {
        return;
    }
    Serial.printf("[BUTTON] GPIO%u pressed\n", increase_edge ? INCREASE_PIN : DECREASE_PIN);

    const uint32_t next_pulse_us = static_cast<uint32_t>(constrain(
        static_cast<int>(pulse_us) + (increase_edge ? STEP_US : -STEP_US),
        static_cast<int>(JAW_OPEN_PULSE_US), static_cast<int>(JAW_CLOSED_PULSE_US)));
    if (next_pulse_us == pulse_us) {
        Serial.println("[LIMIT] End of test range.");
        return;
    }
    pulse_us = next_pulse_us;
    write_pulse();
}
