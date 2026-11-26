#pragma once
#include <stdbool.h>

// Modulo de conexion: WiFi + MQTT con Adafruit IO.
// Nunca bloquea al juego: si no hay internet los puntajes se
// quedan en una cola en RAM y se mandan al reconectar.

void net_init(void);

void net_publicar_puntaje(int puntaje);   // feed "puntaje"
void net_publicar_record(int record);     // feed "record"

bool net_conectado(void);      // WiFi + broker MQTT listos
int  net_dificultad(void);     // 1..3, viene del feed "dificultad"
int  net_record_nube(void);    // ultimo record guardado en la nube
int  net_pendientes(void);     // mensajes esperando para enviarse
