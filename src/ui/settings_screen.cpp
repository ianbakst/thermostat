#include "ui/settings_screen.h"

#include <cstdio>
#include <cstring>

#include "config.h"
#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

// Each section starts with a grey caption, with a gap above it to separate it
// from the section before.
static void makeCaption(lv_obj_t* parent, const char* text) {
  lv_obj_t* caption = lv_label_create(parent);
  lv_label_set_text(caption, text);
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(caption, lv_palette_main(LV_PALETTE_GREY), 0);
  lv_obj_set_style_pad_top(caption, 10, 0);
}

// A few lines of read-only text under a caption.
static lv_obj_t* makeInfoLabel(lv_obj_t* parent) {
  lv_obj_t* label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_set_width(label, 156);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  return label;
}

// A wide button with a label, for the action at the end of a section.
static lv_obj_t* makeActionButton(lv_obj_t* parent, const char* text) {
  lv_obj_t* button = createFlatButton(parent, 156, 36);
  lv_obj_set_style_margin_top(button, 6, 0);
  lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_obj_center(label);
  return button;
}

// The label inside a button made by makeActionButton, for the ones whose text
// changes with what's going on.
static lv_obj_t* buttonLabel(lv_obj_t* button) { return lv_obj_get_child(button, 0); }

// Only touch a label when its text changes; setting it always redraws.
static void setTextIfChanged(lv_obj_t* label, const char* text) {
  if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

static void setEnabled(lv_obj_t* obj, bool enabled) {
  if (enabled) {
    lv_obj_remove_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  }
}

// What a signal strength means in practice, matching the bars on the main
// screen's Wi-Fi icon.
static const char* signalQuality(int rssi) {
  switch (signalBars(rssi)) {
    case 4:  return "strong";
    case 3:  return "good";
    case 2:  return "weak";
    default: return "very weak";
  }
}

// The update status in words, on the line under the version.
static const char* otaDescription(const OtaStatus& status) {
  switch (status.state) {
    case OtaState::Idle:        return "Not checked yet";
    case OtaState::Checking:    return "Checking for updates...";
    case OtaState::UpToDate:    return "Up to date";
    case OtaState::Available:   return "An update is ready to install";
    case OtaState::Downloading: return "Installing...";
    case OtaState::Installed:   return "Installed; restarting";
    case OtaState::Failed:      return status.message;
  }
  return "?";
}

static const char* linkDescription(ClimateLink link) {
  switch (link) {
    case ClimateLink::NoUnit:     return "None chosen";
    case ClimateLink::Connecting: return "Connecting to MQTT...";
    case ClimateLink::Refused:    return "MQTT login refused";
    case ClimateLink::Waiting:    return "Waiting for the unit...";
    case ClimateLink::Offline:    return "Offline";
    case ClimateLink::Online:     return "Online";
  }
  return "?";
}

void SettingsScreen::open(const Callbacks& callbacks, void* context, uint8_t brightness) {
  if (!showOverlay()) return;  // already open
  callbacks_ = callbacks;
  context_ = context;

  // The header stays put while the sections below it scroll.
  addHeader(
      "Settings",
      [](lv_event_t* e) { static_cast<SettingsScreen*>(lv_event_get_user_data(e))->close(); }, this);

  // The sections below the title scroll as one column. Like every other widget
  // here it drops LVGL's default theme styles: among them is the theme's own
  // text color, which stopped the labels and buttons inside inheriting the
  // screen's white and left them looking greyed out. The scrollbar is drawn by
  // hand instead, and dropping the theme drops its shadow too.
  lv_obj_t* content = lv_obj_create(overlay_);
  lv_obj_remove_style_all(content);
  lv_obj_set_pos(content, 0, 52);
  lv_obj_set_size(content, 172, 268);
  lv_obj_set_style_pad_hor(content, 8, 0);
  lv_obj_set_style_pad_row(content, 2, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(content, LV_DIR_VER);
  lv_obj_set_style_bg_opa(content, LV_OPA_30, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(content, lv_color_white(), LV_PART_SCROLLBAR);
  lv_obj_set_style_width(content, 4, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(content, 2, LV_PART_SCROLLBAR);

  makeCaption(content, "Screen Brightness");
  lv_obj_t* slider = lv_slider_create(content);
  lv_obj_remove_style_all(slider);
  lv_obj_set_size(slider, 148, 14);
  lv_obj_set_style_margin_top(slider, 8, 0);
  lv_obj_set_style_margin_bottom(slider, 8, 0);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(slider, buttonColor(), 0);
  lv_obj_set_style_radius(slider, 7, 0);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
  lv_obj_set_style_radius(slider, 7, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_bg_color(slider, lv_color_white(), LV_PART_KNOB);
  lv_obj_set_style_radius(slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 7, LV_PART_KNOB);  // a knob wider than the track
  lv_slider_set_range(slider, BRIGHTNESS_MIN, 255);
  lv_slider_set_value(slider, brightness, LV_ANIM_OFF);

  // Dragging follows the slider; letting go is what saves it.
  const lv_event_cb_t onSlider = [](lv_event_t* e) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!screen->callbacks_.setBrightness) return;
    const auto value = static_cast<uint8_t>(lv_slider_get_value(lv_event_get_target_obj(e)));
    screen->callbacks_.setBrightness(screen->context_, value,
                                     lv_event_get_code(e) == LV_EVENT_RELEASED);
  };
  lv_obj_add_event_cb(slider, onSlider, LV_EVENT_VALUE_CHANGED, this);
  lv_obj_add_event_cb(slider, onSlider, LV_EVENT_RELEASED, this);

  makeCaption(content, "Wi-Fi");
  wifiLabel_ = makeInfoLabel(content);
  lv_obj_add_event_cb(
      makeActionButton(content, "Wi-Fi setup"),
      [](lv_event_t* e) {
        auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.startWifiSetup) screen->callbacks_.startWifiSetup(screen->context_);
      },
      LV_EVENT_CLICKED, this);

  makeCaption(content, "MQTT");
  mqttLabel_ = makeInfoLabel(content);
  mqttButton_ = makeActionButton(content, "MQTT setup");
  lv_obj_add_event_cb(
      mqttButton_,
      [](lv_event_t* e) {
        auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.startBrokerSetup) screen->callbacks_.startBrokerSetup(screen->context_);
      },
      LV_EVENT_CLICKED, this);

  makeCaption(content, "Mini-split");
  climateLabel_ = makeInfoLabel(content);
  lv_obj_add_event_cb(
      makeActionButton(content, "Change"),
      [](lv_event_t* e) {
        auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.openUnitList) screen->callbacks_.openUnitList(screen->context_);
      },
      LV_EVENT_CLICKED, this);

  makeCaption(content, "Firmware");
  otaLabel_ = makeInfoLabel(content);
  otaButton_ = makeActionButton(content, "Check for updates");
  lv_obj_add_event_cb(
      otaButton_,
      [](lv_event_t* e) {
        // One button, two jobs: it offers the install once there's something
        // to install, and asks again otherwise.
        auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        if (screen->ota_.state == OtaState::Available) {
          if (screen->callbacks_.installUpdate) screen->callbacks_.installUpdate(screen->context_);
        } else if (screen->callbacks_.checkForUpdate) {
          screen->callbacks_.checkForUpdate(screen->context_);
        }
      },
      LV_EVENT_CLICKED, this);

  channelLabel_ = buttonLabel(makeActionButton(content, "Channel: Stable"));
  lv_obj_add_event_cb(
      lv_obj_get_parent(channelLabel_),
      [](lv_event_t* e) {
        auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
        if (screen->callbacks_.openChannelList) {
          screen->callbacks_.openChannelList(screen->context_);
        }
      },
      LV_EVENT_CLICKED, this);

  renderWifi();
  renderMqtt();
  renderClimate();
  renderOta();
}

