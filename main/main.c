#include <stdio.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "epd_display/epd_display.h"
#include "epd_display/epd_graphics.h"
#include  "esl/esl_ui.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "mqtt_client.h"

#include "esp_ping.h"
#include "ping/ping_sock.h"
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "assets/hello_images.h"
#include "assets/price_tag_image.h"

#define WIFI_CONNECTED_BIT      BIT0
#define MAX_INFLIGHT_MSGS       4
#define MAX_MQTT_PAYLOAD_LEN    4096

#define STR(x) #x
#define XSTR(x) STR(x)

const char* wifi_ssid = XSTR(WIFI_SSID);
const char* wifi_pass = XSTR(WIFI_PASSWORD);
const char* broker_addr = MQTT_BROKER;

static const char *TAG_WIFI = "WIFI";
static const char *TAG_MAIN = "MAIN";
static const char *TAG_PING = "PING";

static EventGroupHandle_t wifi_event_group;

uint8_t fb[EPD_WIDTH * EPD_HEIGHT / 8];

uint8_t mac[6];
char mac_str[13];
char topic_price[64];
char topic_description[64];

typedef struct {
    bool in_use;
    int msg_id;
    int total_len;
    int received_len;
    char topic[64];
    uint8_t data[MAX_MQTT_PAYLOAD_LEN];
} inflight_msg_t;

static inflight_msg_t inflight_msgs[MAX_INFLIGHT_MSGS] = {0};

static inflight_msg_t *create_inflight_msg(int msg_id, const char *topic, int total_len)
{
    for (int i = 0; i < MAX_INFLIGHT_MSGS; i++) {
        if (!inflight_msgs[i].in_use) {
            inflight_msgs[i].in_use = true;
            inflight_msgs[i].msg_id = msg_id;
            inflight_msgs[i].total_len = total_len;
            inflight_msgs[i].received_len = 0;
            memset(inflight_msgs[i].topic, 0, sizeof(inflight_msgs[i].topic));
            strncpy(inflight_msgs[i].topic, topic, sizeof(inflight_msgs[i].topic) - 1);
            return &inflight_msgs[i];
        }
    }
    return NULL;
}

static inflight_msg_t *find_inflight_msg(int msg_id)
{
    for (int i = 0; i < MAX_INFLIGHT_MSGS; i++) {
        if (inflight_msgs[i].in_use && inflight_msgs[i].msg_id == msg_id) {
            return &inflight_msgs[i];
        }
    }
    return NULL;
}

static void finish_inflight_msg(inflight_msg_t *msg)
{
    msg->in_use = false;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG_WIFI, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG_WIFI, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG_WIFI, "Disconnected, retrying...");
        esp_wifi_connect();
    }
}

void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_LOGI(TAG_WIFI, "Initializing Wi-Fi...");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    strncpy((char *)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, wifi_pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_WIFI, "Waiting for IP...");

    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG_WIFI, "Connected to Wi-Fi");
}

