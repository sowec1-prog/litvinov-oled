#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "esp_wifi.h"
#include "esp_log.h"
#include "hcverva_logo.h"
#include "secrets.h"

// Druha sit je volitelna. Stary lokalni secrets.h bez techto maker zustava
// kompatibilni; po doplneni se ESP pripoji k prvni dostupne ulozene siti.
#ifndef WIFI_SSID_2
#define WIFI_SSID_2 ""
#endif
#ifndef WIFI_PASSWORD_2
#define WIFI_PASSWORD_2 ""
#endif
#ifndef WIFI_SSID_3
#define WIFI_SSID_3 ""
#endif
#ifndef WIFI_PASSWORD_3
#define WIFI_PASSWORD_3 ""
#endif

// ESP32-C3 SuperMini + 1,3" OLED SH1106: SDA=GPIO8, SCL=GPIO9.
static constexpr uint8_t OLED_SDA = 8;
static constexpr uint8_t OLED_SCL = 9;
// Bzučák ověřený jako tónový/PWM: signál GPIO4, druhý vodič GND.
static constexpr uint8_t BUZZER_PIN = 4;
static constexpr uint8_t BUZZER_CHANNEL = 0;
static constexpr uint16_t BUZZER_QUARTER_MS = 250;
static constexpr uint8_t BUZZER_GAP_PERCENT = 8;
// Priame spojeni ESP -> Render (bez zavisleho PC).
static constexpr char API_URL[] = "https://litvinov-server.onrender.com/api/live";
// Slabý 2,4GHz signál potřebuje delší okno pro autentizaci a zotavení.
static constexpr uint32_t WIFI_RETRY_MS = 30000;
static constexpr uint32_t POLL_MS = 30000;
// Pri live zapase kontrolujeme data casteji, ale OLED se prekresli jen pri zmene.
static constexpr uint32_t LIVE_POLL_MS = 5000;
// Když se uložené sítě nepřipojí, vznikne lokální konfigurační AP.
// ESP32-C3 při skenu vrací pouze 2,4GHz sítě, takže zde nelze omylem vybrat 5 GHz.
// Portál je až poslední možnost; při slabém signálu se nejdřív několik minut zotavujeme.
static constexpr uint32_t SETUP_PORTAL_DELAY_MS = 180000;
static constexpr char SETUP_AP_SSID[] = "Litvinov-OLED-Setup";
static constexpr char SETUP_AP_PASSWORD[] = "litvinov";
// Preferujeme hlavní síť; záložní sítě zůstávají pravidelně v rotačním pokusu.
static constexpr uint8_t WIFI_ATTEMPT_ORDER[] = {0, 0, 0, 1, 2, 3};
static constexpr byte DNS_PORT = 53;

Adafruit_SH1106G oled(128, 64, &Wire, -1);
WebServer setupServer(80);
DNSServer setupDns;
Preferences preferences;
bool wifiNetworksConfigured = false;
String setupSavedSsid;
String setupSavedPassword;
uint8_t wifiAttemptIndex = 0;
bool setupPortalActive = false;
uint32_t wifiConnectStarted = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastPoll = 0;
bool liveMode = false;
bool wifiWasConnected = false;
bool countdownActive = false;
uint32_t matchStartEpoch = 0;
uint32_t serverEpoch = 0;
uint32_t serverEpochMillis = 0;
uint8_t tablePosition = 0;  // 1–14, získáno z tabulky extraligy.
String scheduledHome;
String scheduledAway;
uint32_t shownCountdownSecond = UINT32_MAX;

// ESP si pamatuje jen poslední cloudový event, aby znělku nezopakoval při dalším pollu.
bool audioEventBaselineKnown = false;
String previousAudioEventId;
// Poslední obsah, který už OLED opravdu ukazuje. Stejný cloudový payload nepřekreslujeme.
String lastRenderedPayloadKey;
String lastScheduledIdentity;
bool intermissionActive = false;
uint32_t intermissionUntilEpoch = 0;
uint32_t intermissionServerEpoch = 0;
uint32_t intermissionServerMillis = 0;

