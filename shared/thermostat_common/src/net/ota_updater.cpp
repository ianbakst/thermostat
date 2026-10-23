#include "net/ota_updater.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>

#include <cstring>

// A manifest is a few hundred bytes.
static constexpr size_t MANIFEST_MAX_BYTES = 1'024;

// The Arduino core marks a freshly installed image as good the moment it
// boots, which would waste the bootloader's rollback entirely. Taking the job
// over here (this overrides the core's weak hook) lets tick() hold the
// decision back until the device has actually run for a while, so a firmware
// that crashes on the way up is replaced by its predecessor instead.
extern "C" bool verifyRollbackLater() { return true; }

// ---------------------------------------------------------------------------
// Fetching the manifest
// ---------------------------------------------------------------------------

// Where the body lands while esp_http_client hands it over in pieces.
struct Body {
  char* buffer;
  size_t size;  // capacity, including room for the terminator
  size_t used;
};

static esp_err_t onHttpEvent(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
  // Only the answer that actually carries the manifest. A release asset URL
  // arrives via a couple of redirects, and their short bodies would otherwise
  // be pasted on the front of it. Headers are parsed before any body, so the
  // status code is known by now.
  if (esp_http_client_get_status_code(event->client) != 200) return ESP_OK;

  auto* body = static_cast<Body*>(event->user_data);
  const size_t room = body->size - 1 - body->used;
  const size_t take = static_cast<size_t>(event->data_len) < room ? event->data_len : room;
  memcpy(body->buffer + body->used, event->data, take);
  body->used += take;
  body->buffer[body->used] = '\0';
  return ESP_OK;
}

