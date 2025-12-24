#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <AsyncTCP.h>
  #include <ESPmDNS.h>
#else  // ESP8266
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
#endif
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <sntp.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include <queue>

#include "mfactoryfont.h"   // Custom font
#include "tz_lookup.h"      // Timezone lookup, do not duplicate mapping here!
#include "days_lookup.h"    // Languages for the Days of the Week
#include "months_lookup.h"  // Languages for the Months of the Year
#include "weather_lookup.h" // Languages for the Weather
#include "totp.h"           // TOTP

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#ifdef ESP32
  #define CLK_PIN 18
  #define DATA_PIN 23
  #define CS_PIN 2
#else  // ESP8266
  #define CLK_PIN 12  // D6
  #define DATA_PIN 15 // D8
  #define CS_PIN 13   // D7
#endif

#ifdef ESP8266
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
WiFiEventHandler wifiGotIPHandler;
#endif

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
AsyncWebServer server(80);

// --- Global Scroll Speed Settings ---
const int GENERAL_SCROLL_SPEED = 85;  // Default: Adjust this for Weather Description and Countdown Label (e.g., 50 for faster, 200 for slower)
const int IP_SCROLL_SPEED = 115;      // Default: Adjust this for the IP Address display (slower for readability)

// Auth
bool authEnabled = false;
char adminPassword[32] = "";
bool totpEnabled = false;
uint8_t totpSecret[20];
char totpSecretBase32[33] = "";
String sessionToken = "";
unsigned long sessionExpiry = 0;
SimpleTOTP totp;

// WiFi and configuration globals
char ssid[32] = "";
char password[64] = "";
char openMeteoLatitude[64] = "";
char openMeteoLongitude[64] = "";
char weatherUnits[12] = "metric";
char timeZone[64] = "";
char language[8] = "en";
unsigned long lastWifiConnectTime = 0;
const unsigned long WIFI_STABILIZE_DELAY = 5000;

// Timing and display settings
unsigned long clockDuration = 10000;
unsigned long weatherDuration = 5000;
bool displayOff = false;
int brightness = 7;
bool flipDisplay = false;
bool twelveHourToggle = false;
bool showDayOfWeek = true;
bool showDate = false;
bool showHumidity = false;
bool colonBlinkEnabled = true;
char ntpServer1[64] = "pool.ntp.org";
char ntpServer2[256] = "time.nist.gov";

// Dimming
bool dimmingEnabled = false;
bool displayOffByDimming = false;
bool displayOffByBrightness = false;
int dimStartHour = 18;  // 6pm default
int dimStartMinute = 0;
int dimEndHour = 8;  // 8am default
int dimEndMinute = 0;
int dimBrightness = 2;  // Dimming level (0-15)

// Countdown Globals - NEW
bool countdownEnabled = false;
time_t countdownTargetTimestamp = 0;  // Unix timestamp
char countdownLabel[64] = "";         // Label for the countdown
bool isDramaticCountdown = true;      // Default to the dramatic countdown mode

// State management
bool weatherCycleStarted = false;
WiFiClient client;
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Weather provider settings
enum WeatherProviderType {
  PROVIDER_OPEN_METEO,
  PROVIDER_OPEN_WEATHER,
  PROVIDER_PIRATE_WEATHER
};
WeatherProviderType weatherProvider = PROVIDER_OPEN_METEO;
char weatherApiKey[65] = "";

String currentTemp = "";
String weatherDescription = "";
bool showWeatherDescription = false;
bool weatherAvailable = false;
bool weatherFetched = false;
bool weatherFetchInitiated = false;
bool isAPMode = false;
char tempSymbol = '[';
bool shouldFetchWeatherNow = false;

// Weather fetching timing
unsigned long lastFetch = 0;
const unsigned long fetchInterval = 300000;  // 5 minutes

unsigned long lastSwitch = 0;
unsigned long lastColonBlink = 0;
int displayMode = 0;  // 0: Clock, 1: Weather, 2: Weather Description, 3: Countdown
int currentHumidity = -1;
bool ntpSyncSuccessful = false;

// NTP Synchronization State Machine
enum NtpState {
  NTP_IDLE,
  NTP_SYNCING,
  NTP_SUCCESS,
  NTP_FAILED
};
NtpState ntpState = NTP_IDLE;
unsigned long ntpStartTime = 0;
const int ntpTimeout = 30000;  // 30 seconds
const int maxNtpRetries = 30;
int ntpRetryCount = 0;
unsigned long lastNtpStatusPrintTime = 0;
const unsigned long ntpStatusPrintInterval = 1000;  // Print status every 1 seconds (adjust as needed)

// Non-blocking IP display globals
bool showingIp = false;
int ipDisplayCount = 0;
const int ipDisplayMax = 2;  // As per working copy for how long IP shows
String pendingIpToShow = "";

// Countdown display state - NEW
bool countdownScrolling = false;
unsigned long countdownScrollEndTime = 0;
unsigned long countdownStaticStartTime = 0;  // For last-day static display

// mDNS
bool mdnsEnabled = true;
char mdnsHostname[32] = "esptimecast";

// API
bool apiEnabled = false;
String customMessage = "";
bool showCustomMessage = false;
unsigned long customMessageStartTime = 0;
unsigned long customMessageDuration = 5000;
bool customMessageScrolling = false;
int customMessagePriority = 0;

// YouTube Settings
bool youtubeEnabled = false;
char youtubeApiKey[64] = "";
char youtubeChannelId[64] = "";
bool youtubeShortFormat = true;
String currentSubscriberCount = "";
unsigned long lastYoutubeFetch = 0;
const unsigned long youtubeFetchInterval = 1800000; // 30 minutes

// Webhook
bool webhooksEnabled = false;
char webhookKey[32] = "";
int webhookQueueSize = 5;
bool webhookQuietHours = true;

struct WebhookMessage {
  String text;
  int priority;  // 0=low, 1=normal, 2=high
  int duration;  // milliseconds
  bool scroll;
  unsigned long timestamp;
};

struct WebhookMessageCompare {
  bool operator()(const WebhookMessage& a, const WebhookMessage& b) {
    if (a.priority != b.priority) {
      return a.priority < b.priority;
    }
    return a.timestamp > b.timestamp;
  }
};

std::priority_queue<WebhookMessage, std::vector<WebhookMessage>, WebhookMessageCompare> messageQueue;
WebhookMessage currentWebhookMessage;
bool processingWebhook = false;

// --- NEW GLOBAL VARIABLES FOR IMMEDIATE COUNTDOWN FINISH ---
bool countdownFinished = false;                       // Tracks if the countdown has permanently finished
bool countdownShowFinishedMessage = false;            // Flag to indicate "TIMES UP" message is active
unsigned long countdownFinishedMessageStartTime = 0;  // Timer for the 10-second message duration
unsigned long lastFlashToggleTime = 0;                // For controlling the flashing speed
bool currentInvertState = false;                      // Current state of display inversion for flashing
static bool hourglassPlayed = false;

// Weather Description Mode handling
unsigned long descStartTime = 0;  // For static description
bool descScrolling = false;
const unsigned long descriptionDuration = 3000;    // 3s for short text
static unsigned long descScrollEndTime = 0;        // for post-scroll delay (re-used for scroll timing)
const unsigned long descriptionScrollPause = 300;  // 300ms pause after scroll

// --- Safe WiFi credential getters ---
const char *getSafeSsid() {
  return isAPMode ? "" : ssid;
}

const char *getSafePassword() {
  if (strlen(password) == 0) {  // No password set yet — return empty string for fresh install
    return "";
  } else {  // Password exists — mask it in the web UI
    return "********";
  }
}

const char *getSafeAdminPassword() {
  if (strlen(adminPassword) == 0) {
    return "";
  } else {
    return "********";
  }
}

const char *getSafeWeatherApiKey() {
  if (strlen(weatherApiKey) == 0) {
    return "";
  } else {
    return "********";
  }
}

const char *getSafeYoutubeApiKey() {
  if (strlen(youtubeApiKey) == 0) {
    return "";
  } else {
    return "********";
  }
}

const char *getSafeWebhookKey() {
  if (strlen(webhookKey) == 0) {
    return "";
  } else {
    return "********";
  }
}

void generateTOTPSecret() {
  for (int i = 0; i < 20; i++) {
    totpSecret[i] = random(0, 256);
  }

  base32_encode(totpSecret, 20, totpSecretBase32, sizeof(totpSecretBase32));

  int len = strlen(totpSecretBase32);
  while (len > 0 && totpSecretBase32[len-1] == '=') {
    totpSecretBase32[--len] = '\0';
  }

  totp.setSecret(totpSecret, 20);

  Serial.print(F("[AUTH] Generated TOTP Secret: "));
  Serial.println(totpSecretBase32);

  DynamicJsonDocument doc(2048);
  File configFile = LittleFS.open("/config.json", "r");
  if (configFile) {
    deserializeJson(doc, configFile);
    configFile.close();
  }

  doc["totpSecret"] = totpSecretBase32;

  File f = LittleFS.open("/config.json", "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
  }
}

bool checkAuth(AsyncWebServerRequest *request) {
  if (isAPMode) return true;
  if (!authEnabled) return true;

  if (request->hasHeader("X-Auth-Token")) {
    String token = request->getHeader("X-Auth-Token")->value();

    if (token == sessionToken && millis() < sessionExpiry) {
      sessionExpiry = millis() + (30 * 60 * 1000);
      return true;
    }
  }

  return false;
}

bool secureCompare(const char* a, const char* b) {
  size_t len = strlen(b);
  if (strlen(a) != len) return false;

  uint8_t result = 0;
  for (size_t i = 0; i < len; i++) {
    result |= a[i] ^ b[i];
  }
  return result == 0;
}

void base32_encode(const uint8_t *data, int length, char *result, int resultSize) {
  const char *base32_alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  int count = 0;
  int buffer = 0;
  int bitsLeft = 0;

  for (int i = 0; i < length; i++) {
    buffer <<= 8;
    buffer |= data[i];
    bitsLeft += 8;

    while (bitsLeft >= 5) {
      if (count >= resultSize - 1) break;
      int index = (buffer >> (bitsLeft - 5)) & 0x1F;
      result[count++] = base32_alphabet[index];
      bitsLeft -= 5;
    }
  }

  if (bitsLeft > 0 && count < resultSize - 1) {
    int index = (buffer << (5 - bitsLeft)) & 0x1F;
    result[count++] = base32_alphabet[index];
  }

  result[count] = '\0';
}

void base32_decode(const char *encoded, uint8_t *result, int resultSize) {
  int buffer = 0;
  int bitsLeft = 0;
  int count = 0;

  for (int i = 0; encoded[i] && count < resultSize; i++) {
    char ch = encoded[i];

    if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t' || ch == '=') continue;

    int val = -1;
    if (ch >= 'A' && ch <= 'Z') {
      val = ch - 'A';
    } else if (ch >= 'a' && ch <= 'z') {
      val = ch - 'a';
    } else if (ch >= '2' && ch <= '7') {
      val = ch - '2' + 26;
    }

    if (val == -1) continue;

    buffer <<= 5;
    buffer |= val;
    bitsLeft += 5;

    if (bitsLeft >= 8) {
      result[count++] = (buffer >> (bitsLeft - 8)) & 0xFF;
      bitsLeft -= 8;
    }
  }
}

// Scroll flipped
textEffect_t getEffectiveScrollDirection(textEffect_t desiredDirection, bool isFlipped) {
  if (isFlipped) {
    // If the display is horizontally flipped, reverse the horizontal scroll direction
    if (desiredDirection == PA_SCROLL_LEFT) {
      return PA_SCROLL_RIGHT;
    } else if (desiredDirection == PA_SCROLL_RIGHT) {
      return PA_SCROLL_LEFT;
    }
  }
  return desiredDirection;
}

// Normalize text for LED display
String normalizeDisplayText(String str, bool weatherMode = false) {
  // Serbian Cyrillic → Latin
  str.replace("а", "a");
  str.replace("б", "b");
  str.replace("в", "v");
  str.replace("г", "g");
  str.replace("д", "d");
  str.replace("ђ", "dj");
  str.replace("е", "e");
  str.replace("ё", "e");  // Russian
  str.replace("ж", "z");
  str.replace("з", "z");
  str.replace("и", "i");
  str.replace("й", "j");  // Russian
  str.replace("ј", "j");  // Serbian
  str.replace("к", "k");
  str.replace("л", "l");
  str.replace("љ", "lj");
  str.replace("м", "m");
  str.replace("н", "n");
  str.replace("њ", "nj");
  str.replace("о", "o");
  str.replace("п", "p");
  str.replace("р", "r");
  str.replace("с", "s");
  str.replace("т", "t");
  str.replace("ћ", "c");
  str.replace("у", "u");
  str.replace("ф", "f");
  str.replace("х", "h");
  str.replace("ц", "c");
  str.replace("ч", "c");
  str.replace("џ", "dz");
  str.replace("ш", "s");
  str.replace("щ", "sh");  // Russian
  str.replace("ы", "y");   // Russian
  str.replace("э", "e");   // Russian
  str.replace("ю", "yu");  // Russian
  str.replace("я", "ya");  // Russian

  // Latin diacritics → ASCII
  str.replace("å", "a");
  str.replace("ä", "a");
  str.replace("à", "a");
  str.replace("á", "a");
  str.replace("â", "a");
  str.replace("ã", "a");
  str.replace("ā", "a");
  str.replace("ă", "a");
  str.replace("ą", "a");

  str.replace("æ", "ae");

  str.replace("ç", "c");
  str.replace("č", "c");
  str.replace("ć", "c");

  str.replace("ď", "d");

  str.replace("é", "e");
  str.replace("è", "e");
  str.replace("ê", "e");
  str.replace("ë", "e");
  str.replace("ē", "e");
  str.replace("ė", "e");
  str.replace("ę", "e");

  str.replace("ğ", "g");
  str.replace("ģ", "g");

  str.replace("ĥ", "h");

  str.replace("í", "i");
  str.replace("ì", "i");
  str.replace("î", "i");
  str.replace("ï", "i");
  str.replace("ī", "i");
  str.replace("į", "i");

  str.replace("ĵ", "j");

  str.replace("ķ", "k");

  str.replace("ľ", "l");
  str.replace("ł", "l");

  str.replace("ñ", "n");
  str.replace("ń", "n");
  str.replace("ņ", "n");

  str.replace("ó", "o");
  str.replace("ò", "o");
  str.replace("ô", "o");
  str.replace("ö", "o");
  str.replace("õ", "o");
  str.replace("ø", "o");
  str.replace("ō", "o");
  str.replace("ő", "o");

  str.replace("œ", "oe");

  str.replace("ŕ", "r");

  str.replace("ś", "s");
  str.replace("š", "s");
  str.replace("ș", "s");
  str.replace("ŝ", "s");

  str.replace("ß", "ss");

  str.replace("ť", "t");
  str.replace("ț", "t");

  str.replace("ú", "u");
  str.replace("ù", "u");
  str.replace("û", "u");
  str.replace("ü", "u");
  str.replace("ū", "u");
  str.replace("ů", "u");
  str.replace("ű", "u");

  str.replace("ŵ", "w");

  str.replace("ý", "y");
  str.replace("ÿ", "y");
  str.replace("ŷ", "y");

  str.replace("ž", "z");
  str.replace("ź", "z");
  str.replace("ż", "z");

  str.toUpperCase();

  String result = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);

    if (weatherMode) {
      if ((c >= 'A' && c <= 'Z') || c == ' ') {
        result += c;
      }
    }

    else {
      if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
          c == ' ' || c == '.' || c == ',' || c == '!' ||
          c == ':' || c == '-' || c == '_' || c == '%') {
        result += c;
      }
    }
  }
  return result;
}

