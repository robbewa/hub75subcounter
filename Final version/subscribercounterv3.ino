#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <time.h>
#include "logorond128x64.h"

const char* ssid     = "jouw wifi netwerk";
const char* password = "jouw wifi wachtwoord";

const char* apiKey    = "jouw api key";
const char* channelId = "jouw channel id";

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* timeZone   = "CET-1CEST,M3.5.0,M10.5.0/3";

const int AAN_UUR  = 9;
const int UIT_UUR  = 22;
const uint8_t MAX_HELDERHEID = 15;
const uint8_t FIREWORK_HELDERHEID = 20; 
const unsigned long FETCH_INTERVAL = 600000;
const unsigned long IDLE_DELAY = 50; 

#define PANEL_RES_X 128
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

const int TEXT_AREA_X = 54;
const int TEXT_AREA_W = PANEL_RES_X - TEXT_AREA_X;

MatrixPanel_I2S_DMA *dma_display = nullptr;
uint32_t vorigAantalSubs = 0;
bool hasSubs = false;
bool schermActief = true;
unsigned long lastFetch = 0;
uint32_t currentSubs = 0;

bool fireworkActive = false;
unsigned long fireworkStartTime = 0;
const unsigned long FIREWORK_DURATION = 30000;
const unsigned long FIREWORK_FRAME_DELAY = 33;
unsigned long lastFrame = 0;
bool fireworkStopping = false;
bool rocketActive = false;
unsigned long nextRocketDelay = 0;

bool testModeActive = false;
int simulatedSubsIncrement = 1;

bool debugNetwork = true;
const unsigned long NETWORK_RETRY_DELAY = 600000;
unsigned long lastNetworkFailure = 0;

struct FireworkParticle {
  float x;
  float y;
  float dx;
  float dy;
  uint16_t color;
  bool alive;
  int prev_x;
  int prev_y;
};

const int MAX_PARTICLES = 250; 
FireworkParticle particles[MAX_PARTICLES];
int particleCount = 0;

int launchX = PANEL_RES_X / 2;
int rocketY = PANEL_RES_Y - 1;
int rocketPrevY = -1;
int normalRocketDestY = 15;
unsigned long phaseStartTime = 0;
int phase = 0; 

const int FINALE_ROCKETS = 5;
struct FinaleRocket {
  int x;
  int y;
  int prevY;
  int destY;
  bool active;
};
FinaleRocket finaleRockets[FINALE_ROCKETS];

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void drawClockArea(bool forceRedraw = false) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  static int lastSec = -1;
  bool timeChanged = (timeinfo.tm_sec != lastSec);
  
  if (!forceRedraw && !timeChanged) return;
  lastSec = timeinfo.tm_sec;

  const int margin = 3;
  const int textHeight = 7;
  const int baseY = PANEL_RES_Y - margin - textHeight;

  const int textWidth = 29;
  const int timeX = PANEL_RES_X - margin - textWidth;

  if (timeChanged) {
    dma_display->fillRect(timeX, baseY, textWidth, textHeight, 0x0000);
  }

  dma_display->fillRect(margin, baseY, 2, 7, rgb565(255, 255, 255));
  dma_display->fillRect(margin + 3, baseY, 2, 7, rgb565(255, 255, 255));

  char timeText[6];
  snprintf(timeText, sizeof(timeText), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  dma_display->setTextSize(1);
  dma_display->setCursor(timeX, baseY);
  dma_display->setTextColor(rgb565(255, 255, 255));
  dma_display->print(timeText);

  const int barStartX = margin + 5 + 4;
  const int barEndX = timeX - 4;
  const int barWidth = barEndX - barStartX; 
  const int barY = baseY + 3;
  
  int elapsedSeconds = timeinfo.tm_min * 60 + timeinfo.tm_sec;
  int fillWidth = map(elapsedSeconds, 0, 3600, 0, barWidth);
  fillWidth = constrain(fillWidth, 0, barWidth);

  dma_display->fillRect(barStartX, barY, barWidth, 2, rgb565(32, 32, 48));
  if (fillWidth > 0) {
    dma_display->fillRect(barStartX, barY, fillWidth, 2, rgb565(230, 33, 23));
  }

  dma_display->setTextSize(2); 
}

