// main/demo_muyu.c —— 敲木鱼:OK 短按敲一下(木腔"咚"声 + 功德 +1),UP 短按清零。
//
// 设计要点:
//  - 计数逻辑放在 muyu_merit.c,不碰 LVGL/ESP-IDF,由 tests/test_muyu_merit.c 覆盖。
//  - 音频收发会阻塞,故放到独立任务里跑,不占用按键回调与 LVGL 任务。
//  - 视觉沿用 ui_pixel 主题(天空 / 草地 / 墨色描边面板 / 吉祥物),木鱼用 LVGL
//    图元现场绘制,不引入二进制图片素材 —— C3 无 PSRAM,能省则省。
#include "demo.h"
#include "muyu_merit.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"   // bsp_lvgl_lock / bsp_lvgl_unlock
#include "ui_pixel.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

#define SAMPLE_RATE   16000
#define DONG_MS        240
#define CHUNK_SAMPLES  256

#define BODY_W         150
#define BODY_H          88
#define BODY_X          27     // panel 内坐标
#define BODY_Y          62
#define BODY_HIT_DY      4     // 被敲时下沉的像素

#define MUYU_WOOD       0x8B5A2B
#define MUYU_WOOD_HI    0xC0894E

// 不用 math.h 的 M_PI:-std=c11 会定义 __STRICT_ANSI__,部分工具链下 M_PI 不可见。
#define MUYU_PI         3.14159265f

static lv_obj_t   *s_scr, *s_body, *s_total, *s_sub, *s_bat, *s_mascot;
static lv_timer_t *s_timer;
static TaskHandle_t s_task;
static volatile int  s_req;          // 1 = 敲一下
static muyu_merit_t  s_merit;
static int           s_hold;         // >0:木鱼处于下沉状态,由 tick 倒计时还原

// ---------------------------------------------------------------------------
// 计数
// ---------------------------------------------------------------------------
// 设备无 RTC,未对时前 time() 会返回 0 —— 此时降级为"永不跨日",今日 = 累计。
static uint32_t now_day(void)
{
    time_t t = time(NULL);
    struct tm tm_buf;
    if (t <= 0 || !localtime_r(&t, &tm_buf)) return 0;
    return muyu_day_index(tm_buf.tm_year + 1900, tm_buf.tm_mon + 1, tm_buf.tm_mday);
}

static void refresh(void)
{
    if (s_total) lv_label_set_text_fmt(s_total, "%lu", (unsigned long)s_merit.total);
    if (s_sub) lv_label_set_text_fmt(s_sub, "TODAY +%lu   HITS %lu",
                                     (unsigned long)s_merit.today,
                                     (unsigned long)s_merit.hits);
}

// ---------------------------------------------------------------------------
// 音效:合成木鱼"咚"。主体是频率快速下坠的木腔共振,叠一点中频泛音与接触瞬态。
// 相位用累加而非 sin(2*pi*f*t),否则扫频的音高是错的。
// ---------------------------------------------------------------------------
static void play_dong(void)
{
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) return;
    bsp_audio_set_volume(85);

    int16_t buf[CHUNK_SAMPLES];
    const int total = SAMPLE_RATE * DONG_MS / 1000;
    const float dt = 1.0f / (float)SAMPLE_RATE;
    float ph1 = 0.0f, ph2 = 0.0f;
    uint32_t seed = 0x9E3779B9u;
    int done = 0;

    while (done < total) {
        int n = (total - done) < CHUNK_SAMPLES ? (total - done) : CHUNK_SAMPLES;
        for (int i = 0; i < n; i++) {
            const float t = (float)(done + i) * dt;
            const float f1 = 102.0f * expf(-t * 18.0f) + 78.0f;   // 180Hz -> 78Hz
            const float f2 = 215.0f * expf(-t * 22.0f) + 205.0f;  // 420Hz -> 205Hz
            ph1 += 2.0f * MUYU_PI * f1 * dt;
            ph2 += 2.0f * MUYU_PI * f2 * dt;

            const float env = (t < 0.002f) ? (t / 0.002f) : expf(-t * 14.0f);
            float s = env * (0.72f * sinf(ph1) + 0.28f * sinf(ph2));

            if (t < 0.008f) {                                     // 木槌接触的瞬态
                seed = seed * 1103515245u + 12345u;
                float r = (float)((seed >> 16) & 0x7FFF) / 32767.0f * 2.0f - 1.0f;
                s += r * 0.35f * (1.0f - t / 0.008f);
            }

            int32_t v = (int32_t)(s * 11000.0f);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            buf[i] = (int16_t)v;
        }
        bsp_audio_write(buf, (size_t)n * sizeof(int16_t));
        done += n;
    }
}

