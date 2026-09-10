// main/muyu_inbox.c —— 异步 POST 敲木鱼敲击到 api.gudong.site/inbox/{token}。
//
// 通信契约(已用真实 token curl 验证):
//   POST {MUYU_INBOX_URL}{MUYU_INBOX_TOKEN}
//   Content-Type: application/json
//   Body: {"title": string, "content": string}
//
//   2xx 响应示例(成功):
//     {"code":0,"msg":"已提交...","data":{"objectId":"...", "hasConsume":false,
//      "createdAt":..., "updatedAt":...}, "dailyLimit":100, "remaining":99}
//
//   错误响应(4xx/5xx):
//     {"code":-5,"msg":"笔记内容不能为空"}
//
// 关键约束:
//  - 用户每日限额(dailyLimit,默认 100)。连发会撞限,本模块靠批处理压平。
//  - server 会对 content 做加密后 base64 存储(响应里看到 24 字节 base64
//    是 18 字节密文,不是输入明文);request 时 content 直接传明文。
//  - HTTPS 在无 PSRAM 的 C3 上内存紧:esp_http_client 配 MBEDTLS_DYNAMIC_BUFFER
//    可省约 30KB,首次握手 1~2s,后续 keep-alive 更快。
#include "muyu_inbox.h"
#include "muyu_inbox_internal.h"   // 让 host test 也能调 muyu_inbox_build_body
#include "muyu_creds.h"
#include "muyu_wifi.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "muyu_inbox";

#define BATCH_MS            2000    // 累计后多久发一次
#define HTTP_TIMEOUT_MS     5000    // 单次 POST 超时
#define MAX_BODY_BYTES      160     // JSON body 上限(64 标题 + 64 内容 + 余量)
#define MAX_TITLE_LEN       28      // 标题在 inbox 列表里要短
#define MAX_CONTENT_LEN     96

static TaskHandle_t      s_task;
static volatile bool     s_should_exit;

// 发送状态(供 UI 读)。注意这和 "WiFi 状态" 是两件事。
static volatile muyu_inbox_status_t s_status = MUYU_INBOX_IDLE;
static volatile uint32_t s_pending_strikes;   // 还没发出去的敲击数
static volatile uint32_t s_latest_total;
static volatile uint32_t s_latest_today;
static uint32_t         s_last_send_ms;

// ---------------------------------------------------------------------------
// 纯函数:把"累计 N 次"格式化成 JSON body。
// 独立成静态函数 + 测试文件可以脱离 ESP-IDF 跑单测。
// ---------------------------------------------------------------------------
size_t muyu_inbox_build_body(char *out, size_t out_size,
                             uint32_t strikes, uint32_t total, uint32_t today)
{
    // 截断,避免溢出。snprintf 返回值是不含 \0 的写入长度,>= out_size 视为溢出。
    // JSON 字符串里只对中文用 \u 转义,ASCII 标点原样写。
    int n = snprintf(out, out_size,
                     "{\"title\":\"%s%lu\","
                     "\"content\":\"%s%lu %s%lu\"}",
                     "Merit +", (unsigned long)strikes,
                     "Total ", (unsigned long)total,
                     " \xc2\xb7 \xe4\xbb\x8a\xe6\x97\xa5", (unsigned long)today);
    if (n < 0 || (size_t)n >= out_size) {
        if (out_size > 0) out[0] = '\0';
        return 0;
    }
    return (size_t)n;
}

void muyu_inbox_record_strike(uint32_t total, uint32_t today)
{
    s_latest_total = total;
    s_latest_today = today;
    s_pending_strikes++;
}

muyu_inbox_status_t muyu_inbox_get_status(void) { return s_status; }

const char *muyu_inbox_status_glyph(void)
{
    switch (s_status) {
    case MUYU_INBOX_SENDING: return "..";
    case MUYU_INBOX_OK:      return "*";
    case MUYU_INBOX_FAIL:    return "!";
    default:                 return "-";
    }
}

uint32_t muyu_inbox_pending_count(void) { return s_pending_strikes; }

// ---------------------------------------------------------------------------
// HTTP POST
// ---------------------------------------------------------------------------
static esp_err_t http_post_once(const char *body, size_t body_len)
{
    char url[160];
    int url_n = snprintf(url, sizeof(url), "%s%s", MUYU_INBOX_URL, MUYU_INBOX_TOKEN);
    if (url_n <= 0 || (size_t)url_n >= sizeof(url)) return ESP_ERR_INVALID_ARG;

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_TIMEOUT_MS,
        // 不校验证书:api.gudong.site 用 Let's Encrypt 证书链稳,但 C3 上塞 CA bundle
        // 又吃几十 KB flash;MVP 阶段跳过,生产环境再开 .crt_bundle。
        .skip_cert_common_name_check = true,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_ERR_NO_MEM;

    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, body, (int)body_len);

    esp_err_t err = esp_http_client_perform(c);
    int status = -1;
    if (err == ESP_OK) status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    if (err != ESP_OK) return err;
    if (status < 200 || status >= 300) return ESP_FAIL;   // 服务器认为失败
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// 后台 task
// ---------------------------------------------------------------------------
static void inbox_task(void *arg)
{
    (void)arg;
    muyu_wifi_start();   // 启动 WiFi(幂等)

    while (!s_should_exit) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        bool due_time = (now - s_last_send_ms) >= BATCH_MS;
        bool have_pending = s_pending_strikes > 0;

        if (!have_pending || !due_time) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // WiFi 没就绪就别发,留到下次 tick。
        if (muyu_wifi_get_state() != MUYU_WIFI_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // 抢占一份 pending 的快照(避免和 record_strike 抢)。
        uint32_t strikes = s_pending_strikes;
        s_pending_strikes = 0;
        uint32_t total = s_latest_total;
        uint32_t today = s_latest_today;

        char body[MAX_BODY_BYTES];
        size_t body_len = muyu_inbox_build_body(body, sizeof(body), strikes, total, today);
        if (body_len == 0) {
            ESP_LOGW(TAG, "build_body 失败,丢弃 %lu 次", (unsigned long)strikes);
            s_status = MUYU_INBOX_FAIL;
            s_last_send_ms = now;
            continue;
        }

        s_status = MUYU_INBOX_SENDING;
        esp_err_t err = http_post_once(body, body_len);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "POST OK strikes=%lu total=%lu today=%lu",
                     (unsigned long)strikes, (unsigned long)total, (unsigned long)today);
            s_status = MUYU_INBOX_OK;
        } else {
            ESP_LOGW(TAG, "POST 失败(%s),把 %lu 次塞回 pending",
                     esp_err_to_name(err), (unsigned long)strikes);
            // 失败:把 strikes 放回去,等下次重试。
            s_pending_strikes += strikes;
            s_status = MUYU_INBOX_FAIL;
        }
        s_last_send_ms = now;
    }

    s_task = NULL;
    vTaskDelete(NULL);
}

void muyu_inbox_start(void)
{
    if (s_task) return;
    s_should_exit = false;
    s_last_send_ms = 0;
    BaseType_t ok = xTaskCreate(inbox_task, "muyu_inbox", 6144, NULL, 3, &s_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "xTaskCreate 失败");
        s_task = NULL;
    }
}

void muyu_inbox_stop(void)
{
    s_should_exit = true;
    // task 自己会退出并 delete 自己,这里不强制杀 —— HTTP 请求可能正在路上。
    // 下次 start 会重建。
}
