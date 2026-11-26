#include "io.h"
#include "pins.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DEBOUNCE_MS 30

static const gpio_num_t LEDS[NUM_COLORES] = {
    PIN_LED_ROJO, PIN_LED_VERDE, PIN_LED_AZUL, PIN_LED_AMARILLO};
static const gpio_num_t BOTONES[NUM_COLORES] = {
    PIN_BTN_ROJO, PIN_BTN_VERDE, PIN_BTN_AZUL, PIN_BTN_AMARILLO};
static const bool LED_INVERTIDO[NUM_COLORES] = {
    false, false, LED_AZUL_ACTIVO_BAJO, false};

// Tonos clasicos del Simon (Hz): rojo, verde, azul, amarillo
static const uint32_t TONOS[NUM_COLORES] = {310, 415, 209, 252};

static void esperar(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

void io_init(void)
{
    gpio_config_t salidas = {
        .pin_bit_mask = (1ULL << PIN_LED_ROJO) | (1ULL << PIN_LED_VERDE) |
                        (1ULL << PIN_LED_AZUL) | (1ULL << PIN_LED_AMARILLO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&salidas);

    gpio_config_t entradas = {
        .pin_bit_mask = (1ULL << PIN_BTN_ROJO) | (1ULL << PIN_BTN_VERDE) |
                        (1ULL << PIN_BTN_AZUL) | (1ULL << PIN_BTN_AMARILLO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&entradas);
    io_todos_leds(false);

    // PWM (LEDC) para generar los tonos del buzzer pasivo
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 1000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t canal = {
        .gpio_num = PIN_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&canal);
}

void io_led(int color, bool encendido)
{
    gpio_set_level(LEDS[color], encendido != LED_INVERTIDO[color]);
}

void io_todos_leds(bool encendido)
{
    for (int c = 0; c < NUM_COLORES; c++) io_led(c, encendido);
}

void io_tono(uint32_t frecuencia_hz)
{
    if (frecuencia_hz == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, frecuencia_hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512); // 50 %
    }
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void io_mostrar_color(int color, int ms)
{
    io_led(color, true);
    io_tono(TONOS[color]);
    esperar(ms);
    io_tono(0);
    io_led(color, false);
}

int io_leer_boton(int timeout_ms)
{
    int64_t limite = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    while (esp_timer_get_time() < limite) {
        for (int c = 0; c < NUM_COLORES; c++) {
            if (gpio_get_level(BOTONES[c]) != 0) continue;

            // Antirrebote: confirmar que sigue presionado
            esperar(DEBOUNCE_MS);
            if (gpio_get_level(BOTONES[c]) != 0) continue;

            // Retroalimentacion mientras se mantiene presionado
            io_led(c, true);
            io_tono(TONOS[c]);
            while (gpio_get_level(BOTONES[c]) == 0) esperar(10);
            io_tono(0);
            io_led(c, false);

            esperar(DEBOUNCE_MS);   // antirrebote al soltar
            return c;
        }
        esperar(10);
    }
    return -1;
}

void io_sonido_error(void)
{
    io_todos_leds(true);
    io_tono(120);
    esperar(900);
    io_tono(0);
    io_todos_leds(false);
}

void io_sonido_record(void)
{
    static const uint32_t notas[] = {523, 659, 784, 1047};
    for (int i = 0; i < 4; i++) {
        io_led(i, true);
        io_tono(notas[i]);
        esperar(140);
        io_led(i, false);
    }
    io_tono(0);
}