// Definice je níže; animace ji používá, aby lišta zůstala vidět i při gólu.
void presentOled();

struct BuzzerNote {
  uint16_t hz;
  uint8_t sixteenths;  // 4=čtvrtka, 16=celá
};

constexpr uint16_t BUZZER_REST = 0;
// Začátek zápasu / gól Litvínova — uložená vítězná znělka.
const BuzzerNote LIT_GOAL_TUNE[] = {
  {370, 16}, {494, 4}, {415, 4}, {494, 4}, {554, 16},
  {BUZZER_REST, 16}, {415, 16}, {554, 4}, {466, 4}, {554, 4}, {494, 16},
};
// Inkasovaný gól — stejná délka a rytmus, smutnější F# moll.
const BuzzerNote CONCEDED_GOAL_TUNE[] = {
  {370, 16}, {494, 4}, {440, 4}, {494, 4}, {554, 16},
  {BUZZER_REST, 16}, {440, 16}, {554, 4}, {494, 4}, {440, 4}, {370, 16},
};

void playBuzzerTune(const BuzzerNote* tune, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const uint16_t duration = (BUZZER_QUARTER_MS * tune[i].sixteenths) / 4;
    if (tune[i].hz == BUZZER_REST) {
      ledcWriteTone(BUZZER_CHANNEL, 0);
      delay(duration);
      continue;
    }
    const uint16_t soundDuration = duration - (duration * BUZZER_GAP_PERCENT / 100);
    ledcWriteTone(BUZZER_CHANNEL, tune[i].hz);
    delay(soundDuration);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    delay(duration - soundDuration);
  }
}

// Litvinovsky gol: blikajici GOOL jede jednou zprava doleva po dobu 30 sekund.
void playLitGoalAnimation() {
  static constexpr uint32_t GOAL_ANIMATION_MS = 30000;
  static constexpr uint16_t FRAME_MS = 70;
  const String message = "GOOL";
  const uint8_t textSize = 4;
  int16_t x1, y1;
  uint16_t textWidth, textHeight;
  oled.setTextSize(textSize);
  oled.getTextBounds(message, 0, 0, &x1, &y1, &textWidth, &textHeight);
  const uint32_t started = millis();

  while (millis() - started < GOAL_ANIMATION_MS) {
    const uint32_t elapsed = millis() - started;
    const int16_t x = 128 - ((128 + textWidth) * elapsed / GOAL_ANIMATION_MS);
    const bool visible = ((elapsed / 280) % 2) == 0;

    oled.clearDisplay();
    if (visible) {
      oled.setTextColor(SH110X_WHITE);
      oled.setTextSize(textSize);
      oled.setCursor(x, (64 - textHeight) / 2);
      oled.print(message);
    }
    presentOled();
    delay(FRAME_MS);
  }
}

bool handleAudioCue(const String& eventId, const String& cue) {
  // První odpověď po startu pouze nastaví baseline, aby se nepřehrál starší gól.
  if (!audioEventBaselineKnown) {
    previousAudioEventId = eventId;
    audioEventBaselineKnown = true;
    return false;
  }
  if (eventId.isEmpty() || eventId == previousAudioEventId) return false;

  previousAudioEventId = eventId;
  if (cue == "lit_goal") {
    Serial.println("CLOUD_AUDIO=lit_goal");
    playBuzzerTune(LIT_GOAL_TUNE, sizeof(LIT_GOAL_TUNE) / sizeof(LIT_GOAL_TUNE[0]));
    playLitGoalAnimation();
    return true;  // Obnovime aktualni live obrazovku hned po animaci.
  } else if (cue == "conceded_goal") {
    Serial.println("CLOUD_AUDIO=conceded_goal");
    playBuzzerTune(CONCEDED_GOAL_TUNE, sizeof(CONCEDED_GOAL_TUNE) / sizeof(CONCEDED_GOAL_TUNE[0]));
  }
  return false;
}