bool price_received = false;
bool description_received = false;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI("MQTT", "Connected to broker");
            esp_mqtt_client_subscribe(event->client, topic_price, 0);
            esp_mqtt_client_subscribe(event->client, topic_description, 0);
            break;

        case MQTT_EVENT_DATA: {
            // If this chunk has a topic (offset==0), create a new inflight slot
            inflight_msg_t *msg = NULL;
        
            if (event->current_data_offset == 0 && event->topic_len > 0) {
                // Extract the topic into a null-terminated string
                char topic_str[64];
                snprintf(topic_str, sizeof(topic_str), "%.*s", event->topic_len, event->topic);
        
                // Create a new inflight message for this msg_id
                msg = create_inflight_msg(event->msg_id, topic_str, event->total_data_len);
                if (msg) {
                    ESP_LOGI("MQTT", "📥 Start receiving %s (%d bytes) [msg_id=%d]",
                                msg->topic, msg->total_len, msg->msg_id);
                } else {
                    ESP_LOGW("MQTT", "No free inflight slots for msg_id=%d", event->msg_id);
                    break;
                }
            } else {
                // It's a subsequent chunk. Find existing message by msg_id
                msg = find_inflight_msg(event->msg_id);
                if (!msg) {
                    ESP_LOGW("MQTT", "Unknown msg_id=%d for chunk offset=%d", event->msg_id, event->current_data_offset);
                    break;
                }
            }
        
            // Append this chunk's data
            if (msg->received_len + event->data_len <= msg->total_len && msg->received_len + event->data_len < MAX_MQTT_PAYLOAD_LEN) {
                memcpy(&msg->data[event->current_data_offset], event->data, event->data_len);
                msg->received_len += event->data_len;
            } else {
                ESP_LOGW("MQTT", "Overflow in msg_id=%d, ignoring chunk!", event->msg_id);
                break;
            }
        
            // Check if this was the final chunk
            if (event->current_data_offset + event->data_len == msg->total_len) {
                ESP_LOGI("MQTT", "✅ Received full %s (%d bytes) [msg_id=%d]", msg->topic, msg->received_len, msg->msg_id);
        
                // Decide where to draw
                // ...
                if (strstr(msg->topic, "price")) {
                    epd_draw_bin_image(msg->data, PRICE_X, PRICE_Y, PRICE_W, PRICE_H);
                    price_received = true;
                } else if (strstr(msg->topic, "description")) {
                    epd_draw_bin_image(msg->data, DESC_X, DESC_Y, DESC_W, DESC_H);
                    description_received = true;
                }

                // Mark inflight slot free
                finish_inflight_msg(msg);

                if(price_received && description_received) {
                    epd_part_init();
                    epd_display(fb);
                    epd_update();
                    epd_deep_sleep();
                    price_received = false;
                    description_received = false;
                }
            }
        
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW("MQTT", "Disconnected");
            break;

        default:
            break;
    }
}

static void ping_on_success(esp_ping_handle_t hdl, void *args)
{
    ip_addr_t target_addr;
    uint32_t elapsed_time, recv_len, ttl;

    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));

    ESP_LOGI(TAG_PING, "%ld bytes from %s: time=%ldms ttl=%ld",
             recv_len, ipaddr_ntoa(&target_addr), elapsed_time, ttl);
}

static void ping_on_end(esp_ping_handle_t hdl, void *args)
{
    ESP_LOGI(TAG_PING, "Ping finished");
    esp_ping_delete_session(hdl);
}

void ping_test(const char *ip)
{
    ip_addr_t target_addr;
    inet_pton(AF_INET, ip, &target_addr.u_addr.ip4);
    target_addr.type = IPADDR_TYPE_V4;

    esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
    config.target_addr = target_addr;
    config.count = 4;
    config.interval_ms = 1000;
    config.timeout_ms = 1000;
    config.task_stack_size = 4096;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = ping_on_success,
        .on_ping_end = ping_on_end,
    };

    esp_ping_handle_t ping;
    ESP_ERROR_CHECK(esp_ping_new_session(&config, &cbs, &ping));
    ESP_ERROR_CHECK(esp_ping_start(ping));
}

void app_main(void) {
    ESP_LOGI(TAG_MAIN, "Initializing ESL E-Paper Display...");

    ESP_LOGI("WIFI", "SSID: %s", wifi_ssid);
    ESP_LOGI("WIFI", "PASS: %s", wifi_pass);
    ESP_LOGI("WIFI", "BROKER: %s", broker_addr);

    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init_sta();

    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI("NETIF", "Got IP: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI("NETIF", "Gateway: " IPSTR, IP2STR(&ip_info.gw));
        ESP_LOGI("NETIF", "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
    }

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    snprintf(mac_str, sizeof(mac_str),
         "%02x%02x%02x%02x%02x%02x",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    snprintf(topic_price, sizeof(topic_price), "esl/%s/price", mac_str);
    snprintf(topic_description, sizeof(topic_description), "esl/%s/description", mac_str);

    // ping_test("test.mosquitto.org");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_addr,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    epd_set_buffer(fb, EPD_WIDTH, EPD_HEIGHT, EPD_ROTATE_0, WHITE);
    epd_clear_buffer(WHITE);

    epd_spi_init();

    epd_gpio_init();
    epd_enable_power();

    epd_fast_init();
    epd_clear();
    epd_update();
    epd_display(fb);
    epd_update();
    epd_draw_image(0, 0, 416, 240, price_tag_empty_rotated, WHITE);
    epd_draw_string(70, 213, mac_str, 16, BLACK);
    epd_display(fb);
    epd_update();
    epd_deep_sleep();

    ESP_LOGI(TAG_MAIN, "UC8253 EPD Initialized and Cleared.");
}