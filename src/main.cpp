#include <Arduino.h>
#include <driver/ledc.h>
#include <esp_system.h>

namespace {
constexpr uint8_t SERVO_PIN = 4;
constexpr uint8_t LEFT_EYE_PIN = 0;
constexpr uint8_t RIGHT_EYE_PIN = 1;
constexpr uint8_t PIR_PIN = 10;
constexpr uint8_t BUSY_PIN = 7;
constexpr uint8_t PLAYER_RX_PIN = 20;
constexpr uint8_t PLAYER_TX_PIN = 21;
constexpr uint8_t VOLUME = 30;  // Maximum; check clipping, supply sag and amplifier heat.
constexpr int JAW_OPEN_US = 1200;
constexpr int JAW_CLOSED_US = 1600;
constexpr int JAW_START_US = 1500;
constexpr uint32_t PWM_MAX = (1UL << 14) - 1;

constexpr unsigned long COOLDOWN_MS = 10000;
constexpr unsigned long START_TIMEOUT_MS = 5000;
constexpr unsigned long MAX_PLAY_MS = 60000;
constexpr unsigned long BUSY_DEBOUNCE_MS = 80;
constexpr unsigned long SERVO_STEP_MS = 20;
constexpr int SERVO_STEP_US = 10;
constexpr unsigned long JAW_HALF_CYCLE_MS = 1000;
constexpr unsigned long JAW_CYCLE_MS = 2 * JAW_HALF_CYCLE_MS;
constexpr uint8_t JAW_CYCLES = 5;
constexpr unsigned long EFFECTS_MS = JAW_CYCLE_MS * JAW_CYCLES;

enum class State { BOOT, IDLE, WAIT_AUDIO, PLAYING, EFFECTS_ONLY, COOLDOWN, FAULT };
enum class EyeMode { STEADY, ALTERNATE, SLOW, FAST };
HardwareSerial player(1);
State state = State::BOOT;
EyeMode eye_mode = EyeMode::STEADY;
uint8_t init_stage = 0;
uint8_t count_attempts = 0;
constexpr uint16_t MAX_TRACKS = 3000;
uint16_t track_count = 0;
uint16_t current_track = 0;
uint16_t track_bag[MAX_TRACKS];
uint16_t bag_count = 0;
uint16_t bag_position = 0;
unsigned long boot_ms = 0;
unsigned long init_ms = 0;
unsigned long state_ms = 0;
unsigned long eye_ms = 0;
unsigned long effects_ms = 0;
bool effects_running = false;
unsigned long jaw_step_ms = 0;
unsigned long busy_changed_ms = 0;
int jaw_us = JAW_START_US;
int jaw_target_us = JAW_START_US;
bool servo_ready = false;
bool pir_armed = false;
bool busy_raw = false;
bool busy = false;
bool eye_phase = true;
bool finish_received = false;
bool player_error = false;
bool boot_sound_started = false;
bool boot_stop_sent = false;
unsigned long boot_log_ms = 0;
unsigned long boot_stop_ms = 0;

uint32_t choose(uint32_t count) {
    // Rejection sampling avoids modulo bias and does not depend on Wi-Fi entropy.
    const uint32_t threshold = static_cast<uint32_t>(-count) % count;
    uint32_t value;
    do {
        value = esp_random();
    } while (value < threshold);
    return value % count;
}

uint16_t next_track() {
    if (bag_count != track_count || bag_position >= bag_count) {
        bag_count = track_count;
        bag_position = 0;
        for (uint16_t i = 0; i < bag_count; ++i) {
            track_bag[i] = i + 1;
        }
        for (uint16_t remaining = bag_count; remaining > 1; --remaining) {
            const uint16_t index = choose(remaining);
            const uint16_t saved = track_bag[remaining - 1];
            track_bag[remaining - 1] = track_bag[index];
            track_bag[index] = saved;
        }
        // Avoid an immediate repeat across rounds without dropping any track.
        if (bag_count > 1 && track_bag[0] == current_track) {
            const uint16_t index = 1 + choose(bag_count - 1);
            const uint16_t saved = track_bag[0];
            track_bag[0] = track_bag[index];
            track_bag[index] = saved;
        }
        Serial.printf("[SHUFFLE] New round: %u tracks.\n", bag_count);
    }
    return track_bag[bag_position++];
}

void eyes(bool left, bool right) {
    digitalWrite(LEFT_EYE_PIN, left ? HIGH : LOW);
    digitalWrite(RIGHT_EYE_PIN, right ? HIGH : LOW);
}

bool write_jaw() {
    const uint32_t duty = (static_cast<uint32_t>(jaw_us) * PWM_MAX + 10000UL) / 20000UL;
    if (ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty) != ESP_OK
        || ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0) != ESP_OK) {
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        servo_ready = false;
        Serial.println("[FAULT] Servo PWM failed; power off and inspect.");
        return false;
    }
    return true;
}

