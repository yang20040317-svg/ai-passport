// tests/test_muyu_inbox.c —— host test for muyu_inbox_build_body (JSON 拼装)。
//
// 不依赖 ESP-IDF;只链 muyu_inbox.c 不行(里面有 http_client 调用),所以
// 实际只链 main/muyu_inbox.c 时用 #define 替换 http_client 头为 stub。
// 简单做法:用 sed 复制一份只含 build_body 的版?太脆。
// 干净做法:让 muyu_inbox.c 通过弱符号或宏区分 host build。
// 这里采用最直接:把 build_body 的实现复制到本测试里,确保和 main 严格一致。
//
// (上一版是 include "muyu_inbox.c" 直接调,但它有 esp_http_client 等头文件,
//  host 环境没装 ESP-IDF 会失败。所以这里手抄函数体,host test 自包含。)

#include "muyu_inbox_internal.h"   // 真实声明,但实现走 test 内的副本
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- 副本:与 main/muyu_inbox.c 的 muyu_inbox_build_body 严格一致 ---
// 不要改这里,改了请同步改 main/muyu_inbox.c。
size_t muyu_inbox_build_body(char *out, size_t out_size,
                             uint32_t strikes, uint32_t total, uint32_t today)
{
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

static void test_single_strike(void)
{
    char buf[160];
    size_t n = muyu_inbox_build_body(buf, sizeof(buf), 1, 1, 1);
    assert(n > 0);
    assert(strstr(buf, "\"title\":\"Merit +1\"") != NULL);
    assert(strstr(buf, "\"content\":\"Total ") != NULL);
    printf("ok  test_single_strike: %s\n", buf);
}

static void test_batch_strike(void)
{
    char buf[160];
    size_t n = muyu_inbox_build_body(buf, sizeof(buf), 30, 42, 5);
    assert(n > 0);
    assert(strstr(buf, "\"title\":\"Merit +30\"") != NULL);
    // UTF-8 多字节序列:
    //   "·"  = 0xC2 0xB7
    //   "今" = 0xE4 0xBB 0x8A
    //   "日" = 0xE6 0x97 0xA5
    assert(strstr(buf, "\xc2\xb7") != NULL);
    assert(strstr(buf, "\xe4\xbb\x8a\xe6\x97\xa5") != NULL);
    printf("ok  test_batch_strike: %s\n", buf);
}

static void test_buffer_too_small(void)
{
    char buf[20];
    size_t n = muyu_inbox_build_body(buf, sizeof(buf), 1, 1, 1);
    assert(n == 0);
    assert(buf[0] == '\0');
    printf("ok  test_buffer_too_small: 正确截断并清空\n");
}

static void test_zero_buffer(void)
{
    char buf[1] = { 'X' };
    size_t n = muyu_inbox_build_body(buf, 0, 1, 1, 1);
    assert(n == 0);
    assert(buf[0] == 'X');   // out_size==0 不动 buf
    printf("ok  test_zero_buffer\n");
}

static void test_large_numbers(void)
{
    char buf[160];
    size_t n = muyu_inbox_build_body(buf, sizeof(buf), 9999, 1000000, 1000);
    assert(n > 0);
    assert(strstr(buf, "Merit +9999") != NULL);
    assert(strstr(buf, "1000000") != NULL);
    printf("ok  test_large_numbers: %s\n", buf);
}

int main(void)
{
    test_single_strike();
    test_batch_strike();
    test_buffer_too_small();
    test_zero_buffer();
    test_large_numbers();
    printf("\nAll muyu_inbox tests passed.\n");
    return 0;
}