// Stav Wi-Fi je trvalá horní lišta. Všechny obrazovky začínají až pod ní.
static constexpr int16_t WIFI_BAR_HEIGHT = 8;
// Čárky se obnoví jednou za 10 s; nepouští Wi-Fi scan, jen čte aktuální RSSI.
static constexpr uint32_t WIFI_BAR_REFRESH_MS = 10000;
uint32_t lastWifiBarRefresh = 0;

String formatHeaderTime() {
  if (serverEpoch == 0 || serverEpochMillis == 0) return "--:--";
  const time_t now = serverEpoch + (millis() - serverEpochMillis) / 1000;
  struct tm localTime {};
  localtime_r(&now, &localTime);
  char text[6];
  snprintf(text, sizeof(text), "%02d:%02d", localTime.tm_hour, localTime.tm_min);
  return String(text);
}

void drawWifiStatusBar() {
  // Trvalá hlavička: pořadí v tabulce vlevo, skutečný čas uprostřed, Wi-Fi vpravo.
  oled.fillRect(0, 0, 128, WIFI_BAR_HEIGHT, SH110X_BLACK);
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  if (tablePosition > 0) {
    oled.print(String(tablePosition) + ".");
  } else {
    oled.print("--");
  }
  oled.setCursor(49, 0);
  oled.print(formatHeaderTime());

  if (WiFi.status() != WL_CONNECTED) {
    // X = odpojeno; tři tečky = aktivní lokální setup portál.
    if (setupPortalActive) {
      oled.fillCircle(113, 4, 1, SH110X_WHITE);
      oled.fillCircle(118, 4, 1, SH110X_WHITE);
      oled.fillCircle(123, 4, 1, SH110X_WHITE);
    } else {
      oled.drawLine(116, 1, 124, 7, SH110X_WHITE);
      oled.drawLine(124, 1, 116, 7, SH110X_WHITE);
    }
    return;
  }

  const int rssi = WiFi.RSSI();
  uint8_t bars = 0;
  if (rssi >= -60) bars = 4;
  else if (rssi >= -67) bars = 3;
  else if (rssi >= -75) bars = 2;
  else if (rssi >= -82) bars = 1;

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t x = 110 + i * 4;
    const int16_t height = 2 + i * 2;
    if (i < bars) oled.fillRect(x, 7 - height, 3, height, SH110X_WHITE);
    else oled.drawRect(x, 7 - height, 3, height, SH110X_WHITE);
  }
}

void presentOled() {
  drawWifiStatusBar();
  oled.display();
}

void printCentered(const String &text, int16_t y, uint8_t size) {
  int16_t x1, y1;
  uint16_t width, height;
  oled.setTextSize(size);
  oled.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  oled.setCursor(width >= 128 ? 0 : (128 - width) / 2, y);
  oled.print(text);
}

void screen(const String &a, const String &b = "", const String &c = "", const String &d = "") {
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 9);
  oled.println(a);
  oled.println(b);
  oled.println(c);
  oled.println(d);
  presentOled();
}

void screenVervaLogo() {
  // Při připojování nevypisujeme technické Wi-Fi hlášky; zůstane čistý klubový znak.
  oled.clearDisplay();
  oled.drawBitmap((128 - HC_VERVA_LOGO_WIDTH) / 2, WIFI_BAR_HEIGHT + 1,
                  HC_VERVA_LOGO, HC_VERVA_LOGO_WIDTH, HC_VERVA_LOGO_HEIGHT, SH110X_WHITE);
  presentOled();
}

String normalizeDate(String dateTime) {
  String time = dateTime.length() >= 5 ? dateTime.substring(dateTime.length() - 5) : "--:--";
  String date = dateTime.length() > 6 ? dateTime.substring(0, dateTime.length() - 6) : dateTime;
  date.replace("ÚT", "UT");
  date.replace("ČT", "CT");
  date.replace("PÁ", "PA");
  date.replace(". ", ".");
  if (date.endsWith(".")) date.remove(date.length() - 1);
  return date + " " + time;
}