bool setup_servo() {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = 50;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_channel_config_t channel = {};
    channel.gpio_num = SERVO_PIN;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.intr_type = LEDC_INTR_DISABLE;
    channel.timer_sel = LEDC_TIMER_0;
    channel.duty = (JAW_START_US * PWM_MAX + 10000UL) / 20000UL;
    return ledc_timer_config(&timer) == ESP_OK && ledc_channel_config(&channel) == ESP_OK;
}

void send_command(uint8_t command, uint16_t parameter) {
    uint8_t frame[] = {0x7E, 0xFF, 0x06, command, 0x01,
                       static_cast<uint8_t>(parameter >> 8),
                       static_cast<uint8_t>(parameter), 0, 0, 0xEF};
    uint16_t sum = 0;
    for (uint8_t i = 1; i <= 6; ++i) {
        sum += frame[i];
    }
    const uint16_t checksum = static_cast<uint16_t>(0U - sum);
    frame[7] = checksum >> 8;
    frame[8] = checksum & 0xFF;
    player.write(frame, sizeof(frame));
    Serial.printf("[TX] %02X / %u\n", command, parameter);
}

bool active() {
    return state == State::WAIT_AUDIO || state == State::PLAYING;
}

void read_replies() {
    static uint8_t frame[10];
    static uint8_t count = 0;
    static unsigned long last_byte_ms = 0;
    while (player.available()) {
        const unsigned long now = millis();
        if (count && now - last_byte_ms > 100) {
            count = 0;
        }
        const uint8_t value = player.read();
        last_byte_ms = now;
        if (!count && value != 0x7E) {
            continue;
        }
        frame[count++] = value;
        if (count != sizeof(frame)) {
            continue;
        }
        count = 0;
        uint16_t sum = 0;
        for (uint8_t i = 1; i <= 6; ++i) {
            sum += frame[i];
        }
        const uint16_t checksum = (static_cast<uint16_t>(frame[7]) << 8) | frame[8];
        if (frame[1] != 0xFF || frame[2] != 6 || frame[9] != 0xEF
            || checksum != static_cast<uint16_t>(0U - sum)) {
            Serial.println("[RX] Invalid frame; check wiring/power.");
            continue;
        }
        const uint16_t parameter = (static_cast<uint16_t>(frame[5]) << 8) | frame[6];
        Serial.printf("[RX] %02X / %u\n", frame[3], parameter);
        if (frame[3] == 0x48 && state == State::BOOT && parameter > 0 && parameter <= MAX_TRACKS) {
            track_count = parameter;
        } else if (frame[3] == 0x3D && active() && parameter == current_track) {
            finish_received = true;
        } else if (frame[3] == 0x40 && (active() || state == State::EFFECTS_ONLY
                   || (state == State::BOOT && boot_sound_started))) {
            player_error = true;
        } else if (frame[3] == 0x3B && (parameter & 2)) {
            if (active() || state == State::EFFECTS_ONLY) {
                player_error = true;
            } else {
                state = State::FAULT;
                effects_running = false;
                eyes(false, false);
                jaw_target_us = JAW_CLOSED_US;
                Serial.println("[FAULT] SD removed. Power off before reinserting; restart.");
            }
        }
    }
}

void finish_show(unsigned long now, const char* reason, bool stop_audio) {
    if (!stop_audio && effects_running) {
        state = State::EFFECTS_ONLY;
        Serial.printf("[AUDIO END] %s; eyes/jaw continue to 10 seconds.\n", reason);
        return;
    }
    effects_running = false;
    if (stop_audio) {
        send_command(0x16, 0);
    }
    eyes(false, false);
    jaw_target_us = JAW_CLOSED_US;
    state = State::COOLDOWN;
    state_ms = now;
    pir_armed = false;
    Serial.printf("[END] %s; cooldown 10 seconds.\n", reason);
}

