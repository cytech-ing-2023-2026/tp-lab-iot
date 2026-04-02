#define BME_SCK 36 //9
#define BME_MOSI 35 //10
#define BME_MISO 37 //4
#define BME_CS 34 //11

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

void setup() {
  // Prepare print
  Serial.begin(115200);
  Serial.println("Serial is ready");

  while (!bme.begin()) {
    Serial.println("Erreur: BME680 non detecte. Verifiez le cablage SPI (CS, MOSI, MISO, SCK) et l'alimentation.");
    delay(1000);
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320 C pendant 150 ms

  Serial.println("BME680 detecte");
}

void loop() {
  if (!bme.performReading()) {
    Serial.println("Erreur lecture BME680");
    delay(1000);
    return;
  }

  Serial.print("Temperature: ");
  Serial.print(bme.temperature);
  Serial.println(" C");

  Serial.print("Humidite: ");
  Serial.print(bme.humidity);
  Serial.println(" %");

  Serial.print("Pression: ");
  Serial.print(bme.pressure / 100.0);
  Serial.println(" hPa");

  Serial.print("Gaz: ");
  Serial.print(bme.gas_resistance / 1000.0);
  Serial.println(" kOhm");

  Serial.println();
  delay(100);
}
