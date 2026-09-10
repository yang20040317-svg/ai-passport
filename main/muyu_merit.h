// main/muyu_merit.h —— 敲木鱼的计数逻辑。
// 与 ESP-IDF / LVGL 完全解耦,可被 tests/test_muyu_merit.c 直接编译成主机测试。
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 把年月日折算成"距 1970-01-01 的天数",页面用它判断是否跨日。
// 纯计算,不依赖任何时间库,便于主机测试。
uint32_t muyu_day_index(int year, int month, int day);

typedef struct {
    uint32_t total;   // 累计功德
    uint32_t today;   // 今日功德
    uint32_t hits;    // 累计敲击次数
    uint32_t day;     // 最近一次记账的天序号
} muyu_merit_t;

void muyu_merit_init(muyu_merit_t *m, uint32_t day);

// 记录一次敲击。若 day 与上次不同,先把 today 清零再累加(即跨日自动归零)。
// 返回 true 表示本次发生了跨日清零。
bool muyu_merit_tap(muyu_merit_t *m, uint32_t day);

// 只推进跨日、不敲击。返回 true 表示 today 被清零。
bool muyu_merit_sync_day(muyu_merit_t *m, uint32_t day);

// 全部清零并把记账日设为 day。
void muyu_merit_reset(muyu_merit_t *m, uint32_t day);