// GETs `url` into `out`. Redirects are followed (a release asset URL bounces
// through one or two of them before landing on GitHub's storage host), and
// every hop is verified against the built-in certificate bundle.
static bool httpGet(const OtaUpdater::Options& options, const char* userAgent, const char* url,
                    char* out, size_t outSize, char* error, size_t errorSize) {
  Body body{out, outSize, 0};
  out[0] = '\0';

  esp_http_client_config_t config = {};
  config.url = url;
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = options.httpTimeoutMs;
  config.buffer_size = options.httpRxBuffer;
  config.buffer_size_tx = options.httpTxBuffer;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.event_handler = onHttpEvent;
  config.user_data = &body;
  config.user_agent = userAgent;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    snprintf(error, errorSize, "Out of memory");
    return false;
  }

  const esp_err_t err = esp_http_client_perform(client);
  const int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (err != ESP_OK) {
    snprintf(error, errorSize, "Can't reach GitHub (%s)", esp_err_to_name(err));
    return false;
  }
  if (status == 404) {
    // The channel has never been published, or its release was deleted.
    snprintf(error, errorSize, "No release on this channel yet");
    return false;
  }
  if (status != 200) {
    snprintf(error, errorSize, "GitHub answered %d", status);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------

void OtaUpdater::begin(OtaChannel channel, const Options& options) {
  options_ = options;
  channel_ = channel;
  lock_ = xSemaphoreCreateMutex();
  snprintf(userAgent_, sizeof(userAgent_), "thermostat/%s", options.firmwareVersion);

  const esp_partition_t* running = esp_ota_get_running_partition();
  Serial.printf("Firmware: %s (%s channel), running from %s\n", options_.firmwareVersion,
                channelName(channel), running != nullptr ? running->label : "?");

  esp_ota_img_states_t state;
  if (running != nullptr && esp_ota_get_state_partition(running, &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    Serial.printf("Firmware: just updated, on trial for %lus\n", options_.selfTestMs / 1000);
  } else {
    imageConfirmed_ = true;  // nothing to prove: this image was already accepted
  }
}

void OtaUpdater::setChannel(OtaChannel channel) {
  if (channel == channel_) return;
  channel_ = channel;
  downloadUrl_[0] = '\0';
  checkedOnce_ = false;  // what the other channel offers says nothing about this one
  publish(OtaState::Idle, "", "");
}

void OtaUpdater::check(unsigned long nowMs) {
  if (running_.load() && job_ == Job::Check) {
    checkPending_ = true;  // tick() starts it once this one is done
    return;
  }
  if (!start(Job::Check)) return;
  lastCheckMs_ = nowMs;
  checkedOnce_ = true;
}

void OtaUpdater::install() {
  if (status().state != OtaState::Available) return;
  start(Job::Install);
}

bool OtaUpdater::installing() const { return status().state == OtaState::Downloading; }

bool OtaUpdater::start(Job job) {
  if (running_.load()) return false;

  // Once an image is installed, the only thing left to do is restart into it.
  // Installed is a hand-off rather than a resting place: loop() watches for it
  // and restarts the device. Letting anything else start here would publish
  // over it before loop() had seen it, leaving a finished update sitting on a
  // device that never reboots.
  if (status().state == OtaState::Installed) return false;

  job_ = job;
  publish(job == Job::Check ? OtaState::Checking : OtaState::Downloading, status().version, "");
  running_.store(true);

  if (xTaskCreate(runJob, "ota", options_.taskStack, this, options_.taskPriority, nullptr) != pdPASS) {
    running_.store(false);
    publish(OtaState::Failed, status().version, "Not enough memory");
    return false;
  }
  return true;
}

void OtaUpdater::runJob(void* self) {
  auto* updater = static_cast<OtaUpdater*>(self);
  if (updater->job_ == Job::Check) {
    updater->doCheck();
  } else {
    updater->doInstall();
  }
  updater->running_.store(false);
  vTaskDelete(nullptr);
}

void OtaUpdater::doCheck() {
  const OtaChannel channel = channel_.load();
  const char* url =
      channel == OtaChannel::Beta ? options_.betaManifestUrl : options_.stableManifestUrl;
  Serial.printf("Update: checking %s\n", url);

  char json[MANIFEST_MAX_BYTES];
  char error[sizeof(OtaStatus::message)];
  const bool fetched = httpGet(options_, userAgent_, url, json, sizeof(json), error, sizeof(error));

  // If the channel changed while this was out asking, its answer is about
  // the old one and goes nowhere: the check asked for with the change runs
  // next. Showing it would put the old channel's news under the new name.
  if (channel_.load() != channel) {
    Serial.println("Update: the channel changed during the check, so its answer is dropped");
    return;
  }

  if (!fetched) {
    Serial.printf("Update: check failed, %s\n", error);
    publish(OtaState::Failed, "", error);
    return;
  }

  JsonDocument manifest;
  if (deserializeJson(manifest, json) != DeserializationError::Ok) {
    publish(OtaState::Failed, "", "That release's manifest is unreadable");
    return;
  }

  const char* version = manifest["version"] | "";
  const char* firmware = manifest["url"] | "";
  if (version[0] == '\0' || firmware[0] == '\0') {
    publish(OtaState::Failed, "", "That release has no firmware in it");
    return;
  }
  snprintf(downloadUrl_, sizeof(downloadUrl_), "%s", firmware);

  // Any version other than the one running counts, so switching to a channel
  // that's behind offers the way back down as readily as the way up.
  if (strcmp(version, options_.firmwareVersion) == 0) {
    Serial.printf("Update: %s is the newest on the %s channel\n", version, channelName(channel));
    publish(OtaState::UpToDate, version, "");
  } else {
    Serial.printf("Update: %s available (running %s)\n", version, options_.firmwareVersion);
    publish(OtaState::Available, version, "");
  }
}

void OtaUpdater::doInstall() {
  Serial.printf("Update: downloading %s\n", downloadUrl_);

  esp_http_client_config_t http = {};
  http.url = downloadUrl_;
  http.timeout_ms = options_.httpTimeoutMs;
  http.buffer_size = options_.httpRxBuffer;
  http.buffer_size_tx = options_.httpTxBuffer;
  http.crt_bundle_attach = esp_crt_bundle_attach;
  http.keep_alive_enable = true;
  http.user_agent = userAgent_;

  esp_https_ota_config_t config = {};
  config.http_config = &http;

  const OtaStatus before = status();
  esp_https_ota_handle_t handle = nullptr;
  esp_err_t err = esp_https_ota_begin(&config, &handle);
  if (err != ESP_OK) {
    Serial.printf("Update: download failed, %s\n", esp_err_to_name(err));
    publish(OtaState::Failed, before.version, "Couldn't start the download");
    return;
  }

  // Content-Length; -1 when the server won't say, in which case there's no
  // percentage to show and the bar stays indeterminate.
  const int total = esp_https_ota_get_image_size(handle);
  int shown = -2;  // neither a percentage nor the "unknown" -1, so the first one always shows
  while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
    const int percent =
        total > 0 ? static_cast<int>(esp_https_ota_get_image_len_read(handle) * 100LL / total) : -1;
    if (percent != shown) {
      shown = percent;
      setProgress(percent);
    }
  }

  if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
    Serial.printf("Update: download failed, %s\n", esp_err_to_name(err));
    esp_https_ota_abort(handle);
    publish(OtaState::Failed, before.version, "The download didn't finish");
    return;
  }

  // Checks the image's own SHA-256 before pointing the bootloader at it, so a
  // corrupted download is refused here rather than at the next boot.
  err = esp_https_ota_finish(handle);
  if (err != ESP_OK) {
    Serial.printf("Update: rejected the image, %s\n", esp_err_to_name(err));
    publish(OtaState::Failed, before.version,
            err == ESP_ERR_OTA_VALIDATE_FAILED ? "That firmware didn't verify"
                                               : "Couldn't install the update");
    return;
  }

  Serial.printf("Update: %s installed, restarting into it\n", before.version);
  publish(OtaState::Installed, before.version, "");
}