String formatCountdown(uint32_t remaining) {
  if (remaining >= 3600) return "ZA " + String(remaining / 3600) + " H";
  if (remaining >= 600) return "ZA " + String(remaining / 60) + " MIN";
  char text[12];
  snprintf(text, sizeof(text), "%02lu:%02lu", remaining / 60, remaining % 60);
  return text;
}

void screenLive(const String &homeCode, const String &awayCode, int homeScore, int awayScore, const String &clock, const String &penaltyIndicator) {
  const String teams = homeCode + "-" + awayCode;
  const String score = String(homeScore) + ":" + String(awayScore);
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  // Horních 8 px trvale zabírá Wi-Fi lišta.
  printCentered(teams, 9, 1);
  printCentered(score, 19, 3);
  if (!penaltyIndicator.isEmpty()) printCentered(penaltyIndicator, 45, 1);
  printCentered(clock, penaltyIndicator.isEmpty() ? 48 : 55, 1);
  presentOled();
}

void screenFinished(const String &homeCode, const String &awayCode, int homeScore, int awayScore) {
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  printCentered(homeCode + "-" + awayCode, 9, 1);
  printCentered(String(homeScore) + ":" + String(awayScore), 19, 2);
  printCentered("ZAPAS", 38, 1);
  printCentered("SKONCIL", 51, 1);
  presentOled();
}

void screenFixture(const String &home, const String &away, const String &bottom, uint8_t bottomSize) {
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  // Domácí LIT je natrvalo v liště; tabulka tak má více místa pro soupeře a termín.
  if (home.isEmpty()) {
    printCentered("proti", 15, 1);
    printCentered(away, 28, 1);
  } else {
    printCentered(home, 9, 1);
    printCentered("proti", 20, 1);
    printCentered(away, 31, 1);
  }
  printCentered(bottom, 46, bottomSize);
  presentOled();
}

void screenBigScheduled(const String &home, const String &away, const String &dateTime) {
  // Pořadí v liště není název klubu; pro soupeře musí zůstat vidět oba týmy.
  screenFixture(home, away, normalizeDate(dateTime), 1);
}

void screenCountdown(uint32_t remaining) {
  screenFixture(scheduledHome, scheduledAway, formatCountdown(remaining), 2);
}

void refreshCountdown() {
  if (!countdownActive || matchStartEpoch == 0 || serverEpoch == 0) return;
  const uint32_t now = serverEpoch + (millis() - serverEpochMillis) / 1000;
  const uint32_t remaining = matchStartEpoch > now ? matchStartEpoch - now : 0;
  if (remaining != shownCountdownSecond) {
    shownCountdownSecond = remaining;
    screenCountdown(remaining);
  }
}

void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_START:
      ESP_LOGI("LIT", "WIFI_EVENT=STA_START");
      Serial.println("WIFI_EVENT=STA_START");
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      ESP_LOGI("LIT", "WIFI_EVENT=STA_CONNECTED");
      Serial.println("WIFI_EVENT=STA_CONNECTED");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      ESP_LOGI("LIT", "WIFI_EVENT=GOT_IP ip=%s rssi=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      Serial.printf("WIFI_EVENT=GOT_IP ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      ESP_LOGW("LIT", "WIFI_EVENT=DISCONNECTED reason=%d", info.wifi_sta_disconnected.reason);
      Serial.printf("WIFI_EVENT=DISCONNECTED reason=%d\n", info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
  }
}

void htmlOption(String &page, const String &ssid) {
  String escaped = ssid;
  escaped.replace("&", "&amp;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  escaped.replace("\"", "&quot;");
  page += "<option value=\"" + escaped + "\">" + escaped + "</option>";
}

