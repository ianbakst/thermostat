#include "ui/airflow_screen.h"

#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

static void makeCaption(lv_obj_t* parent, const char* text, int32_t y) {
  lv_obj_t* caption = lv_label_create(parent);
  lv_label_set_text(caption, text);
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(caption, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_pos(caption, 12, y);
}

// A wide button whose label is the current position, so the page reads as a
// summary as well as a way in.
static lv_obj_t* makePositionButton(lv_obj_t* parent, int32_t y, const char* value,
                                    lv_event_cb_t onClick, void* userData, lv_obj_t** label) {
  lv_obj_t* button = createFlatButton(parent, 156, 44);
  lv_obj_set_pos(button, 8, y);
  *label = lv_label_create(button);
  lv_label_set_text(*label, value);
  lv_obj_set_style_text_font(*label, &lv_font_montserrat_14, 0);
  lv_obj_center(*label);
  lv_obj_add_event_cb(button, onClick, LV_EVENT_CLICKED, userData);
  return button;
}

void AirflowScreen::open(const Callbacks& callbacks, void* context, const char* vertical,
                         const char* horizontal) {
  if (!showOverlay()) return;  // already open
  callbacks_ = callbacks;
  context_ = context;

  addHeader(
      "Airflow",
      [](lv_event_t* e) { static_cast<AirflowScreen*>(lv_event_get_user_data(e))->close(); }, this);

  makeCaption(overlay_, "Vertical", 64);
  makePositionButton(
      overlay_, 84, vertical,
      [](lv_event_t* e) {
        auto* screen = static_cast<AirflowScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.openVaneList) screen->callbacks_.openVaneList(screen->context_);
      },
      this, &verticalLabel_);

  makeCaption(overlay_, "Horizontal", 148);
  makePositionButton(
      overlay_, 168, horizontal,
      [](lv_event_t* e) {
        auto* screen = static_cast<AirflowScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.openWideVaneList) {
          screen->callbacks_.openWideVaneList(screen->context_);
        }
      },
      this, &horizontalLabel_);

  lv_obj_t* note = lv_label_create(overlay_);
  lv_label_set_text(note, "Horizontal isn't in Home Assistant, so this is the place to set it.");
  lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(note, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_width(note, 148);
  lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(note, 12, 232);
}

void AirflowScreen::showPositions(const char* vertical, const char* horizontal) {
  if (!isOpen()) return;
  if (strcmp(lv_label_get_text(verticalLabel_), vertical) != 0) {
    lv_label_set_text(verticalLabel_, vertical);
  }
  if (strcmp(lv_label_get_text(horizontalLabel_), horizontal) != 0) {
    lv_label_set_text(horizontalLabel_, horizontal);
  }
}

void AirflowScreen::close() {
  hideOverlay();
  verticalLabel_ = nullptr;
  horizontalLabel_ = nullptr;
}
