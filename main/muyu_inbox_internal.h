// main/muyu_inbox_internal.h —— 暴露给 host test 的实现细节。
// 不属于公开 API,只用于 main/ 与 tests/ 之间共享代码。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 把"累计 N 次"格式化成 JSON body,返回写入长度(不含 \0)。
// 失败时返回 0,out[0] = '\0'。
// 参数同 muyu_inbox.c 内的 build_body;提出来便于 tests/test_muyu_inbox.c 复用。
size_t muyu_inbox_build_body(char *out, size_t out_size,
                             uint32_t strikes, uint32_t total, uint32_t today);
