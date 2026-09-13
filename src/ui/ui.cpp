#include "ui/ui.h"

#include <cstring>

#include "ui/fonts/fonts.h"
#include "ui/widgets.h"

// ---------------------------------------------------------------------------
// How each setting looks
// ---------------------------------------------------------------------------

static const char* modeIcon(HvacMode mode) {
  switch (mode) {
    case HvacMode::Off:     return ICON_POWER;
    case HvacMode::Heat:    return ICON_FIRE;
    case HvacMode::Cool:    return ICON_SNOWFLAKE;
    case HvacMode::Auto:    return ICON_SUN_SNOWFLAKE_VARIANT;
    case HvacMode::Dry:     return ICON_WATER_PERCENT;
    case HvacMode::FanOnly: return ICON_FAN;
  }
  return ICON_POWER;
}

static lv_palette_t modePalette(HvacMode mode) {
  switch (mode) {
    case HvacMode::Heat:    return LV_PALETTE_DEEP_ORANGE;
    case HvacMode::Cool:    return LV_PALETTE_BLUE;
    case HvacMode::Auto:    return LV_PALETTE_GREEN;
    case HvacMode::Dry:     return LV_PALETTE_TEAL;
    case HvacMode::FanOnly: return LV_PALETTE_BLUE_GREY;
    case HvacMode::Off:     break;
  }
  return LV_PALETTE_GREY;
}

// Bright version, for icons and highlights in the mode picker.
static lv_color_t modeColor(HvacMode mode) { return lv_palette_main(modePalette(mode)); }

// The whole screen's background. Heat, Cool and Dry also show whether the
// unit is running: a bright color while it's heating/cooling/drying, a much
// darker one once it's idle. The idle colors are half the brightness of
// LVGL's darkest red/blue/teal, so they can't be mistaken for the running
// ones, but still clearly colored rather than Off's black. Other modes use a
// dark version of their color. All are dark enough for white text.
static lv_color_t modeBackground(HvacMode mode, HvacAction action) {
  switch (mode) {
    case HvacMode::Heat:
      return action == HvacAction::Heating ? lv_palette_darken(LV_PALETTE_ORANGE, 3)  // orange #E65100
                                           : lv_color_hex(0x5C0E0E);                  // dark red
    case HvacMode::Cool:
      return action == HvacAction::Cooling ? lv_palette_darken(LV_PALETTE_BLUE, 2)    // blue #1976D2
                                           : lv_color_hex(0x062450);                  // deep blue
    case HvacMode::Dry:
      return action == HvacAction::Drying ? lv_palette_darken(LV_PALETTE_TEAL, 2)     // teal #00796B
                                          : lv_color_hex(0x002620);                   // dark teal
    case HvacMode::Off:
      return lv_color_black();
    default:
      return lv_palette_darken(modePalette(mode), 3);
  }
}

// The unit has 4 speeds plus Quiet, but MDI only has fan-speed-1 to 3, so
// Quiet and High are a fan with a down/up chevron.
static const char* fanIcon(FanMode fan) {
  switch (fan) {
    case FanMode::Auto:   return ICON_FAN_AUTO;
    case FanMode::Quiet:  return ICON_FAN_CHEVRON_DOWN;
    case FanMode::Low:    return ICON_FAN_SPEED_1;
    case FanMode::Middle: return ICON_FAN_SPEED_2;
    case FanMode::Medium: return ICON_FAN_SPEED_3;
    case FanMode::High:   return ICON_FAN_CHEVRON_UP;
  }
  return ICON_FAN_AUTO;
}

static const char* wifiIcon(const WifiStatus& status) {
  if (!status.connected) return ICON_WIFI_STRENGTH_OFF_OUTLINE;
  switch (signalBars(status.rssi)) {
    case 4:  return ICON_WIFI_STRENGTH_4;
    case 3:  return ICON_WIFI_STRENGTH_3;
    case 2:  return ICON_WIFI_STRENGTH_2;
    default: return ICON_WIFI_STRENGTH_1;
  }
}