void start_show(unsigned long now) {
    current_track = next_track();
    eye_mode = static_cast<EyeMode>(choose(4));
    finish_received = false;
    player_error = false;
    pir_armed = false;
    state = State::WAIT_AUDIO;
    state_ms = now;
    effects_ms = now;
    effects_running = true;
    eye_ms = now;
    eye_phase = true;
    send_command(0x03, current_track);  // Global SD index, independent of filename.
    Serial.printf("[SHOW] Track %u/%u, eye mode %u\n", current_track, track_count,
                  static_cast<unsigned>(eye_mode));
}

void update_effects(unsigned long now) {
    if (effects_running && now - effects_ms >= EFFECTS_MS) {
        effects_running = false;
        jaw_target_us = JAW_CLOSED_US;
        eyes(false, false);
        Serial.println("[EFFECTS END] Five 2-second jaw cycles complete; eyes off.");
    }
    if (effects_running) {
        const unsigned long phase = (now - effects_ms) % JAW_CYCLE_MS;
        const unsigned long travel = phase <= JAW_HALF_CYCLE_MS
            ? phase : JAW_CYCLE_MS - phase;
        jaw_target_us = JAW_CLOSED_US - static_cast<int>(
            (JAW_CLOSED_US - JAW_OPEN_US) * travel / JAW_HALF_CYCLE_MS);
        if (state == State::BOOT) {
            const bool pir_high = digitalRead(PIR_PIN) == HIGH;
            const bool flash = ((now - effects_ms) / 150) % 2 == 0;
            eyes(pir_high && flash, pir_high && flash);
        } else if (eye_mode == EyeMode::STEADY) {
            eyes(true, true);
        } else {
            const unsigned long interval = eye_mode == EyeMode::FAST ? 80
                : eye_mode == EyeMode::SLOW ? 500 : 200;
            if (now - eye_ms >= interval) {
                eye_ms = now;
                eye_phase = !eye_phase;
            }
            eyes(eye_phase, eye_mode == EyeMode::ALTERNATE ? !eye_phase : eye_phase);
        }
    }
    if (servo_ready && now - jaw_step_ms >= SERVO_STEP_MS && jaw_us != jaw_target_us) {
        jaw_step_ms = now;
        jaw_us += constrain(jaw_target_us - jaw_us, -SERVO_STEP_US, SERVO_STEP_US);
        if (!write_jaw()) {
            send_command(0x16, 0);
            eyes(false, false);
            effects_running = false;
            state = State::FAULT;
        }
    }
}

void update_boot(unsigned long now) {
    if (player_error) {
        send_command(0x16, 0);
        effects_running = false;
        eyes(false, false);
        jaw_target_us = JAW_CLOSED_US;
        state = State::FAULT;
        Serial.println("[FAULT] DFPlayer error during boot audio test; inspect SD/power and restart.");
        return;
    }
    if (init_stage == 0 && now - boot_ms >= 3000) {
        send_command(0x09, 2);  // Select microSD.
        init_stage = 1;
        init_ms = now;
    } else if (init_stage == 1 && now - init_ms >= 300) {
        send_command(0x16, 0);
        init_stage = 2;
        init_ms = now;
    } else if (init_stage == 2 && now - init_ms >= 300) {
        send_command(0x06, VOLUME);
        init_stage = 3;
        init_ms = now;
    } else if (init_stage == 3 && now - init_ms >= 300) {
        send_command(0x48, 0);  // Query actual SD track count.
        ++count_attempts;
        init_stage = 4;
        init_ms = now;
    } else if (init_stage == 4 && !track_count && now - init_ms >= 2000) {
        if (count_attempts < 3) {
            init_stage = 3;
        } else {
            state = State::FAULT;
            effects_running = false;
            eyes(false, false);
            jaw_target_us = JAW_CLOSED_US;
            send_command(0x16, 0);
            Serial.println("[FAULT] No SD track count. Check UART TX/RX, SD and power; restart.");
        }
    }
    if (state != State::BOOT) {
        return;
    }
    if (now - boot_log_ms >= 1000) {
        boot_log_ms = now;
        Serial.printf("[SELF TEST] PIR GPIO10=%s, player BUSY=%d, tracks=%u\n",
                      digitalRead(PIR_PIN) == HIGH ? "HIGH" : "LOW", busy, track_count);
    }
    if (now - boot_ms < EFFECTS_MS) {
        if (init_stage == 4 && track_count && !boot_sound_started && !busy) {
            current_track = next_track();
            send_command(0x03, current_track);
            boot_sound_started = true;
            Serial.printf("[SELF TEST] Random audio track %u.\n", current_track);
        }
    } else {
        if (!boot_stop_sent) {
            send_command(0x16, 0);
            boot_stop_sent = true;
            boot_stop_ms = now;
            eyes(false, false);
            jaw_target_us = JAW_CLOSED_US;
            Serial.println("[SELF TEST END] 10 seconds elapsed; stop audio, close jaw, eyes off.");
        }
        // Wait for STOP to settle before allowing a new playback command.
        if (init_stage == 4 && track_count && !busy && now - boot_stop_ms >= 300) {
            state = State::IDLE;
            pir_armed = false;
            Serial.printf("[READY] %u tracks. Waiting for PIR LOW then HIGH.\n", track_count);
        }
    }
}
}  // namespace

