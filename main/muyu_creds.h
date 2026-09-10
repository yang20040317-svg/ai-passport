// main/muyu_creds.h —— 敲木鱼 inbox 推送所需凭据。
//
// 这些值是个人隐私(SSID/密码 + inbox userToken),绝不进公开仓库。
// 本文件只是模板:真值在构建时提供,两种方式二选一 ——
//
//   1) 【推荐】构建前生成 main/muyu_creds_local.h(CI 与本地都走这条):
//
//        #define MUYU_INBOX_TOKEN "your-token"
//        #define MUYU_WIFI_SSID   "your-ssid"
//        #define MUYU_WIFI_PASS   "your-pass"
//
//      tools/validate.sh 会在读到 MUYU_* 环境变量时自动生成它,构建结束即删除;
//      该文件已被 .gitignore 忽略。CI 里三个值来自 repo secrets,不落进源码。
//
//   2) 编译期 -D 注入。注意这些宏在代码里是当字符串字面量用的
//      (strncpy(..., MUYU_WIFI_SSID, ...)),所以 -D 的值必须自带引号,
//      shell 传递极易出错,除非必要否则用方式 1。
//
// 换 WiFi/换 token:改构建参数或重新生成 local header 重编一次即可,不需要改代码。
#pragma once

// 若三个宏没被 -D 全部提供,则尝试包含本地生成的头文件。
#if !defined(MUYU_INBOX_TOKEN) || !defined(MUYU_WIFI_SSID) || !defined(MUYU_WIFI_PASS)
#  if defined(__has_include)
#    if __has_include("muyu_creds_local.h")
#      include "muyu_creds_local.h"
#    endif
#  endif
#endif

// ---- 必填 ----
#ifndef MUYU_INBOX_TOKEN
#  error "MUYU_INBOX_TOKEN must be provided at build time. See main/muyu_creds.h."
#endif
#ifndef MUYU_WIFI_SSID
#  error "MUYU_WIFI_SSID must be provided at build time. See main/muyu_creds.h."
#endif
#ifndef MUYU_WIFI_PASS
#  error "MUYU_WIFI_PASS must be provided at build time. See main/muyu_creds.h."
#endif

// ---- 端点(可改,但默认就是这个)----
#ifndef MUYU_INBOX_URL
#  define MUYU_INBOX_URL "https://api.gudong.site/inbox/"
#endif
