// DJI BG30 (TI BQ40Z307) battery reader for Raspberry Pi Pico / Pico 2 W
// READ ONLY - writes nothing to the battery
// Wiring: Battery P- -> Pico GND, SDA -> GP4, SCL -> GP5. Do NOT connect P+.
#include <Wire.h>
#include "hardware/gpio.h"
const uint8_t ADDR = 0x0B;

bool readWord(uint8_t reg, uint16_t &v) {
  Wire.beginTransmission(ADDR); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(ADDR, (uint8_t)2) != 2) return false;
  v = Wire.read() | (Wire.read() << 8); return true;
}

int readBlock(uint8_t reg, uint8_t *buf, int max) {
  Wire.beginTransmission(ADDR); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  int n = Wire.requestFrom(ADDR, (uint8_t)(max + 1));
  if (n < 1) return -1;
  int len = Wire.read();
  for (int i = 0; i < len && i < max && Wire.available(); i++) buf[i] = Wire.read();
  return len;
}

int mac(uint16_t cmd, uint8_t *b) {   // ManufacturerBlockAccess read (no flash writes)
  Wire.beginTransmission(ADDR); Wire.write(0x44); Wire.write(2);
  Wire.write(cmd & 0xFF); Wire.write(cmd >> 8); Wire.endTransmission();
  delay(5);
  return readBlock(0x44, b, 34);
}

void line(const char *label, const char *meaning) { Serial.printf("%-26s %s\n", label, meaning); }

void num(uint8_t reg, const char *label, const char *unit, float scale = 1, float offset = 0) {
  uint16_t v; char s[64];
  if (readWord(reg, v)) snprintf(s, sizeof s, "%.1f %s", v * scale + offset, unit);
  else snprintf(s, sizeof s, "no reply");
  line(label, s);
}

void text(uint8_t reg, const char *label) {
  uint8_t b[33] = {0}; int n = readBlock(reg, b, 32);
  char s[40]; if (n < 0) strcpy(s, "no reply"); else { b[n < 32 ? n : 32] = 0; snprintf(s, sizeof s, "%s", (char*)b); }
  line(label, s);
}

void runReads() {
  Serial.println("\n===== DJI BG30 battery chip report (read only) =====");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) found++; }
  if (!found) { Serial.println("No chip found (asleep, or check wiring)."); return; }

  Serial.println("\n-- Identity --");
  text(0x20, "Manufacturer name");
  text(0x21, "Device name");
  text(0x22, "Cell chemistry");
  uint16_t v;
  if (readWord(0x1B, v)) { char s[32]; snprintf(s, sizeof s, "%04d-%02d-%02d", 1980 + (v >> 9), (v >> 5) & 0x0F, v & 0x1F); line("Manufacture date", s); }
  if (readWord(0x1C, v)) { char s[16]; snprintf(s, sizeof s, "%u", v); line("Serial number", s); }
  uint8_t b[36]; int n;
  n = mac(0x0001, b); if (n >= 4) { char s[32]; snprintf(s, sizeof s, "0x%02X%02X (chip type)", b[3], b[2]); line("Chip type", s); }
  n = mac(0x0002, b); if (n >= 6) { char s[48]; snprintf(s, sizeof s, "v%X.%02X", b[4], b[5]); line("Chip firmware", s); }

  Serial.println("\n-- Live readings --");
  num(0x09, "Pack voltage", "V", 0.001);
  if (readWord(0x0A, v)) { char s[32]; snprintf(s, sizeof s, "%d mA (0 = no current)", (int16_t)v); line("Current", s); }
  num(0x08, "Temperature", "C", 0.1, -273.15);
  num(0x3F, "Cell 1 voltage", "V", 0.001);
  num(0x3E, "Cell 2 voltage", "V", 0.001);
  num(0x3D, "Cell 3 voltage", "V", 0.001);
  num(0x3C, "Cell 4 voltage", "V", 0.001);

  Serial.println("\n-- Capacity and health --");
  num(0x0D, "Charge level", "%");
  num(0x0F, "Charge remaining", "mAh");
  num(0x10, "Full charge capacity now", "mAh");
  num(0x18, "Capacity when new", "mAh");
  num(0x19, "Design voltage", "V", 0.001);
  num(0x4F, "Health (vs new)", "%");
  num(0x17, "Charge cycles used", "");

  Serial.println("\n-- Status --");
  if (readWord(0x16, v)) {
    char s[24]; snprintf(s, sizeof s, "0x%04X", v); line("Battery status code", s);
    if (v & 0x8000) line("", "- Over-charged alarm");
    if (v & 0x4000) line("", "- Stop charging alarm");
    if (v & 0x1000) line("", "- Over-temperature alarm");
    if (v & 0x0800) line("", "- Stop discharging alarm (cells too low)");
    if (v & 0x0200) line("", "- Remaining charge alarm");
    if (v & 0x0100) line("", "- Remaining time alarm");
    if (v & 0x0080) line("", "- Gauge started up OK");
    if (v & 0x0040) line("", "- In discharge mode");
    if (v & 0x0020) line("", "- Fully charged");
    if (v & 0x0010) line("", "- Fully discharged");
  }
  n = mac(0x0051, b);
  if (n >= 6) { uint32_t x = b[2] | b[3] << 8 | b[4] << 16 | (uint32_t)b[5] << 24; char s[48];
    snprintf(s, sizeof s, x ? "0x%08lX - protection tripped" : "0x%08lX - all clear", (unsigned long)x); line("Safety status", s); }
  n = mac(0x0053, b);
  if (n >= 6) { uint32_t x = b[2] | b[3] << 8 | b[4] << 16 | (uint32_t)b[5] << 24; char s[56];
    snprintf(s, sizeof s, x ? "0x%08lX - PERMANENTLY FAILED" : "0x%08lX - not failed", (unsigned long)x); line("Permanent failure", s); }
  n = mac(0x0054, b);
  if (n >= 6) { uint32_t x = b[2] | b[3] << 8 | b[4] << 16 | (uint32_t)b[5] << 24; char s[48];
    uint8_t sec = (x >> 8) & 3;
    const char *m = sec == 3 ? "sealed (normal locked mode)" : sec == 2 ? "unsealed" : sec == 1 ? "full access" : "unknown";
    snprintf(s, sizeof s, "0x%08lX", (unsigned long)x); line("Operation status code", s); line("Security mode", m); }
  Serial.println("\n===== End of report =====\nPress Enter to read again.");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  Wire.setSDA(4); Wire.setSCL(5);
  Wire.setClock(10000);
  Wire.begin();
  gpio_pull_up(4); gpio_pull_up(5);   // built-in pull-ups, no resistors needed
  runReads();
}

void loop() {
  if (Serial.available()) { while (Serial.available()) Serial.read(); runReads(); }
}
