#include "app.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static QueueHandle_t s_events = nullptr;
static QueueHandle_t s_toasts = nullptr;
struct ToastMsg { char text[96]; };

void app_events_init() {
  s_events = xQueueCreate(24, sizeof(AppEvent));
  s_toasts = xQueueCreate(6, sizeof(ToastMsg));
}

bool app_post(AppEventType type, void *ptr, const char *s) {
  if (!s_events) return false;
  AppEvent ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = type;
  ev.ptr = ptr;
  if (s) strncpy(ev.s, s, sizeof(ev.s) - 1);
  return xQueueSend(s_events, &ev, pdMS_TO_TICKS(50)) == pdTRUE;
}

bool app_poll(AppEvent &ev) { return s_events && xQueueReceive(s_events, &ev, 0) == pdTRUE; }

void app_toast(const char *fmt, ...) {
  if (!s_toasts) return;
  ToastMsg m;
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(m.text, sizeof(m.text), fmt, ap);
  va_end(ap);
  xQueueSend(s_toasts, &m, 0);
}

bool app_pop_toast(char *buf, size_t len) {
  ToastMsg m;
  if (!s_toasts || xQueueReceive(s_toasts, &m, 0) != pdTRUE) return false;
  strncpy(buf, m.text, len - 1);
  buf[len - 1] = 0;
  return true;
}
