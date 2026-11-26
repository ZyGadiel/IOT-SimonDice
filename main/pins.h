#pragma once
// ---------------------------------------------------------------
// Mapa de pines del Simon Dice IoT (ESP32 DevKit)
// Se evitan los pines de arranque (0, 2, 5, 12, 15), los de la
// flash (6-11) y los de solo entrada (34-39).
// ---------------------------------------------------------------

#define NUM_COLORES 4

// Indices de color usados en todo el programa
#define COLOR_ROJO      0
#define COLOR_VERDE     1
#define COLOR_AZUL      2
#define COLOR_AMARILLO  3

// LEDs (GPIO -> resistencia 220 ohm -> pata larga del LED)
#define PIN_LED_ROJO      25
#define PIN_LED_VERDE     26
#define PIN_LED_AZUL      27   // pata azul del LED RGB del kit
#define PIN_LED_AMARILLO  14

// Botones (una pata al GPIO, la pata diagonal a GND).
// Se usa el pull-up interno: suelto = 1, presionado = 0.
// (Botones y OLED del lado derecho del ESP32, LEDs y buzzer del izquierdo)
#define PIN_BTN_ROJO      23
#define PIN_BTN_VERDE     19
#define PIN_BTN_AZUL      18
#define PIN_BTN_AMARILLO  4

// Buzzer PASIVO (el que permite distintos tonos)
#define PIN_BUZZER        13

// Pantalla OLED 0.96" I2C (SSD1306)
#define PIN_OLED_SDA      21
#define PIN_OLED_SCL      22
#define OLED_ADDR         0x3C

// El LED RGB puede ser de catodo comun (pata larga a GND) o de
// anodo comun (pata larga a 3V3). Si es de anodo comun pon 1.
#define LED_AZUL_ACTIVO_BAJO 0