// A triangle-shaped setpoint control with a black +/- inside. The icon glyph
// is itself the tappable object, so there's no rectangular button behind it.
// Disabled fades it and LVGL ignores taps on it.
//
// `signOffsetY` moves the sign from the middle of the icon's box to the
// middle of the triangle, which sits lower in an up-triangle (wide base at
// the bottom) and higher in a down-triangle. From the 40px glyph metrics:
// up is drawn at y 1-33 with its inner center at ~22, down at y 3-35 with
// its inner center at ~14, in a 37px-tall box (middle at 18.5).
static lv_obj_t* makeStepArrow(lv_obj_t* parent, const char* icon, const char* sign,
                               int32_t signOffsetY) {
  lv_obj_t* arrow = createIcon(parent, icon, &mdi_icons_40);
  lv_obj_add_flag(arrow, LV_OBJ_FLAG_CLICKABLE);  // labels aren't by default
  lv_obj_set_ext_click_area(arrow, 12);
  lv_obj_set_style_text_opa(arrow, LV_OPA_40, LV_STATE_DISABLED);

  lv_obj_t* signLabel = lv_label_create(arrow);
  lv_label_set_text(signLabel, sign);
  lv_obj_set_style_text_font(signLabel, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(signLabel, lv_color_black(), 0);
  lv_obj_align(signLabel, LV_ALIGN_CENTER, 0, signOffsetY);
  return arrow;
}

// A bottom-corner button (mode, fan) showing an icon. The main screen's
// background changes color with the mode, so the button can't have a fixed
// color that suits every background. Instead it's translucent white: a
// lighter tint of whatever color is behind it. Disabled fades it, icon and
// all.
static lv_obj_t* makeBottomButton(lv_obj_t* parent, lv_align_t align, int32_t width,
                                  lv_obj_t** icon) {
  const int32_t height = 56;
  const int32_t margin = 8;

  lv_obj_t* button = createFlatButton(parent, width, height);
  lv_obj_set_style_bg_color(button, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_20, 0);
  lv_obj_set_style_opa(button, LV_OPA_40, LV_STATE_DISABLED);
  const int32_t x = align == LV_ALIGN_BOTTOM_LEFT    ? margin
                    : align == LV_ALIGN_BOTTOM_RIGHT ? -margin
                                                     : 0;  // centred
  lv_obj_align(button, align, x, -margin);
  // The screen is ~0.1mm per pixel, so these small buttons also accept taps
  // a few pixels outside their edges.
  lv_obj_set_ext_click_area(button, 8);

  *icon = createIcon(button, "", &mdi_icons_32);
  lv_obj_center(*icon);
  return button;
}

static void setEnabled(lv_obj_t* obj, bool enabled) {
  if (enabled) {
    lv_obj_remove_state(obj, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(obj, LV_STATE_DISABLED);
  }
}

// ---------------------------------------------------------------------------
// Ui
// ---------------------------------------------------------------------------

void Ui::begin(const Callbacks& callbacks) {
  callbacks_ = callbacks;

  lv_obj_t* scr = lv_screen_active();
  screen_ = scr;
  lv_obj_set_style_bg_color(scr, lv_color_black(), 0);  // show() sets it per mode
  lv_obj_set_style_text_color(scr, lv_color_white(), 0);

  // Positions below are offsets on the 172x320 screen.

  // Top-left: settings. Just the gear, no button behind it: the icon itself
  // is tappable, with a 16px margin so the tap target stays ~56px square.
  lv_obj_t* settingsButton = createIcon(scr, ICON_COG, &mdi_icons_24);
  lv_obj_add_flag(settingsButton, LV_OBJ_FLAG_CLICKABLE);  // labels aren't by default
  lv_obj_set_ext_click_area(settingsButton, 16);
  lv_obj_align(settingsButton, LV_ALIGN_TOP_LEFT, 16, 17);
  lv_obj_add_event_cb(
      settingsButton, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openSettings(); },
      LV_EVENT_CLICKED, this);

  // Top-right corner: Wi-Fi status icon, like a phone's.
  wifiIcon_ = createIcon(scr, ICON_WIFI_STRENGTH_OFF_OUTLINE, &mdi_icons_24);
  lv_obj_align(wifiIcon_, LV_ALIGN_TOP_RIGHT, -8, 17);

  // Top center: current temperature with its unit. Even a wide value like
  // "21.5°C" (~62px) stays clear of the settings button and Wi-Fi icon.
  currentLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(currentLabel_, &lv_font_montserrat_20, 0);
  lv_obj_align(currentLabel_, LV_ALIGN_TOP_MID, 0, 17);

  // Center: target temperature, with up/down arrows above and below it
  // (24px clear of the number). temperature_48 only contains the characters
  // a temperature needs, so its line box is shorter than a full font's; the
  // digits are drawn at y 135-169.
  targetLabel_ = lv_label_create(scr);
  lv_obj_set_style_text_font(targetLabel_, &temperature_48, 0);
  lv_obj_align(targetLabel_, LV_ALIGN_TOP_MID, 0, 134);

  upButton_ = makeStepArrow(scr, ICON_TRIANGLE, LV_SYMBOL_PLUS, 4);
  lv_obj_align(upButton_, LV_ALIGN_TOP_MID, 0, 65);
  lv_obj_add_event_cb(
      upButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(+1); },
      LV_EVENT_CLICKED, this);

  downButton_ = makeStepArrow(scr, ICON_TRIANGLE_DOWN, LV_SYMBOL_MINUS, -5);
  lv_obj_align(downButton_, LV_ALIGN_TOP_MID, 0, 202);
  lv_obj_add_event_cb(
      downButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->onStep(-1); },
      LV_EVENT_CLICKED, this);

  // Bottom row: mode (left), airflow (centre), fan (right). 52 + 40 + 52 with
  // the 8px side margins leaves 6px between each — at 56 + 44 + 56 they sat
  // flush against each other and read as one bar. Every target is still 40px
  // or wider, and each takes taps 8px beyond its edges. The airflow icon never
  // changes: there are two louvres behind it, so no one glyph could show both.
  modeButton_ = makeBottomButton(scr, LV_ALIGN_BOTTOM_LEFT, 52, &modeIcon_);
  lv_obj_add_event_cb(
      modeButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openModePicker(); },
      LV_EVENT_CLICKED, this);

  fanButton_ = makeBottomButton(scr, LV_ALIGN_BOTTOM_RIGHT, 52, &fanIcon_);
  lv_obj_add_event_cb(
      fanButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openFanPicker(); },
      LV_EVENT_CLICKED, this);

  lv_obj_t* airflowIcon = nullptr;
  airflowButton_ = makeBottomButton(scr, LV_ALIGN_BOTTOM_MID, 40, &airflowIcon);
  lv_label_set_text(airflowIcon, ICON_ARROW_OSCILLATING);
  lv_obj_add_event_cb(
      airflowButton_, [](lv_event_t* e) { static_cast<Ui*>(lv_event_get_user_data(e))->openAirflow(); },
      LV_EVENT_CLICKED, this);

  show(shown_);  // "--" and locked until the mini-split reports in
}

