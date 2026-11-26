#pragma once

// Modulo de pantalla: OLED SSD1306 128x64 por I2C.
// 'fila' va de 0 a 7 (cada fila mide 8 pixeles de alto).
// 'escala' 1 = letra chica (21 caracteres por linea), 2 = grande.
// Si en la practica 4 usaron otro driver, pueden reemplazar este
// archivo siempre que mantengan estas mismas funciones.

void display_init(void);
void display_limpiar(void);
void display_texto(int x, int fila, const char *texto, int escala);
void display_texto_centrado(int fila, const char *texto, int escala);
void display_mostrar(void);   // manda el buffer a la pantalla
