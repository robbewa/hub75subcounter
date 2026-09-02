#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

#define PANEL_RES_X 128
#define PANEL_RES_Y 64
#define PANEL_CHAIN 1

MatrixPanel_I2S_DMA *dma_display = nullptr;

struct FireworkParticle {
  float x;
  float y;
  float dx;
  float dy;
  uint16_t color;
  bool alive;
};

FireworkParticle particles[64];
int particleCount = 0;
int launchX = PANEL_RES_X / 2;
int rocketY = PANEL_RES_Y - 1;
bool rocketActive = false;
unsigned long lastFrame = 0;
unsigned long phaseStartTime = 0;
int phase = 0;
uint16_t bgColor;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void startRocket() {
  launchX = random(20, PANEL_RES_X - 20);
  rocketY = PANEL_RES_Y - 1;
  rocketActive = true;
  phase = 1;
  phaseStartTime = millis();
}

void startExplosion() {
  phaseStartTime = millis();
  particleCount = 0;
  for (int i = 0; i < 48; i++) {
    float angle = random(0, 360) * 0.0174533;
    float speed = random(40, 90) / 40.0;
    particles[particleCount++] = {
      (float)launchX,
      (float)rocketY,
      cos(angle) * speed,
      sin(angle) * speed,
      rgb565(random(200, 255), random(80, 255), random(0, 255)),
      true
    };
  }
  phase = 2;
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
  bool anyAlive = false;
  for (int i = 0; i < particleCount; i++) {
    if (!particles[i].alive) continue;
    particles[i].x += particles[i].dx;
    particles[i].y += particles[i].dy;
    particles[i].dx *= 0.95;
    particles[i].dy += 0.12;
    if (particles[i].x < 0 || particles[i].x >= PANEL_RES_X || particles[i].y < 0 || particles[i].y >= PANEL_RES_Y) {
      particles[i].alive = false;
      continue;
    }
    if (random(0, 100) < 10) {
      particles[i].color = rgb565(random(180, 255), random(80, 255), random(0, 255));
    }
    anyAlive = true;
  }
  if (!anyAlive) phase = 3;
}

void drawExplosion() {
  for (int i = 0; i < particleCount; i++) {
    if (!particles[i].alive) continue;
    int px = round(particles[i].x);
    int py = round(particles[i].y);
    dma_display->drawPixel(px, py, particles[i].color);
  }
}

void setup() {
  delay(1000);

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

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(80);
  dma_display->clearScreen();

  randomSeed(analogRead(0));
  bgColor = rgb565(0, 0, 16);
  startRocket();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < 40) return;
  lastFrame = now;

  dma_display->fillScreen(0x0000);

  if (phase == 1) {
    drawRocketTrail(launchX, rocketY);
    dma_display->drawPixel(launchX, rocketY, rgb565(255, 255, 255));
    rocketY -= 2;
    if (rocketY < 18) {
      rocketActive = false;
      startExplosion();
    }
  } else if (phase == 2) {
    updateExplosion();
    drawExplosion();
  } else if (phase == 3) {
    unsigned long elapsed = now - phaseStartTime;
    if (elapsed >= 80) {
      startRocket();
    }
  }

  delay(20);
}
