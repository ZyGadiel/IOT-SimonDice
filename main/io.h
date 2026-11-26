#pragma once
#include <stdbool.h>
#include <stdint.h>

// Modulo de entradas y salidas: LEDs, buzzer y botones.

void io_init(void);
void io_led(int color, bool encendido);
void io_todos_leds(bool encendido);
void io_tono(uint32_t frecuencia_hz);          // 0 = silencio

// Enciende el LED y su tono durante 'ms' milisegundos.
void io_mostrar_color(int color, int ms);

// Espera a que se presione un boton (con antirrebote).
// Devuelve el color presionado, o -1 si pasa 'timeout_ms'.
int io_leer_boton(int timeout_ms);

void io_sonido_error(void);
void io_sonido_record(void);
