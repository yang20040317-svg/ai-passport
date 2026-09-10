# Muyu inbox 凭据:从构建环境变量 MUYU_* 注入编译定义。
# 这些值来自 CI secrets(yang20040317-svg/ai-passport -> Settings -> Secrets)或本地 export 的变量,
# 绝不写进源码或仓库。未定义时 muyu_creds.h 的 #error 会在编译期明确报错。
foreach(_v MUYU_INBOX_TOKEN MUYU_WIFI_SSID MUYU_WIFI_PASS)
  if(DEFINED ENV{${_v}} AND NOT "$ENV{${_v}}" STREQUAL "")
    add_compile_definitions(${_v}="$ENV{${_v}}")
  endif()
endforeach()

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
    REQUIRES bsp bt esp_event esp_http_client esp_hw_support esp_netif esp_timer esp_wifi nvs_flash
)