// -----------------------------------------------------------------------------
// Configuration Load & Save
// -----------------------------------------------------------------------------
void loadConfig() {
  Serial.println(F("[CONFIG] Loading configuration..."));

  // Check if config.json exists, if not, create default
  if (!LittleFS.exists("/config.json")) {
    Serial.println(F("[CONFIG] config.json not found, creating with defaults..."));
    DynamicJsonDocument doc(1024);
    doc[F("ssid")] = "";
    doc[F("password")] = "";
    doc[F("authEnabled")] = false;
    doc[F("adminPassword")] = "";
    doc[F("totpEnabled")] = false;
    doc[F("totpSecret")] = "";
    doc[F("openMeteoLatitude")] = "";
    doc[F("openMeteoLongitude")] = "";
    doc[F("weatherUnits")] = "metric";
    doc[F("clockDuration")] = 10000;
    doc[F("weatherDuration")] = 5000;
    doc[F("weatherProvider")] = "openmeteo";
    doc[F("weatherApiKey")] = "";
    doc[F("timeZone")] = "";
    doc[F("language")] = "en";
    doc[F("brightness")] = brightness;
    doc[F("flipDisplay")] = flipDisplay;
    doc[F("twelveHourToggle")] = twelveHourToggle;
    doc[F("showDayOfWeek")] = showDayOfWeek;
    doc[F("showDate")] = false;
    doc[F("showHumidity")] = showHumidity;
    doc[F("colonBlinkEnabled")] = colonBlinkEnabled;
    doc[F("ntpServer1")] = ntpServer1;
    doc[F("ntpServer2")] = ntpServer2;
    doc[F("dimmingEnabled")] = dimmingEnabled;
    doc[F("dimStartHour")] = dimStartHour;
    doc[F("dimStartMinute")] = dimStartMinute;
    doc[F("dimEndHour")] = dimEndHour;
    doc[F("dimEndMinute")] = dimEndMinute;
    doc[F("dimBrightness")] = dimBrightness;
    doc[F("showWeatherDescription")] = showWeatherDescription;
    doc[F("apiEnabled")] = false;
    doc[F("webhooksEnabled")] = false;
    doc[F("webhookKey")] = "";
    doc[F("webhookQueueSize")] = 5;
    doc[F("webhookQuietHours")] = true;

    // Add countdown defaults when creating a new config.json
    JsonObject countdownObj = doc.createNestedObject("countdown");
    countdownObj["enabled"] = false;
    countdownObj["targetTimestamp"] = 0;
    countdownObj["label"] = "";
    countdownObj["isDramaticCountdown"] = true;

    File f = LittleFS.open("/config.json", "w");
    if (f) {
      serializeJsonPretty(doc, f);
      f.close();
      Serial.println(F("[CONFIG] Default config.json created."));
    } else {
      Serial.println(F("[ERROR] Failed to create default config.json"));
    }
  }

  Serial.println(F("[CONFIG] Attempting to open config.json for reading."));
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println(F("[ERROR] Failed to open config.json for reading. Cannot load config."));
    return;
  }

  DynamicJsonDocument doc(1024);  // Size based on ArduinoJson Assistant + buffer
  DeserializationError error = deserializeJson(doc, configFile);
  configFile.close();

  if (error) {
    Serial.print(F("[ERROR] JSON parse failed during load: "));
    Serial.println(error.f_str());
    return;
  }

  strlcpy(ssid, doc["ssid"] | "", sizeof(ssid));
  strlcpy(password, doc["password"] | "", sizeof(password));

  // Auth loading
  authEnabled = doc["authEnabled"] | false;
  strlcpy(adminPassword, doc["adminPassword"] | "", sizeof(adminPassword));
  totpEnabled = doc["totpEnabled"] | false;
  strlcpy(totpSecretBase32, doc["totpSecret"] | "", sizeof(totpSecretBase32));

  if (totpEnabled && strlen(totpSecretBase32) > 0) {
    time_t now = time(nullptr);

    int len = strlen(totpSecretBase32);
    while (len > 0 && totpSecretBase32[len-1] == '=') {
      totpSecretBase32[--len] = '\0';
    }

    base32_decode(totpSecretBase32, totpSecret, sizeof(totpSecret));
    totp.setSecret(totpSecret, 20);

    Serial.println(F("[AUTH] TOTP secret loaded from config"));
  }

  strlcpy(openMeteoLatitude, doc["openMeteoLatitude"] | "", sizeof(openMeteoLatitude));
  strlcpy(openMeteoLongitude, doc["openMeteoLongitude"] | "", sizeof(openMeteoLongitude));
  strlcpy(weatherUnits, doc["weatherUnits"] | "metric", sizeof(weatherUnits));
  clockDuration = doc["clockDuration"] | 10000;
  weatherDuration = doc["weatherDuration"] | 5000;
  strlcpy(timeZone, doc["timeZone"] | "Etc/UTC", sizeof(timeZone));
  if (doc.containsKey("language")) {
    strlcpy(language, doc["language"], sizeof(language));
  } else {
    strlcpy(language, "en", sizeof(language));
    Serial.println(F("[CONFIG] 'language' key not found in config.json, defaulting to 'en'."));
  }

  brightness = doc["brightness"] | 7;
  flipDisplay = doc["flipDisplay"] | false;
  twelveHourToggle = doc["twelveHourToggle"] | false;
  showDayOfWeek = doc["showDayOfWeek"] | true;
  showDate = doc["showDate"] | false;
  showHumidity = doc["showHumidity"] | false;
  colonBlinkEnabled = doc.containsKey("colonBlinkEnabled") ? doc["colonBlinkEnabled"].as<bool>() : true;
  showWeatherDescription = doc["showWeatherDescription"] | false;
  mdnsEnabled = doc["mdnsEnabled"] | true;
  strlcpy(mdnsHostname, doc["mdnsHostname"] | "esptimecast", sizeof(mdnsHostname));
  apiEnabled = doc["apiEnabled"] | false;
  webhooksEnabled = doc["webhooksEnabled"] | false;
  strlcpy(webhookKey, doc["webhookKey"] | "", sizeof(webhookKey));
  webhookQueueSize = doc["webhookQueueSize"] | 5;
  webhookQuietHours = doc["webhookQuietHours"] | true;

  String de = doc["dimmingEnabled"].as<String>();
  dimmingEnabled = (de == "true" || de == "on" || de == "1");

  dimStartHour = doc["dimStartHour"] | 18;
  dimStartMinute = doc["dimStartMinute"] | 0;
  dimEndHour = doc["dimEndHour"] | 8;
  dimEndMinute = doc["dimEndMinute"] | 0;
  dimBrightness = doc["dimBrightness"] | 0;

  strlcpy(ntpServer1, doc["ntpServer1"] | "pool.ntp.org", sizeof(ntpServer1));
  strlcpy(ntpServer2, doc["ntpServer2"] | "time.nist.gov", sizeof(ntpServer2));

  // Weather provider
  String provider = doc["weatherProvider"] | "openmeteo";
  if (provider == "openweather") {
    weatherProvider = PROVIDER_OPEN_WEATHER;
  } else if (provider == "pirateweather") {
    weatherProvider = PROVIDER_PIRATE_WEATHER;
  } else {
    weatherProvider = PROVIDER_OPEN_METEO;
  }
  strlcpy(weatherApiKey, doc["weatherApiKey"] | "", sizeof(weatherApiKey));

  if (strcmp(weatherUnits, "imperial") == 0)
    tempSymbol = ']';
  else
    tempSymbol = '[';


  // --- COUNTDOWN CONFIG LOADING ---
  if (doc.containsKey("countdown")) {
    JsonObject countdownObj = doc["countdown"];

    countdownEnabled = countdownObj["enabled"] | false;
    countdownTargetTimestamp = countdownObj["targetTimestamp"] | 0;
    isDramaticCountdown = countdownObj["isDramaticCountdown"] | true;

    JsonVariant labelVariant = countdownObj["label"];
    if (labelVariant.isNull() || !labelVariant.is<const char *>()) {
      strcpy(countdownLabel, "");
    } else {
      const char *labelTemp = labelVariant.as<const char *>();
      size_t labelLen = strlen(labelTemp);
      if (labelLen >= sizeof(countdownLabel)) {
        Serial.println(F("[CONFIG] label from JSON too long, truncating."));
      }
      strlcpy(countdownLabel, labelTemp, sizeof(countdownLabel));
    }
    countdownFinished = false;
  } else {
    countdownEnabled = false;
    countdownTargetTimestamp = 0;
    strcpy(countdownLabel, "");
    isDramaticCountdown = true;
    Serial.println(F("[CONFIG] Countdown object not found, defaulting to disabled."));
    countdownFinished = false;
  }

  if (doc.containsKey("youtube")) {
    JsonObject youtubeObj = doc["youtube"];
    youtubeEnabled = youtubeObj["enabled"] | false;
    strlcpy(youtubeApiKey, youtubeObj["apiKey"] | "", sizeof(youtubeApiKey));
    strlcpy(youtubeChannelId, youtubeObj["channelId"] | "", sizeof(youtubeChannelId));
    youtubeShortFormat = youtubeObj["shortFormat"] | true;
  } else {
    youtubeEnabled = false;
    youtubeApiKey[0] = '\0';
    youtubeChannelId[0] = '\0';
    youtubeShortFormat = true;
    Serial.println(F("[CONFIG] YouTube object not found, defaulting to disabled."));
  }

  Serial.println(F("[CONFIG] Configuration loaded."));
}



// -----------------------------------------------------------------------------
// WiFi Setup
// -----------------------------------------------------------------------------
const char *DEFAULT_AP_PASSWORD = "12345678";
const char *AP_SSID = "ESPTimeCast";

void connectWiFi() {
  Serial.println(F("[WIFI] Connecting to WiFi..."));

  bool credentialsExist = (strlen(ssid) > 0);

  if (!credentialsExist) {
    Serial.println(F("[WIFI] No saved credentials. Starting AP mode directly."));
    WiFi.mode(WIFI_AP);
    WiFi.disconnect(true);
    delay(100);

    if (strlen(DEFAULT_AP_PASSWORD) < 8) {
      WiFi.softAP(AP_SSID);
      Serial.println(F("[WIFI] AP Mode started (no password, too short)."));
    } else {
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      Serial.println(F("[WIFI] AP Mode started."));
    }

    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    Serial.print(F("[WIFI] AP IP address: "));
    Serial.println(WiFi.softAPIP());
    isAPMode = true;

    WiFiMode_t mode = WiFi.getMode();
    Serial.printf("[WIFI] WiFi mode after setting AP: %s\n",
                  mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                           : mode == WIFI_AP     ? "AP ONLY"
                                           : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                 : "UNKNOWN");

    Serial.println(F("[WIFI] AP Mode Started"));
    return;
  }

  // If credentials exist, attempt STA connection
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();

  const unsigned long timeout = 30000;
  unsigned long animTimer = 0;
  int animFrame = 0;
  bool animating = true;

  while (animating) {
    unsigned long now = millis();
    if (WiFi.status() == WL_CONNECTED) {
      #ifdef ESP32
        Serial.println("[WIFI] Connected: " + WiFi.localIP().toString());
      #else  // ESP8266
        Serial.println(F("[WIFI] Connected: ") + WiFi.localIP().toString());
      #endif
      isAPMode = false;

      WiFiMode_t mode = WiFi.getMode();
      Serial.printf("[WIFI] WiFi mode after STA connection: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                             : mode == WIFI_AP     ? "AP ONLY"
                                             : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                   : "UNKNOWN");

      // --- IP Display initiation ---
      pendingIpToShow = WiFi.localIP().toString();
      showingIp = true;
      ipDisplayCount = 0;  // Reset count for IP display
      P.displayClear();
      P.setCharSpacing(1);  // Set spacing for IP scroll
      textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
      P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, IP_SCROLL_SPEED);
      // --- END IP Display initiation ---

      animating = false;  // Exit the connection loop
      break;
    } else if (now - startAttemptTime >= timeout) {
      Serial.println(F("[WiFi] Failed. Starting AP mode..."));
      WiFi.mode(WIFI_AP);
      WiFi.softAP(AP_SSID, DEFAULT_AP_PASSWORD);
      Serial.print(F("[WiFi] AP IP address: "));
      Serial.println(WiFi.softAPIP());
      dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
      isAPMode = true;

      #ifdef ESP32
        auto mode = WiFi.getMode();
      #else  // ESP8266
        WiFiMode_t mode = WiFi.getMode();
      #endif
      Serial.printf("[WIFI] WiFi mode after STA failure and setting AP: %s\n",
                    mode == WIFI_OFF ? "OFF" : mode == WIFI_STA    ? "STA ONLY"
                                             : mode == WIFI_AP     ? "AP ONLY"
                                             : mode == WIFI_AP_STA ? "AP + STA (Error!)"
                                                                   : "UNKNOWN");

      animating = false;
      Serial.println(F("[WIFI] AP Mode Started"));
      break;
    }
    if (now - animTimer > 750) {
      animTimer = now;
      P.setTextAlignment(PA_CENTER);
      switch (animFrame % 3) {
        case 0: P.print(F("# ©")); break;
        case 1: P.print(F("# ª")); break;
        case 2: P.print(F("# «")); break;
      }
      animFrame++;
    }
    #ifdef ESP32
      delay(1);
    #else // ESP8266
      yield();
    #endif
  }
}

void clearWiFiCredentialsInConfig() {
  DynamicJsonDocument doc(2048);

  // Open existing config, if present
  File configFile = LittleFS.open("/config.json", "r");
  if (configFile) {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err) {
      Serial.print(F("[SECURITY] Error parsing config.json: "));
      Serial.println(err.f_str());
      return;
    }
  }

  doc["ssid"] = "";
  doc["password"] = "";

  // Optionally backup previous config
  if (LittleFS.exists("/config.json")) {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) {
    Serial.println(F("[SECURITY] ERROR: Cannot write to /config.json to clear credentials!"));
    return;
  }
  serializeJson(doc, f);
  f.close();
  Serial.println(F("[SECURITY] Cleared WiFi credentials in config.json."));
}

// -----------------------------------------------------------------------------
// Time / NTP Functions
// -----------------------------------------------------------------------------
void setupTime() {
  #ifdef ESP8266
    sntp_stop();  // ESP8266 needs explicit stop before reconfiguring
  #endif
  if (!isAPMode) {
    Serial.println(F("[TIME] Starting NTP sync"));
  }

  bool serverOk = false;
  IPAddress resolvedIP;

  // Try first server if it's not empty
  if (strlen(ntpServer1) > 0 && WiFi.hostByName(ntpServer1, resolvedIP) == 1) {
    serverOk = true;
  }
  // Try second server if first failed
  else if (strlen(ntpServer2) > 0 && WiFi.hostByName(ntpServer2, resolvedIP) == 1) {
    serverOk = true;
  }

  if (serverOk) {
    configTime(0, 0, ntpServer1, ntpServer2);  // safe to call now
    setenv("TZ", ianaToPosix(timeZone), 1);
    tzset();
    ntpState = NTP_SYNCING;
    ntpStartTime = millis();
    ntpRetryCount = 0;
    ntpSyncSuccessful = false;
  } else {
    Serial.println(F("[TIME] NTP server lookup failed — retry sync in 30 seconds"));
    ntpSyncSuccessful = false;
    ntpState = NTP_SYNCING;   // instead of NTP_IDLE
    ntpStartTime = millis();  // start the failed timer (so retry delay counts from now)
  }
}