// ---------------------------------------------------------------------------

bool OtaUpdater::tick(unsigned long nowMs, bool networkReady) {
  // The bootloader is holding the previous firmware in reserve until this one
  // shows it can run and reach the network. Once it has, let it go.
  if (!imageConfirmed_ && networkReady && nowMs >= options_.selfTestMs) {
    imageConfirmed_ = true;
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      Serial.printf("Firmware: %s passed its trial run and is now the one to keep\n",
                    options_.firmwareVersion);
    }
  }

  // The first check waits a little after the network arrives, so it doesn't
  // compete with Wi-Fi, mDNS and the broker connection for the radio. While
  // there's no network the clock keeps being pushed forward, so the wait is
  // measured from the moment there is one.
  if (!networkReady) lastCheckMs_ = nowMs;
  if (networkReady && !running_.load()) {
    const unsigned long due = checkedOnce_ ? options_.checkIntervalMs : options_.firstCheckDelayMs;
    if (checkPending_) {
      // Asked for while the last one was running: see check().
      checkPending_ = false;
      check(nowMs);
    } else if (nowMs - lastCheckMs_ >= due) {
      // This runs even when an offer is already pending, so that a stale one
      // is replaced when a newer release appears. It costs nothing to re-offer
      // the same version: publish() writes an identical status, tick() sees no
      // change and nothing is redrawn. Skipping the check here instead left a
      // device showing the first release it ever saw, with no way to refresh it
      // short of changing channel and back.
      check(nowMs);
    }
  }

  const OtaStatus current = status();
  if (current.state == reported_.state && current.progress == reported_.progress &&
      strcmp(current.version, reported_.version) == 0 &&
      strcmp(current.message, reported_.message) == 0) {
    return false;
  }
  reported_ = current;
  return true;
}

OtaStatus OtaUpdater::status() const {
  if (lock_ == nullptr) return status_;
  xSemaphoreTake(lock_, portMAX_DELAY);
  const OtaStatus copy = status_;
  xSemaphoreGive(lock_);
  return copy;
}

void OtaUpdater::publish(OtaState state, const char* version, const char* message) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  status_.state = state;
  status_.progress = 0;
  snprintf(status_.version, sizeof(status_.version), "%s", version);
  snprintf(status_.message, sizeof(status_.message), "%s", message);
  xSemaphoreGive(lock_);
}

void OtaUpdater::setProgress(int percent) {
  xSemaphoreTake(lock_, portMAX_DELAY);
  status_.progress = percent;
  xSemaphoreGive(lock_);
}