void restoreBackgroundPixel(int x, int y) {
  if (x < 0 || x >= PANEL_RES_X || y < 0 || y >= PANEL_RES_Y) return;

  if (x < TEXT_AREA_X) {
    int logo_x = x + 3;
    int logo_y = y;
    
    if (logo_x >= 0 && logo_x < 128 && logo_y >= 0 && logo_y < 64) {
      uint16_t color = logorond128x64[logo_y * 128 + logo_x];
      dma_display->drawPixel(x, y, color);
    } else {
      dma_display->drawPixel(x, y, 0x0000);
    }
  } else {
    dma_display->drawPixel(x, y, 0x0000);
  }
}

void startRocket() {
  launchX = random(10, PANEL_RES_X - 10); 
  rocketY = PANEL_RES_Y - 1;
  rocketPrevY = -1;
  normalRocketDestY = random(5, 25); 
  rocketActive = true;
  phase = 1;
  phaseStartTime = millis();
}

void addExplosion(int centerX, int centerY, int count) {
  for (int i = 0; i < count; i++) {
    if (particleCount >= MAX_PARTICLES) break;
    
    float angle = random(0, 360) * 0.0174533;
    float speed = random(40, 90) / 40.0;
    uint16_t color = rgb565(random(200, 255), random(80, 255), random(0, 255));

    particles[particleCount++] = {
      (float)centerX,
      (float)centerY,
      (float)(cos(angle) * speed * 1.5), 
      (float)(sin(angle) * speed),
      color,
      true,
      centerX,
      centerY
    };
  }
}

void startExplosion(int centerX, int centerY, int count) {
  phaseStartTime = millis();
  particleCount = 0; 
  addExplosion(centerX, centerY, count);
  phase = 2;
}

void startGrandFinale() {
  phaseStartTime = millis();
  particleCount = 0; 
  
  int sectorWidth = PANEL_RES_X / FINALE_ROCKETS; 
  
  for (int i = 0; i < FINALE_ROCKETS; i++) {
    finaleRockets[i].x = random(i * sectorWidth + 4, (i + 1) * sectorWidth - 4);
    finaleRockets[i].y = PANEL_RES_Y - 1;
    finaleRockets[i].prevY = -1;
    finaleRockets[i].destY = random(5, 22); 
    finaleRockets[i].active = true;
  }
  
  phase = 4; 
}

void drawRocketTrail(int x, int y) {
  for (int i = 0; i < 4; i++) {
    int ty = y + i;
    if (ty < PANEL_RES_Y) {
      uint16_t color = rgb565(255, 180 - i * 40, 60);
      dma_display->drawPixel(x, ty, color);
    }
  }
}

void updateExplosion() {
  for (int i = 0; i < particleCount; i++) {
    if (particles[i].alive || particles[i].prev_x != -1) {
      if (particles[i].prev_x >= 0) {
        restoreBackgroundPixel(particles[i].prev_x, particles[i].prev_y);
      }
    }
  }

  bool anyAlive = false;
  for (int i = 0; i < particleCount; i++) {
    if (!particles[i].alive) {
      particles[i].prev_x = -1;
      continue;
    }

    particles[i].x += particles[i].dx;
    particles[i].y += particles[i].dy;
    particles[i].dx *= 0.95; 
    particles[i].dy += 0.12; 

    int nx = round(particles[i].x);
    int ny = round(particles[i].y);

    if (nx < 0 || nx >= PANEL_RES_X || ny < 0 || ny >= PANEL_RES_Y) {
      particles[i].alive = false;
      particles[i].prev_x = -1; 
    } else {
      particles[i].prev_x = nx;
      particles[i].prev_y = ny;
      
      if (random(0, 100) < 10) {
        particles[i].color = rgb565(random(180, 255), random(80, 255), random(0, 255));
      }
      anyAlive = true;
    }
  }
}

bool anyExplosionAlive() {
  for (int i = 0; i < particleCount; i++) {
    if (particles[i].alive) return true;
  }
  return false;
}

void drawExplosion() {
  for (int i = 0; i < particleCount; i++) {
    if (particles[i].alive) {
      dma_display->drawPixel(particles[i].prev_x, particles[i].prev_y, particles[i].color);
    }
  }
}

