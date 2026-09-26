// Reference sketch recreated from the hardware setup and surviving project material.
// The original 2025 team sketch was not retained.

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_BMP280.h>
#include <SensorQMI8658.hpp>

#define TFT_CS         7
#define TFT_DC         39
#define TFT_RST        40
#define TFT_BACKLIGHT  45

#define SPI_SCK        36
#define SPI_MISO       37
#define SPI_MOSI       35

#define I2C_SDA        42
#define I2C_SCL        41

#define BMP_ADDR       0x77
#define QMI_ADDR       0x6B

#define SD_CS_PIN      10  // set this to match the SD breakout wiring

constexpr uint32_t SAMPLE_INTERVAL_MS = 50; // 20 Hz

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
Adafruit_BMP280 bmp;
SensorQMI8658 qmi;
IMUdata acc;
IMUdata gyr;

File logFile;
uint32_t lastSampleMs = 0;
float launchAltitudeM = 0.0f;

void initDisplay() {
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  tft.init(135, 240);
  tft.setRotation(3);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
}

bool initSensors() {
  if (!bmp.begin(BMP_ADDR)) {
    Serial.println("BMP280 not found");
    return false;
  }

  if (!qmi.begin(Wire, QMI_ADDR, I2C_SDA, I2C_SCL)) {
    Serial.println("QMI8658C not found");
    return false;
  }

  qmi.configAccelerometer(
    SensorQMI8658::ACC_RANGE_4G,
    SensorQMI8658::ACC_ODR_1000Hz,
    SensorQMI8658::LPF_MODE_0
  );
  qmi.enableAccelerometer();

  qmi.configGyroscope(
    SensorQMI8658::GYR_RANGE_64DPS,
    SensorQMI8658::GYR_ODR_896_8Hz,
    SensorQMI8658::LPF_MODE_3
  );
  qmi.enableGyroscope();

  return true;
}

bool initStorage() {
  if (!SD.begin(SD_CS_PIN, SPI)) {
    Serial.println("microSD init failed");
    return false;
  }

  logFile = SD.open("/flight.csv", FILE_APPEND);
  if (!logFile) {
    Serial.println("Could not open flight.csv");
    return false;
  }

  if (logFile.size() == 0) {
    logFile.println(
      "time_ms,pressure_pa,altitude_m,relative_altitude_m,"
      "ax,ay,az,gx,gy,gz"
    );
    logFile.flush();
  }

  return true;
}

void showStatus(const char* message, uint16_t colour = ST77XX_WHITE) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(colour);
  tft.print(message);
}

void setup() {
  Serial.begin(115200);

  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, TFT_CS);
  Wire.begin(I2C_SDA, I2C_SCL);

  initDisplay();
  showStatus("Starting...");

  if (!initSensors()) {
    showStatus("Sensor error", ST77XX_RED);
    while (true) delay(1000);
  }

  launchAltitudeM = bmp.readAltitude(1013.25f);

  if (!initStorage()) {
    showStatus("SD error", ST77XX_RED);
    while (true) delay(1000);
  }

  showStatus("Logger ready", ST77XX_GREEN);
  delay(1000);
}

void loop() {
  const uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = now;

  const float pressurePa = bmp.readPressure();
  const float altitudeM = bmp.readAltitude(1013.25f);
  const float relativeAltitudeM = altitudeM - launchAltitudeM;

  bool imuReady = false;
  if (qmi.getDataReady()) {
    imuReady = qmi.getAccelerometer(acc.x, acc.y, acc.z);
    qmi.getGyroscope(gyr.x, gyr.y, gyr.z);
  }

  if (!imuReady) {
    acc.x = acc.y = acc.z = NAN;
    gyr.x = gyr.y = gyr.z = NAN;
  }

  // CSV log
  if (logFile) {
    logFile.printf(
      "%lu,%.2f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
      now,
      pressurePa,
      altitudeM,
      relativeAltitudeM,
      acc.x, acc.y, acc.z,
      gyr.x, gyr.y, gyr.z
    );

    static uint8_t flushCounter = 0;
    if (++flushCounter >= 20) {
      logFile.flush();
      flushCounter = 0;
    }
  }

  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ST77XX_CYAN);
  tft.printf("t %lu ms\n", now);
  tft.setTextColor(ST77XX_YELLOW);
  tft.printf("Alt %.1f m\n", relativeAltitudeM);
  tft.setTextColor(ST77XX_GREEN);
  tft.printf("a %.2f %.2f %.2f", acc.x, acc.y, acc.z);

  Serial.printf(
    "%lu,%.2f,%.3f,%.3f,%.5f,%.5f,%.5f\n",
    now, pressurePa, altitudeM, relativeAltitudeM,
    acc.x, acc.y, acc.z
  );
}
