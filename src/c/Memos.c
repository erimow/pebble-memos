#include <pebble.h>

/* ---------- AppMessage keys (must match package.json + JS) ---------- */
enum {
  KEY_MEMO_CHUNK = 0,
  KEY_MEMO_DONE = 1,
  KEY_MEMO_OK = 2,
  KEY_MEMO_FAIL = 3
};

/* ---------- UI ---------- */
static Window *s_main_window;
static TextLayer *s_text_layer;

/* ---------- Dictation ---------- */
static DictationSession *s_dictation;

/* ---------- Timers ---------- */
static AppTimer *s_status_timer;

/* ---------- Forward declarations ---------- */
static void start_dictation(void);
static void show_status(const char *msg, bool success);
static void clear_status(void *data);

/* =================================================================== */
/*                               UI                                    */
/* =================================================================== */

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_text_layer =
      text_layer_create(GRect(5, 20, bounds.size.w - 10, bounds.size.h - 40));

  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_text_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_text_layer, "Press Select\nto Dictate");

  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

/* =================================================================== */
/*                          STATUS HANDLING                             */
/* =================================================================== */

static void clear_status(void *data) {
  text_layer_set_text(s_text_layer, "Press Select\nto Dictate");
  s_status_timer = NULL;
}

static void show_status(const char *msg, bool success) {
  text_layer_set_text(s_text_layer, msg);

  if (success) {
    vibes_short_pulse();
  } else {
    vibes_double_pulse();
  }

  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }

  s_status_timer = app_timer_register(2000, clear_status, NULL);
}

/* =================================================================== */
/*                          DICTATION                                   */
/* =================================================================== */

static void dictation_callback(DictationSession *session,
                               DictationSessionStatus status,
                               char *transcription, void *context) {
  if (status != DictationSessionStatusSuccess) {
    show_status("Dictation Failed", false);
    return;
  }

  text_layer_set_text(s_text_layer, "Sending…");

  /* ---- Send transcription to JS (JS handles chunking) ---- */
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    show_status("Send Failed", false);
    return;
  }

  dict_write_cstring(iter, KEY_MEMO_CHUNK, transcription);
  dict_write_uint8(iter, KEY_MEMO_DONE, 1);

  if (app_message_outbox_send() != APP_MSG_OK) {
    show_status("Send Failed", false);
  }
}

static void start_dictation(void) {
  if (!s_dictation) {
    s_dictation = dictation_session_create(
        0, dictation_callback,
        NULL); // could limit byte size. Fomerly 1024. 0 is unlimited I believe
  }

  dictation_session_start(s_dictation);
}

/* =================================================================== */
/*                         APPMESSAGE                                   */
/* =================================================================== */

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  if (dict_find(iter, KEY_MEMO_OK)) {
    show_status("Sent 👍", true);
  } else if (dict_find(iter, KEY_MEMO_FAIL)) {
    show_status("Send Failed", false);
  }
}

static void inbox_dropped_handler(AppMessageResult reason, void *context) {
  show_status("Phone Error", false);
}

/* =================================================================== */
/*                           INPUT                                      */
/* =================================================================== */

static void select_click_handler(ClickRecognizerRef ref, void *context) {
  start_dictation();
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
}

/* =================================================================== */
/*                          APP LIFECYCLE                                */
/* =================================================================== */

static void init(void) {
  /* ---- AppMessage ---- */
  app_message_open(512, 512);
  app_message_register_inbox_received(inbox_received_handler);
  app_message_register_inbox_dropped(inbox_dropped_handler);

  /* ---- Window ---- */
  s_main_window = window_create();
  window_set_window_handlers(
      s_main_window,
      (WindowHandlers){.load = main_window_load, .unload = main_window_unload});
  window_set_click_config_provider(s_main_window, click_config_provider);
  window_stack_push(s_main_window, true);

  /* ---- Quick launch: start dictation immediately ---- */
  if (launch_reason() == APP_LAUNCH_QUICK_LAUNCH) {
    app_timer_register(200, (AppTimerCallback)start_dictation, NULL);
  }
}

static void deinit(void) {
  if (s_dictation) {
    dictation_session_destroy(s_dictation);
  }

  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }

  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
