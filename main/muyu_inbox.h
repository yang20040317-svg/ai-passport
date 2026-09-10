// main/muyu_inbox.h —— 把敲木鱼的敲击推送到用户私人 inbox (api.gudong.site)。
//
// 设计:
//  - 单实例,跨 demo 页面持有。
//  - 批处理:每次敲击只入本地 pending 计数,后台 task 每 ~2 秒把累计打成一条
//    POST 上去(避免连敲 30 下就发 30 条撞 100/日 限)。
//  - 与 WiFi 解耦:WiFi 不可用时敲击照常累计,等恢复后批量补发。
//  - 失败/限流/超时都不丢敲击计数(UI 计数照常累加,只是 inbox 少一条)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MUYU_INBOX_IDLE = 0,
    MUYU_INBOX_SENDING,    // 正在 POST
    MUYU_INBOX_OK,         // 最近一次成功
    MUYU_INBOX_FAIL,       // 最近一次失败(网络 / 限流 / 服务器错误)
} muyu_inbox_status_t;

// 启动后台 task(系统生命周期内只调一次)。失败/重试由 task 自己处理。
// 会触发一次 WiFi 启动(muyu_wifi_start)。不需要再单独调 muyu_wifi_start。
void muyu_inbox_start(void);

// 关 task,不再发请求。下次 start 重建。
void muyu_inbox_stop(void);

// 由 demo 在每次敲击时调;total/today 是最新累计值,会随下一条 POST 一起寄出。
// 任何状态下都可以调 —— pending 永远会保留直到下一次成功发出。
void muyu_inbox_record_strike(uint32_t total, uint32_t today);

// 供 UI 轮询。
muyu_inbox_status_t muyu_inbox_get_status(void);

// 给 UI 看的短字符(同 muyu_wifi_state_glyph 的设计),静态串。
const char *muyu_inbox_status_glyph(void);

// 给调试日志用的累计待发数。
uint32_t muyu_inbox_pending_count(void);
