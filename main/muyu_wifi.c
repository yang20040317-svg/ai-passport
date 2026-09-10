// main/muyu_wifi.c —— 木鱼 demo 的 Wi-Fi STA 连接管理。
//
// 实现要点:
//  - 复用 demo_radio_nvs_prepare / demo_radio_network_prepare,避免和 demo_wifi 重复建 netif。
//  - 失败 N 次后转 FAILED,但不重置:调用方可以靠 muyu_wifi_start() 重启,或干脆让用户重启。
//  - 不订阅 IP_EVENT_STA_LOST_IP;ESP-IDF 的 GOT_IP 重新触发可以保证"连上就有 IP"。
#include "muyu_wifi.h"
#include "muyu_creds.h"
#include "demo_radio.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static const char *TAG = "muyu_wifi";

#define MUYU_WIFI_MAX_FAIL   6       // 6 次连不上认为放弃(给启动留时间)

static volatile muyu_wifi_state_t s_state = MUYU_WIFI_OFF;
static esp_netif_t *s_sta_netif;
static esp_event_handler_instance_t s_any_id_handler;   // 监听 WIFI_EVENT_ANY
static esp_event_handler_instance_t s_got_ip_handler;   // 监听 IP_EVENT_STA_GOT_IP
static int s_fail_count;
static bool s_wifi_initialized;
static bool s_wifi_started;

static void set_state(muyu_wifi_state_t st)
{
    if (s_state != st) {
        s_state = st;
        ESP_LOGI(TAG, "state -> %d", (int)st);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        s_fail_count++;
        if (s_fail_count >= MUYU_WIFI_MAX_FAIL) {
            set_state(MUYU_WIFI_FAILED);
            return;
        }
        set_state(MUYU_WIFI_CONNECTING);
        // 让 ESP-IDF 内部机制自动重连(默认开启);不需要再 esp_wifi_connect。
    } else if (id == WIFI_EVENT_STA_CONNECTED) {
        s_fail_count = 0;
        // IP 由 on_got_ip 转 CONNECTED,这里只清失败计数。
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    s_fail_count = 0;
    set_state(MUYU_WIFI_CONNECTED);
}

void muyu_wifi_start(void)
{
    if (s_wifi_started) return;

    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    err = demo_radio_network_prepare();
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }

    if (!s_sta_netif) s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    s_wifi_initialized = true;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              on_wifi_event, NULL, &s_any_id_handler);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              on_got_ip, NULL, &s_got_ip_handler);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }

    wifi_config_t wcfg = { 0 };
    strncpy((char *)wcfg.sta.ssid, MUYU_WIFI_SSID, sizeof(wcfg.sta.ssid) - 1);
    strncpy((char *)wcfg.sta.password, MUYU_WIFI_PASS, sizeof(wcfg.sta.password) - 1);
    wcfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;   // 旧 WPA/WEP 拒掉
    wcfg.sta.pmf_cfg.capable = true;
    wcfg.sta.pmf_cfg.required = false;

    err = esp_wifi_set_config(WIFI_IF_STA, &wcfg);
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }

    err = esp_wifi_start();
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    s_wifi_started = true;

    err = esp_wifi_connect();
    if (err != ESP_OK) { set_state(MUYU_WIFI_FAILED); return; }
    s_fail_count = 0;
    set_state(MUYU_WIFI_CONNECTING);
    ESP_LOGI(TAG, "connecting to SSID='%s' ...", MUYU_WIFI_SSID);
}

void muyu_wifi_stop(void)
{
    if (s_wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
    }
    if (s_any_id_handler) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_any_id_handler);
        s_any_id_handler = NULL;
    }
    if (s_got_ip_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_got_ip_handler);
        s_got_ip_handler = NULL;
    }
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
    set_state(MUYU_WIFI_OFF);
}

muyu_wifi_state_t muyu_wifi_get_state(void)
{
    return s_state;
}

const char *muyu_wifi_state_glyph(void)
{
    switch (s_state) {
    case MUYU_WIFI_CONNECTED:  return "*";  // 一个小点表示"在线"
    case MUYU_WIFI_CONNECTING: return "..";
    case MUYU_WIFI_FAILED:     return "x";
    default:                   return "-";
    }
}
