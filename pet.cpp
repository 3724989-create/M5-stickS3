#include "pet.h"
#include <FS.h>            // File 类型
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include <ArduinoJson.h>

// ========== 常量 ==========
#define PET_W 96                       // 宠物画布宽
#define PET_H 100                      // 宠物画布高
#define PET_X 19                       // 宠物区在屏幕的 x（(135-96)/2 居中）
#define PET_Y 50                       // 宠物区在屏幕的 y（自己定）
#define PET_BG M5.Display.color565(10, 12, 16)   // CLR_BG

// ========== 模块私有状态 ==========
static M5Canvas petSprite(&M5.Display);   // 宠物专属画布（解码目标，之后叠到主 canvas）
static AnimatedGIF gif;                   // GIF 解码器
static File petFile;                      // 当前打开的 GIF 文件句柄
static bool  gifOpen = false;             // 是否已打开 GIF
static uint8_t petState = 0xFF;           // 当前状态（0xFF=未初始化）
static uint32_t nextFrameAt = 0;          // 下一帧时间（毫秒）
static bool  petDirty = false;            // 帧变了 → 需要推屏
static char  gifPath[64];                 // 当前 GIF 完整路径
static char  stateFiles[4][32];           // manifest 解析出的每个状态的文件名

// 状态名（必须和 manifest.json 的 states 键一一对应）
static const char* STATE_KEYS[4] = { "idle", "sleep", "dizzy", "busy" };

// ========== ① 文件读取 4 回调（接 LittleFS）==========
static void* petOpenCb(const char* fname, int32_t* pSize) {
  petFile = LittleFS.open(fname, "r");
  if (!petFile) return nullptr;
  *pSize = petFile.size();
  return (void*)&petFile;
}
static void petCloseCb(void* h) {
  File* f = (File*)h;
  if (f) f->close();
}
static int32_t petReadCb(GIFFILE* pFile, uint8_t* buf, int32_t len) {
  File* f = (File*)pFile->fHandle;
  int32_t n = f->read(buf, len);
  pFile->iPos = f->position();
  return n;
}
static int32_t petSeekCb(GIFFILE* pFile, int32_t pos) {
  File* f = (File*)pFile->fHandle;
  f->seek(pos);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// ========== ② 渲染回调（画进 petSprite）==========
static uint32_t petPixels = 0;   // ★ 调试：累计绘制像素数
static void petDrawCb(GIFDRAW* d) {
  uint16_t* pal = d->pPalette;   // RGB565 调色板
  uint8_t*  px  = d->pPixels;    // 这一行的调色板索引
  int y = d->iY + d->y;          // 行号（相对于 petSprite）
  for (int x = 0; x < d->iWidth; x++) {
    petSprite.drawPixel(d->iX + x, y, pal[px[x]]);
  }
  petPixels += d->iWidth;
  if (petPixels < 4000) {        // ★ 调试：只打前几行
    Serial.printf("[pet] drawCb y=%d w=%d 累计px=%u 首色=0x%04X\n",
                  d->y, d->iWidth, petPixels, pal[px[0]]);
  }
}

// ========== 公共接口 ==========
void petInit() {
  gif.begin();                          // ★ 关键！初始化调色板类型（RGB565_LE），不调它绘制会发黑
  // 创建宠物画布（PSRAM）
  petSprite.setPsram(true);
  if (!petSprite.createSprite(PET_W, PET_H)) {
    petSprite.setPsram(false);
    petSprite.createSprite(PET_W, PET_H);
  }
  petSprite.fillScreen(PET_BG);

  // 挂载 LittleFS + 读 manifest
  if (!LittleFS.begin(true)) { Serial.println("[pet] LittleFS 挂载失败"); return; }
  File mf = LittleFS.open("/characters/xiaoqi/manifest.json", "r");
  if (!mf) { Serial.println("[pet] manifest 打开失败"); return; }
  JsonDocument doc;
  if (deserializeJson(doc, mf)) {
    Serial.println("[pet] manifest 解析失败");
    mf.close();
    return;
  }
  mf.close();

  // 把 manifest 里每个状态的文件名记下来
  for (int i = 0; i < 4; i++) {
    const char* fn = doc["states"][STATE_KEYS[i]] | "";
    strncpy(stateFiles[i], fn, sizeof(stateFiles[i]) - 1);
    stateFiles[i][sizeof(stateFiles[i]) - 1] = 0;
  }
  Serial.println("[pet] 初始化完成");
  petSetState(0);   // 默认 idle
}

void petSetState(uint8_t s) {
  if (s >= 4) return;
  if (petState == s && gifOpen) return;   // 同状态不重开（省 LittleFS 打开开销）
  petState = s;
  if (gifOpen) { gif.close(); gifOpen = false; }

  snprintf(gifPath, sizeof(gifPath), "/characters/xiaoqi/%s", stateFiles[s]);
  if (gif.open(gifPath, petOpenCb, petCloseCb, petReadCb, petSeekCb, petDrawCb)) {
    gifOpen = true;
    petSprite.fillScreen(PET_BG);
    nextFrameAt = 0;          // 立刻解码第一帧
    petDirty = true;
    Serial.printf("[pet] %s (%dx%d)\n", gifPath, gif.getCanvasWidth(), gif.getCanvasHeight());
  } else {
    Serial.printf("[pet] 打开失败: %s\n", gifPath);
  }
}

void petTick() {
  if (!gifOpen) return;
  uint32_t now = millis();
  if (now < nextFrameAt) return;          // 没到下一帧时间
  int delayMs = 0;
  int rc = gif.playFrame(false, &delayMs);  // 解码一帧 → petDrawCb 画进 petSprite
  static int dbg = 0;                    // ★ 调试：只打前 6 次
  if (dbg < 6) {
    dbg++;
    Serial.printf("[pet] tick rc=%d delay=%d\n", rc, delayMs);
  }
  nextFrameAt = now + (delayMs > 0 ? delayMs : 100);
  petDirty = (rc != -1);                  // ★ rc=-1 是错误（没画成）；0/1 都算画了帧
}

bool petIsDirty() { return petDirty; }

void petRender() {
  // 把宠物画布的像素直接拷进主 canvas（pushImage 底层像素拷贝，最可靠）
  canvas.pushImage(PET_X, PET_Y, PET_W, PET_H, (uint16_t*)petSprite.getBuffer());
}

void petClearDirty() { petDirty = false; }
