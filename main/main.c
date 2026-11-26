#include "nvs_flash.h"
#include "io.h"
#include "display.h"
#include "net.h"
#include "game.h"

void app_main(void)
{
    // El WiFi necesita la NVS inicializada
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    io_init();
    display_init();
    net_init();     // no bloquea: el juego arranca aunque no haya internet
    game_run();     // nunca regresa
}
