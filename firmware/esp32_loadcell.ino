#include <Arduino.h>
#include <Wire.h>
#include <HX711.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// Pins 
#define DOUT_PIN  18
#define SCK_PIN   19
#define SDA_PIN   21
#define SCL_PIN   22
#define LED_PIN    2   // built-in LED

// LCD — 20 columns, 4 rows 
// If LCD shows nothing / garbled boxes, change 0x27 to 0x3F
LiquidCrystal_I2C lcd(0x27, 20, 4);

// HX711 
HX711 scale;

// Calibration
float calFactor = 245.0f;
long  calOffset = 0L;
const float NOISE_G = 5.0f;

// EEPROM
#define EEPROM_SIZE 64
#define MAGIC       0xCD

// Filter
const int AVG = 10;
float buf[AVG];
int   bIdx  = 0;
bool  bFull = false;
float smoothed = 0;
bool  smOk     = false;

// State
float         grams    = 0;
int           pktCount = 0;
unsigned long lastMs   = 0;
bool          hxReady  = false;

//  SETUP
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Init LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcdMsg("Smart Balance", "Starting...", "", "");

  // Load calibration
  EEPROM.begin(EEPROM_SIZE);
  loadCal();

  // Init HX711
  scale.begin(DOUT_PIN, SCK_PIN);
  scale.set_scale(calFactor);
  scale.set_offset(calOffset);

  unsigned long t = millis();
  while (!scale.is_ready() && millis() - t < 3000) delay(10);
  hxReady = scale.is_ready();

  Serial.println(" Smart Balance - Single Cell");
  Serial.println(" Topic: balance/sba20");
  Serial.println(hxReady ? "HX711: READY" : "HX711: NOT READY - check wiring!");
  Serial.println("Commands: t=tare  c=calibrate  s=status  r=reboot");
  Serial.println();

  if (!hxReady) {
    lcdMsg("HX711 ERROR!", "Check wiring:", "DT->GPIO18", "SCK->GPIO19");
    delay(3000);
  }

  // Tare at startup
  lcdMsg("Remove all weight", "Taring in 3s...", "", "");
  Serial.println("Taring in 3s — remove all weight...");
  for (int i = 3; i > 0; i--) {
    Serial.printf("  %d...\n", i);
    lcd.setCursor(0, 2);
    lcd.printf("  %d...", i);
    delay(1000);
  }
  doTare();

  digitalWrite(LED_PIN, HIGH);
  Serial.println("Streaming started.");
}

//  MAIN LOOP
void loop() {
  if (Serial.available()) handleCmd();

  if (millis() - lastMs >= 250) {
    lastMs = millis();
    readCell();
    updateLCD();
    sendJSON();
    pktCount++;
    digitalWrite(LED_PIN, pktCount % 2);
  }
}

//  READ + FILTER
void readCell() {
  float raw = 0;
  if (scale.is_ready()) raw = scale.get_units(1);
  if (fabsf(raw) < NOISE_G) raw = 0;
  if (raw < 0) raw = 0;

  // Moving average
  buf[bIdx] = raw;
  bIdx = (bIdx + 1) % AVG;
  if (bIdx == 0) bFull = true;
  int n = bFull ? AVG : (bIdx ? bIdx : 1);
  float sum = 0;
  for (int i = 0; i < n; i++) sum += buf[i];
  float avg = sum / n;

  // Exponential smooth
  if (!smOk) { smoothed = avg; smOk = true; }
  smoothed = 0.25f * avg + 0.75f * smoothed;
  grams = smoothed;
}

//  LCD DISPLAY  (20x4)
void updateLCD() {
  lcd.clear();

  // Row 0 — title
  lcd.setCursor(0, 0);
  lcd.print("Smart Balance SBA20");

  // Row 1 — weight value
  lcd.setCursor(0, 1);
  if (!hxReady) {
    lcd.print("  HX711 NOT READY ");
  } else if (grams >= 1000.0f) {
    char buf[21];
    snprintf(buf, sizeof(buf), "  %9.3f kg     ", grams / 1000.0f);
    lcd.print(buf);
  } else {
    char buf[21];
    snprintf(buf, sizeof(buf), "  %9.1f g      ", grams);
    lcd.print(buf);
  }

  // Row 2 — fill bar (assumes 5000g max capacity)
  lcd.setCursor(0, 2);
  int filled = (int)(constrain(grams / 5000.0f, 0, 1) * 18);
  lcd.print("[");
  for (int i = 0; i < 18; i++) lcd.print(i < filled ? '#' : '-');
  lcd.print("]");

  // Row 3 — status
  lcd.setCursor(0, 3);
  char st[21];
  snprintf(st, sizeof(st), "Cal:%.0f Pkt:%-6d", calFactor, pktCount);
  lcd.print(st);
}

// Boot message helper
void lcdMsg(const char* r0, const char* r1, const char* r2, const char* r3) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(r0);
  lcd.setCursor(0, 1); lcd.print(r1);
  lcd.setCursor(0, 2); lcd.print(r2);
  lcd.setCursor(0, 3); lcd.print(r3);
  delay(800);
}