// -----------------------------------------------------------------------------
// mDNS
// -----------------------------------------------------------------------------
void setupMDNS() {
  if (!mdnsEnabled) {
    Serial.println(F("[MDNS] Disabled by config"));
    return;
  }
  if (MDNS.begin(mdnsHostname)) {
    Serial.printf("[MDNS] Started: http://%s.local\n", mdnsHostname);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println(F("[MDNS] Failed to start"));
  }
}

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------
void printConfigToSerial() {
  Serial.println(F("========= Loaded Configuration ========="));
  Serial.print(F("WiFi SSID: "));
  Serial.println(ssid);
  Serial.print(F("WiFi Password: "));
  Serial.println(password);
  Serial.print(F("Latitude: "));
  Serial.println(openMeteoLatitude);
  Serial.print(F("Longitude: "));
  Serial.println(openMeteoLongitude);
  Serial.print(F("Temperature Unit: "));
  Serial.println(weatherUnits);
  Serial.print(F("Clock duration: "));
  Serial.println(clockDuration);
  Serial.print(F("Weather duration: "));
  Serial.println(weatherDuration);
  Serial.print(F("TimeZone (IANA): "));
  Serial.println(timeZone);
  Serial.print(F("Days of the Week/Weather description language: "));
  Serial.println(language);
  Serial.print(F("Brightness: "));
  Serial.println(brightness);
  Serial.print(F("Flip Display: "));
  Serial.println(flipDisplay ? "Yes" : "No");
  Serial.print(F("Show 12h Clock: "));
  Serial.println(twelveHourToggle ? "Yes" : "No");
  Serial.print(F("Show Day of the Week: "));
  Serial.println(showDayOfWeek ? "Yes" : "No");
  Serial.print(F("Show Date: "));
  Serial.println(showDate ? "Yes" : "No");
  Serial.print(F("Show Weather Description: "));
  Serial.println(showWeatherDescription ? "Yes" : "No");
  Serial.print(F("Show Humidity: "));
  Serial.println(showHumidity ? "Yes" : "No");
  Serial.print(F("Blinking colon: "));
  Serial.println(colonBlinkEnabled ? "Yes" : "No");
  Serial.print(F("NTP Server 1: "));
  Serial.println(ntpServer1);
  Serial.print(F("NTP Server 2: "));
  Serial.println(ntpServer2);
  Serial.print(F("Dimming Enabled: "));
  Serial.println(dimmingEnabled);
  Serial.print(F("Dimming Start Hour: "));
  Serial.println(dimStartHour);
  Serial.print(F("Dimming Start Minute: "));
  Serial.println(dimStartMinute);
  Serial.print(F("Dimming End Hour: "));
  Serial.println(dimEndHour);
  Serial.print(F("Dimming End Minute: "));
  Serial.println(dimEndMinute);
  Serial.print(F("Dimming Brightness: "));
  Serial.println(dimBrightness);
  Serial.print(F("Countdown Enabled: "));
  Serial.println(countdownEnabled ? "Yes" : "No");
  Serial.print(F("Countdown Target Timestamp: "));
  Serial.println(countdownTargetTimestamp);
  Serial.print(F("Countdown Label: "));
  Serial.println(countdownLabel);
  Serial.print(F("Dramatic Countdown Display: "));
  Serial.println(isDramaticCountdown ? "Yes" : "No");
  Serial.print(F("API Enabled: "));
  Serial.println(apiEnabled ? "Yes" : "No");
  Serial.print(F("Webhooks Enabled: "));
  Serial.println(webhooksEnabled ? "Yes" : "No");
  Serial.print(F("Webhook Key: "));
  Serial.println(webhookKey);
  Serial.print(F("Webhook Queue Size: "));
  Serial.println(webhookQueueSize);
  Serial.print(F("Webhook Quiet Hours: "));
  Serial.println(webhookQuietHours ? "Yes" : "No");
  Serial.print(F("YouTube Enabled: "));
  Serial.println(youtubeEnabled ? "Yes" : "No");
  Serial.print(F("YouTube Channel ID: "));
  Serial.println(youtubeChannelId);
  Serial.print(F("YouTube Short Format: "));
  Serial.println(youtubeShortFormat ? "Yes" : "No");
  Serial.print(F("Auth Enabled: "));
  Serial.println(authEnabled ? "Yes" : "No");
  Serial.print(F("2FA Enabled: "));
  Serial.println(totpEnabled ? "Yes" : "No");
  Serial.println(F("========================================"));
  Serial.println();
}

// -----------------------------------------------------------------------------
// Web Server and Captive Portal
// -----------------------------------------------------------------------------
void handleCaptivePortal(AsyncWebServerRequest *request);

