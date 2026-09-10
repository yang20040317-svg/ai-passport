# Muyu inbox 凭据:从构建环境变量 MUYU_* 注入 -D 编译定义。
# 走 CFLAGS/CXXFLAGS 而非 add_compile_definitions,是因为 ESP-IDF 在依赖扫描阶段就 include
# 组件 CMakeLists,那时 project() 尚未调用,add_compile_definitions 等命令还不可用。
# 这些值来自 CI secrets(yang20040317-svg/ai-passport -> Settings -> Secrets)或本地 export 的变量,
# 绝不写进源码或仓库。未设置时 muyu_creds.h 的 #error 会在编译期明确报错。
set(_muyu_defs "")
if(DEFINED ENV{MUYU_INBOX_TOKEN} AND NOT "$ENV{MUYU_INBOX_TOKEN}" STREQUAL "")
  string(APPEND _muyu_defs " -DMUYU_INBOX_TOKEN=$ENV{MUYU_INBOX_TOKEN}")
endif()
if(DEFINED ENV{MUYU_WIFI_SSID} AND NOT "$ENV{MUYU_WIFI_SSID}" STREQUAL "")
  string(APPEND _muyu_defs " -DMUYU_WIFI_SSID=$ENV{MUYU_WIFI_SSID}")
endif()
if(DEFINED ENV{MUYU_WIFI_PASS} AND NOT "$ENV{MUYU_WIFI_PASS}" STREQUAL "")
  string(APPEND _muyu_defs " -DMUYU_WIFI_PASS=$ENV{MUYU_WIFI_PASS}")
endif()

idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "demo_display.c"
         "demo_button.c"
         "demo_audio.c"
         "demo_battery.c"
         "demo_radio.c"
         "demo_wifi.c"
         "demo_ble.c"
         "demo_low_power.c"
         "muyu_merit.c"
         "muyu_wifi.c"
         "muyu_inbox.c"
         "demo_muyu.c"
    INCLUDE_DIRS "."
    CFLAGS "${_muyu_defs}"
    CXXFLAGS "${_muyu_defs}"
    REQUIRES bsp bt esp_event esp_http_client esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