void drawTextOnly(uint32_t subs, bool valid) {
  char text[16];
  if (!valid) {
    strcpy(text, "--");
  } else {
    snprintf(text, sizeof(text), "%u", subs);
  }
  dma_display->setCursor(54, 13);
  dma_display->print(text);
  dma_display->setCursor(54, 35);
  dma_display->print("SUBS");
}

void drawTextArea(uint32_t subs, bool valid) {
  char text[16];
  if (!valid) {
    strcpy(text, "--");
  } else {
    snprintf(text, sizeof(text), "%u", subs);
  }
  
  dma_display->fillRect(TEXT_AREA_X, 0, TEXT_AREA_W, 53, 0x0000);
  dma_display->setCursor(54, 13);
  dma_display->print(text);
  dma_display->setCursor(54, 35);
  dma_display->print("SUBS");
}

void restoreFullCleanScreen(uint32_t subs, bool valid) {
  dma_display->fillScreen(0x0000);
  dma_display->drawRGBBitmap(-3, 0, logorond128x64, PANEL_RES_X, PANEL_RES_Y);
  drawTextArea(subs, valid);
  drawClockArea(true);
}

void startFirework() {
  if (fireworkActive) return;

  fireworkActive = true;
  fireworkStopping = false;
  fireworkStartTime = millis();
  lastFrame = 0;
  phase = 0;
  dma_display->setBrightness8(FIREWORK_HELDERHEID);
  
  drawTextArea(currentSubs, hasSubs); 
  startRocket();
  lastFetch = millis();
}

void renderFireworkFrame(uint32_t subs, bool valid) {
  unsigned long now = millis();
  if (now - lastFrame < FIREWORK_FRAME_DELAY) return;
  lastFrame = now;

  if (phase == 1) {
    if (rocketPrevY != -1) {
      for (int i = 0; i < 4; i++) {
        int ty = rocketPrevY + i;
        if (ty < PANEL_RES_Y) restoreBackgroundPixel(launchX, ty);
      }
    }

    rocketPrevY = rocketY;
    rocketY -= 2;

    if (rocketY >= normalRocketDestY) {
      drawRocketTrail(launchX, rocketY);
      dma_display->drawPixel(launchX, rocketY, rgb565(255, 255, 0));
    } else {
      rocketActive = false;
      for (int i = 0; i < 4; i++) {
        int ty = rocketPrevY + i;
        if (ty < PANEL_RES_Y) restoreBackgroundPixel(launchX, ty);
      }
      startExplosion(launchX, rocketY, 48); 
    }
  } 
  else if (phase == 2) {
    updateExplosion(); 
    drawExplosion();   
    
    if (!anyExplosionAlive()) {
      phase = 3;
      phaseStartTime = millis();
      nextRocketDelay = random(50, 250); 
    }
  } 
  else if (phase == 3) { 
    if (now - fireworkStartTime >= FIREWORK_DURATION && !fireworkStopping) {
      fireworkStopping = true;
      startGrandFinale(); 
      return;
    }

    unsigned long elapsed = now - phaseStartTime;
    if (elapsed >= nextRocketDelay) {
      startRocket();
    }
  }
  else if (phase == 4) { 
    bool anyRocketMoving = false;

    for (int i = 0; i < FINALE_ROCKETS; i++) {
      if (!finaleRockets[i].active) continue;

      if (finaleRockets[i].prevY != -1) {
        for (int t = 0; t < 4; t++) {
          int ty = finaleRockets[i].prevY + t;
          if (ty < PANEL_RES_Y) restoreBackgroundPixel(finaleRockets[i].x, ty);
        }
      }

      finaleRockets[i].prevY = finaleRockets[i].y;
      finaleRockets[i].y -= 2; 

      if (finaleRockets[i].y >= finaleRockets[i].destY) {
        drawRocketTrail(finaleRockets[i].x, finaleRockets[i].y);
        dma_display->drawPixel(finaleRockets[i].x, finaleRockets[i].y, rgb565(255, 255, 0));
        anyRocketMoving = true;
      } else {
        finaleRockets[i].active = false;
        for (int t = 0; t < 4; t++) {
          int ty = finaleRockets[i].prevY + t;
          if (ty < PANEL_RES_Y) restoreBackgroundPixel(finaleRockets[i].x, ty);
        }
        addExplosion(finaleRockets[i].x, finaleRockets[i].destY, 48);
      }
    }

    updateExplosion(); 
    drawExplosion();

    if (!anyRocketMoving) {
      phase = 5; 
    }
  }
  else if (phase == 5) { 
    updateExplosion();
    drawExplosion();

    if (!anyExplosionAlive()) {
      fireworkActive = false;
      phase = 0;
      dma_display->setBrightness8(MAX_HELDERHEID);

      if (testModeActive) {
        testModeActive = false;
        currentSubs = vorigAantalSubs; 
      } else {
        if (valid) vorigAantalSubs = currentSubs;
      }

      drawTextArea(currentSubs, valid); 
    }
  }

  if (phase >= 1 && phase <= 5) {
    drawTextOnly(subs, valid);
    drawClockArea(true);
  }
}

