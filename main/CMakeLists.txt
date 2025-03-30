set(SRC_FILES
    "main.c"
    "epd_display/epd_display.c"
    "epd_display/epd_graphics.c"
    "esl/esl_ui.c"
)

set(REQ_COMPONENTS
    spiffs
    esp_driver_sdspi
    fatfs
    json
    nvs_flash
    esp_wifi
    mqtt
    esp_netif
)

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES ${REQ_COMPONENTS}
)

set(MQTT_BROKER_ESCAPED "\"${ENV_MQTT_BROKER}\"")

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    WIFI_SSID=${ENV_WIFI_SSID}
    WIFI_PASSWORD=${ENV_WIFI_PASSWORD}
    MQTT_BROKER=${MQTT_BROKER_ESCAPED}
)
