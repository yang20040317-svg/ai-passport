// main/muyu_wifi.h —— 木鱼 demo 用的 Wi-Fi STA 连接管理。
//
// 设计:
//  - 单实例,跨 demo 生命周期持有(不绑定某个 LVGL 页面)。
//  - 用 esp_event 订阅 WIFI_EVENT_STA_DISCONNECTED 自动重连,不写显式循环。
//  - 状态只读 getter 供 UI 读,UI 不直接订阅事件,降低耦合。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MUYU_WIFI_OFF = 0,   // 未启动
    MUYU_WIFI_CONNECTING,
    MUYU_WIFI_CONNECTED,  // 已拿到 IP
    MUYU_WIFI_FAILED,    // 启动后多次失败(SSID 错/密码错/AP 不在)
} muyu_wifi_state_t;

// 初始化 + 启动连接。会调 demo_radio_*_prepare;若已连接过的 netif/event loop 存在则复用。
// 失败不擦 NVS,不卡住其他 demo。
void muyu_wifi_start(void);

// 主动断开 + 释放资源。下次 start 会重建。
void muyu_wifi_stop(void);

// 供 UI 周期性 poll。
muyu_wifi_state_t muyu_wifi_get_state(void);

// 给 UI 看的短状态文本:"·" / "· ·" / "x" 等。返回的指针指向静态串。
const char *muyu_wifi_state_glyph(void);