//  SEND JSON to Serial (windows_sender.py reads)
void sendJSON() {
  float kg = grams / 1000.0f;
  float r2 = round(kg * 100) / 100.0;

  StaticJsonDocument<512> doc;
  doc["load1"]        = r2;
  doc["load2"]        = 0.0;
  doc["load3"]        = 0.0;
  doc["load4"]        = 0.0;
  doc["totalWeight"]  = r2;
  doc["leftWeight"]   = round(kg / 2 * 100) / 100.0;
  doc["rightWeight"]  = round(kg / 2 * 100) / 100.0;
  doc["frontWeight"]  = round(kg / 2 * 100) / 100.0;
  doc["rearWeight"]   = round(kg / 2 * 100) / 100.0;
  doc["leftPercent"]  = 50.0;
  doc["rightPercent"] = 50.0;
  doc["frontPercent"] = 50.0;
  doc["rearPercent"]  = 50.0;
  doc["balanceX"]     = 0.5;
  doc["balanceY"]     = 0.5;

  serializeJson(doc, Serial);
  Serial.println();

  if (pktCount % 20 == 0)
    Serial.printf("[#%d] %.1fg (%.3fkg)\n", pktCount, grams, kg);
}

//  TARE
void doTare() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("   Taring...");
  lcd.setCursor(0, 1); lcd.print("  Remove all weight");
  Serial.println("[TARE] Zeroing...");

  if (scale.is_ready()) {
    scale.tare(20);
    calOffset = scale.get_offset();
    bIdx = 0; bFull = false; smOk = false;
    saveCal();
    Serial.printf("[TARE] Done. Offset=%ld\n", calOffset);
    lcd.clear();
    lcd.setCursor(0, 1); lcd.print("   Tare complete!");
    delay(1000);
  } else {
    Serial.println("[TARE] HX711 not ready!");
    lcd.clear();
    lcd.setCursor(0, 1); lcd.print("  HX711 not ready!");
    delay(2000);
  }
}

//  CALIBRATION WIZARD
void doCalibrate() {
  Serial.println("\nCALIBRATION");
  Serial.println("STEP 1: Remove all weight. Press Enter...");
  lcdMsg("CALIBRATION", "Remove all weight", "then press Enter", "in Serial Monitor");
  waitEnter();

  scale.tare(30);
  calOffset = scale.get_offset();
  Serial.println("  Tared.");

  Serial.println("STEP 2: Enter known weight in GRAMS (e.g. 1000 for 1kg):");
  Serial.print("Grams: ");
  while (!Serial.available()) delay(50);
  float known = Serial.parseFloat();
  while (Serial.available()) Serial.read();
  if (known < 1 || known > 100000) { Serial.println("Invalid!"); return; }
  Serial.printf("  Using %.1fg\n", known);

  Serial.printf("STEP 3: Place %.0fg on load cell. Press Enter...\n", known);
  lcdMsg("Place weight on", "load cell", "then press Enter", "");
  waitEnter();

  lcdMsg("Reading...", "", "", "");
  scale.set_scale(1.0f);
  long raw = scale.get_value(30);
  float factor = (float)raw / known;
  scale.set_scale(factor);
  calFactor = factor;

  float verify = scale.get_units(10);
  bool  good   = fabsf(verify - known) < known * 0.03f;
  Serial.printf("Factor: %.2f  Verify: %.1fg  %s\n",
                factor, verify, good ? "GOOD!" : "Recheck wiring");

  saveCal();
  Serial.println("Saved to EEPROM. Remove weight — resuming...");

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Calibration saved!");
  lcd.setCursor(0, 1); char b[21]; snprintf(b, 21, "Factor: %.2f", factor); lcd.print(b);
  lcd.setCursor(0, 2); snprintf(b, 21, "Check: %.1fg", verify); lcd.print(b);
  lcd.setCursor(0, 3); lcd.print(good ? "   GOOD!" : "   Recheck wiring");
  delay(3000);
  doTare();
}

//  EEPROM
void saveCal() {
  EEPROM.write(0, MAGIC);
  EEPROM.put(4, calFactor);
  EEPROM.put(4 + sizeof(float), calOffset);
  EEPROM.commit();
}

void loadCal() {
  if (EEPROM.read(0) != MAGIC) {
    Serial.println("[CAL] No saved cal — using default 245.0");
    return;
  }
  EEPROM.get(4, calFactor);
  EEPROM.get(4 + sizeof(float), calOffset);
  Serial.printf("[CAL] Loaded: factor=%.2f offset=%ld\n", calFactor, calOffset);
}

//  SERIAL COMMANDS
void handleCmd() {
  char c = Serial.read();
  while (Serial.available()) Serial.read();
  if      (c=='t'||c=='T') doTare();
  else if (c=='c'||c=='C') doCalibrate();
  else if (c=='s'||c=='S') {
    Serial.printf("Weight: %.1fg (%.3fkg)  Factor:%.2f  Pkts:%d  HX711:%s\n",
      grams, grams/1000.0f, calFactor, pktCount,
      scale.is_ready() ? "OK" : "NOT READY");
  }
  else if (c=='r'||c=='R') { Serial.println("Rebooting..."); delay(300); ESP.restart(); }
  else if (c >= 32) Serial.println("Commands: t=tare  c=calibrate  s=status  r=reboot");
}

void waitEnter() {
  while (Serial.available()) Serial.read();
  while (!Serial.available()) delay(50);
  while (Serial.available()) Serial.read();
}
