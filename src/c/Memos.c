#include <pebble.h>
#include <stdint.h>
#include <string.h>

/* ---------- AppMessage keys ---------- */
enum {
  KEY_MEMO_CHUNK = 0,
  KEY_MEMO_DONE = 1,
  KEY_MEMO_OK = 2,
  KEY_MEMO_FAIL = 3
};

/* ---------- UI ---------- */
static Window *s_main_window;
static TextLayer *s_text_layer;
static MenuLayer *s_menu_layer;
static ActionBarLayer *s_actionbar_layer;
static ScrollLayer *s_scroll_layer;

/* ---------- Images ---------- */
static GBitmap *send_image;
static GBitmap *mic_image;

/* ---------- Dictation ---------- */
static DictationSession *s_dictation;

/* ---------- Timers ---------- */
static AppTimer *s_status_timer;

/* ---------- Values ----------- */
static uint16_t s_num_menu_rows = 0;
static char s_message[1024] = "";

/* ---------- Forward declarations ---------- */
static void start_dictation(void);
static void show_status(const char *msg, bool success);
static void clear_status(void *data);
static void click_config_provider(void *context);
static void scroll_select_callback(ClickRecognizerRef ref, void *context);
static void action_bar_click_config_provider(void *context);

/* =================================================================== */
/*                           CALLBACKS                                 */
/* =================================================================== */

static uint16_t menu_get_num_rows_callback(MenuLayer *menuLayer,
                                           uint16_t section_index,
                                           void *context) {
  return s_num_menu_rows;
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer,
                                   MenuIndex *cell_index, void *data) {
  if (cell_index->row == 0) {
    if (menu_cell_layer_is_highlighted(cell_layer)) {
      menu_cell_title_draw(ctx, cell_layer, "+");
    }
  }
}

/* =================================================================== */
/*                               UI                                    */
/* =================================================================== */

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_text_layer =
      text_layer_create(GRect(1, 0, bounds.size.w - 1, bounds.size.h * 4));

  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_font(s_text_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text(s_text_layer, "Press Select\nto Dictate");

  s_scroll_layer = scroll_layer_create(bounds);

  scroll_layer_set_callbacks(
      s_scroll_layer,
      (ScrollLayerCallbacks){.click_config_provider = click_config_provider});
  scroll_layer_set_content_size(s_scroll_layer,
                                (GSize){bounds.size.w, bounds.size.h * 4});

  scroll_layer_add_child(s_scroll_layer, text_layer_get_layer(s_text_layer));
  layer_add_child(window_layer, scroll_layer_get_layer(s_scroll_layer));
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_main_window);

  // s_menu_layer = menu_layer_create(bounds);
  // menu_layer_set_click_config_onto_window(s_menu_layer, window);
  // menu_layer_set_callbacks(
  //     s_menu_layer, NULL,
  //     (MenuLayerCallbacks){.get_num_rows = menu_get_num_rows_callback,
  //                          .draw_row = menu_draw_row_callback,
  //                          .select_long_click = menu_select_long_callback,
  //                          .select_click = menu_select_callback});
  // layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
  mic_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DICTATE);
  send_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SEND);

  s_actionbar_layer = action_bar_layer_create();
  action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_DOWN,
                                     mic_image, true);
  action_bar_layer_set_icon_animated(s_actionbar_layer, BUTTON_ID_SELECT,
                                     send_image, true);
  //
  //  layer_add_child(window_layer,
  //  action_bar_layer_get_layer(s_actionbar_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

/* =================================================================== */
/*                          STATUS HANDLING                             */
/* =================================================================== */

static void clear_status(void *data) {
  if (s_message[0] == '\0')
    text_layer_set_text(s_text_layer, "Press Select\nto Dictate");
  else
    text_layer_set_text(s_text_layer, s_message);
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
/*                          SENDING                                    */
/* =================================================================== */
static void send(char *message) {

  /* ---- Send transcription to JS (JS handles chunking) ---- */
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    show_status("Send Failed", false);
    return;
  }

  dict_write_cstring(iter, KEY_MEMO_CHUNK, message);
  dict_write_uint8(iter, KEY_MEMO_DONE, 1);

  if (app_message_outbox_send() != APP_MSG_OK) {
    show_status("Send Failed", false);
  }
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
  // if dictation does not fail
  if ((strlen(s_message) + strlen(transcription)) > 1024) {
    show_status("Buffer overload", false);
    return;
  }

  if (s_message[0] != '\0')
    strcat(s_message, " "); // add a space if not the first dictation
  strcat(s_message, transcription);
  // strcat_s(s_message, transcription, 1024);
  // strncpy(s_message, transcription,1024);
  text_layer_set_text(s_text_layer, s_message);
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
    s_message[0] = '\0';
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

// static void menu_select_callback(MenuLayer *menu_layer, MenuIndex
// *cell_index,
//                                  void *context) {
//   // enlarge note
// }

// static void menu_select_long_callback(MenuLayer *menu_layer,
//                                       MenuIndex *cell_index, void *context) {
//   // do something on long press?
// }

static void
action_select_click_handler(ClickRecognizerRef ref,
                            void *context) { // on action bar select, send
  send(s_message);
  action_bar_layer_remove_from_window(s_actionbar_layer);
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_main_window);
  scroll_layer_set_content_offset(s_scroll_layer, GPointZero, true);
}
static void action_up_click_handler(ClickRecognizerRef ref, void *context) {
  //???
  action_bar_layer_remove_from_window(s_actionbar_layer);
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_main_window);
}
static void action_down_click_handler(ClickRecognizerRef ref, void *context) {
  start_dictation();
  action_bar_layer_remove_from_window(s_actionbar_layer);
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_main_window);
}
static void action_back_click_handler(ClickRecognizerRef ref, void *context) {
  action_bar_layer_remove_from_window(s_actionbar_layer);
  scroll_layer_set_click_config_onto_window(s_scroll_layer, s_main_window);
}
static void scroll_back_callback(ClickRecognizerRef ref, void *context) {
  window_stack_pop(true);
}
static void scroll_select_callback(ClickRecognizerRef ref, void *context) {
  // start_dictation();
  action_bar_layer_add_to_window(s_actionbar_layer, s_main_window);
  action_bar_layer_set_click_config_provider(s_actionbar_layer,
                                             action_bar_click_config_provider);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, scroll_select_callback);
  window_single_click_subscribe(BUTTON_ID_BACK, scroll_back_callback);
  //   window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void action_bar_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, action_back_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, action_up_click_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, action_select_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, action_down_click_handler);
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
  if (mic_image) {
    gbitmap_destroy(mic_image);
  }
  if (send_image) {
    gbitmap_destroy(send_image);
  }

  if (s_dictation) {
    dictation_session_destroy(s_dictation);
  }

  if (s_scroll_layer) {
    scroll_layer_destroy(s_scroll_layer);
  }

  if (s_status_timer) {
    app_timer_cancel(s_status_timer);
  }

  if (s_menu_layer) {
    menu_layer_destroy(s_menu_layer);
  }

  if (s_actionbar_layer) {
    action_bar_layer_destroy(s_actionbar_layer);
  }

  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