void Ui::show(const ClimateState& state) {
  shown_ = state;

  // Until the mini-split is online, the last known values stay on screen
  // but faded, and nothing can be tapped.
  const bool online = state.link == ClimateLink::Online;

  char buf[16];
  formatTemp(buf, sizeof(buf), state.currentTemp, state.unit, state.tempStep);
  lv_label_set_text(currentLabel_, buf);
  lv_obj_set_style_text_opa(currentLabel_, online ? LV_OPA_COVER : LV_OPA_40, 0);

  formatTemp(buf, sizeof(buf), state.targetTemp, state.unit, state.tempStep);
  lv_label_set_text(targetLabel_, buf);

  // The setpoint also doesn't apply in Off, Dry or Fan mode.
  const bool adjustable = online && usesTargetTemp(state.mode);
  lv_obj_set_style_text_opa(targetLabel_, adjustable ? LV_OPA_COVER : LV_OPA_40, 0);
  setEnabled(upButton_, adjustable);
  setEnabled(downButton_, adjustable);

  // The whole background shows the mode; the mode button's icon names it.
  lv_obj_set_style_bg_color(screen_, modeBackground(state.mode, state.action), 0);
  lv_label_set_text(modeIcon_, modeIcon(state.mode));
  setEnabled(modeButton_, online);

  lv_label_set_text(fanIcon_, fanIcon(state.fan));
  setEnabled(fanButton_, online);
  setEnabled(airflowButton_, online);

  settingsScreen_.showClimate(state.link, state.unitName);
  airflowScreen_.showPositions(vaneName(state.vane), wideVaneName(state.wideVane));
}

