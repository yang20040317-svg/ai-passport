// main/muyu_merit.c —— 敲木鱼计数逻辑实现(纯 C,无 ESP-IDF / LVGL 依赖)。
#include "muyu_merit.h"

// Howard Hinnant 的 days_from_civil 算法:公历日期 -> 距 1970-01-01 的天数。
// 支持 1970 年以前的日期(era 向下取整处理负数年)。
uint32_t muyu_day_index(int year, int month, int day)
{
    int y = year - (month <= 2 ? 1 : 0);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                       // [0, 399]
    const unsigned mp = (unsigned)(month + (month > 2 ? -3 : 9));         // [0, 11]
    const unsigned doy = (153u * mp + 2u) / 5u + (unsigned)(day - 1);     // [0, 365]
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;        // [0, 146096]
    return (uint32_t)(era * 146097 + (int)doe - 719468);
}

void muyu_merit_init(muyu_merit_t *m, uint32_t day)
{
    m->total = 0;
    m->today = 0;
    m->hits = 0;
    m->day = day;
}

bool muyu_merit_sync_day(muyu_merit_t *m, uint32_t day)
{
    if (m->day == day) return false;
    m->day = day;
    m->today = 0;
    return true;
}

bool muyu_merit_tap(muyu_merit_t *m, uint32_t day)
{
    const bool rolled = muyu_merit_sync_day(m, day);
    // 溢出保护:到达上限后停在最大值,不回绕成 0 让人误以为清零失败。
    if (m->total < UINT32_MAX) m->total++;
    if (m->today < UINT32_MAX) m->today++;
    if (m->hits < UINT32_MAX) m->hits++;
    return rolled;
}

void muyu_merit_reset(muyu_merit_t *m, uint32_t day)
{
    muyu_merit_init(m, day);
}
