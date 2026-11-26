#include <string.h>
#include <stdbool.h>
#include "display.h"
#include "pins.h"
#include "font5x7.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define TAG "display"
#define ANCHO 128
#define ALTO  64

static i2c_master_dev_handle_t oled;
static bool disponible = false;

// buffer[0] es el byte de control 0x40 (datos); el resto son los pixeles
static uint8_t buffer[1 + ANCHO * ALTO / 8];
static uint8_t *const pixeles = &buffer[1];

static void comandos(const uint8_t *cmd, size_t n)
{
    uint8_t tx[32];
    tx[0] = 0x00;                       // byte de control: comandos
    memcpy(&tx[1], cmd, n);
    i2c_master_transmit(oled, tx, n + 1, 100);
}

void display_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_OLED_SDA,
        .scl_io_num = PIN_OLED_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    // Las SSD1306 usan 0x3C o 0x3D segun el modulo
    uint8_t direccion = 0;
    const uint8_t candidatas[] = {OLED_ADDR, 0x3D};
    for (int i = 0; i < 2 && direccion == 0; i++) {
        if (i2c_master_probe(bus, candidatas[i], 100) == ESP_OK) direccion = candidatas[i];
    }

    if (direccion == 0) {
        ESP_LOGE(TAG, "No se encontro la OLED (0x3C/0x3D). Revisa SDA=GPIO%d, SCL=GPIO%d, VCC y GND",
                 PIN_OLED_SDA, PIN_OLED_SCL);
        // Escaneo del bus para diagnostico
        int encontrados = 0;
        for (uint8_t a = 0x08; a < 0x78; a++) {
            if (i2c_master_probe(bus, a, 20) == ESP_OK) {
                ESP_LOGW(TAG, "Dispositivo I2C encontrado en 0x%02X", a);
                encontrados++;
            }
        }
        if (encontrados == 0)
            ESP_LOGE(TAG, "Escaneo I2C: ningun dispositivo responde (cables o alimentacion)");
        return;                         // el juego sigue sin pantalla
    }
    ESP_LOGI(TAG, "OLED encontrada en 0x%02X", direccion);

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = direccion,
        .scl_speed_hz = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &oled));

    static const uint8_t init[] = {
        0xAE,             // apagar
        0xD5, 0x80,       // reloj
        0xA8, 0x3F,       // multiplex 64
        0xD3, 0x00,       // offset
        0x40,             // linea inicial 0
        0x8D, 0x14,       // charge pump on
        0x20, 0x00,       // direccionamiento horizontal
        0xA1, 0xC8,       // orientacion
        0xDA, 0x12,       // pines COM
        0x81, 0xCF,       // contraste
        0xD9, 0xF1,       // pre-carga
        0xDB, 0x40,       // VCOMH
        0xA4, 0xA6,       // mostrar RAM, modo normal
        0xAF,             // encender
    };
    comandos(init, sizeof init);
    disponible = true;

    display_limpiar();
    display_mostrar();
}

void display_limpiar(void)
{
    memset(pixeles, 0, ANCHO * ALTO / 8);
}

static void pixel(int x, int y)
{
    if (x < 0 || x >= ANCHO || y < 0 || y >= ALTO) return;
    pixeles[x + (y / 8) * ANCHO] |= (1 << (y % 8));
}

static void caracter(int x, int y, char c, int escala)
{
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glifo = FONT5X7[c - 32];
    for (int col = 0; col < 5; col++)
        for (int fil = 0; fil < 7; fil++)
            if (glifo[col] & (1 << fil))
                for (int dx = 0; dx < escala; dx++)
                    for (int dy = 0; dy < escala; dy++)
                        pixel(x + col * escala + dx, y + fil * escala + dy);
}

void display_texto(int x, int fila, const char *texto, int escala)
{
    int y = fila * 8;
    while (*texto) {
        caracter(x, y, *texto++, escala);
        x += 6 * escala;
    }
}

void display_texto_centrado(int fila, const char *texto, int escala)
{
    int ancho = (int)strlen(texto) * 6 * escala;
    int x = (ANCHO - ancho) / 2;
    display_texto(x < 0 ? 0 : x, fila, texto, escala);
}

void display_mostrar(void)
{
    if (!disponible) return;
    static const uint8_t ventana[] = {0x21, 0, ANCHO - 1, 0x22, 0, 7};
    comandos(ventana, sizeof ventana);
    buffer[0] = 0x40;
    i2c_master_transmit(oled, buffer, sizeof buffer, 100);
}