void Ui::openSettings() {
  settingsScreen_.open(
      {[](void* context) { static_cast<Ui*>(context)->callbacks_.startWifiSetup(); },
       [](void* context) { static_cast<Ui*>(context)->callbacks_.startBrokerSetup(); },
       [](void* context) { static_cast<Ui*>(context)->callbacks_.openUnitList(); },
       [](void* context) { static_cast<Ui*>(context)->callbacks_.checkForUpdate(); },
       [](void* context) { static_cast<Ui*>(context)->callbacks_.installUpdate(); },
       [](void* context) { static_cast<Ui*>(context)->callbacks_.openChannelList(); },
       [](void* context, uint8_t value, bool save) {
         static_cast<Ui*>(context)->brightness_ = value;
         static_cast<Ui*>(context)->callbacks_.setBrightness(value, save);
       }},
      this, brightness_);
}

void Ui::showMqttStatus(const char* broker, bool connected, bool loginRefused) {
  settingsScreen_.showMqtt(broker, connected, loginRefused);
}

void Ui::showBrightness(uint8_t brightness) { brightness_ = brightness; }

void Ui::showOta(const OtaStatus& status, OtaChannel channel) {
  settingsScreen_.showOta(status, channel);
}

void Ui::showSetup(const char* qrData, const char* line1, const char* line2, const char* hint,
                   bool cancellable) {
  setupScreen_.open(
      qrData, line1, line2, hint, cancellable,
      [](void* context) { static_cast<Ui*>(context)->callbacks_.cancelSetup(); }, this);
}

void Ui::showSetupProgress(const char* text) { setupScreen_.showProgress(text); }

void Ui::showUnits(const char* const* names, int count, const char* selected) {
  unitNames_ = names;

  int selectedIndex = -1;
  for (int i = 0; i < count; i++) {
    if (strcmp(names[i], selected) == 0) selectedIndex = i;
  }

  listScreen_.open(
      "Mini-split", names, count, selectedIndex,
      "No mini-splits found yet. They appear once the panel reaches the MQTT broker; check the "
      "Wi-Fi section for why it hasn't.",
      [](void* context, int index) {
        auto* ui = static_cast<Ui*>(context);
        ui->callbacks_.setUnit(ui->unitNames_[index]);
      },
      this);
}

void Ui::showChannels(OtaChannel current) {
  // Fixed rows, so a row's position is the channel and there's no mapping to
  // keep (unlike the lists above, whose contents depend on the unit).
  static const char* const labels[] = {"Stable", "Beta"};

  listScreen_.open(
      "Update channel", labels, 2, current == OtaChannel::Beta ? 1 : 0, "",
      [](void* context, int index) {
        static_cast<Ui*>(context)->callbacks_.setOtaChannel(index == 1 ? OtaChannel::Beta
                                                                      : OtaChannel::Stable);
      },
      this);
}

void Ui::openAirflow() {
  airflowScreen_.open(
      {[](void* context) { static_cast<Ui*>(context)->openVaneList(); },
       [](void* context) { static_cast<Ui*>(context)->openWideVaneList(); }},
      this, vaneName(shown_.vane), wideVaneName(shown_.wideVane));
}