void setupWebServer() {
  Serial.println(F("[WEBSERVER] Setting up web server..."));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /"));
    request->send(LittleFS, "/index.html", "text/html");
  });

  server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/css/style.css", "text/css");
    response->addHeader("Cache-Control", "public, max-age=31536000");
    request->send(response);
  });

  server.on("/js/app.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/js/app.js", "application/javascript");
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
  });

  server.on("/js/login.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/js/login.js", "application/javascript");
    response->addHeader("Cache-Control", "public, max-age=604800");
    request->send(response);
  });

  server.on("/js/qrcode.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/js/qrcode.min.js.gz", "application/javascript");
    response->addHeader("Content-Encoding", "gzip");
    response->addHeader("Cache-Control", "public, max-age=31536000");
    request->send(response);
  });

  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /config.json"));
    File f = LittleFS.open("/config.json", "r");
    if (!f) {
      Serial.println(F("[WEBSERVER] Error opening /config.json"));
      request->send(500, "application/json", "{\"error\":\"Failed to open config.json\"}");
      return;
    }
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
      Serial.print(F("[WEBSERVER] Error parsing /config.json: "));
      Serial.println(err.f_str());
      request->send(500, "application/json", "{\"error\":\"Failed to parse config.json\"}");
      return;
    }

    if (!isAPMode && authEnabled && !checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
      return;
    }

    // Always sanitize before sending to browser
    doc[F("ssid")] = getSafeSsid();
    doc[F("password")] = getSafePassword();
    doc["adminPassword"] = getSafeAdminPassword();
    doc["weatherApiKey"] = getSafeWeatherApiKey();
    doc[F("mode")] = isAPMode ? "ap" : "sta";

    if (doc["youtube"]) doc["youtube"]["apiKey"] = getSafeYoutubeApiKey();
    doc["webhookKey"] = getSafeWebhookKey();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /save"));
    DynamicJsonDocument doc(2048);

    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      Serial.println(F("[WEBSERVER] Existing config.json found, loading for update..."));
      DeserializationError err = deserializeJson(doc, configFile);
      configFile.close();
      if (err) {
        Serial.print(F("[WEBSERVER] Error parsing existing config.json: "));
        Serial.println(err.f_str());
      }
    } else {
      Serial.println(F("[WEBSERVER] config.json not found, starting with empty doc for save."));
    }

    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
      return;
    }

    for (int i = 0; i < request->params(); i++) {
      const AsyncWebParameter *p = request->getParam(i);
      String n = p->name();
      String v = p->value();

      if (n == "brightness") doc[n] = v.toInt();
      else if (n == "clockDuration") doc[n] = v.toInt();
      else if (n == "weatherDuration") doc[n] = v.toInt();
      else if (n == "flipDisplay") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "twelveHourToggle") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showDayOfWeek") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showDate") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "showHumidity") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "colonBlinkEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "dimStartHour") doc[n] = v.toInt();
      else if (n == "dimStartMinute") doc[n] = v.toInt();
      else if (n == "dimEndHour") doc[n] = v.toInt();
      else if (n == "dimEndMinute") doc[n] = v.toInt();
      else if (n == "dimBrightness") doc[n] = v.toInt();
      else if (n == "showWeatherDescription") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "dimmingEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "weatherUnits") doc[n] = v;
      else if (n == "mdnsEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "mdnsHostname") {
        if (v.length() > 0 && v.length() < 32) {
          doc[n] = v;
        }
      }
      else if (n == "weatherProvider") doc[n] = v;
      else if (n == "weatherApiKey") {
      if (v != "********" && v.length() > 0) {
        doc[n] = v;
        }
      }
      else if (n == "apiEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "webhooksEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "webhookKey") {
        if (v != "********" && v.length() > 0) {
          doc[n] = v;
        } else if (v != "********") {
          doc[n] = "";
        }
      }
      else if (n == "webhookQueueSize") doc[n] = v.toInt();
      else if (n == "webhookQuietHours") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "authEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "adminPassword") {
        if (v != "********" && v.length() > 0) {
          doc[n] = v;
        }
      }
      else if (n == "totpEnabled") doc[n] = (v == "true" || v == "on" || v == "1");
      else if (n == "totpSecret") doc[n] = totpSecretBase32;

      else if (n == "password") {
        if (v != "********" && v.length() > 0) {
          doc[n] = v;  // user entered a new password
        } else {
          Serial.println(F("[SAVE] Password unchanged."));
          // do nothing, keep the one already in doc
        }
      }

      else {
        doc[n] = v;
      }
    }

    bool newCountdownEnabled = (request->hasParam("countdownEnabled", true) && (request->getParam("countdownEnabled", true)->value() == "true" || request->getParam("countdownEnabled", true)->value() == "on" || request->getParam("countdownEnabled", true)->value() == "1"));
    String countdownDateStr = request->hasParam("countdownDate", true) ? request->getParam("countdownDate", true)->value() : "";
    String countdownTimeStr = request->hasParam("countdownTime", true) ? request->getParam("countdownTime", true)->value() : "";
    String countdownLabelStr = request->hasParam("countdownLabel", true) ? request->getParam("countdownLabel", true)->value() : "";
    bool newIsDramaticCountdown = (request->hasParam("isDramaticCountdown", true) && (request->getParam("isDramaticCountdown", true)->value() == "true" || request->getParam("isDramaticCountdown", true)->value() == "on" || request->getParam("isDramaticCountdown", true)->value() == "1"));

    time_t newTargetTimestamp = 0;
    if (newCountdownEnabled && countdownDateStr.length() > 0 && countdownTimeStr.length() > 0) {
      int year = countdownDateStr.substring(0, 4).toInt();
      int month = countdownDateStr.substring(5, 7).toInt();
      int day = countdownDateStr.substring(8, 10).toInt();
      int hour = countdownTimeStr.substring(0, 2).toInt();
      int minute = countdownTimeStr.substring(3, 5).toInt();

      struct tm tm;
      tm.tm_year = year - 1900;
      tm.tm_mon = month - 1;
      tm.tm_mday = day;
      tm.tm_hour = hour;
      tm.tm_min = minute;
      tm.tm_sec = 0;
      tm.tm_isdst = -1;

      newTargetTimestamp = mktime(&tm);
      if (newTargetTimestamp == (time_t)-1) {
        Serial.println("[SAVE] Error converting countdown date/time to timestamp.");
        newTargetTimestamp = 0;
      } else {
        Serial.printf("[SAVE] Converted countdown target: %s -> %lu\n", countdownDateStr.c_str(), newTargetTimestamp);
      }
    }

    JsonObject countdownObj = doc.createNestedObject("countdown");
    countdownObj["enabled"] = newCountdownEnabled;
    countdownObj["targetTimestamp"] = newTargetTimestamp;
    countdownObj["label"] = countdownLabelStr;
    countdownObj["isDramaticCountdown"] = newIsDramaticCountdown;

    bool newYoutubeEnabled = (request->hasParam("youtubeEnabled", true) &&
                              (request->getParam("youtubeEnabled", true)->value() == "true" ||
                               request->getParam("youtubeEnabled", true)->value() == "on" ||
                               request->getParam("youtubeEnabled", true)->value() == "1"));
    String youtubeApiKeyStr = request->hasParam("youtubeApiKey", true) ?
                              request->getParam("youtubeApiKey", true)->value() : "";
    String youtubeChannelIdStr = request->hasParam("youtubeChannelId", true) ?
                                 request->getParam("youtubeChannelId", true)->value() : "";
    bool newYoutubeShortFormat = (request->hasParam("youtubeShortFormat", true) &&
                                  (request->getParam("youtubeShortFormat", true)->value() == "true" ||
                                  request->getParam("youtubeShortFormat", true)->value() == "on" ||
                                  request->getParam("youtubeShortFormat", true)->value() == "1"));

    if (youtubeApiKeyStr == "********" && doc.containsKey("youtube")) {
      youtubeApiKeyStr = doc["youtube"]["apiKey"] | "";
    }

    JsonObject youtubeObj = doc.createNestedObject("youtube");
    youtubeObj["enabled"] = newYoutubeEnabled;
    youtubeObj["apiKey"] = youtubeApiKeyStr;
    youtubeObj["channelId"] = youtubeChannelIdStr;
    youtubeObj["shortFormat"] = newYoutubeShortFormat;

    #ifdef ESP32
      size_t total = LittleFS.totalBytes();
      size_t used = LittleFS.usedBytes();
      Serial.printf("[SAVE] LittleFS total bytes: %llu, used bytes: %llu\n", LittleFS.totalBytes(), LittleFS.usedBytes());
    #else  // ESP8266
      FSInfo fs_info;
      LittleFS.info(fs_info);
      Serial.printf("[SAVE] LittleFS total bytes: %u, used bytes: %u\n", fs_info.totalBytes, fs_info.usedBytes);
    #endif

    if (LittleFS.exists("/config.json")) {
      Serial.println(F("[SAVE] Renaming /config.json to /config.bak"));
      LittleFS.rename("/config.json", "/config.bak");
    }
    File f = LittleFS.open("/config.json", "w");
    if (!f) {
      Serial.println(F("[SAVE] ERROR: Failed to open /config.json for writing!"));
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = "Failed to write config file.";
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    size_t bytesWritten = serializeJson(doc, f);
    Serial.printf("[SAVE] Bytes written to /config.json: %u\n", bytesWritten);
    f.close();
    Serial.println(F("[SAVE] /config.json file closed."));

    File verify = LittleFS.open("/config.json", "r");
    if (!verify) {
      Serial.println(F("[SAVE] ERROR: Failed to open /config.json for reading during verification!"));
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = "Verification failed: Could not re-open config file.";
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    while (verify.available()) {
      verify.read();
    }
    verify.seek(0);

    DynamicJsonDocument test(2048);
    DeserializationError err = deserializeJson(test, verify);
    verify.close();

    if (err) {
      Serial.print(F("[SAVE] Config corrupted after save: "));
      Serial.println(err.f_str());
      DynamicJsonDocument errorDoc(256);
      errorDoc[F("error")] = String("Config corrupted. Reboot cancelled. Error: ") + err.f_str();
      String response;
      serializeJson(errorDoc, response);
      request->send(500, "application/json", response);
      return;
    }

    Serial.println(F("[SAVE] Config verification successful."));
    DynamicJsonDocument okDoc(128);
    okDoc[F("message")] = "Saved successfully. Rebooting...";
    String response;
    serializeJson(okDoc, response);
    request->send(200, "application/json", response);
    Serial.println(F("[WEBSERVER] Sending success response and scheduling reboot..."));

    request->onDisconnect([]() {
      Serial.println(F("[WEBSERVER] Client disconnected, rebooting ESP..."));
      ESP.restart();
    });
  });

  server.on("/restore", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /restore"));
    if (LittleFS.exists("/config.bak")) {
      File src = LittleFS.open("/config.bak", "r");
      if (!src) {
        Serial.println(F("[WEBSERVER] Failed to open /config.bak"));
        DynamicJsonDocument errorDoc(128);
        errorDoc[F("error")] = "Failed to open backup file.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }
      File dst = LittleFS.open("/config.json", "w");
      if (!dst) {
        src.close();
        Serial.println(F("[WEBSERVER] Failed to open /config.json for writing"));
        DynamicJsonDocument errorDoc(128);
        errorDoc[F("error")] = "Failed to open config for writing.";
        String response;
        serializeJson(errorDoc, response);
        request->send(500, "application/json", response);
        return;
      }

      if (!checkAuth(request)) {
        request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
      }

      while (src.available()) {
        dst.write(src.read());
      }
      src.close();
      dst.close();

      DynamicJsonDocument okDoc(128);
      okDoc[F("message")] = "✅ Backup restored! Device will now reboot.";
      String response;
      serializeJson(okDoc, response);
      request->send(200, "application/json", response);
      request->onDisconnect([]() {
        Serial.println(F("[WEBSERVER] Rebooting after restore..."));
        ESP.restart();
      });

    } else {
      Serial.println(F("[WEBSERVER] No backup found"));
      DynamicJsonDocument errorDoc(128);
      errorDoc[F("error")] = "No backup found.";
      String response;
      serializeJson(errorDoc, response);
      request->send(404, "application/json", response);
    }
  });

  server.on("/clear_wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBSERVER] Request: /clear_wifi"));
    clearWiFiCredentialsInConfig();

    DynamicJsonDocument okDoc(128);
    okDoc[F("message")] = "✅ WiFi credentials cleared! Rebooting...";
    String response;
    serializeJson(okDoc, response);
    request->send(200, "application/json", response);

    request->onDisconnect([]() {
      Serial.println(F("[WEBSERVER] Rebooting after clearing WiFi..."));
      ESP.restart();
    });
  });

  server.on("/ap_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.print(F("[WEBSERVER] Request: /ap_status. isAPMode = "));
    Serial.println(isAPMode);
    String json = "{\"isAP\": ";
    json += (isAPMode) ? "true" : "false";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/set_brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }
    int newBrightness = request->getParam("value", true)->value().toInt();

    // Handle "off" request
    if (newBrightness == -1) {
      P.displayShutdown(true);  // Fully shut down display driver
      P.displayClear();
      displayOff = true;
      Serial.println("[WEBSERVER] Display set to OFF (shutdown mode)");
      request->send(200, "application/json", "{\"ok\":true, \"display\":\"off\"}");
      return;
    }

    // Clamp brightness to valid range
    if (newBrightness < 0) newBrightness = 0;
    if (newBrightness > 15) newBrightness = 15;

    // Only run robust clear/reset when coming from "off"
    if (displayOff) {
      P.setIntensity(newBrightness);
      advanceDisplayModeSafe();
      P.displayShutdown(false);
      brightness = newBrightness;
      displayOff = false;
      Serial.println("[WEBSERVER] Display woke from OFF");
    } else {
      // Display already on, just set brightness
      brightness = newBrightness;
      P.setIntensity(brightness);
      Serial.printf("[WEBSERVER] Set brightness to %d\n", brightness);
    }

    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_flip", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool flip = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      flip = (v == "1" || v == "true" || v == "on");
    }
    flipDisplay = flip;
    P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
    P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);
    Serial.printf("[WEBSERVER] Set flipDisplay to %d\n", flipDisplay);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_twelvehour", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool twelveHour = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      twelveHour = (v == "1" || v == "true" || v == "on");
    }
    twelveHourToggle = twelveHour;
    Serial.printf("[WEBSERVER] Set twelveHourToggle to %d\n", twelveHourToggle);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_dayofweek", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDay = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDay = (v == "1" || v == "true" || v == "on");
    }
    showDayOfWeek = showDay;
    Serial.printf("[WEBSERVER] Set showDayOfWeek to %d\n", showDayOfWeek);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_showdate", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDateVal = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDateVal = (v == "1" || v == "true" || v == "on");
    }
    showDate = showDateVal;
    Serial.printf("[WEBSERVER] Set showDate to %d\n", showDate);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_humidity", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showHumidityNow = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showHumidityNow = (v == "1" || v == "true" || v == "on");
    }
    showHumidity = showHumidityNow;
    Serial.printf("[WEBSERVER] Set showHumidity to %d\n", showHumidity);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_colon_blink", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableBlink = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableBlink = (v == "1" || v == "true" || v == "on");
    }
    colonBlinkEnabled = enableBlink;
    Serial.printf("[WEBSERVER] Set colonBlinkEnabled to %d\n", colonBlinkEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_language", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value\"}");
      return;
    }

    String lang = request->getParam("value", true)->value();
    lang.trim();         // Remove whitespace/newlines
    lang.toLowerCase();  // Normalize to lowercase

    strlcpy(language, lang.c_str(), sizeof(language));              // Safe copy to char[]
    Serial.printf("[WEBSERVER] Set language to '%s'\n", language);  // Use quotes for debug

    shouldFetchWeatherNow = true;

    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_weatherdesc", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool showDesc = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      showDesc = (v == "1" || v == "true" || v == "on");
    }

    if (showWeatherDescription == true && showDesc == false) {
      Serial.println(F("[WEBSERVER] showWeatherDescription toggled OFF. Checking display mode..."));
      if (displayMode == 2) {
        Serial.println(F("[WEBSERVER] Currently in Weather Description mode. Forcing mode advance/cleanup."));
        advanceDisplayMode();
      }
    }

    showWeatherDescription = showDesc;
    Serial.printf("[WEBSERVER] Set Show Weather Description to %d\n", showWeatherDescription);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_units", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      if (v == "1" || v == "true" || v == "on") {
        strcpy(weatherUnits, "imperial");
        tempSymbol = ']';
      } else {
        strcpy(weatherUnits, "metric");
        tempSymbol = '[';
      }
      Serial.printf("[WEBSERVER] Set weatherUnits to %s\n", weatherUnits);
      shouldFetchWeatherNow = true;
      request->send(200, "application/json", "{\"ok\":true}");
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
    }
  });

  server.on("/set_countdown_enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableCountdownNow = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableCountdownNow = (v == "1" || v == "true" || v == "on");
    }

    if (countdownEnabled == enableCountdownNow) {
      Serial.println(F("[WEBSERVER] Countdown enable state unchanged, ignoring."));
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    }

    if (countdownEnabled == true && enableCountdownNow == false) {
      Serial.println(F("[WEBSERVER] Countdown toggled OFF. Checking display mode..."));
      if (displayMode == 3) {
        Serial.println(F("[WEBSERVER] Currently in Countdown mode. Forcing mode advance/cleanup."));
        advanceDisplayMode();
      }
    }

    countdownEnabled = enableCountdownNow;
    Serial.printf("[WEBSERVER] Set Countdown Enabled to %d\n", countdownEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_dramatic_countdown", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableDramaticNow = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableDramaticNow = (v == "1" || v == "true" || v == "on");
    }

    // Check if the state has changed
    if (isDramaticCountdown == enableDramaticNow) {
      Serial.println(F("[WEBSERVER] Dramatic Countdown state unchanged, ignoring."));
      request->send(200, "application/json", "{\"ok\":true}");
      return;
    }

    // Update the global variable
    isDramaticCountdown = enableDramaticNow;

    // Call saveCountdownConfig with only the existing parameters.
    // It will read the updated global variable 'isDramaticCountdown'.
    saveCountdownConfig(countdownEnabled, countdownTargetTimestamp, countdownLabel);

    Serial.printf("[WEBSERVER] Set Dramatic Countdown to %d\n", isDramaticCountdown);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // API: Send custom message
  server.on("/api/message", HTTP_POST, [](AsyncWebServerRequest *request) {

    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    Serial.println(F("[API] Message received"));
    DynamicJsonDocument response(256);

    if (!request->hasParam("text", true)) {
      response["error"] = "Missing text parameter";
      String json;
      serializeJson(response, json);
      request->send(400, "application/json", json);
      return;
    }

    customMessage = normalizeDisplayText(request->getParam("text", true)->value());

    if (request->hasParam("duration", true)) {
      int dur = request->getParam("duration", true)->value().toInt();
      customMessageDuration = (dur > 0 ? dur : 5) * 1000;
    } else {
      customMessageDuration = 5000;
    }

    if (request->hasParam("scroll", true)) {
      customMessageScrolling = (request->getParam("scroll", true)->value() == "1");
    } else {
      customMessageScrolling = (customMessage.length() > 8);
    }

    if (request->hasParam("priority", true)) {
      customMessagePriority = request->getParam("priority", true)->value().toInt();
      if (customMessagePriority > 1) displayMode = 10;  // Force message mode
    }

    showCustomMessage = true;
    customMessageStartTime = millis();

    response["status"] = "ok";
    response["message"] = customMessage;

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // API: Get status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {

    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    DynamicJsonDocument status(512);
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    status["time"]["hour"] = timeinfo.tm_hour;
    status["time"]["minute"] = timeinfo.tm_min;
    status["weather"]["temp"] = currentTemp;
    status["weather"]["humidity"] = currentHumidity;
    status["system"]["uptime"] = millis() / 1000;
    status["system"]["wifi_rssi"] = WiFi.RSSI();

    if (showCustomMessage) {
      status["message"]["active"] = true;
      status["message"]["text"] = customMessage;
    }

    String json;
    serializeJson(status, json);
    request->send(200, "application/json", json);
  });


  // Get full system information
  server.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    DynamicJsonDocument doc(2048);

    // System info
    doc["system"]["uptime"] = millis() / 1000;
    doc["system"]["freeHeap"] = ESP.getFreeHeap();
    doc["system"]["rssi"] = WiFi.RSSI();
    doc["system"]["ip"] = WiFi.localIP().toString();
    doc["system"]["mac"] = WiFi.macAddress();

    // Display info
    doc["display"]["mode"] = displayMode;
    doc["display"]["brightness"] = brightness;
    doc["display"]["off"] = displayOff;
    doc["display"]["flipped"] = flipDisplay;

    // Config
    doc["config"]["clockDuration"] = clockDuration;
    doc["config"]["weatherDuration"] = weatherDuration;
    doc["config"]["twelveHour"] = twelveHourToggle;
    doc["config"]["showDayOfWeek"] = showDayOfWeek;
    doc["config"]["showDate"] = showDate;
    doc["config"]["showHumidity"] = showHumidity;
    doc["config"]["language"] = language;

    // Time
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    doc["time"]["unix"] = now;
    char timeFormatted[6];
    sprintf(timeFormatted, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    doc["time"]["formatted"] = timeFormatted;

    // Weather
    doc["weather"]["temp"] = currentTemp;
    doc["weather"]["humidity"] = currentHumidity;
    doc["weather"]["description"] = weatherDescription;
    doc["weather"]["lastFetch"] = weatherFetchInitiated ? (millis() - lastFetch) / 1000 : -1;

    // Countdown
    if (countdownEnabled) {
      doc["countdown"]["enabled"] = true;
      doc["countdown"]["target"] = countdownTargetTimestamp;
      doc["countdown"]["remaining"] = max(0L, (long)(countdownTargetTimestamp - now));
      doc["countdown"]["label"] = countdownLabel;
    }

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Control display mode
  server.on("/api/mode", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    if (!request->hasParam("mode", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing mode parameter\"}");
      return;
    }

    int newMode = request->getParam("mode", true)->value().toInt();
    String response;

    if (newMode >= 0 && newMode <= 6) {
      displayMode = newMode;
      lastSwitch = millis();
      response = "{\"status\":\"ok\",\"mode\":" + String(newMode) + "}";
      request->send(200, "application/json", response);
    } else {
      request->send(400, "application/json", "{\"error\":\"Invalid mode\"}");
    }
  });

  // Control brightness
  server.on("/api/brightness", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    if (!request->hasParam("value", true)) {
      request->send(400, "application/json", "{\"error\":\"Missing value parameter\"}");
      return;
    }

    int newBrightness = request->getParam("value", true)->value().toInt();

    if (newBrightness >= -1 && newBrightness <= 15) {
      brightness = newBrightness;

      if (brightness == -1) {
        P.displayShutdown(true);
        displayOff = true;
      } else {
        if (displayOff) {
          P.displayShutdown(false);
          displayOff = false;
        }
        P.setIntensity(brightness);
      }

      String response = "{\"status\":\"ok\",\"brightness\":" + String(brightness) + "}";
      request->send(200, "application/json", response);
    } else {
      request->send(400, "application/json", "{\"error\":\"Invalid brightness value\"}");
    }
  });

  // Refresh weather
  server.on("/api/weather/refresh", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    shouldFetchWeatherNow = true;
    request->send(200, "application/json", "{\"status\":\"Weather refresh triggered\"}");
  });

  // Get weather data
  server.on("/api/weather", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    DynamicJsonDocument doc(512);
    doc["temp"] = currentTemp;
    doc["humidity"] = currentHumidity;
    doc["description"] = weatherDescription;
    doc["available"] = weatherAvailable;
    doc["provider"] = weatherProvider == PROVIDER_OPEN_METEO ? "openmeteo" :
                      weatherProvider == PROVIDER_OPEN_WEATHER ? "openweather" : "pirateweather";
    doc["units"] = weatherUnits;
    doc["location"]["lat"] = openMeteoLatitude;
    doc["location"]["lon"] = openMeteoLongitude;

    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Control countdown
  server.on("/api/countdown", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    DynamicJsonDocument response(256);
    bool changed = false;

    if (request->hasParam("enabled", true)) {
      countdownEnabled = (request->getParam("enabled", true)->value() == "true" ||
                         request->getParam("enabled", true)->value() == "1");
      changed = true;
    }

    if (request->hasParam("timestamp", true)) {
      countdownTargetTimestamp = request->getParam("timestamp", true)->value().toInt();
      changed = true;
    }

    if (request->hasParam("label", true)) {
      String label = request->getParam("label", true)->value();
      strlcpy(countdownLabel, label.c_str(), sizeof(countdownLabel));
      changed = true;
    }

    if (changed) {
      saveCountdownConfig(countdownEnabled, countdownTargetTimestamp, countdownLabel);
      response["status"] = "ok";
      response["countdown"]["enabled"] = countdownEnabled;
      response["countdown"]["target"] = countdownTargetTimestamp;
      response["countdown"]["label"] = countdownLabel;
    } else {
      response["status"] = "No changes";
    }

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Reboot device
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!apiEnabled) {
      request->send(403, "application/json", "{\"error\":\"API disabled\"}");
      return;
    }

    request->send(200, "application/json", "{\"status\":\"Rebooting...\"}");
    delay(1000);
    ESP.restart();
  });

  server.on("/set_api", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableApi = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableApi = (v == "1" || v == "true" || v == "on");
    }
    apiEnabled = enableApi;
    Serial.printf("[WEBSERVER] Set apiEnabled to %d\n", apiEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_youtube_enabled", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enableYoutube = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enableYoutube = (v == "1" || v == "true" || v == "on");
    }

    if (youtubeEnabled == true && enableYoutube == false) {
      Serial.println(F("[WEBSERVER] YouTube toggled OFF. Checking display mode..."));
      if (displayMode == 6) {
        Serial.println(F("[WEBSERVER] Currently in YouTube mode. Forcing mode advance."));
        advanceDisplayMode();
      }
    }

    youtubeEnabled = enableYoutube;
    Serial.printf("[WEBSERVER] Set youtubeEnabled to %d\n", youtubeEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/set_youtube_format", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool shortFormat = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      shortFormat = (v == "1" || v == "true" || v == "on");
    }
    youtubeShortFormat = shortFormat;
    currentSubscriberCount = "";  // Force refresh
    Serial.printf("[WEBSERVER] Set youtubeShortFormat to %d\n", youtubeShortFormat);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // Webhook
  server.on("/webhook", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println(F("[WEBHOOK] Request received"));

    // Check if webhooks are enabled
    if (!webhooksEnabled) {
      request->send(403, "application/json", "{\"error\":\"Webhooks disabled\"}");
      return;
    }

    // Validate key
    if (!request->hasParam("key", true) ||
        !secureCompare(request->getParam("key", true)->value().c_str(), webhookKey)) {
      Serial.println(F("[WEBHOOK] Invalid key"));
      request->send(403, "application/json", "{\"error\":\"Invalid key\"}");
      return;
    }

    // Check quiet hours
    if (webhookQuietHours && dimmingEnabled) {
      time_t now = time(nullptr);
      struct tm timeinfo;
      localtime_r(&now, &timeinfo);
      int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
      int startMinutes = dimStartHour * 60 + dimStartMinute;
      int endMinutes = dimEndHour * 60 + dimEndMinute;

      bool inQuietHours = false;
      if (startMinutes < endMinutes) {
        inQuietHours = (currentMinutes >= startMinutes && currentMinutes < endMinutes);
      } else {
        inQuietHours = (currentMinutes >= startMinutes || currentMinutes < endMinutes);
      }

      if (inQuietHours) {
        int priority = request->hasParam("priority", true) ?
                      request->getParam("priority", true)->value().toInt() : 1;
        if (priority < 2) {  // Only urgent messages during quiet hours
          request->send(202, "application/json", "{\"status\":\"quiet_hours\"}");
          return;
        }
      }
    }

    // Parse message
    WebhookMessage msg;
    String rawMessage = request->hasParam("message", true) ?
               request->getParam("message", true)->value() : "WEBHOOK";
    msg.text = normalizeDisplayText(rawMessage);

    msg.priority = request->hasParam("priority", true) ?
                   request->getParam("priority", true)->value().toInt() : 1;
    msg.duration = request->hasParam("duration", true) ?
                   request->getParam("duration", true)->value().toInt() * 1000 : 5000;
    msg.scroll = request->hasParam("scroll", true) ?
                 request->getParam("scroll", true)->value() == "1" : (msg.text.length() > 8);
    msg.timestamp = millis();

    // Check queue size
    if (messageQueue.size() >= (unsigned int)webhookQueueSize) {
      // Remove oldest low-priority message or reject
      if (msg.priority > 1) {
        // High priority: remove oldest message
        messageQueue.pop();
      } else {
        request->send(429, "application/json", "{\"error\":\"Queue full\"}");
        return;
      }
    }

    // Add to queue
    messageQueue.push(msg);
    Serial.printf("[WEBHOOK] Message queued: %s (priority=%d)\n",
                  msg.text.c_str(), msg.priority);

    // Response
    DynamicJsonDocument response(256);
    response["status"] = "queued";
    response["message"] = msg.text;
    response["queue_size"] = messageQueue.size();

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Live toggle
  server.on("/set_webhooks", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool enable = false;
    if (request->hasParam("value", true)) {
      String v = request->getParam("value", true)->value();
      enable = (v == "1" || v == "true" || v == "on");
    }
    webhooksEnabled = enable;
    Serial.printf("[WEBSERVER] Set webhooksEnabled to %d\n", webhooksEnabled);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  // Login endpoint
  server.on("/auth/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    String password = request->hasParam("password", true) ?
                      request->getParam("password", true)->value() : "";
    String totpCode = request->hasParam("totp", true) ?
                      request->getParam("totp", true)->value() : "";

    if (password != adminPassword) {
      request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
      return;
    }

    if (totpEnabled && strlen(totpSecretBase32) > 0) {
      time_t now = time(nullptr);

      if (now < 1000) {
        request->send(500, "application/json", "{\"error\":\"Time not synced\"}");
        return;
      }

      if (!totp.verify(totpCode, now, 2)) {
        Serial.println(F("[AUTH] Login failed: TOTP invalid"));
        request->send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
        return;
      }
      Serial.println(F("[AUTH] Login successful"));
    }

    sessionToken = "";
    for (int i = 0; i < 32; i++) {
      sessionToken += String(random(16), HEX);
    }
    sessionExpiry = millis() + (30 * 60 * 1000); // 30 minutes

    DynamicJsonDocument response(256);
    response["token"] = sessionToken;
    response["expires"] = sessionExpiry;

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  server.on("/auth/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument response(256);
    response["authEnabled"] = authEnabled;
    response["totpEnabled"] = totpEnabled;
    response["hasSecret"] = (strlen(totpSecretBase32) > 0);
    response["hasPassword"] = (strlen(adminPassword) > 0);
    response["timeSync"] = (time(nullptr) > 1000);

    if (strlen(totpSecretBase32) > 0 && time(nullptr) > 1000) {
      // Only for debug
      response["currentCode"] = totp.getCode(time(nullptr));
    }

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // QR Code endpoint
  server.on("/auth/qrcode", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (strlen(totpSecretBase32) == 0) {
      request->send(404, "application/json", "{\"error\":\"No TOTP secret generated\"}");
      return;
    }

    String uri = "otpauth://totp/";
    uri += "ESPTimeCast%3Aadmin";
    uri += "?secret=";
    uri += totpSecretBase32;
    uri += "&issuer=ESPTimeCast";
    uri += "&algorithm=SHA1";
    uri += "&digits=6";
    uri += "&period=30";

    time_t now = time(nullptr);
    String currentCode = (now > 1000) ? totp.getCode(now) : "000000";

    DynamicJsonDocument doc(512);
    doc["uri"] = uri;
    doc["secret"] = totpSecretBase32;
    doc["currentCode"] = currentCode; // Pour debug
    doc["algorithm"] = "SHA1";
    doc["digits"] = 6;
    doc["period"] = 30;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // Generate TOTP endpoint
  server.on("/auth/generate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401);
      return;
    }
    generateTOTPSecret();
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/auth/enable", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) {
      request->send(401, "application/json", "{\"error\":\"Unauthorized\"}");
      return;
    }

    bool enableTotp = request->hasParam("totp", true) &&
                      request->getParam("totp", true)->value() == "true";

    if (enableTotp && strlen(totpSecretBase32) == 0) {
      generateTOTPSecret();
    }

    totpEnabled = enableTotp;

    DynamicJsonDocument doc(2048);
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      deserializeJson(doc, configFile);
      configFile.close();
    }

    doc["authEnabled"] = authEnabled;
    doc["totpEnabled"] = totpEnabled;

    File f = LittleFS.open("/config.json", "w");
    if (f) {
      serializeJson(doc, f);
      f.close();
    }

    DynamicJsonDocument response(256);
    response["success"] = true;
    response["totpEnabled"] = totpEnabled;
    if (enableTotp) {
      response["secret"] = totpSecretBase32;
    }

    String json;
    serializeJson(response, json);
    request->send(200, "application/json", json);
  });

  // Login page
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/login.html", "text/html");
  });

  server.begin();
  Serial.println(F("[WEBSERVER] Web server started"));
}

