#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#if USAR_TLS
#include "esp_crt_bundle.h"
#endif

#define TAG "net"
#define MAX_PENDIENTES      20
// La cuenta gratis de Adafruit IO permite 30 datos/minuto.
// Un mensaje cada 2.5 s = 24/min como maximo.
#define INTERVALO_ENVIO_MS  2500

typedef enum { FEED_PUNTAJE, FEED_RECORD } feed_t;
typedef struct { feed_t feed; int valor; } mensaje_t;

// ---- Cola circular de mensajes pendientes (en RAM) ----
static mensaje_t cola[MAX_PENDIENTES];
static int cola_inicio = 0, cola_cantidad = 0;
static SemaphoreHandle_t cola_mutex;

// ---- Estado de la conexion ----
static esp_mqtt_client_handle_t cliente;
static bool mqtt_iniciado = false;
static volatile bool mqtt_ok = false;
static volatile int dificultad = DIFICULTAD_INICIAL;
static volatile int record_nube = 0;

// Topics: usuario/feeds/nombre
static char t_puntaje[64], t_record[64], t_dificultad[64];

// Para medir latencia (publicacion -> PUBACK del broker)
static volatile int ultimo_msg_id = -1;
static volatile int64_t t_envio_us = 0;

// ================= Cola =================
static void encolar(feed_t feed, int valor)
{
    xSemaphoreTake(cola_mutex, portMAX_DELAY);
    if (cola_cantidad == MAX_PENDIENTES) {        // llena: se tira el mas viejo
        cola_inicio = (cola_inicio + 1) % MAX_PENDIENTES;
        cola_cantidad--;
        ESP_LOGW(TAG, "Cola llena, se descarta el mensaje mas viejo");
    }
    cola[(cola_inicio + cola_cantidad) % MAX_PENDIENTES] = (mensaje_t){feed, valor};
    cola_cantidad++;
    xSemaphoreGive(cola_mutex);
}

static bool sacar(mensaje_t *m)
{
    bool hay = false;
    xSemaphoreTake(cola_mutex, portMAX_DELAY);
    if (cola_cantidad > 0) {
        *m = cola[cola_inicio];
        cola_inicio = (cola_inicio + 1) % MAX_PENDIENTES;
        cola_cantidad--;
        hay = true;
    }
    xSemaphoreGive(cola_mutex);
    return hay;
}

static void regresar_al_frente(mensaje_t m)
{
    xSemaphoreTake(cola_mutex, portMAX_DELAY);
    if (cola_cantidad < MAX_PENDIENTES) {
        cola_inicio = (cola_inicio - 1 + MAX_PENDIENTES) % MAX_PENDIENTES;
        cola[cola_inicio] = m;
        cola_cantidad++;
    }
    xSemaphoreGive(cola_mutex);
}

// Tarea que vacia la cola respetando el limite de Adafruit IO
static void tarea_envio(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(INTERVALO_ENVIO_MS));
        if (!mqtt_ok) continue;

        mensaje_t m;
        if (!sacar(&m)) continue;

        char payload[12];
        snprintf(payload, sizeof payload, "%d", m.valor);
        const char *topic = (m.feed == FEED_PUNTAJE) ? t_puntaje : t_record;

        t_envio_us = esp_timer_get_time();
        int id = esp_mqtt_client_publish(cliente, topic, payload, 0, 1, 0);
        if (id >= 0) {
            ultimo_msg_id = id;
            ESP_LOGI(TAG, "Enviado %s = %s (quedan %d)", topic, payload, cola_cantidad);
        } else {
            ESP_LOGW(TAG, "Fallo el envio, se reintenta despues");
            regresar_al_frente(m);
        }
    }
}

// ================= MQTT =================
static bool topic_es(esp_mqtt_event_handle_t e, const char *topic)
{
    return e->topic_len == (int)strlen(topic) &&
           strncmp(e->topic, topic, e->topic_len) == 0;
}