void setup() {
    digitalWrite(LEFT_EYE_PIN, LOW);
    digitalWrite(RIGHT_EYE_PIN, LOW);
    pinMode(LEFT_EYE_PIN, OUTPUT);
    pinMode(RIGHT_EYE_PIN, OUTPUT);
    pinMode(PIR_PIN, INPUT_PULLDOWN);
    pinMode(BUSY_PIN, INPUT_PULLUP);
    digitalWrite(SERVO_PIN, LOW);
    pinMode(SERVO_PIN, OUTPUT);
    Serial.begin(115200);
    player.begin(9600, SERIAL_8N1, PLAYER_RX_PIN, PLAYER_TX_PIN);
    servo_ready = setup_servo();
    boot_ms = millis();
    init_ms = boot_ms;
    busy_raw = digitalRead(BUSY_PIN) == LOW;
    busy_changed_ms = boot_ms;
    jaw_target_us = JAW_CLOSED_US;
    effects_ms = boot_ms;
    effects_running = servo_ready;
    if (!servo_ready) {
        state = State::FAULT;
    }
    Serial.printf("[BOOT] Skull; reset=%d. Initial servo command 1500 us, then close to 1600 us.\n",
                  static_cast<int>(esp_reset_reason()));
    Serial.println("[BOOT] 10s SELF TEST: jaw cycles, eyes flash on PIR HIGH, random audio when SD ready. Volume 30/30.");
}

void loop() {
    read_replies();
    const unsigned long now = millis();
    const bool reading = digitalRead(BUSY_PIN) == LOW;
    if (reading != busy_raw) {
        busy_raw = reading;
        busy_changed_ms = now;
    }
    if (now - busy_changed_ms >= BUSY_DEBOUNCE_MS) {
        busy = busy_raw;
    }
    if (state == State::BOOT) {
        update_boot(now);
    } else if (active()) {
        if (player_error) {
            finish_show(now, "DFPlayer error", true);
        } else if (finish_received) {
            finish_show(now, "track finished", false);
        } else if (state == State::WAIT_AUDIO) {
            if (busy) {
                state = State::PLAYING;
                // Keep state_ms at the play command for the absolute safety timeout.

                Serial.println("[PLAYING] BUSY confirmed.");
            } else if (now - state_ms >= START_TIMEOUT_MS) {
                finish_show(now, "audio did not start / BUSY missing", true);
            }
        } else if (!busy) {
            finish_show(now, "BUSY idle", false);
        } else if (now - state_ms >= MAX_PLAY_MS) {
            finish_show(now, "60-second safety timeout", true);
        }
    } else if (state == State::EFFECTS_ONLY) {
        if (player_error) {
            finish_show(now, "DFPlayer error during effects", true);
        } else if (!effects_running) {
            finish_show(now, "audio and effects complete", false);
        }
    } else if (state == State::COOLDOWN && now - state_ms >= COOLDOWN_MS) {
        state = State::IDLE;
        pir_armed = false;
        Serial.println("[READY] Wait for PIR LOW then HIGH.");
    } else if (state == State::IDLE) {
        if (digitalRead(PIR_PIN) == LOW) {
            pir_armed = true;
        } else if (pir_armed) {
            // Consume the edge even if the player is unexpectedly busy; never queue a trigger.
            pir_armed = false;
            if (!busy) {
                start_show(now);
            }
        }
    }
    update_effects(now);
}