void handleCaptivePortal(AsyncWebServerRequest *request) {
  Serial.print(F("[WEBSERVER] Captive Portal Redirecting: "));
  Serial.println(request->url());
  request->redirect(String("http://") + WiFi.softAPIP().toString() + "/");
}

bool isNumber(const char *str) {
  for (int i = 0; str[i]; i++) {
    if (!isdigit(str[i]) && str[i] != '.' && str[i] != '-') return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
// Weather Fetching
// -----------------------------------------------------------------------------
String getWeatherDescription(int code, const char* lang) {
  const char* description = getWeatherByCode(code, lang);
  return normalizeDisplayText(String(description), true);
}

void fetchOpenWeather() {
  if (millis() - lastWifiConnectTime < WIFI_STABILIZE_DELAY) {
    Serial.println(F("[WEATHER] Skipped: WiFi stabilizing..."));
    return;
  }

  Serial.println(F("[WEATHER] Fetching weather data from OpenWeatherMap..."));

  if (strlen(weatherApiKey) == 0) {
    Serial.println(F("[WEATHER] No API key for OpenWeatherMap"));
    weatherAvailable = false;
    return;
  }

  float lat = atof(openMeteoLatitude);
  float lon = atof(openMeteoLongitude);

  String url = "https://api.openweathermap.org/data/2.5/weather?";
  url += "lat=" + String(lat, 6);
  url += "&lon=" + String(lon, 6);
  url += "&appid=" + String(weatherApiKey);

  if (strcmp(language, "en") != 0) {
    url += "&lang=" + String(language);
  }

  if (strcmp(weatherUnits, "imperial") == 0) {
    url += "&units=imperial";
  } else {
    url += "&units=metric";
  }

  Serial.print(F("[WEATHER] URL: "));
  Serial.println(url);

  #ifdef ESP32
    WiFiClientSecure client;
    client.setInsecure();
  #else // ESP8266
    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);
  #endif

  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(5000);
  http.setReuse(false);

  int httpCode = http.GET();

  auto cleanup = [&]() {
    http.end();
    client.stop();
  };

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println(F("[WEATHER] Response received."));

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("[WEATHER] JSON parse error: "));
      Serial.println(error.f_str());
      weatherAvailable = false;
      cleanup();
      return;
    }

    if (doc["main"]["temp"]) {
      float temp = doc["main"]["temp"];
      currentTemp = String((int)round(temp)) + "º";
      weatherAvailable = true;
      Serial.printf("[WEATHER] Temp: %s\n", currentTemp.c_str());
    }

    if (doc["main"]["humidity"]) {
      currentHumidity = doc["main"]["humidity"];
      Serial.printf("[WEATHER] Humidity: %d%%\n", currentHumidity);
    }

    if (doc["weather"][0]["description"]) {
      String desc = doc["weather"][0]["description"].as<String>();
      weatherDescription = normalizeDisplayText(desc, true);
      translateAPIWeatherTerms(weatherDescription, language);
      Serial.printf("[WEATHER] Description: %s\n", weatherDescription.c_str());
    }

    weatherFetched = true;
  } else if (httpCode == 401) {
    Serial.println(F("[WEATHER] Invalid API key"));
    weatherAvailable = false;
  } else {
    Serial.printf("[WEATHER] HTTP GET failed, code: %d\n", httpCode);
    weatherAvailable = false;
  }

  cleanup();
}

void fetchPirateWeather() {
  if (millis() - lastWifiConnectTime < WIFI_STABILIZE_DELAY) {
    Serial.println(F("[WEATHER] Skipped: WiFi stabilizing..."));
    return;
  }

  Serial.println(F("[WEATHER] Fetching from PirateWeather..."));

  if (strlen(weatherApiKey) == 0) {
    Serial.println(F("[WEATHER] No API key for PirateWeather"));
    weatherAvailable = false;
    return;
  }

  float lat = atof(openMeteoLatitude);
  float lon = atof(openMeteoLongitude);

  String url = "http://api.pirateweather.net/forecast/";
  url += String(weatherApiKey) + "/";
  url += String(lat, 6) + "," + String(lon, 6);
  url += "?units=" + String(strcmp(weatherUnits, "imperial") == 0 ? "us" : "si");
  url += "&exclude=minutely,hourly,daily,alerts,flags";

  Serial.print(F("[WEATHER] URL: "));
  Serial.println(url);

  WiFiClient client;

  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(5000);
  http.setReuse(false);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println(F("[WEATHER] Response received."));

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("[WEATHER] JSON parse error: "));
      Serial.println(error.f_str());
      weatherAvailable = false;
      http.end();
      return;
    }

    if (doc["currently"]["temperature"]) {
      float temp = doc["currently"]["temperature"];
      currentTemp = String((int)round(temp)) + "º";
      weatherAvailable = true;
      Serial.printf("[WEATHER] Temp: %s\n", currentTemp.c_str());
    }

    if (doc["currently"]["humidity"]) {
      float hum = doc["currently"]["humidity"];
      currentHumidity = (int)(hum * 100);
      Serial.printf("[WEATHER] Humidity: %d%%\n", currentHumidity);
    }

    if (doc["currently"]["summary"]) {
      String desc = doc["currently"]["summary"].as<String>();
      weatherDescription = normalizeDisplayText(desc, true);
      translateAPIWeatherTerms(weatherDescription, language);
      Serial.printf("[WEATHER] Description: %s\n", weatherDescription.c_str());
    }

    weatherFetched = true;
  } else {
    Serial.printf("[WEATHER] HTTP GET failed, code: %d\n", httpCode);
    weatherAvailable = false;
  }

  http.end();
}

void fetchOpenMeteo() {
  if (millis() - lastWifiConnectTime < WIFI_STABILIZE_DELAY) {
    Serial.println(F("[WEATHER] Skipped: WiFi stabilizing..."));
    return;
  }

  Serial.println(F("[WEATHER] Fetching weather data from Open-Meteo..."));

  float lat = atof(openMeteoLatitude);
  float lon = atof(openMeteoLongitude);

  String url = "https://api.open-meteo.com/v1/forecast?";
  url += "latitude=" + String(lat, 6);
  url += "&longitude=" + String(lon, 6);
  url += "&current=temperature_2m,relative_humidity_2m,weather_code";

  if (strcmp(weatherUnits, "imperial") == 0) {
    url += "&temperature_unit=fahrenheit";
  } else {
    url += "&temperature_unit=celsius";
  }

  Serial.print(F("[WEATHER] URL: "));
  Serial.println(url);

  #ifdef ESP32
    WiFiClientSecure client;
    client.setInsecure();
  #else // ESP8266
    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);
  #endif

  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(5000);
  http.setReuse(false);

  int httpCode = http.GET();

  auto cleanup = [&]() {
    http.end();
    client.stop();
  };

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println(F("[WEATHER] Response received."));

    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("[WEATHER] JSON parse error: "));
      Serial.println(error.f_str());
      weatherAvailable = false;
      cleanup();
      return;
    }

    if (doc["current"]["temperature_2m"]) {
      float temp = doc["current"]["temperature_2m"];
      currentTemp = String((int)round(temp)) + "º";
      weatherAvailable = true;
      Serial.printf("[WEATHER] Temp: %s\n", currentTemp.c_str());
    }

    if (doc["current"]["relative_humidity_2m"]) {
      currentHumidity = doc["current"]["relative_humidity_2m"];
      Serial.printf("[WEATHER] Humidity: %d%%\n", currentHumidity);
    }

    if (doc["current"]["weather_code"]) {
      int code = doc["current"]["weather_code"];
      weatherDescription = getWeatherDescription(code, language);
      Serial.printf("[WEATHER] Code: %d, Description: %s\n", code, weatherDescription.c_str());
    }

    weatherFetched = true;
  } else {
    Serial.printf("[WEATHER] HTTP GET failed, code: %d\n", httpCode);
    weatherAvailable = false;
  }

  cleanup();
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WEATHER] Skipped: WiFi not connected"));
    weatherAvailable = false;
    weatherFetched = false;
    return;
  }

  float lat = atof(openMeteoLatitude);
  float lon = atof(openMeteoLongitude);

  if (!isNumber(openMeteoLatitude) || !isNumber(openMeteoLongitude) ||
      lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    Serial.println(F("[WEATHER] Skipped: Need valid coordinates"));
    weatherAvailable = false;
    return;
  }

  if (weatherProvider == PROVIDER_OPEN_WEATHER) {
    fetchOpenWeather();
  } else if (weatherProvider == PROVIDER_PIRATE_WEATHER) {
    fetchPirateWeather();
  } else {
    fetchOpenMeteo();
  }
}

// -----------------------------------------------------------------------------
// YouTube
// -----------------------------------------------------------------------------

String formatSubscriberCount(long subscribers) {
  if (!youtubeShortFormat) {
    return String(subscribers);
  }

  if (subscribers >= 1000000000) {
    float billions = subscribers / 1000000000.0;
    return String(billions, 3) + "B";
  }
  else if (subscribers >= 1000000) {
    float millions = subscribers / 1000000.0;
    return String(millions, 3) + "M";
  }
  else if (subscribers >= 1000) {
    float thousands = subscribers / 1000.0;

    if (subscribers % 1000 == 0) {
      return String((int)thousands) + "K";
    }

    char buffer[20];
    sprintf(buffer, "%.3f", thousands);

    String result = String(buffer);
    while (result.endsWith("0") && result.indexOf(".") != -1) {
      result.remove(result.length() - 1);
    }
    if (result.endsWith(".")) {
      result.remove(result.length() - 1);
    }

    return result + "K";
  }
  else {
    return String(subscribers);
  }
}

