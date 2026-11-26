if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/config.h)
    message(FATAL_ERROR
        "Falta main/config.h. Copia main/config.example.h como main/config.h "
        "y escribe ahi tu WiFi y tu llave de Adafruit IO.")
endif()

idf_component_register(
    SRCS "main.c" "game.c" "io.c" "display.c" "net.c"
    INCLUDE_DIRS "."
)