void showSetupPage() {
  const int count = WiFi.scanNetworks(false, true);
  String page = "<!doctype html><meta name=viewport content='width=device-width,initial-scale=1'>"
                "<title>Litvinov OLED Wi-Fi</title><style>body{font-family:sans-serif;max-width:32rem;margin:2rem auto;padding:0 1rem}input,select,button{box-sizing:border-box;width:100%;font-size:1rem;padding:.7rem;margin:.4rem 0}button{background:#bd0000;color:white;border:0;border-radius:.3rem}</style>"
                "<h2>Litvinov OLED - Wi-Fi</h2><p>Jsou zde jen site 2,4 GHz, ktere ESP32-C3 umi pouzit. Pokud sit v seznamu chybi, napis jeji presny nazev rucne.</p>"
                "<form method=post action=/save><label>Wi-Fi sit</label><input name=ssid list=ssids placeholder='Presny nazev Wi-Fi' required><datalist id=ssids>";
  for (int i = 0; i < count; ++i) htmlOption(page, WiFi.SSID(i));
  page += "</datalist>";
  if (count <= 0) page += "<p>Zadna sit pri skenu nenalezena. Rucni zadani funguje normalne.</p>";
  page += "<label>Heslo Wi-Fi</label><input name=pass type=password autocomplete=current-password required>"
          "<button type=submit>Ulozit a pripojit</button></form><p>Po ulozeni se OLED sam pripoji a tento hotspot zmizi.</p>";
  setupServer.send(200, "text/html; charset=utf-8", page);
}

void saveSetupWifi() {
  const String ssid = setupServer.arg("ssid");
  const String password = setupServer.arg("pass");
  if (ssid.isEmpty() || password.isEmpty()) {
    setupServer.send(400, "text/plain; charset=utf-8", "Chybi nazev site nebo heslo.");
    return;
  }
  preferences.begin("wifi", false);
  preferences.putString("setup_ssid", ssid);
  preferences.putString("setup_pass", password);
  preferences.end();
  setupServer.send(200, "text/html; charset=utf-8", "<meta name=viewport content='width=device-width'><h2>Overuji pripojeni OLED...</h2><p>Vyckej asi pul minuty. Po uspechu hotspot zmizi.</p>");
  WiFi.begin(ssid.c_str(), password.c_str());
  screen("OVERUJI WIFI", "VYBRANA 2.4G SIT");
  wifiConnectStarted = millis();
}

void startSetupPortal() {
  if (setupPortalActive) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);
  setupDns.start(DNS_PORT, "*", WiFi.softAPIP());
  setupServer.on("/", HTTP_GET, showSetupPage);
  setupServer.on("/save", HTTP_POST, saveSetupWifi);
  setupServer.on("/generate_204", HTTP_GET, showSetupPage);
  setupServer.onNotFound(showSetupPage);
  setupServer.begin();
  setupPortalActive = true;
  screen("NASTAVENI WIFI", SETUP_AP_SSID, "HESLO: litvinov", "192.168.4.1");
  Serial.println("WIFI_SETUP_PORTAL_ACTIVE");
}

void stopSetupPortal() {
  if (!setupPortalActive) return;
  setupDns.stop();
  setupServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  setupPortalActive = false;
}

void configureWifiNetworks() {
  if (wifiNetworksConfigured) return;
  // Hesla jsou v secrets.h nebo NVS; nevypisujeme je ani názvy sítí do sériového logu.
  preferences.begin("wifi", false);
  setupSavedSsid = preferences.isKey("setup_ssid") ? preferences.getString("setup_ssid") : "";
  setupSavedPassword = preferences.isKey("setup_pass") ? preferences.getString("setup_pass") : "";
  preferences.end();
  wifiNetworksConfigured = true;
}

