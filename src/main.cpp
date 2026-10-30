// ═══════════════════════════════════════════════════════════════════════════
// Міні-проєкт 2 · світлофор за ПДР (базовий рівень, без індикаторів)
// Плата: YD-ESP32-S3 (ESP32-S3-WROOM-1-N16R8) · PlatformIO · Arduino framework
//
// Цикл: зелений → зелений миготливий → жовтий → червоний → червоний + жовтий → зелений.
// Кнопка: увімк/вимк режим «жовтий миготливий». Вихід із нього — завжди з червоного.
//
// Три частини, як радив викладач: ініціалізація · логіка станів · відображення.
// Лише логіка міняє стан і лише render() пише в GPIO.
//
// Залізо: map.svg
// ═══════════════════════════════════════════════════════════════════════════
#include <Arduino.h>

namespace {
    // ── 1. Конфігурація ──────────────────────────────────────────────────────────
    struct Config {
        static constexpr uint8_t RED_PIN = 18;
        static constexpr uint8_t GREEN_PIN = 17; // на макетці зліва направо: червоний, зелений, жовтий
        static constexpr uint8_t YELLOW_PIN = 16;
        static constexpr uint8_t BTN_PIN = 4; // зовнішня підтяжка 10 кОм, RC

        static constexpr uint32_t GREEN_MS = 10000;
        static constexpr uint32_t GREEN_BLINK_MS = 3000;
        static constexpr uint32_t YELLOW_MS = 3000;
        static constexpr uint32_t RED_MS = 10000;
        static constexpr uint32_t RED_YELLOW_MS = 2000;
        static constexpr uint32_t BLINK_HALF_MS = 500; // 1 Гц: 500 мс горить, 500 мс ні

        static constexpr uint32_t POLL_MS = 5; // кнопка: спосіб 4 з домашки 2.4
        static constexpr uint8_t STABLE_READS = 4; // 4 × 5 мс = 20 мс однакового рівня
    };

    enum class Phase : uint8_t { Green, GreenBlink, Yellow, Red, RedYellow, YellowBlink };

    // Що означає кожен стан: назва, скільки триває, який наступний
    struct PhaseInfo {
        const char *name;
        uint32_t durationMs; // 0 = без обмеження часу
        Phase next;
    };

    PhaseInfo infoOf(const Phase p) {
        switch (p) {
            case Phase::Green:
                return {
                    .name = "зелений", .durationMs = Config::GREEN_MS, .next = Phase::GreenBlink
                };
            case Phase::GreenBlink:
                return {
                    .name = "зелений миготливий", .durationMs = Config::GREEN_BLINK_MS, .next = Phase::Yellow
                };
            case Phase::Yellow:
                return {
                    .name = "жовтий", .durationMs = Config::YELLOW_MS, .next = Phase::Red
                };
            case Phase::Red:
                return {
                    .name = "червоний", .durationMs = Config::RED_MS, .next = Phase::RedYellow
                };
            case Phase::RedYellow:
                return {
                    .name = "червоний + жовтий", .durationMs = Config::RED_YELLOW_MS, .next = Phase::Green
                };
            case Phase::YellowBlink:
                return {
                    .name = "жовтий миготливий", .durationMs = 0, .next = Phase::YellowBlink
                };
        }

        return {
            .name = "?", .durationMs = 0, .next = Phase::Red
        };
    }

    // ── 2. Логіка станів ─────────────────────────────────────────────────────────
    auto phase = Phase::Red;
    uint32_t phaseStart = 0;

    void enter(const Phase p, const uint32_t now) {
        phase = p;
        phaseStart = now;

        const PhaseInfo info = infoOf(p);
        Serial.printf("[%7lu мс] %s", static_cast<unsigned long>(now), info.name);
        if (info.durationMs > 0)
            Serial.printf(" · %lu с", static_cast<unsigned long>(info.durationMs / 1000));
        Serial.println();
    }

    void updatePhase(const uint32_t now) {
        const PhaseInfo info = infoOf(phase);
        if (info.durationMs > 0 && now - phaseStart >= info.durationMs)
            enter(info.next, now);
    }

    // Кнопка: новий рівень приймаємо, лише коли він тримається 20 мс
    bool buttonPressed(const uint32_t now) {
        static uint32_t lastPoll = 0;
        static uint8_t same = 0;
        static bool stable = HIGH;

        if (now - lastPoll < Config::POLL_MS)
            return false;
        lastPoll = now;

        const bool level = digitalRead(Config::BTN_PIN);
        same = (level == stable) ? 0 : same + 1;
        if (same < Config::STABLE_READS)
            return false;

        stable = level;
        same = 0;

        return stable == LOW; // подія лише на натисканні, не на відпусканні
    }

    // ── 3. Відображення: єдине місце, що пише в GPIO ─────────────────────────────
    void render(const uint32_t now) {
        const bool blinkOn = ((now - phaseStart) / Config::BLINK_HALF_MS) % 2 == 0;

        bool red = false, yellow = false, green = false;
        switch (phase) {
            case Phase::Green: green = true;
                break;
            case Phase::GreenBlink: green = blinkOn;
                break;
            case Phase::Yellow: yellow = true;
                break;
            case Phase::Red: red = true;
                break;
            case Phase::RedYellow: red = yellow = true;
                break;
            case Phase::YellowBlink: yellow = blinkOn;
                break;
        }

        digitalWrite(Config::RED_PIN, red ? HIGH : LOW);
        digitalWrite(Config::YELLOW_PIN, yellow ? HIGH : LOW);
        digitalWrite(Config::GREEN_PIN, green ? HIGH : LOW);
    }
} // namespace

// ── 4. Ініціалізація ─────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1500); // час на підключення монітора (рідний USB)

    pinMode(Config::RED_PIN, OUTPUT);
    pinMode(Config::YELLOW_PIN, OUTPUT);
    pinMode(Config::GREEN_PIN, OUTPUT);
    pinMode(Config::BTN_PIN, INPUT);

    enter(Phase::Red, millis());
}

void loop() {
    const uint32_t now = millis();

    if (buttonPressed(now))
        enter(phase == Phase::YellowBlink ? Phase::Red : Phase::YellowBlink, now);

    updatePhase(now);
    render(now);
}
