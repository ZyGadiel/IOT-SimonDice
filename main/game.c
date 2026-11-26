#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "game.h"
#include "io.h"
#include "display.h"
#include "net.h"
#include "pins.h"
#include "esp_random.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "juego"
#define MAX_SECUENCIA        64
#define TIEMPO_RESPUESTA_MS  5000   // tiempo maximo para cada boton

// Velocidad segun la dificultad que llega de la nube (1, 2, 3)
typedef struct { int encendido_ms; int pausa_ms; } velocidad_t;
static const velocidad_t VELOCIDADES[3] = {
    {700, 300},   // 1 = facil
    {450, 180},   // 2 = normal
    {250,  90},   // 3 = dificil
};

static uint8_t secuencia[MAX_SECUENCIA];
static int record = 0;

static void esperar(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

// ---------- Pantallas ----------
static void barra_estado(void)
{
    char linea[24];
    if (net_conectado())
        snprintf(linea, sizeof linea, "Dif:%d  En linea", net_dificultad());
    else
        snprintf(linea, sizeof linea, "Dif:%d  Sin conexion", net_dificultad());
    display_texto(0, 7, linea, 1);

    if (net_pendientes() > 0) {
        snprintf(linea, sizeof linea, "Pendientes: %d", net_pendientes());
        display_texto(0, 6, linea, 1);
    }
}

static void pantalla_inicio(void)
{
    char linea[24];
    display_limpiar();
    display_texto_centrado(0, "SIMON DICE", 2);
    snprintf(linea, sizeof linea, "Record: %d", record);
    display_texto_centrado(3, linea, 1);
    display_texto_centrado(4, "Presiona un boton", 1);
    barra_estado();
    display_mostrar();
}

static void pantalla_nivel(int nivel, const char *mensaje)
{
    char linea[24];
    display_limpiar();
    snprintf(linea, sizeof linea, "Nivel %d", nivel);
    display_texto_centrado(0, linea, 2);
    display_texto_centrado(3, mensaje, 1);
    snprintf(linea, sizeof linea, "Record: %d", record);
    display_texto_centrado(5, linea, 1);
    barra_estado();
    display_mostrar();
}

static void pantalla_fin(int puntaje, bool nuevo_record)
{
    char linea[24];
    display_limpiar();
    display_texto_centrado(0, "FIN", 2);
    snprintf(linea, sizeof linea, "Puntaje: %d", puntaje);
    display_texto_centrado(3, linea, 1);
    display_texto_centrado(4, nuevo_record ? "NUEVO RECORD!" : "", 1);
    barra_estado();
    display_mostrar();
}

// ---------- Una partida completa ----------
static int jugar_partida(void)
{
    int nivel = 0;

    while (nivel < MAX_SECUENCIA) {
        secuencia[nivel] = esp_random() % NUM_COLORES;   // agrega un color nuevo
        nivel++;

        // La dificultad se lee al inicio de cada ronda, asi un cambio
        // desde el dashboard se aplica en la siguiente ronda.
        velocidad_t v = VELOCIDADES[net_dificultad() - 1];

        pantalla_nivel(nivel, "Observa...");
        esperar(700);
        for (int i = 0; i < nivel; i++) {
            io_mostrar_color(secuencia[i], v.encendido_ms);
            esperar(v.pausa_ms);
        }

        pantalla_nivel(nivel, "Tu turno!");
        for (int i = 0; i < nivel; i++) {
            int boton = io_leer_boton(TIEMPO_RESPUESTA_MS);
            if (boton != secuencia[i]) {
                ESP_LOGI(TAG, "Error en nivel %d (esperado %d, presiono %d)",
                         nivel, secuencia[i], boton);
                return nivel - 1;       // rondas completadas
            }
        }
        esperar(400);                   // pausa entre rondas
    }
    return nivel;                       // completo la secuencia maxima
}

void game_run(void)
{
    while (1) {
        // Esperar a que el jugador inicie (se redibuja para ver cambios
        // de conexion o dificultad)
        int boton;
        do {
            if (net_record_nube() > record) record = net_record_nube();
            pantalla_inicio();
            boton = io_leer_boton(500);
        } while (boton < 0);
        esperar(500);

        int puntaje = jugar_partida();
        io_sonido_error();

        bool nuevo = puntaje > record;
        if (nuevo) {
            record = puntaje;
            net_publicar_record(record);
        }
        net_publicar_puntaje(puntaje);   // se envia ahora o al reconectar
        ESP_LOGI(TAG, "Partida terminada: puntaje %d, record %d", puntaje, record);

        pantalla_fin(puntaje, nuevo);
        if (nuevo) io_sonido_record();
        esperar(3000);
    }
}