static void evento_mqtt(void *args, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = data;
    char tmp[80];

    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Conectado a Adafruit IO");
        mqtt_ok = true;
        esp_mqtt_client_subscribe(cliente, t_dificultad, 1);
        esp_mqtt_client_subscribe(cliente, t_record, 1);
        // En Adafruit IO, publicar en <feed>/get pide el ultimo valor guardado
        snprintf(tmp, sizeof tmp, "%s/get", t_dificultad);
        esp_mqtt_client_publish(cliente, tmp, "", 0, 0, 0);
        snprintf(tmp, sizeof tmp, "%s/get", t_record);
        esp_mqtt_client_publish(cliente, tmp, "", 0, 0, 0);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Desconectado del broker");
        mqtt_ok = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        if (e->msg_id == ultimo_msg_id) {
            int64_t ms = (esp_timer_get_time() - t_envio_us) / 1000;
            ESP_LOGI(TAG, "LATENCIA (publish -> PUBACK): %lld ms", ms);
        }
        break;

    case MQTT_EVENT_DATA: {
        int n = e->data_len < 15 ? e->data_len : 15;
        memcpy(tmp, e->data, n);
        tmp[n] = '\0';
        int valor = atoi(tmp);          // "2" o "2.0" -> 2

        if (topic_es(e, t_dificultad)) {
            if (valor >= 1 && valor <= 3) {
                dificultad = valor;
                ESP_LOGI(TAG, "Nueva dificultad desde el dashboard: %d", valor);
            }
        } else if (topic_es(e, t_record)) {
            if (valor > record_nube) record_nube = valor;
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        if (e->error_handle &&
            e->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGE(TAG, "Adafruit IO rechazo la conexion (revisa usuario/llave)");
        } else {
            ESP_LOGW(TAG, "Error de red MQTT (se perdio la conexion, reintentando)");
        }
        break;

    default:
        break;
    }
}

// ================= WiFi =================
static void evento_wifi(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        mqtt_ok = false;
        ESP_LOGW(TAG, "WiFi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi conectado");
        if (!mqtt_iniciado) {           // despues, esp-mqtt reconecta solo
            esp_mqtt_client_start(cliente);
            mqtt_iniciado = true;
        }
    }
}

void net_init(void)
{
    cola_mutex = xSemaphoreCreateMutex();

    snprintf(t_puntaje,    sizeof t_puntaje,    "%s/feeds/puntaje",    AIO_USERNAME);
    snprintf(t_record,     sizeof t_record,     "%s/feeds/record",     AIO_USERNAME);
    snprintf(t_dificultad, sizeof t_dificultad, "%s/feeds/dificultad", AIO_USERNAME);

    // --- WiFi en modo estacion ---
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, evento_wifi, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, evento_wifi, NULL);

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof wc.sta.ssid);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof wc.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    // --- Cliente MQTT (se arranca cuando haya IP) ---
    esp_mqtt_client_config_t mc = {
#if USAR_TLS
        .broker.address.uri = "mqtts://io.adafruit.com:8883",
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
#else
        .broker.address.uri = "mqtt://io.adafruit.com:1883",
#endif
        .credentials.username = AIO_USERNAME,
        .credentials.authentication.password = AIO_KEY,
    };
    cliente = esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(cliente, ESP_EVENT_ANY_ID, evento_mqtt, NULL);

    ESP_ERROR_CHECK(esp_wifi_start());
    xTaskCreate(tarea_envio, "envio_mqtt", 4096, NULL, 4, NULL);
}

void net_publicar_puntaje(int puntaje) { encolar(FEED_PUNTAJE, puntaje); }
void net_publicar_record(int record)   { encolar(FEED_RECORD, record); }
bool net_conectado(void)   { return mqtt_ok; }
int  net_dificultad(void)  { return dificultad; }
int  net_record_nube(void) { return record_nube; }
int  net_pendientes(void)  { return cola_cantidad; }