void fetchYoutubeSubscribers() {
  if (WiFi.status() != WL_CONNECTED || strlen(youtubeApiKey) == 0 || strlen(youtubeChannelId) == 0) {
    Serial.println(F("[YouTube] Skipped: WiFi not connected or missing config"));
    return;
  }

  if (millis() - lastWifiConnectTime < WIFI_STABILIZE_DELAY) {
    Serial.println(F("[YouTube] Skipped: WiFi stabilizing..."));
    return;
  }

  Serial.println(F("[YouTube] Fetching subscriber count..."));

  String url = "https://www.googleapis.com/youtube/v3/channels?";
  url += "part=statistics";
  url += "&id=" + String(youtubeChannelId);
  url += "&key=" + String(youtubeApiKey);

  #ifdef ESP32
    WiFiClientSecure client;
    client.setInsecure();
  #else // ESP8266
    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.setBufferSizes(512, 512);
  #endif

  HTTPClient https;
  https.begin(client, url);
  https.setTimeout(5000);

  int httpCode = https.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = https.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["items"][0]["statistics"]["subscriberCount"]) {
      long subscribers = doc["items"][0]["statistics"]["subscriberCount"].as<long>();
      currentSubscriberCount = formatSubscriberCount(subscribers);

      Serial.printf("[YouTube] Raw count: %ld, Formatted: %s\n",
                    subscribers, currentSubscriberCount.c_str());
    } else {
      Serial.println(F("[YouTube] JSON parse error or no data"));
      currentSubscriberCount = "ERR";
    }
  } else {
    Serial.printf("[YouTube] HTTP error: %d\n", httpCode);
    currentSubscriberCount = "---";
  }

  https.end();
  client.stop();
  lastYoutubeFetch = millis();
}

// -----------------------------------------------------------------------------
// Main setup() and loop()
// -----------------------------------------------------------------------------
/*
DisplayMode key:
  0: Clock
  1: Weather
  2: Weather Description
  3: Countdown
  4: Nightscout
  5: Date
  6: YouTube
  10: API
  11: Webhook
*/

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println(F("[SETUP] Starting setup..."));

  #ifdef ESP32
    if (!LittleFS.begin(true)) {
  #else  // ESP8266
    if (!LittleFS.begin()) {
  #endif
    Serial.println(F("[ERROR] LittleFS mount failed in setup! Halting."));
    while (true) {
      delay(1000);
      yield();
    }
  }
  Serial.println(F("[SETUP] LittleFS file system mounted successfully."));

  P.begin();  // Initialize Parola library

  P.setCharSpacing(0);
  P.setFont(mFactory);
  loadConfig();  // This function now has internal yields and prints

  P.setIntensity(brightness);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_UD);
  P.setZoneEffect(0, flipDisplay, PA_FLIP_LR);

  Serial.println(F("[SETUP] Parola (LED Matrix) initialized"));

  connectWiFi();

  #if defined(ESP32)
    WiFi.setSleep(false);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
      if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
        Serial.println(F("[WIFI] Got IP"));
        lastWifiConnectTime = millis();
      } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        Serial.println(F("[WIFI] Disconnected"));
      }
    });

  #elif defined(ESP8266)
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    wifiGotIPHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &ev) {
      Serial.println(F("[WIFI] Got IP"));
      lastWifiConnectTime = millis();
    });
    wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &ev) {
      Serial.printf("[WIFI] Disconnected (reason: %d)\n", ev.reason);
    });
  #endif

  if (isAPMode) {
    Serial.println(F("[SETUP] WiFi connection failed. Device is in AP Mode."));
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("[SETUP] WiFi connected successfully to local network."));
  } else {
    Serial.println(F("[SETUP] WiFi state is uncertain after connection attempt."));
  }

  setupWebServer();
  Serial.println(F("[SETUP] Webserver setup complete"));
  setupMDNS();
  Serial.println(F("[SETUP] Setup complete"));
  Serial.println();
  printConfigToSerial();
  setupTime();
  displayMode = 0;
  lastSwitch = millis();
  lastColonBlink = millis();
}

void advanceDisplayMode() {
  int oldMode = displayMode;
  if (showCustomMessage && customMessagePriority > 0 && displayMode != 10) {
    displayMode = 10;
    Serial.println(F("[DISPLAY] Switching to display mode: MESSAGE (priority)"));
    lastSwitch = millis();
    return;
  }
  String ntpField = String(ntpServer2);
  bool nightscoutConfigured = ntpField.startsWith("https://");

  if (displayMode == 0) {  // Clock -> ...
    if (showDate) {
      displayMode = 5;  // Date mode right after Clock
      Serial.println(F("[DISPLAY] Switching to display mode: DATE (from Clock)"));
    } else if (weatherAvailable && (strlen(openMeteoLatitude) > 0) && (strlen(openMeteoLongitude) > 0)) {
      displayMode = 1;
      Serial.println(F("[DISPLAY] Switching to display mode: WEATHER (from Clock)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Clock, weather skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Clock -> Nightscout (if weather & countdown are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Clock, weather & countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Staying in CLOCK (from Clock)"));
    }
  } else if (displayMode == 5) {  // Date mode
    if (weatherAvailable && (strlen(openMeteoLatitude) > 0) && (strlen(openMeteoLongitude) > 0)) {
      displayMode = 1;
      Serial.println(F("[DISPLAY] Switching to display mode: WEATHER (from Date)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Date, weather skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Date, weather & countdown skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Date)"));
    }
  } else if (displayMode == 1) {  // Weather -> ...
    if (showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) {
      displayMode = 2;
      Serial.println(F("[DISPLAY] Switching to display mode: DESCRIPTION (from Weather)"));
    } else if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Weather, description skipped)"));
    } else if (youtubeEnabled && strlen(youtubeApiKey) > 0 && strlen(youtubeChannelId) > 0) {
      displayMode = 6;  // Weather -> YouTube (if description & countdown are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: YOUTUBE (from Weather, description & countdown skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Weather -> Nightscout (if description, countdown & youtube are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Weather, all skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Weather)"));
    }
  } else if (displayMode == 2) {  // Weather Description -> ...
    if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && countdownTargetTimestamp > time(nullptr)) {
      displayMode = 3;
      Serial.println(F("[DISPLAY] Switching to display mode: COUNTDOWN (from Description)"));
    } else if (youtubeEnabled && strlen(youtubeApiKey) > 0 && strlen(youtubeChannelId) > 0) {
      displayMode = 6;  // Description -> YouTube (if countdown is skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: YOUTUBE (from Description, countdown skipped)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Description -> Nightscout (if countdown & youtube are skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Description, countdown & youtube skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Description)"));
    }
  } else if (displayMode == 3) {  // Countdown -> YouTube, Nightscout or Clock
    if (youtubeEnabled && strlen(youtubeApiKey) > 0 && strlen(youtubeChannelId) > 0) {
      displayMode = 6;  // Countdown -> YouTube
      Serial.println(F("[DISPLAY] Switching to display mode: YOUTUBE (from Countdown)"));
    } else if (nightscoutConfigured) {
      displayMode = 4;  // Countdown -> Nightscout (if youtube is skipped)
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from Countdown, YouTube skipped)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Countdown)"));
    }
  } else if (displayMode == 6) {  // YouTube -> Nightscout or Clock
    String ntpField = String(ntpServer2);
    bool nightscoutConfigured = ntpField.startsWith("https://");
    if (nightscoutConfigured) {
      displayMode = 4;
      Serial.println(F("[DISPLAY] Switching to display mode: NIGHTSCOUT (from YouTube)"));
    } else {
      displayMode = 0;
      Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from YouTube)"));
    }
  } else if (displayMode == 4) {  // Nightscout -> Clock
    displayMode = 0;
    Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Nightscout)"));
  } else if (displayMode == 10) {  // Message -> Clock
    displayMode = 0;
    Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Message)"));
  } else if (displayMode == 11) {  // Webhook -> Clock
    displayMode = 0;
    Serial.println(F("[DISPLAY] Switching to display mode: CLOCK (from Webhook)"));
  }

  // --- Common cleanup/reset logic remains the same ---
  lastSwitch = millis();
}

void advanceDisplayModeSafe() {
  int attempts = 0;
  const int MAX_ATTEMPTS = 6;  // Number of possible modes + 1
  int startMode = displayMode;
  bool valid = false;
  do {
    advanceDisplayMode();  // One step advance
    attempts++;
    // Recalculate validity for the new mode
    valid = false;
    String ntpField = String(ntpServer2);
    bool nightscoutConfigured = ntpField.startsWith("https://");

    if (displayMode == 0) valid = true;  // Clock always valid
    else if (displayMode == 5 && showDate) valid = true;
    else if (displayMode == 1 && weatherAvailable && (strlen(openMeteoLatitude) > 0) && (strlen(openMeteoLongitude) > 0)) valid = true;
    else if (displayMode == 2 && showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) valid = true;
    else if (displayMode == 3 && countdownEnabled && !countdownFinished && ntpSyncSuccessful) valid = true;
    else if (displayMode == 4 && nightscoutConfigured) valid = true;
    else if (displayMode == 10 && showCustomMessage) valid = true;

    // If we've looped back to where we started, break to avoid infinite loop
    if (displayMode == startMode) break;

    if (valid) break;
  } while (attempts < MAX_ATTEMPTS);

  // If no valid mode found, fall back to Clock
  if (!valid) {
    displayMode = 0;
    Serial.println(F("[DISPLAY] Safe fallback to CLOCK"));
  }
  lastSwitch = millis();
}

//config save after countdown finishes
bool saveCountdownConfig(bool enabled, time_t targetTimestamp, const String &label) {
  DynamicJsonDocument doc(2048);

  File configFile = LittleFS.open("/config.json", "r");
  if (configFile) {
    DeserializationError err = deserializeJson(doc, configFile);
    configFile.close();
    if (err) {
      Serial.print(F("[saveCountdownConfig] Error parsing config.json: "));
      Serial.println(err.f_str());
      return false;
    }
  }

  JsonObject countdownObj = doc["countdown"].is<JsonObject>() ? doc["countdown"].as<JsonObject>() : doc.createNestedObject("countdown");
  countdownObj["enabled"] = enabled;
  countdownObj["targetTimestamp"] = targetTimestamp;
  countdownObj["label"] = label;
  countdownObj["isDramaticCountdown"] = isDramaticCountdown;
  doc.remove("countdownEnabled");
  doc.remove("countdownDate");
  doc.remove("countdownTime");
  doc.remove("countdownLabel");

  if (LittleFS.exists("/config.json")) {
    LittleFS.rename("/config.json", "/config.bak");
  }

  File f = LittleFS.open("/config.json", "w");
  if (!f) {
    Serial.println(F("[saveCountdownConfig] ERROR: Cannot write to /config.json"));
    return false;
  }

  size_t bytesWritten = serializeJson(doc, f);
  f.close();

  Serial.printf("[saveCountdownConfig] Config updated. %u bytes written.\n", bytesWritten);
  return true;
}