void SettingsScreen::showWifiStatus(const WifiStatus& status) {
  wifiStatus_ = status;
  renderWifi();
}

void SettingsScreen::showMqtt(const char* broker, bool connected, bool loginRefused) {
  snprintf(broker_, sizeof(broker_), "%s", broker);
  mqttConnected_ = connected;
  mqttLoginRefused_ = loginRefused;
  renderMqtt();
}

void SettingsScreen::showClimate(ClimateLink link, const char* unitName) {
  climateLink_ = link;
  snprintf(unitName_, sizeof(unitName_), "%s", unitName);
  renderClimate();
}

void SettingsScreen::showOta(const OtaStatus& status, OtaChannel channel) {
  ota_ = status;
  otaChannel_ = channel;
  renderOta();
}

void SettingsScreen::renderWifi() {
  if (!overlay_) return;

  char text[96];
  if (wifiStatus_.connected) {
    snprintf(text, sizeof(text), "%s\n%s\nSignal: %d dBm (%s)", wifiStatus_.ssid, wifiStatus_.ip,
             wifiStatus_.rssi, signalQuality(wifiStatus_.rssi));
  } else if (wifiStatus_.ssid[0] != '\0') {
    snprintf(text, sizeof(text), "Connecting to %s...", wifiStatus_.ssid);
  } else {
    snprintf(text, sizeof(text), "No network saved");
  }
  setTextIfChanged(wifiLabel_, text);

  // Changing the broker means testing it, which needs the network up. So does
  // reaching GitHub, so the firmware section follows the same rule.
  setEnabled(mqttButton_, wifiStatus_.connected);
  renderOta();
}

