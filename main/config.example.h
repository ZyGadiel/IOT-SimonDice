#pragma once
// ---------------------------------------------------------------
// Copia este archivo como config.h y pon tus datos reales.
// config.h esta en .gitignore: NUNCA se sube al repositorio.
// ---------------------------------------------------------------

// WiFi de 2.4 GHz con contrasena normal (WPA2-Personal).
// Las redes de la universidad con portal de inicio de sesion no
// funcionan: usa el hotspot de tu celular en 2.4 GHz.
#define WIFI_SSID      "nombre_de_tu_red"
#define WIFI_PASS      "contrasena_de_tu_red"

// Adafruit IO -> icono de llave amarilla ("My Key")
#define AIO_USERNAME   "tu_usuario_adafruit"
#define AIO_KEY        "aio_XXXXXXXXXXXXXXXXXXXXXXXXXXXX"

// 1 = MQTT cifrado (mqtts, puerto 8883). 0 = sin cifrar (1883).
#define USAR_TLS       1

// Dificultad al encender, antes de recibir la del dashboard (1-3)
#define DIFICULTAD_INICIAL 2
