#include <assert.h>
#include <limits.h>
#include "muyu_merit.h"

int main(void)
{
    // day_index:对齐公历已知锚点,含闰日与世纪非闰年。
    assert(muyu_day_index(1970, 1, 1) == 0);
    assert(muyu_day_index(2000, 1, 1) == 10957);
    assert(muyu_day_index(2024, 2, 29) == 19782);
    assert(muyu_day_index(2026, 1, 1) == 20454);
    assert(muyu_day_index(2026, 9, 10) == 20706);
    assert(muyu_day_index(2026, 12, 31) == 20818);

    // 2100 不是闰年:2 月只有 28 天。
    assert(muyu_day_index(2100, 3, 1) == 47541);

    // 连续日期必须递增 1 天(跨月、跨年、跨闰日)。
    assert(muyu_day_index(2024, 2, 28) + 1 == muyu_day_index(2024, 2, 29));
    assert(muyu_day_index(2024, 2, 29) + 1 == muyu_day_index(2024, 3, 1));
    assert(muyu_day_index(2025, 12, 31) + 1 == muyu_day_index(2026, 1, 1));

    muyu_merit_t m;
    muyu_merit_init(&m, muyu_day_index(2026, 9, 10));
    assert(m.total == 0 && m.today == 0 && m.hits == 0);

    // 同一天内敲击:三个计数同时递增,today 不清零。
    assert(muyu_merit_tap(&m, m.day) == false);
    assert(muyu_merit_tap(&m, m.day) == false);
    assert(m.total == 2 && m.today == 2 && m.hits == 2);

    // 跨日:total / hits 继续累加,today 归零并重新计数。
    const uint32_t next = muyu_day_index(2026, 9, 11);
    assert(muyu_merit_tap(&m, next) == true);
    assert(m.total == 3 && m.today == 1 && m.hits == 3);
    assert(m.day == next);

    // 同一天重复 sync 不应重复清零。
    assert(muyu_merit_sync_day(&m, next) == false);
    assert(m.today == 1);

    // 未敲击时单纯推进到第三天:清零 today,但 total / hits 不变。
    const uint32_t third = muyu_day_index(2026, 9, 12);
    assert(muyu_merit_sync_day(&m, third) == true);
    assert(m.total == 3 && m.today == 0 && m.hits == 3);

    // 重置:全清零并把记账日设为当天。
    muyu_merit_reset(&m, third);
    assert(m.total == 0 && m.today == 0 && m.hits == 0 && m.day == third);

    // 溢出保护:停在 UINT32_MAX,不回绕成 0。
    m.total = UINT32_MAX;
    m.today = UINT32_MAX;
    m.hits = UINT32_MAX;
    muyu_merit_tap(&m, m.day);
    assert(m.total == UINT32_MAX && m.today == UINT32_MAX && m.hits == UINT32_MAX);

    return 0;
}