bool beginStoredWifi(uint8_t slot) {
  const char* ssid = "";
  const char* password = "";
  switch (slot) {
    case 0: ssid = WIFI_SSID; password = WIFI_PASSWORD; break;
    case 1: ssid = WIFI_SSID_2; password = WIFI_PASSWORD_2; break;
    case 2: ssid = WIFI_SSID_3; password = WIFI_PASSWORD_3; break;
    case 3:
      if (!setupSavedSsid.isEmpty() && !setupSavedPassword.isEmpty()) {
        WiFi.begin(setupSavedSsid.c_str(), setupSavedPassword.c_str());
        return true;
      }
      return false;
    default: return false;
  }
  if (ssid[0] == '\0') return false;
  WiFi.begin(ssid, password);
  return true;
}

void diagnosePrimaryWifiScan() {
  const int count = WiFi.scanNetworks(false, true);
  bool primaryFound = false;
  int primaryChannel = 0;
  int primaryRssi = 0;
  for (int i = 0; i < count; ++i) {
    if (WiFi.SSID(i) == WIFI_SSID) {
      primaryFound = true;
      primaryChannel = WiFi.channel(i);
      primaryRssi = WiFi.RSSI(i);
      break;
    }
  }
  Serial.printf("WIFI_BOOT_SCAN total=%d primary_found=%d channel=%d rssi=%d\n", count, primaryFound, primaryChannel, primaryRssi);
  WiFi.scanDelete();
}

void startWifi() {
  // I při aktivním portálu zůstává stanice v režimu AP+STA a smí opakovat
  // uložené připojení. Portál skončí až po skutečném WL_CONNECTED.
  if (!setupPortalActive) screenVervaLogo();
  ESP_LOGI("LIT", "WIFI_CONNECT_CYCLE");
  Serial.println("WIFI_CONNECT_CYCLE");
  // WiFi.begin je neblokující. Slabému Vodafone-2g dáme tři pokusy,
  // pak pravidelně ověříme i další uložené sítě.
  for (uint8_t checked = 0; checked < 4; ++checked) {
    const uint8_t slot = WIFI_ATTEMPT_ORDER[wifiAttemptIndex++ % (sizeof(WIFI_ATTEMPT_ORDER) / sizeof(WIFI_ATTEMPT_ORDER[0]))];
    if (beginStoredWifi(slot)) {
      Serial.printf("WIFI_ATTEMPT_SLOT=%u\n", slot + 1);
      break;
    }
  }
  if (wifiConnectStarted == 0) wifiConnectStarted = millis();
  lastWifiAttempt = millis();
}