void startTestSubscriber() {
  if (!hasSubs) return;
  testModeActive = true;
  Serial.println("[TEST] Test-subscriber geactiveerd voor 30s");
  
  currentSubs = vorigAantalSubs + simulatedSubsIncrement;
  if (!fireworkActive) startFirework();
}

void stopTestSubscriber() {
  if (!testModeActive) return;
  
  testModeActive = false;
  fireworkActive = false; 
  Serial.println("[TEST] Test-subscriber gedeactiveerd. Echte teller wordt direct teruggezet...");
  
  currentSubs = vorigAantalSubs; 
  dma_display->setBrightness8(MAX_HELDERHEID);
  drawTextArea(currentSubs, hasSubs);
}

void checkSerialCommands() {
  if (!Serial || !Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  
  if (cmd.equalsIgnoreCase("test")) {
    startTestSubscriber();
    return;
  }
  
  if (cmd.equalsIgnoreCase("test off") || cmd.equalsIgnoreCase("stop test")) {
    stopTestSubscriber();
    return;
  }

  if (cmd.equalsIgnoreCase("debug on") || cmd.equalsIgnoreCase("debug on\r")) {
    debugNetwork = true;
    Serial.println("[DEBUG] Network debug ON");
    return;
  }
  if (cmd.equalsIgnoreCase("debug off") || cmd.equalsIgnoreCase("debug off\r")) {
    debugNetwork = false;
    Serial.println("[DEBUG] Network debug OFF");
    return;
  }
}

void setTimeFromNTP() {
  configTzTime(timeZone, ntpServer1, ntpServer2);
  Serial.print("Wachten op NTP tijd-synchronisatie");

  struct tm timeinfo;
  int counter = 0;
  while (!getLocalTime(&timeinfo) && counter < 30) {
    delay(100);
    yield();
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

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > 10000) {
      Serial.println("[WARN] WiFi connect timeout");
      return false;
    }
    delay(100);
    yield();
  }
  Serial.println("\nWiFi verbonden!");
  Serial.print("IP Adres: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool getSubscriberCount(uint32_t &subs) {
  if (!connectWiFi()) {
    Serial.println("[ERROR] Geen WiFi verbinding!");
    return false;
  }

  if (millis() - lastNetworkFailure < NETWORK_RETRY_DELAY && lastNetworkFailure != 0 && hasSubs) {
    Serial.println("[WARN] Google HTTPS is recentelijk geblokkeerd; toon laatst bekende waarde.");
    subs = vorigAantalSubs;
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); 
  client.setTimeout(15000);

  HTTPClient http;
  http.setTimeout(15000);
  
  String requestURL = String("https://www.googleapis.com/youtube/v3/channels?part=statistics&id=") + channelId + "&key=" + apiKey;
  requestURL.reserve(220); 
  
  if (!http.begin(client, requestURL)) {
    Serial.println("[ERROR] Kon HTTP verbinding niet initialiseren.");
    if (hasSubs) subs = vorigAantalSubs;
    return false;
  }

  int httpCode = http.GET();
  if (debugNetwork) {
    Serial.print("[HTTP] Response code: ");
    Serial.println(httpCode);
  }

  if (httpCode <= 0 || httpCode != HTTP_CODE_OK) {
    lastNetworkFailure = millis();
    Serial.print("[HTTP] GET failed: ");
    if (httpCode <= 0) {
      Serial.println(http.errorToString(httpCode)); 
    } else {
      Serial.println("Server error code: " + String(httpCode)); 
    }
    http.end();
    if (hasSubs) {
      subs = vorigAantalSubs;
    }
    return false;
  }

  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error || !doc["items"].is<JsonArray>() || doc["items"].size() == 0) {
    Serial.println("[ERROR] JSON parsing mislukt of kanaal niet gevonden.");
    if (hasSubs) {
      subs = vorigAantalSubs;
    }
    return false;
  }

  subs = doc["items"][0]["statistics"]["subscriberCount"].as<uint32_t>();

  lastNetworkFailure = 0;
  Serial.print("[SUCCESS] Subs ververst: ");
  Serial.println(subs);
  return true;
}