void Ui::openVaneList() {
  int count = 0;
  int selected = 0;
  for (int i = 0; i < VANE_MODE_COUNT; i++) {
    if (!isSupported(shown_.vaneMask, i)) continue;
    const auto vane = static_cast<VaneMode>(i);
    if (vane == shown_.vane) selected = count;
    vaneOptions_[count] = vane;
    vaneLabels_[count] = vaneName(vane);
    count++;
  }

  listScreen_.open(
      "Vertical", vaneLabels_, count, selected,
      "This unit doesn't report any vane positions.",
      [](void* context, int index) {
        auto* ui = static_cast<Ui*>(context);
        ui->callbacks_.setVane(ui->vaneOptions_[index]);
      },
      this);
}

void Ui::openWideVaneList() {
  // The horizontal louvre isn't in the unit's discovery config, so there's no
  // advertised list to filter against: every position is on offer.
  for (int i = 0; i < WIDE_VANE_MODE_COUNT; i++) {
    wideVaneLabels_[i] = wideVaneName(static_cast<WideVaneMode>(i));
  }

  listScreen_.open(
      "Horizontal", wideVaneLabels_, WIDE_VANE_MODE_COUNT, static_cast<int>(shown_.wideVane), "",
      [](void* context, int index) {
        static_cast<Ui*>(context)->callbacks_.setWideVane(static_cast<WideVaneMode>(index));
      },
      this);
}

void Ui::showRestarting() {
  lv_obj_t* overlay = createOverlay();
  lv_obj_t* label = lv_label_create(overlay);
  lv_label_set_text(label, "Restarting...");
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_obj_center(label);
}

void Ui::onStep(int direction) {
  const float next =
      stepTargetTemp(shown_.targetTemp, direction, shown_.tempStep, shown_.minTemp, shown_.maxTemp);
  if (next != shown_.targetTemp) callbacks_.setTargetTemp(next);
}

void Ui::showWifiStatus(const WifiStatus& status) {
  // Called every few seconds; only redraw the icon when it actually changes.
  const char* icon = wifiIcon(status);
  if (strcmp(lv_label_get_text(wifiIcon_), icon) != 0) lv_label_set_text(wifiIcon_, icon);

  settingsScreen_.showWifiStatus(status);
}

// Both pickers show only what the unit says it has, so the row tapped has to
// be mapped back through the list that was built (see pickerModes_).
void Ui::openModePicker() {
  PickerOption options[HVAC_MODE_COUNT];
  int count = 0;
  int selected = 0;
  for (int i = 0; i < HVAC_MODE_COUNT; i++) {
    if (!isSupported(shown_.modeMask, i)) continue;
    const auto mode = static_cast<HvacMode>(i);
    if (mode == shown_.mode) selected = count;
    pickerModes_[count] = mode;
    options[count] = {modeIcon(mode), modeName(mode), modeColor(mode)};
    count++;
  }

  picker_.open(
      "Mode", options, count, selected,
      [](void* context, int index) {
        auto* ui = static_cast<Ui*>(context);
        ui->callbacks_.setMode(ui->pickerModes_[index]);
      },
      this);
}

void Ui::openFanPicker() {
  PickerOption options[FAN_MODE_COUNT];
  int count = 0;
  int selected = 0;
  for (int i = 0; i < FAN_MODE_COUNT; i++) {
    if (!isSupported(shown_.fanMask, i)) continue;
    const auto fan = static_cast<FanMode>(i);
    if (fan == shown_.fan) selected = count;
    pickerFans_[count] = fan;
    options[count] = {fanIcon(fan), fanModeName(fan), lv_palette_main(LV_PALETTE_BLUE)};
    count++;
  }

  picker_.open(
      "Fan", options, count, selected,
      [](void* context, int index) {
        auto* ui = static_cast<Ui*>(context);
        ui->callbacks_.setFanMode(ui->pickerFans_[index]);
      },
      this);
}