void loop() {
  if (isAPMode) {
    dnsServer.processNextRequest();
  }

  #ifdef ESP8266
    if (mdnsEnabled) {
      MDNS.update();
    }
  #endif

  static bool colonVisible = true;
  const unsigned long colonBlinkInterval = 800;
  if (millis() - lastColonBlink > colonBlinkInterval) {
    colonVisible = !colonVisible;
    lastColonBlink = millis();
  }

  static unsigned long ntpAnimTimer = 0;
  static int ntpAnimFrame = 0;
  static bool tzSetAfterSync = false;

  // AP Mode animation
  static unsigned long apAnimTimer = 0;
  static int apAnimFrame = 0;
  if (isAPMode) {
    unsigned long now = millis();
    if (now - apAnimTimer > 750) {
      apAnimTimer = now;
      apAnimFrame++;
    }
    P.setTextAlignment(PA_CENTER);
    switch (apAnimFrame % 3) {
      case 0: P.print(F("= ©")); break;
      case 1: P.print(F("= ª")); break;
      case 2: P.print(F("= «")); break;
    }
    yield();
    return;
  }

  if (!processingWebhook && !messageQueue.empty() && !showingIp) {
    WebhookMessage msg = messageQueue.top();

    bool shouldProcess = false;

    switch(msg.priority) {
      case 2:  // HIGH - Interrupts everything
        shouldProcess = true;
        break;
      case 1:  // NORMAL - Interrupts except countdown/youtube
        shouldProcess = (displayMode != 3 && displayMode != 6);
        break;
      case 0:  // LOW - Only when showing clock
        shouldProcess = (displayMode == 0);
        break;
      default:
        shouldProcess = (displayMode == 0);
    }

    if (shouldProcess) {
      messageQueue.pop();
      currentWebhookMessage = msg;
      processingWebhook = true;
      displayMode = 11;
      lastSwitch = millis();
      Serial.printf("[WEBHOOK] Processing: %s (priority=%d)\n",
                    msg.text.c_str(), msg.priority);
    }
  }

  // Dimming
  time_t now_time = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now_time, &timeinfo);
  int curHour = timeinfo.tm_hour;
  int curMinute = timeinfo.tm_min;
  int curTotal = curHour * 60 + curMinute;
  int startTotal = dimStartHour * 60 + dimStartMinute;
  int endTotal = dimEndHour * 60 + dimEndMinute;
  bool isDimmingActive = false;

  if (dimmingEnabled) {
    // Determine if dimming is active (overnight-aware)
    if (startTotal < endTotal) {
      isDimmingActive = (curTotal >= startTotal && curTotal < endTotal);
    } else {
      isDimmingActive = (curTotal >= startTotal || curTotal < endTotal);
    }

    int targetBrightness = isDimmingActive ? dimBrightness : brightness;

    if (targetBrightness == -1) {
      if (!displayOff) {
        Serial.println(F("[DISPLAY] Turning display OFF (dimming -1)"));
        P.displayShutdown(true);
        P.displayClear();
        displayOff = true;
        displayOffByDimming = true;
        displayOffByBrightness = false;
      }
    } else {
      if (displayOff && displayOffByDimming) {
        Serial.println(F("[DISPLAY] Waking display (dimming end)"));
        P.displayShutdown(false);
        displayOff = false;
        displayOffByDimming = false;
      }
      P.setIntensity(targetBrightness);
    }
  } else {
    // Dimming disabled: just obey brightness slider
    if (brightness == -1) {
      if (!displayOff) {
        Serial.println(F("[DISPLAY] Turning display OFF (brightness -1)"));
        P.displayShutdown(true);
        P.displayClear();
        displayOff = true;
        displayOffByBrightness = true;
        displayOffByDimming = false;
      }
    } else {
      if (displayOff && displayOffByBrightness) {
        Serial.println(F("[DISPLAY] Waking display (brightness changed)"));
        P.displayShutdown(false);
        displayOff = false;
        displayOffByBrightness = false;
      }
      P.setIntensity(brightness);
    }
  }

  // --- IMMEDIATE COUNTDOWN FINISH TRIGGER ---
  if (countdownEnabled && !countdownFinished && ntpSyncSuccessful && countdownTargetTimestamp > 0 && now_time >= countdownTargetTimestamp) {
    countdownFinished = true;
    displayMode = 3;  // Let main loop handle animation + TIMES UP
    countdownShowFinishedMessage = true;
    hourglassPlayed = false;
    countdownFinishedMessageStartTime = millis();

    Serial.println("[SYSTEM] Countdown target reached! Switching to Mode 3 to display finish sequence.");
    yield();
  }

  // --- IP Display ---
  if (showingIp) {
    if (P.displayAnimate()) {
      ipDisplayCount++;
      if (ipDisplayCount < ipDisplayMax) {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(pendingIpToShow.c_str(), PA_CENTER, actualScrollDirection, 120);
      } else {
        showingIp = false;
        P.displayClear();
        delay(500);  // Blocking delay as in working copy
        displayMode = 0;
        lastSwitch = millis();
      }
    }
    yield();
    return;  // Exit loop early if showing IP
  }

  // --- BRIGHTNESS/OFF CHECK ---
  if (brightness == -1) {
    if (!displayOff) {
      Serial.println(F("[DISPLAY] Turning display OFF"));
      P.displayShutdown(true);  // fully off
      P.displayClear();
      displayOff = true;
    }
    yield();
  }

  // --- NTP State Machine ---
  switch (ntpState) {
    case NTP_IDLE: break;
    case NTP_SYNCING:
      {
        time_t now = time(nullptr);
        if (now > 1000) {  // NTP sync successful
          Serial.println(F("[TIME] NTP sync successful."));
          ntpSyncSuccessful = true;
          ntpState = NTP_SUCCESS;
        } else if (millis() - ntpStartTime > ntpTimeout || ntpRetryCount >= maxNtpRetries) {
          Serial.println(F("[TIME] NTP sync failed."));
          ntpSyncSuccessful = false;
          ntpState = NTP_FAILED;
        } else {
          // Periodically print a more descriptive status message
          if (millis() - lastNtpStatusPrintTime >= ntpStatusPrintInterval) {
            Serial.printf("[TIME] NTP sync in progress (attempt %d of %d)...\n", ntpRetryCount + 1, maxNtpRetries);
            lastNtpStatusPrintTime = millis();
          }
          // Still increment ntpRetryCount based on your original timing for the timeout logic
          // (even if you don't print a dot for every increment)
          if (millis() - ntpStartTime > ((unsigned long)(ntpRetryCount + 1) * 1000UL)) {
            ntpRetryCount++;
          }
        }
        break;
      }
    case NTP_SUCCESS:
      if (!tzSetAfterSync) {
        const char *posixTz = ianaToPosix(timeZone);
        setenv("TZ", posixTz, 1);
        tzset();
        tzSetAfterSync = true;
      }
      ntpAnimTimer = 0;
      ntpAnimFrame = 0;
      break;

    case NTP_FAILED:
      ntpAnimTimer = 0;
      ntpAnimFrame = 0;

      static unsigned long lastNtpRetryAttempt = 0;
      static bool firstRetry = true;

      if (lastNtpRetryAttempt == 0) {
        lastNtpRetryAttempt = millis();  // set baseline on first fail
      }

      unsigned long ntpRetryInterval = firstRetry ? 30000UL : 300000UL;  // first retry after 30s, after that every 5 minutes

      if (millis() - lastNtpRetryAttempt > ntpRetryInterval) {
        lastNtpRetryAttempt = millis();
        ntpRetryCount = 0;
        ntpStartTime = millis();
        ntpState = NTP_SYNCING;
        Serial.println(F("[TIME] Retrying NTP sync..."));

        firstRetry = false;
      }
      break;
  }

  // Only advance mode by timer for clock/weather, not description!
  unsigned long displayDuration = (displayMode == 0) ? clockDuration : weatherDuration;
  if ((displayMode == 0 || displayMode == 1) && millis() - lastSwitch > displayDuration) {
    advanceDisplayMode();
  }

  // --- MODIFIED WEATHER FETCHING LOGIC ---
  if (WiFi.status() == WL_CONNECTED) {
    if (!weatherFetchInitiated || shouldFetchWeatherNow || (millis() - lastFetch > fetchInterval)) {
      if (shouldFetchWeatherNow) {
        Serial.println(F("[LOOP] Immediate weather fetch requested by web server."));
        shouldFetchWeatherNow = false;
      } else if (!weatherFetchInitiated) {
        Serial.println(F("[LOOP] Initial weather fetch."));
      } else {
        Serial.println(F("[LOOP] Regular interval weather fetch."));
      }
      weatherFetchInitiated = true;
      weatherFetched = false;
      fetchWeather();
      lastFetch = millis();
    }
  } else {
    weatherFetchInitiated = false;
    shouldFetchWeatherNow = false;
  }

  const char *const *daysOfTheWeek = getDaysOfWeek(language);
  const char *daySymbol = daysOfTheWeek[timeinfo.tm_wday];

  // build base HH:MM first ---
  char baseTime[9];
  if (twelveHourToggle) {
    int hour12 = timeinfo.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    sprintf(baseTime, "%d:%02d", hour12, timeinfo.tm_min);
  } else {
    sprintf(baseTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  }

  // add seconds only if colon blink enabled AND weekday hidden ---
  char timeWithSeconds[12];
  if (!showDayOfWeek && colonBlinkEnabled) {
    // Remove any leading space from baseTime
    const char *trimmedBase = baseTime;
    if (baseTime[0] == ' ') trimmedBase++;  // skip leading space
    sprintf(timeWithSeconds, "%s:%02d", trimmedBase, timeinfo.tm_sec);
  } else {
    strcpy(timeWithSeconds, baseTime);  // no seconds
  }

  // keep spacing logic the same ---
  char timeSpacedStr[24];
  int j = 0;
  for (int i = 0; timeWithSeconds[i] != '\0'; i++) {
    timeSpacedStr[j++] = timeWithSeconds[i];
    if (timeWithSeconds[i + 1] != '\0') {
      timeSpacedStr[j++] = ' ';
    }
  }
  timeSpacedStr[j] = '\0';

  // build final string ---
  String formattedTime;
  if (showDayOfWeek) {
    formattedTime = String(daySymbol) + "   " + String(timeSpacedStr);
  } else {
    formattedTime = String(timeSpacedStr);
  }

  unsigned long currentDisplayDuration = 0;
  if (displayMode == 0) {
    currentDisplayDuration = clockDuration;
  } else if (displayMode == 1) {  // Weather
    currentDisplayDuration = weatherDuration;
  }

  // Only advance mode by timer for clock/weather static (Mode 0 & 1).
  // Other modes (2, 3) have their own internal timers/conditions for advancement.
  if ((displayMode == 0 || displayMode == 1) && (millis() - lastSwitch > currentDisplayDuration)) {
    advanceDisplayMode();
  }

  // Persistent variables (declare near top of file or loop)
  static int prevDisplayMode = -1;
  static bool clockScrollDone = false;

  // === Custom Message Display (Mode 10) ===
  if (showCustomMessage) {
    if (showCustomMessage && millis() - customMessageStartTime < customMessageDuration) {
      P.setCharSpacing(1);

      if (customMessageScrolling) {
        static bool msgScrollInit = false;
        if (!msgScrollInit) {
          textEffect_t scrollDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
          P.displayScroll(customMessage.c_str(), PA_CENTER, scrollDir, GENERAL_SCROLL_SPEED);
          msgScrollInit = true;
        }
        if (P.displayAnimate()) {
          if (millis() - customMessageStartTime >= customMessageDuration) {
            showCustomMessage = false;
            msgScrollInit = false;
            displayMode = 0;
          } else {
            msgScrollInit = false; // Recommencer le scroll
          }
        }
      } else {
        P.setTextAlignment(PA_CENTER);
        P.print(customMessage.c_str());
      }
    } else {
      showCustomMessage = false;
      if (displayMode == 10) displayMode = 0;
    }
    yield();
    return;
  }

  // --- CLOCK Display Mode ---
  if (displayMode == 0) {
    P.setCharSpacing(0);

    // --- NTP SYNC ---
    if (ntpState == NTP_SYNCING) {
      if (ntpSyncSuccessful || ntpRetryCount >= maxNtpRetries || millis() - ntpStartTime > ntpTimeout) {
        ntpState = NTP_FAILED;
      } else if (millis() - ntpAnimTimer > 750) {
        ntpAnimTimer = millis();
        switch (ntpAnimFrame % 3) {
          case 0: P.print(F("S Y N C ®")); break;
          case 1: P.print(F("S Y N C ¯")); break;
          case 2: P.print(F("S Y N C °")); break;
        }
        ntpAnimFrame++;
      }
    }
    // --- NTP / WEATHER ERROR ---
    else if (!ntpSyncSuccessful) {
      P.setTextAlignment(PA_CENTER);
      static unsigned long errorAltTimer = 0;
      static bool showNtpError = true;

      if (!ntpSyncSuccessful && !weatherAvailable) {
        if (millis() - errorAltTimer > 2000) {
          errorAltTimer = millis();
          showNtpError = !showNtpError;
        }
        P.print(showNtpError ? F("?/") : F("?*"));
      } else if (!ntpSyncSuccessful) {
        P.print(F("?/"));
      } else if (!weatherAvailable) {
        P.print(F("?*"));
      }
    }
    // --- DISPLAY CLOCK ---
    else {
      String timeString = formattedTime;
      if (showDayOfWeek && colonBlinkEnabled && !colonVisible) {
        timeString.replace(":", " ");
      }

      // --- SCROLL IN ONLY WHEN COMING FROM SPECIFIC MODES OR FIRST BOOT ---
      bool shouldScrollIn = false;
      if (prevDisplayMode == -1 || prevDisplayMode == 3 || prevDisplayMode == 4) {
        shouldScrollIn = true;  // first boot or other special modes
      } else if (prevDisplayMode == 2 && weatherDescription.length() > 8) {
        shouldScrollIn = true;  // only scroll in if weather was scrolling
      }

      if (shouldScrollIn && !clockScrollDone) {
        textEffect_t inDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);

        P.displayText(
          timeString.c_str(),
          PA_CENTER,
          GENERAL_SCROLL_SPEED,
          0,
          inDir,
          PA_NO_EFFECT);
        while (!P.displayAnimate()) yield();
        clockScrollDone = true;  // mark scroll done
      } else {
        P.setTextAlignment(PA_CENTER);
        P.print(timeString);
      }
    }

    yield();
  } else {
    // --- leaving clock mode ---
    if (prevDisplayMode == 0) {
      clockScrollDone = false;  // reset for next time we enter clock
    }
  }

  // --- update prevDisplayMode ---
  prevDisplayMode = displayMode;

  // --- WEATHER Display Mode ---
  static bool weatherWasAvailable = false;
  if (displayMode == 1) {
    P.setCharSpacing(1);
    if (weatherAvailable) {
      String weatherDisplay;
      if (showHumidity && currentHumidity != -1) {
        int cappedHumidity = (currentHumidity > 99) ? 99 : currentHumidity;
        weatherDisplay = currentTemp + " " + String(cappedHumidity) + "%";
      } else {
        weatherDisplay = currentTemp + tempSymbol;
      }
      P.print(weatherDisplay.c_str());
      weatherWasAvailable = true;
    } else {
      if (weatherWasAvailable) {
        Serial.println(F("[DISPLAY] Weather not available, showing clock..."));
        weatherWasAvailable = false;
      }
      if (ntpSyncSuccessful) {
        String timeString = formattedTime;
        if (!colonVisible) timeString.replace(":", " ");
        P.setCharSpacing(0);
        P.print(timeString);
      } else {
        P.setCharSpacing(0);
        P.setTextAlignment(PA_CENTER);
        P.print(F("?*"));
      }
    }
    yield();
    return;
  }

  // --- WEATHER DESCRIPTION Display Mode ---
  if (displayMode == 2 && showWeatherDescription && weatherAvailable && weatherDescription.length() > 0) {
    String desc = weatherDescription;

    // prepare safe buffer
    static char descBuffer[128];  // large enough for OWM translations
    desc.toCharArray(descBuffer, sizeof(descBuffer));

    if (desc.length() > 8) {
      if (!descScrolling) {
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(descBuffer, PA_CENTER, actualScrollDirection, GENERAL_SCROLL_SPEED);
        descScrolling = true;
        descScrollEndTime = 0;  // reset end time at start
      }
      if (P.displayAnimate()) {
        if (descScrollEndTime == 0) {
          descScrollEndTime = millis();  // mark the time when scroll finishes
        }
        // wait small pause after scroll stops
        if (millis() - descScrollEndTime > descriptionScrollPause) {
          descScrolling = false;
          descScrollEndTime = 0;
          advanceDisplayMode();
        }
      } else {
        descScrollEndTime = 0;  // reset if not finished
      }
      yield();
      return;
    } else {
      if (descStartTime == 0) {
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(1);
        P.print(descBuffer);
        descStartTime = millis();
      }
      if (millis() - descStartTime > descriptionDuration) {
        descStartTime = 0;
        advanceDisplayMode();
      }
      yield();
      return;
    }
  }

  // --- Countdown Display Mode ---
  if (displayMode == 3 && countdownEnabled && ntpSyncSuccessful) {
    static int countdownSegment = 0;
    static unsigned long segmentStartTime = 0;
    const unsigned long SEGMENT_DISPLAY_DURATION = 1500;  // 1.5 seconds for each static segment

    long timeRemaining = countdownTargetTimestamp - now_time;

    // --- Countdown Finished Logic ---
    // This part of the code remains unchanged.
    if (timeRemaining <= 0 || countdownShowFinishedMessage) {
      // NEW: Only show "TIMES UP" if countdown target timestamp is valid and expired
      time_t now = time(nullptr);
      if (countdownTargetTimestamp == 0 || countdownTargetTimestamp > now) {
        // Target invalid or in the future, don't show "TIMES UP" yet, advance display instead
        countdownShowFinishedMessage = false;
        countdownFinished = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false;  // Reset if we decide not to show it
        Serial.println("[COUNTDOWN-FINISH] Countdown target invalid or not reached yet, skipping 'TIMES UP'. Advancing display.");
        advanceDisplayMode();
        yield();
        return;
      }

      // Define these static variables here if they are not global (or already defined in your loop())
      static const char *flashFrames[] = { "{|", "}~" };
      static unsigned long lastFlashingSwitch = 0;
      static int flashingMessageFrame = 0;

      // --- Initial Combined Sequence: Play Hourglass THEN start Flashing ---
      // This 'if' runs ONLY ONCE when the "finished" sequence begins.
      if (!hourglassPlayed) {                          // <-- This is the single entry point for the combined sequence
        countdownFinished = true;                      // Mark as finished overall
        countdownShowFinishedMessage = true;           // Confirm we are in the finished sequence
        countdownFinishedMessageStartTime = millis();  // Start the 15-second timer for the flashing duration

        // 1. Play Hourglass Animation (Blocking)
        const char *hourglassFrames[] = { "¡", "¢", "£", "¤" };
        for (int repeat = 0; repeat < 3; repeat++) {
          for (int i = 0; i < 4; i++) {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(0);
            P.print(hourglassFrames[i]);
            delay(350);  // This is blocking! (Total ~4.2 seconds for hourglass)
          }
        }
        Serial.println("[COUNTDOWN-FINISH] Played hourglass animation.");
        P.displayClear();  // Clear display after hourglass animation

        // 2. Initialize Flashing "TIMES UP" for its very first frame
        flashingMessageFrame = 0;
        lastFlashingSwitch = millis();  // Set initial time for first flash frame
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(0);
        P.print(flashFrames[flashingMessageFrame]);             // Display the first frame immediately
        flashingMessageFrame = (flashingMessageFrame + 1) % 2;  // Prepare for the next frame

        hourglassPlayed = true;  // <-- Mark that this initial combined sequence has completed!
        countdownSegment = 0;    // Reset segment counter after finished sequence initiation
        segmentStartTime = 0;    // Reset segment timer after finished sequence initiation
      }

      // --- Continue Flashing "TIMES UP" for its duration (after initial combined sequence) ---
      // This part runs in subsequent loop iterations after the hourglass has played.
      if (millis() - countdownFinishedMessageStartTime < 15000) {  // Flashing duration
        if (millis() - lastFlashingSwitch >= 500) {                // Check for flashing interval
          lastFlashingSwitch = millis();
          P.displayClear();
          P.setTextAlignment(PA_CENTER);
          P.setCharSpacing(0);
          P.print(flashFrames[flashingMessageFrame]);
          flashingMessageFrame = (flashingMessageFrame + 1) % 2;
        }
        P.displayAnimate();  // Ensure display updates
        yield();
        return;  // Stay in this mode until the 15 seconds are over
      } else {
        // 15 seconds are over, clean up and advance
        Serial.println("[COUNTDOWN-FINISH] Flashing duration over. Advancing to Clock.");
        countdownShowFinishedMessage = false;
        countdownFinishedMessageStartTime = 0;
        hourglassPlayed = false;  // <-- RESET this flag for the next countdown cycle!

        // Final cleanup (persisted)
        countdownEnabled = false;
        countdownTargetTimestamp = 0;
        countdownLabel[0] = '\0';
        saveCountdownConfig(false, 0, "");

        P.setInvert(false);
        advanceDisplayMode();
        yield();
        return;  // Exit loop after processing
      }
    }  // END of 'if (timeRemaining <= 0 || countdownShowFinishedMessage)'


    // --- NORMAL COUNTDOWN LOGIC ---
    // This 'else' block will only run if `timeRemaining > 0` and `!countdownShowFinishedMessage`
    else {

      // The new variable `isDramaticCountdown` toggles between the two modes
      if (isDramaticCountdown) {
        // --- EXISTING DRAMATIC COUNTDOWN LOGIC ---
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;
        String currentSegmentText = "";

        if (segmentStartTime == 0 || (millis() - segmentStartTime > SEGMENT_DISPLAY_DURATION)) {
          segmentStartTime = millis();
          P.displayClear();

          switch (countdownSegment) {
            case 0:  // Days
              if (days > 0) {
                currentSegmentText = String(days) + " " + (days == 1 ? "DAY" : "DAYS");
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
              } else {
                // Skip days if zero
                countdownSegment++;
                segmentStartTime = 0;
              }
              break;
            case 1:
              {  // Hours
                char buf[10];
                sprintf(buf, "%02ld HRS", hours);  // pad hours with 0
                currentSegmentText = String(buf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
                break;
              }
            case 2:
              {  // Minutes
                char buf[10];
                sprintf(buf, "%02ld MINS", minutes);  // pad minutes with 0
                currentSegmentText = String(buf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment %d: %s\n", countdownSegment, currentSegmentText.c_str());
                countdownSegment++;
                break;
              }
            case 3:
              {  // Seconds & Label Scroll
                time_t segmentStartTime = time(nullptr);
                unsigned long segmentStartMillis = millis();

                long nowRemaining = countdownTargetTimestamp - segmentStartTime;
                long currentSecond = nowRemaining % 60;
                char secondsBuf[10];
                sprintf(secondsBuf, "%02ld %s", currentSecond, currentSecond == 1 ? "SEC" : "SECS");
                String secondsText = String(secondsBuf);
                Serial.printf("[COUNTDOWN-STATIC] Displaying segment 3: %s\n", secondsText.c_str());
                P.displayClear();
                P.setTextAlignment(PA_CENTER);
                P.setCharSpacing(1);
                P.print(secondsText.c_str());
                delay(SEGMENT_DISPLAY_DURATION - 400);

                unsigned long elapsed = millis() - segmentStartMillis;
                long adjustedSecond = (countdownTargetTimestamp - segmentStartTime - (elapsed / 1000)) % 60;
                sprintf(secondsBuf, "%02ld %s", adjustedSecond, adjustedSecond == 1 ? "SEC" : "SECS");
                secondsText = String(secondsBuf);
                P.displayClear();
                P.setTextAlignment(PA_CENTER);
                P.setCharSpacing(1);
                P.print(secondsText.c_str());
                delay(400);

                String label;
                if (strlen(countdownLabel) > 0) {
                  label = String(countdownLabel);
                  label.trim();
                  if (!label.startsWith("TO:") && !label.startsWith("to:")) {
                    label = "TO: " + label;
                  }
                  label.replace('.', ',');
                } else {
                  static const char *fallbackLabels[] = {
                    "TO: PARTY TIME!", "TO: SHOWTIME!", "TO: CLOCKOUT!", "TO: BLASTOFF!",
                    "TO: GO TIME!", "TO: LIFTOFF!", "TO: THE BIG REVEAL!",
                    "TO: ZERO HOUR!", "TO: THE FINAL COUNT!", "TO: MISSION COMPLETE"
                  };
                  int randomIndex = random(0, 10);
                  label = fallbackLabels[randomIndex];
                }

                P.setTextAlignment(PA_LEFT);
                P.setCharSpacing(1);
                textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
                P.displayScroll(label.c_str(), PA_LEFT, actualScrollDirection, GENERAL_SCROLL_SPEED);

                while (!P.displayAnimate()) {
                  yield();
                }
                countdownSegment++;
                segmentStartTime = millis();
                break;
              }
            case 4:  // Exit countdown
              Serial.println("[COUNTDOWN-STATIC] All segments and label displayed. Advancing to Clock.");
              countdownSegment = 0;
              segmentStartTime = 0;
              P.setTextAlignment(PA_CENTER);
              P.setCharSpacing(1);
              advanceDisplayMode();
              yield();
              return;

            default:
              Serial.println("[COUNTDOWN-ERROR] Invalid countdownSegment, resetting.");
              countdownSegment = 0;
              segmentStartTime = 0;
              break;
          }

          if (currentSegmentText.length() > 0) {
            P.setTextAlignment(PA_CENTER);
            P.setCharSpacing(1);
            P.print(currentSegmentText.c_str());
          }
        }
        P.displayAnimate();
      }

      // --- NEW: SINGLE-LINE COUNTDOWN LOGIC ---
      else {
        long days = timeRemaining / (24 * 3600);
        long hours = (timeRemaining % (24 * 3600)) / 3600;
        long minutes = (timeRemaining % 3600) / 60;
        long seconds = timeRemaining % 60;

        String label;
        // Check if countdownLabel is empty and grab a random one if needed
        if (strlen(countdownLabel) > 0) {
          label = String(countdownLabel);
          label.trim();
        } else {
          static const char *fallbackLabels[] = {
            "PARTY TIME", "SHOWTIME", "CLOCKOUT", "BLASTOFF",
            "GO TIME", "LIFTOFF", "THE BIG REVEAL",
            "ZERO HOUR", "THE FINAL COUNT", "MISSION COMPLETE"
          };
          int randomIndex = random(0, 10);
          label = fallbackLabels[randomIndex];
        }

        // Format the full string
        char buf[50];
        // Only show days if there are any, otherwise start with hours
        if (days > 0) {
          sprintf(buf, "%s IN: %ldD %02ldH %02ldM %02ldS", label.c_str(), days, hours, minutes, seconds);
        } else {
          sprintf(buf, "%s IN: %02ldH %02ldM %02ldS", label.c_str(), hours, minutes, seconds);
        }

        String fullString = String(buf);

        // Display the full string and scroll it
        P.setTextAlignment(PA_LEFT);
        P.setCharSpacing(1);
        textEffect_t actualScrollDirection = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(fullString.c_str(), PA_LEFT, actualScrollDirection, GENERAL_SCROLL_SPEED);

        // Blocking loop to ensure the full message scrolls
        while (!P.displayAnimate()) {
          yield();
        }

        // After scrolling is complete, we're done with this display mode
        // Move to the next mode and exit the function.
        P.setTextAlignment(PA_CENTER);
        advanceDisplayMode();
        yield();
        return;
      }
    }

    // Keep alignment reset just in case
    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(1);
    yield();
    return;
  }  // End of if (displayMode == 3 && ...)

  // --- NIGHTSCOUT Display Mode ---
  if (displayMode == 4) {
    String ntpField = String(ntpServer2);

    // These static variables will retain their values between calls to this block
    static unsigned long lastNightscoutFetchTime = 0;
    const unsigned long NIGHTSCOUT_FETCH_INTERVAL = 150000;  // 2.5 minutes
    static int currentGlucose = -1;
    static String currentDirection = "?";

    // Check if it's time to fetch new data or if we have no data yet
    if (currentGlucose == -1 || millis() - lastNightscoutFetchTime >= NIGHTSCOUT_FETCH_INTERVAL) {
      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient https;
      https.begin(client, ntpField);
      #ifdef ESP32
        https.setConnectTimeout(5000);
        https.setTimeout(5000);
      #else  // ESP8266
        client.setBufferSizes(512, 512);
        https.setTimeout(5000);  // Sets both connection and response timeout
      #endif

      Serial.print("[HTTPS] Nightscout fetch initiated...\n");
      int httpCode = https.GET();

      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.is<JsonArray>() && doc.size() > 0) {
          JsonObject firstReading = doc[0].as<JsonObject>();
          currentGlucose = firstReading["glucose"] | firstReading["sgv"] | -1;
          currentDirection = firstReading["direction"] | "?";

          Serial.printf("Nightscout data fetched: mg/dL %d %s\n", currentGlucose, currentDirection.c_str());
        } else {
          Serial.println("Failed to parse Nightscout JSON");
        }
      } else {
        Serial.printf("[HTTPS] GET failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
      client.stop();
      lastNightscoutFetchTime = millis();  // Update the timestamp
    }

    // Display the data we have, which is now stored in static variables
    if (currentGlucose != -1) {
      char arrow;
      if (currentDirection == "Flat") arrow = 139;
      else if (currentDirection == "SingleUp") arrow = 134;
      else if (currentDirection == "DoubleUp") arrow = 135;
      else if (currentDirection == "SingleDown") arrow = 136;
      else if (currentDirection == "DoubleDown") arrow = 137;
      else if (currentDirection == "FortyFiveUp") arrow = 138;
      else if (currentDirection == "FortyFiveDown") arrow = 140;
      else arrow = '?';

      String displayText = String(currentGlucose) + String(arrow);

      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(1);
      P.print(displayText.c_str());

      delay(weatherDuration);
      advanceDisplayMode();
      return;
    } else {
      // If no data is available after the first fetch attempt, show an error and advance
      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(0);
      P.print(F("?)"));
      delay(2000);  // Wait 2 seconds before advancing
      advanceDisplayMode();
      return;
    }
  }

  // --- YOUTUBE Display Mode ---
  else if (displayMode == 6 && youtubeEnabled) {
    if (currentSubscriberCount.length() == 0 ||
        millis() - lastYoutubeFetch > youtubeFetchInterval) {
      fetchYoutubeSubscribers();
    }

    if (currentSubscriberCount.length() > 0 &&
        currentSubscriberCount != "---" &&
        currentSubscriberCount != "ERR") {

      String display = "YT: " + currentSubscriberCount;

      if (display.length() > 10) {
        static bool youtubeScrolling = false;
        static unsigned long youtubeScrollEndTime = 0;

        if (!youtubeScrolling) {
          P.setCharSpacing(1);
          textEffect_t scrollDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
          P.displayScroll(display.c_str(), PA_CENTER, scrollDir, GENERAL_SCROLL_SPEED);
          youtubeScrolling = true;
          youtubeScrollEndTime = 0;
        }

        if (P.displayAnimate()) {
          if (youtubeScrollEndTime == 0) {
            youtubeScrollEndTime = millis();
          }
          if (millis() - youtubeScrollEndTime > 300) {
            youtubeScrolling = false;
            youtubeScrollEndTime = 0;
            advanceDisplayMode();
          }
        }
      } else {
        P.setTextAlignment(PA_CENTER);
        P.setCharSpacing(1);
        P.print(display.c_str());

        if (millis() - lastSwitch > weatherDuration) {
          advanceDisplayMode();
        }
      }
    } else {
      P.setTextAlignment(PA_CENTER);
      P.setCharSpacing(1);

      if (currentSubscriberCount == "ERR") {
        P.print("YT: ERR");
      } else {
        P.print("YT: ---");
      }

      if (millis() - lastSwitch > 2000) {
        advanceDisplayMode();
      }
    }

    yield();
    return;
  }

  // --- WEBHOOK Display Mode ---
  else if (displayMode == 11 && processingWebhook) {
    P.setCharSpacing(1);

    if (currentWebhookMessage.scroll) {
      static bool webhookScrollInit = false;
      if (!webhookScrollInit) {
        textEffect_t scrollDir = getEffectiveScrollDirection(PA_SCROLL_LEFT, flipDisplay);
        P.displayScroll(currentWebhookMessage.text.c_str(), PA_CENTER,
                       scrollDir, GENERAL_SCROLL_SPEED);
        webhookScrollInit = true;
      }

      if (P.displayAnimate()) {
        if (millis() - lastSwitch >= currentWebhookMessage.duration) {
          webhookScrollInit = false;
          processingWebhook = false;
          advanceDisplayMode();
        } else {
          webhookScrollInit = false; // Re-scroll
        }
      }
    } else {
      P.setTextAlignment(PA_CENTER);
      P.print(currentWebhookMessage.text.c_str());

      if (millis() - lastSwitch >= currentWebhookMessage.duration) {
        processingWebhook = false;
        advanceDisplayMode();
      }
    }

    yield();
    return;
  }

  //DATE Display Mode
  else if (displayMode == 5 && showDate) {

    // --- VALID DATE CHECK ---
    if (timeinfo.tm_year < 120 || timeinfo.tm_mday <= 0 || timeinfo.tm_mon < 0 || timeinfo.tm_mon > 11) {
      advanceDisplayMode();
      return;  // skip drawing
    }
    // -------------------------
    String dateString;

    // Get localized month names
    const char *const *months = getMonthsOfYear(language);
    String monthAbbr = String(months[timeinfo.tm_mon]).substring(0, 5);
    monthAbbr.toLowerCase();

    // Add spaces between day digits
    String dayString = String(timeinfo.tm_mday);
    String spacedDay = "";
    for (size_t i = 0; i < dayString.length(); i++) {
      spacedDay += dayString[i];
      if (i < dayString.length() - 1) spacedDay += " ";
    }

    // Function to check if day should come first for given language
    auto isDayFirst = [](const String &lang) {
      // Languages with DD-MM order
      const char *dayFirstLangs[] = {
        "af",  // Afrikaans
        "cs",  // Czech
        "da",  // Danish
        "de",  // German
        "eo",  // Esperanto
        "es",  // Spanish
        "et",  // Estonian
        "fi",  // Finnish
        "fr",  // French
        "ga",  // Irish
        "hr",  // Croatian
        "hu",  // Hungarian
        "it",  // Italian
        "lt",  // Lithuanian
        "lv",  // Latvian
        "nl",  // Dutch
        "no",  // Norwegian
        "pl",  // Polish
        "pt",  // Portuguese
        "ro",  // Romanian
        "ru",  // Russian
        "sk",  // Slovak
        "sl",  // Slovenian
        "sr",  // Serbian
        "sv",  // Swedish
        "sw",  // Swahili
        "tr"   // Turkish
      };
      for (auto lf : dayFirstLangs) {
        if (lang.equalsIgnoreCase(lf)) {
          return true;
        }
      }
      return false;
    };

    String langForDate = String(language);

    if (langForDate == "ja") {
      // Japanese: month number (spaced digits) + day + symbol
      String spacedMonth = "";
      String monthNum = String(timeinfo.tm_mon + 1);
      dateString = monthAbbr + "  " + spacedDay + " ±";

    } else {
      if (isDayFirst(language)) {
        dateString = spacedDay + "   " + monthAbbr;
      } else {
        dateString = monthAbbr + "   " + spacedDay;
      }
    }

    P.setTextAlignment(PA_CENTER);
    P.setCharSpacing(0);
    P.print(dateString);

    if (millis() - lastSwitch > weatherDuration) {
      advanceDisplayMode();
    }
  }
  yield();
}