void setup() {
  delay(2000);
  Serial.begin(115200);

  HUB75_I2S_CFG::i2s_pins custom_pins = {
    26, 27, 25,
    12, 13, 14,
    33, 32, 22,
    19, 23,
    2, 15, 4
  };

  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, custom_pins, HUB75_I2S_CFG::SHIFTREG);
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.double_buff = false; 
  mxconfig.clkphase = false;
  mxconfig.latch_blanking = 4;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setRotation(2);
  dma_display->setBrightness8(MAX_HELDERHEID);
  dma_display->clearScreen();
  dma_display->setTextWrap(false);
  dma_display->setTextSize(2);
  dma_display->setTextColor(dma_display->color565(255, 255, 255));

  randomSeed(analogRead(0));

  if (!connectWiFi()) {
    Serial.println("[WARN] WiFi niet verbonden; setup gaat verder met de laatst bekende waarden.");
  }

  setTimeFromNTP();
  if (getSubscriberCount(currentSubs)) {
    vorigAantalSubs = currentSubs;
    hasSubs = true;
  }
  
  restoreFullCleanScreen(currentSubs, hasSubs);
  lastFetch = millis();
}

void loop() {
  bool moetAanStaan = isDisplayTime();

  if (moetAanStaan) {
    checkSerialCommands();
    
    if (!schermActief) {
      Serial.println("[TIMER] Scherm gaat AAN (10:00 - 22:00)");
      dma_display->setBrightness8(MAX_HELDERHEID);
      if (!connectWiFi()) {
        Serial.println("[WARN] WiFi reconnect mislukt; scherm blijft actief met laatst bekende data.");
      }
      schermActief = true;
      hasSubs = false;
      currentSubs = 0;
      vorigAantalSubs = 0;
      restoreFullCleanScreen(currentSubs, hasSubs); 
    }

    if (!fireworkActive && !testModeActive && (millis() - lastFetch > FETCH_INTERVAL || !hasSubs)) {
      uint32_t nieuweSubs = vorigAantalSubs;
      if (getSubscriberCount(nieuweSubs)) {
        if (hasSubs && nieuweSubs > vorigAantalSubs) {
          currentSubs = nieuweSubs;
          startFirework();
        }
        if (!fireworkActive) {
          currentSubs = nieuweSubs;
        }
        vorigAantalSubs = nieuweSubs;
        hasSubs = true;
      } else if (hasSubs) {
        currentSubs = vorigAantalSubs;
        hasSubs = true;
      }
      lastFetch = millis();
    }

    if (hasSubs && currentSubs != vorigAantalSubs && !fireworkActive) {
      drawTextArea(currentSubs, true); 
      vorigAantalSubs = currentSubs;
    }

    if (fireworkActive) {
      renderFireworkFrame(currentSubs, hasSubs);
      delay(1);
      return;
    }

    drawClockArea();

  } else {
    if (schermActief) {
      Serial.println("[TIMER] Scherm gaat UIT (Nachtstand)");
      dma_display->clearScreen();
      dma_display->setBrightness8(0);
      WiFi.disconnect(true, false);
      WiFi.mode(WIFI_OFF);
      schermActief = false;
      fireworkActive = false;
      testModeActive = false;
      hasSubs = false;
      vorigAantalSubs = 0;
    }
  }

  delay(IDLE_DELAY); 
}
