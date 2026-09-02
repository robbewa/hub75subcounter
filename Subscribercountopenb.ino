#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>

const char* ssid     = "jouw wifi netwerk";
const char* password = "jouw wifi wachtwoord";

const char* apiKey    = "jouw api key";
const char* channelId = "jouwchannel id"; 

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* timeZone   = "CET-1CEST,M3.5.0,M10.5.0/3"; 

const int AAN_UUR = 10;
const int UIT_UUR = 22;
const uint8_t MAX_HELDERHEID = 40;

#define PANEL_RES_X 128
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *dma_display = nullptr;

String vorigAantalSubs = ""; 
bool schermActief      = true;

void setTimeFromNTP() {
  configTzTime(timeZone, ntpServer1, ntpServer2);
  Serial.print("Wachten op NTP tijd-synchronisatie");
  
  struct tm timeinfo;
  int counter = 0;
  while (!getLocalTime(&timeinfo) && counter < 30) {
    delay(500);
    Serial.print(".");
    counter++;
  }
  Serial.println("\nTijd gesynchroniseerd!");
}

bool isDisplayTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[WARN] Kon lokale tijd niet ophalen. Scherm blijft aan.");
    return true;
  }

  int huidigUur = timeinfo.tm_hour;
  return (huidigUur >= AAN_UUR && huidigUur < UIT_UUR);
}

String getSubscriberCount() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] Geen WiFi verbinding!");
    return "NoWiFi";
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  String requestURL = "https://www.googleapis.com/youtube/v3/channels?part=statistics&id=" + String(channelId) + "&key=" + String(apiKey);

  http.begin(client, requestURL);
  int httpCode = http.GET();
  String subs = "Err";

  Serial.print("[HTTP] Response code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc["items"].size() > 0) {
        subs = doc["items"][0]["statistics"]["subscriberCount"].as<String>();
        Serial.print("[SUCCESS] Subs: ");
        Serial.println(subs);
      } else {
        Serial.println("[ERROR] JSON Parsing mislukt of kanaal niet gevonden.");
      }
    } else {
      Serial.print("[ERROR] Server weigert verzoek, HTTP code: ");
      Serial.println(httpCode);
    }
  } else {
    Serial.print("[ERROR] Verbinding mislukt, error: ");
    Serial.println(http.errorToString(httpCode));
  }
  
  http.end();
  return subs;
}

void drawYouTubeLogo(int x, int y) {
  uint16_t red   = dma_display->color565(255, 0, 0);
  uint16_t white = dma_display->color565(255, 255, 255);

  int logoW = 36;
  int logoH = 24;
  int radius = 5;

  dma_display->fillRect(x + radius, y, logoW - 2 * radius, logoH, red);
  dma_display->fillRect(x, y + radius, logoW, logoH - 2 * radius, red);
  dma_display->fillCircle(x + radius, y + radius, radius, red);
  dma_display->fillCircle(x + logoW - radius - 1, y + radius, radius, red);
  dma_display->fillCircle(x + radius, y + logoH - radius - 1, radius, red);
  dma_display->fillCircle(x + logoW - radius - 1, y + logoH - radius - 1, radius, red);

  int centerX = x + logoW / 2;
  int centerY = y + logoH / 2;
  int triHalfHeight = 5;
  int triHalfWidth  = 5;

  dma_display->fillTriangle(
    centerX - triHalfWidth / 2, centerY - triHalfHeight,
    centerX - triHalfWidth / 2, centerY + triHalfHeight,
    centerX + triHalfWidth,     centerY,
    white
  );
}

void updateDisplay(String subs) {
  dma_display->clearScreen();
  drawYouTubeLogo(12, 20);

  dma_display->setTextWrap(false);
  dma_display->setTextSize(2);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));
  dma_display->setCursor(54, 25);
  dma_display->print(subs);

  dma_display->setTextSize(1);
  dma_display->setTextColor(dma_display->color565(255, 0, 0));
  dma_display->setCursor(54, 43);
  dma_display->print("SUBS");
}

void setup() {
  delay(2000);
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins custom_pins = {
    26, 27, 25,
    12, 13, 14,
    33, 32, 22,
    19, 23,    
    2,  15, 4  
  };

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN,
    custom_pins, HUB75_I2S_CFG::SHIFTREG
  );

  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.double_buff = false;
  mxconfig.clkphase = false;
  mxconfig.latch_blanking = 4;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  
  dma_display->setBrightness8(MAX_HELDERHEID);
  dma_display->clearScreen();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Verbinden met WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi verbonden!");
  Serial.print("IP Adres: ");
  Serial.println(WiFi.localIP());

  setTimeFromNTP();
}

void loop() {
  bool moetAanStaan = isDisplayTime();

  if (moetAanStaan) {
    if (!schermActief) {
      Serial.println("[TIMER] Scherm gaat AAN (10:00 - 22:00)");
      dma_display->setBrightness8(MAX_HELDERHEID);
      schermActief = true;
      vorigAantalSubs = "";
    }

    String subs = getSubscriberCount();

    if (subs != vorigAantalSubs && subs != "Err") {
      updateDisplay(subs);
      vorigAantalSubs = subs; 
    }

  } else {
    if (schermActief) {
      Serial.println("[TIMER] Scherm gaat UIT (Nachtstand)");
      dma_display->clearScreen();
      dma_display->setBrightness8(0);
      schermActief = false;
      vorigAantalSubs = ""; 
    }
    Serial.println("[INFO] Scherm staat uit vanwege de timer.");
  }

  delay(60 * 1000);
}