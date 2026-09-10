// main/muyu_creds.h —— 敲木鱼 inbox 推送所需凭据。
//
// 这些值是个人隐私(SSID/密码 + inbox userToken),不进公开仓库。
// 本文件已被 .gitignore 忽略,提交时只放这个 .h 模板,真值由构建时 -D 注入:
//
//   idf.py build
//     -DMUYU_INBOX_TOKEN="<your-token>"
//     -DMUYU_WIFI_SSID="<your-ssid>"
//     -DMUYU_WIFI_PASS="<your-pass>"
//
// CI: .github/workflows/build-firmware.yml 会从 repo secrets 读这三个 key
//     并转发为 -D,因此本地/CI 都不需要碰这个文件。
//
// 换 WiFi/换 token:改构建参数重编一次,不需要重写这个文件,也不需要碰代码。
#pragma once

// ---- 必填 ----
#ifndef MUYU_INBOX_TOKEN
#  error "MUYU_INBOX_TOKEN must be defined at build time. See main/muyu_creds.h."
#endif
#ifndef MUYU_WIFI_SSID
#  error "MUYU_WIFI_SSID must be defined at build time. See main/muyu_creds.h."
#endif
#ifndef MUYU_WIFI_PASS
#  error "MUYU_WIFI_PASS must be defined at build time. See main/muyu_creds.h."
#endif

// ---- 端点(可改,但默认就是这个)----
#ifndef MUYU_INBOX_URL
#  define MUYU_INBOX_URL "https://api.gudong.site/inbox/"
#endif