bool fetchAndDisplayMatch() {
  // HTTPS načítáme na pozadí; OLED se při běžném pollu nemaže textem „Načítám“.

  WiFiClientSecure client;
  client.setInsecure();  // C3 nema RTC; jinak nelze overit platnost retezce CA.
  client.setTimeout(30000);
  client.setHandshakeTimeout(30);
  HTTPClient http;
  http.setReuse(false);
  http.setConnectTimeout(30000);
  http.setTimeout(30000);
  http.useHTTP10(true);
  if (!http.begin(client, API_URL)) {
    Serial.println("HTTPS_START_SELHAL");
    return false;
  }

  Serial.printf("HTTPS_GET_START endpoint=%s\n", API_URL);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTPS_CODE=%d %s\n", code, http.errorToString(code).c_str());
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, http.getStream());
  http.end();
  if (error) {
    Serial.printf("JSON=%s\n", error.c_str());
    return false;
  }

  const String home = doc["home_code"] | "LIT";
  const String away = doc["away_code"] | "?";
  const String homeDisplay = doc["home_display"] | home;
  const String awayDisplay = doc["away_display"] | away;
  const String state = doc["state"] | "scheduled";
  // API čas je společný zdroj pro hlavičku ve všech stavech zápasu.
  const uint32_t payloadServerEpoch = doc["server_epoch"] | 0;
  if (payloadServerEpoch > 0) {
    serverEpoch = payloadServerEpoch;
    serverEpochMillis = millis();
  }
  const int payloadTablePosition = doc["table_position"] | 0;
  tablePosition = (payloadTablePosition >= 1 && payloadTablePosition <= 14)
      ? static_cast<uint8_t>(payloadTablePosition)
      : 0;

  if (state == "scheduled") {
    liveMode = false;
    audioEventBaselineKnown = false;
    previousAudioEventId = "";
    scheduledHome = homeDisplay;
    scheduledAway = awayDisplay;
    matchStartEpoch = doc["match_start_epoch"] | 0;
    serverEpoch = doc["server_epoch"] | 0;
    serverEpochMillis = millis();
    countdownActive = doc["is_match_day"] | false;
    const String scheduledIdentity = String(doc["match_id"] | "") + "|" + homeDisplay + "|" + awayDisplay + "|" + String(matchStartEpoch) + "|" + String(countdownActive);
    const bool scheduleChanged = scheduledIdentity != lastScheduledIdentity;
    lastScheduledIdentity = scheduledIdentity;
    if (scheduleChanged) shownCountdownSecond = UINT32_MAX;
    if (countdownActive && matchStartEpoch > serverEpoch) {
      refreshCountdown();
    } else {
      const String displayKey = String("scheduled|") + scheduledIdentity + "|" + String(doc["game_clock"] | "Termin neznamy");
      if (displayKey != lastRenderedPayloadKey) {
        screenBigScheduled(homeDisplay, awayDisplay, String(doc["game_clock"] | "Termin neznamy"));
        lastRenderedPayloadKey = displayKey;
      }
    }
  } else if (state == "live") {
    liveMode = true;
    countdownActive = false;
    const int scoreHome = doc["score_home"] | 0;
    const int scoreAway = doc["score_away"] | 0;
    String clock = doc["game_clock"] | "";
    const bool intermission = doc["intermission"] | false;
    intermissionActive = intermission;
    if (intermission) {
      intermissionUntilEpoch = doc["intermission_until_epoch"] | 0;
      intermissionServerEpoch = doc["server_epoch"] | 0;
      intermissionServerMillis = millis();
      clock = doc["intermission_note"] | "PRESTAVKA";
    } else {
      const String lastGoalCode = doc["last_goal_code"] | "";
      const String lastGoalScorer = doc["last_goal_scorer"] | "";
      if (!lastGoalScorer.isEmpty()) clock = lastGoalCode + " " + lastGoalScorer;
    }
    const String penaltyIndicator = doc["penalty_indicator"] | "";
    const String displayKey = String("live|") + String(doc["match_id"] | "") + "|" + home + "|" + away + "|" + String(scoreHome) + "|" + String(scoreAway) + "|" + clock + "|" + penaltyIndicator;
    if (displayKey != lastRenderedPayloadKey) {
      screenLive(home, away, scoreHome, scoreAway, clock, penaltyIndicator);
      lastRenderedPayloadKey = displayKey;
      Serial.println("OLED_LIVE_UPDATED");
    }
    const String eventId = doc["event_id"] | "";
    const String audioCue = doc["audio_cue"] | "";
    if (handleAudioCue(eventId, audioCue)) {
      screenLive(home, away, scoreHome, scoreAway, clock, penaltyIndicator);
    }
  } else {
    liveMode = false;
    countdownActive = false;
    audioEventBaselineKnown = false;
    previousAudioEventId = "";
    const String score = String((int)(doc["score_home"] | 0)) + ":" + String((int)(doc["score_away"] | 0));
    const String displayKey = String("finished|") + String(doc["match_id"] | "") + "|" + home + "|" + away + "|" + score;
    if (displayKey != lastRenderedPayloadKey) {
      screenFinished(home, away, (int)(doc["score_home"] | 0), (int)(doc["score_away"] | 0));
      lastRenderedPayloadKey = displayKey;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  // Čas z ověřeného server_epoch zobrazujeme v českém časovém pásmu.
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  ESP_LOGI("LIT", "BOOT_SETUP_START");
  Serial.println("BOOT_SETUP_START");
  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  ledcWriteTone(BUZZER_CHANNEL, 0);
  Serial.println("BOOT_I2C_START");
  Wire.begin(OLED_SDA, OLED_SCL);
  Serial.println("BOOT_OLED_START");
  const bool oledReady = oled.begin(0x3C, true);
  Serial.printf("BOOT_OLED_READY=%d\n", oledReady ? 1 : 0);
  // Lišta je první obraz po resetu a zůstává aktivní po celý běh.
  if (oledReady) {
    oled.clearDisplay();
    presentOled();
  }

  WiFi.persistent(false);
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  // Maximální povolený výkon; pomáhá, když AP nedostává autentizační rámce C3.
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setAutoReconnect(true);
  Serial.printf("BOOT_WIFI_TX_POWER=%d\n", WiFi.getTxPower());
  Serial.println("BOOT_WIFI_START");
  configureWifiNetworks();
  startWifi();
  Serial.println("BOOT_SETUP_DONE");
}

void loop() {
  static uint32_t lastDiag = 0;
  if (millis() - lastDiag >= 5000) {
    lastDiag = millis();
    ESP_LOGI("LIT", "HEARTBEAT wifi_status=%d ip=%s portal=%d", WiFi.status(), WiFi.localIP().toString().c_str(), setupPortalActive);
    Serial.printf("HEARTBEAT wifi_status=%d ip=%s portal=%d\n", WiFi.status(), WiFi.localIP().toString().c_str(), setupPortalActive);
  }
  // Překreslí pouze osmipixelovou Wi-Fi lištu nad stávajícím obsahem;
  // nezastaví polling, portál ani běžné obrazovky.
  if (millis() - lastWifiBarRefresh >= WIFI_BAR_REFRESH_MS) {
    lastWifiBarRefresh = millis();
    drawWifiStatusBar();
    oled.display();
  }
  if (WiFi.status() != WL_CONNECTED) {
    wifiWasConnected = false;
    if (setupPortalActive) {
      setupDns.processNextRequest();
      setupServer.handleClient();
      // Portál je fallback, nikoli konečný stav: dále zkoušíme uložené Wi-Fi.
      if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) startWifi();
      delay(10);
      return;
    }
    if (wifiConnectStarted != 0 && millis() - wifiConnectStarted >= SETUP_PORTAL_DELAY_MS) {
      startSetupPortal();
      return;
    }
    if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) startWifi();
    delay(100);
    return;
  }

  stopSetupPortal();
  wifiConnectStarted = 0;

  if (!wifiWasConnected) {
    wifiWasConnected = true;
    screen("Wi-Fi PRIPOJENA", WiFi.localIP().toString(), String(WiFi.RSSI()) + " dBm");
    Serial.printf("CONNECTED ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    delay(2500);
    lastPoll = 0; // Nacti zapas hned po zobrazeni uspesneho pripojeni.
  }

  refreshCountdown();

  // U naplanovaneho zapasu je datum/cas uz pevny. Cloud znovu dotazeme az
  // presne pri startu, kdy se muze zmenit stav na live a prijde online skore.
  if (countdownActive && matchStartEpoch > 0 && serverEpoch > 0) {
    const uint32_t now = serverEpoch + (millis() - serverEpochMillis) / 1000;
    if (now < matchStartEpoch) {
      delay(100);
      return;
    }
    countdownActive = false;
    lastPoll = 0;
  }

  if (intermissionActive && intermissionUntilEpoch > 0 && intermissionServerEpoch > 0) {
    const uint32_t now = intermissionServerEpoch + (millis() - intermissionServerMillis) / 1000;
    if (now < intermissionUntilEpoch) {
      delay(100);
      return;  // Během přestávky se cloud zbytečně nedotazuje.
    }
    intermissionActive = false;
    lastPoll = 0;
  }

  const uint32_t pollInterval = liveMode ? LIVE_POLL_MS : POLL_MS;
  if (millis() - lastPoll >= pollInterval) {
    lastPoll = millis();
    fetchAndDisplayMatch();
  }
  delay(100);
}