static void muyu_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (s_req == 1) { s_req = 0; play_dong(); }
        else vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------
static void tick(lv_timer_t *t)
{
    (void)t;
    if (s_hold > 0 && --s_hold == 0) {
        if (s_body) lv_obj_set_y(s_body, BODY_Y);
    }
    int soc = bsp_battery_soc();          // -1 = 不可用,保持上次文本不动
    if (soc >= 0 && s_bat) lv_label_set_text_fmt(s_bat, "%d%%", soc);
}

static lv_obj_t *wood_piece(lv_obj_t *parent, int w, int h, int r, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, r, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void tap(void)
{
    muyu_merit_tap(&s_merit, now_day());
    refresh();
    if (s_body) { lv_obj_set_y(s_body, BODY_Y + BODY_HIT_DY); s_hold = 1; }
    if (s_mascot) ui_pixel_mascot_jump(s_mascot);
    s_req = 1;
}

void demo_muyu_enter(void)
{
    muyu_merit_init(&s_merit, now_day());
    s_hold = 0;
    s_req = 0;

    s_scr = ui_pixel_screen_create("MUYU");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 18, 52, 204, 216, UI_PAPER);

    // 电量:标题牌(x 5..156)与白云(x 188..231)之间的空蓝天区。
    s_bat = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_bat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_bat, lv_color_hex(UI_INK), 0);
    lv_obj_set_pos(s_bat, 158, 14);
    lv_label_set_text(s_bat, "--");

    lv_obj_t *cap = ui_pixel_label(panel, "MERIT", &lv_font_montserrat_14, UI_SKY_DARK);
    lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 6);

    s_total = ui_pixel_label(panel, "0", &lv_font_montserrat_20, UI_INK);
    lv_obj_align(s_total, LV_ALIGN_TOP_MID, 0, 24);

    s_body = wood_piece(panel, BODY_W, BODY_H, 44, MUYU_WOOD);
    lv_obj_set_pos(s_body, BODY_X, BODY_Y);
    lv_obj_set_style_border_color(s_body, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_body, 4, 0);

    lv_obj_t *gloss = wood_piece(s_body, 46, 10, 5, MUYU_WOOD_HI);
    lv_obj_align(gloss, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_t *slit = wood_piece(s_body, 80, 14, 7, UI_INK);
    lv_obj_align(slit, LV_ALIGN_CENTER, 0, 10);

    s_sub = ui_pixel_label(panel, "TODAY +0   HITS 0", &lv_font_montserrat_14, UI_INK);
    lv_obj_align(s_sub, LV_ALIGN_TOP_MID, 0, 162);
    lv_obj_t *hint = ui_pixel_label(panel, "OK: TAP   UP: RESET",
                                    &lv_font_montserrat_14, UI_SKY_DARK);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

    s_mascot = ui_pixel_mascot_create(s_scr, 101, 244);

    refresh();
    s_timer = lv_timer_create(tick, 100, NULL);
    if (!s_task) xTaskCreate(muyu_task, "demo_muyu", 4096, NULL, 4, &s_task);
    lv_screen_load(s_scr);
}

void demo_muyu_exit(void)
{
    s_req = 0;
    s_hold = 0;
    if (s_task) { vTaskDelete(s_task); s_task = NULL; }
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_body = s_total = s_sub = s_bat = s_mascot = NULL;
}

void demo_muyu_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_OK || btn == BSP_BTN_DOWN) tap();
    else if (btn == BSP_BTN_UP) { muyu_merit_reset(&s_merit, now_day()); refresh(); }
}