void SettingsScreen::renderMqtt() {
  if (!overlay_) return;

  char text[112];
  snprintf(text, sizeof(text), "%s\n%s", broker_[0] ? broker_ : "(not set)",
           mqttLoginRefused_ ? "Login refused" : mqttConnected_ ? "Connected" : "Connecting...");
  setTextIfChanged(mqttLabel_, text);
}

void SettingsScreen::renderClimate() {
  if (!overlay_) return;

  char text[64];
  snprintf(text, sizeof(text), "%s\n%s", unitName_[0] ? unitName_ : "(none)",
           linkDescription(climateLink_));
  setTextIfChanged(climateLabel_, text);
}

void SettingsScreen::renderOta() {
  if (!overlay_) return;

  // What's running, then what's on offer. The channel is deliberately not on
  // this line: it's a setting (with its own button below), not a property of
  // the firmware running, and the two can legitimately differ — installing a
  // beta and then switching to stable would otherwise render as though that
  // beta were a stable release. The version string already says where a build
  // came from.
  char text[64 + sizeof(OtaStatus::message)];
  if (ota_.state == OtaState::Downloading && ota_.progress >= 0) {
    snprintf(text, sizeof(text), "%s\nInstalling... %d%%", FIRMWARE_VERSION, ota_.progress);
  } else if (ota_.state == OtaState::Available) {
    snprintf(text, sizeof(text), "%s\nUpdate available: %s", FIRMWARE_VERSION, ota_.version);
  } else {
    snprintf(text, sizeof(text), "%s\n%s", FIRMWARE_VERSION, otaDescription(ota_));
  }
  setTextIfChanged(otaLabel_, text);

  char action[48];
  if (ota_.state == OtaState::Available) {
    snprintf(action, sizeof(action), "Install %s", ota_.version);
  } else {
    snprintf(action, sizeof(action), "Check for updates");
  }
  setTextIfChanged(buttonLabel(otaButton_), action);

  // Nothing to press while a check or a download is running, or once the panel
  // is on its way into the new firmware.
  const bool working = ota_.state == OtaState::Checking || ota_.state == OtaState::Downloading ||
                       ota_.state == OtaState::Installed;
  setEnabled(otaButton_, wifiStatus_.connected && !working);
  setEnabled(lv_obj_get_parent(channelLabel_), !working);

  char channel[32];
  snprintf(channel, sizeof(channel), "Channel: %s",
           otaChannel_ == OtaChannel::Beta ? "Beta" : "Stable");
  setTextIfChanged(channelLabel_, channel);
}

void SettingsScreen::close() {
  hideOverlay();
  wifiLabel_ = nullptr;
  mqttLabel_ = nullptr;
  mqttButton_ = nullptr;
  climateLabel_ = nullptr;
  otaLabel_ = nullptr;
  otaButton_ = nullptr;
  channelLabel_ = nullptr;
}
