#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include <Seeed_TMG3993.h>
#include <Wire.h>

// Pins BME680 en SPI
#define BME_SCK 36   // 9
#define BME_MOSI 35  // 10
#define BME_MISO 37  // 4
#define BME_CS 34    // 11

#define HB_PIN 1

Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
TMG3993 tmg3993;

void setup() {
    // Serial initialization
    Serial.begin(115200);
    Serial.println("Serial is ready");

    // BME680 initialization
    while (!bme.begin()) {
        Serial.println("Erreur: BME680 non detecte. Verifiez le cablage SPI (CS, MOSI, MISO, SCK) et l'alimentation.");
        delay(1000);
    }
    Serial.println("BME680 detecte");

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150);  // 320 C pendant 150 ms

    // TMG3993 initialization
    Wire.begin();

    Serial.println("Initialisation du TMG3993...");

    while (!tmg3993.initialize()) {
        Serial.println("Erreur: TMG3993 non detecte. Verifiez le cablage I2C (SDA, SCL) et l'alimentation.");
        delay(1000);
    }
    Serial.println("TMG3993 détecté");

    tmg3993.setupRecommendedConfigForProximity();
    tmg3993.setProximityInterruptThreshold(25, 150);  // less than 5cm will trigger the proximity event

    tmg3993.setADCIntegrationTime(0xdb);  // the integration time: 103ms
    tmg3993.enableEngines(ENABLE_PON | ENABLE_PEN | ENABLE_PIEN | ENABLE_AEN | ENABLE_AIEN);
}

void loop() {
    Serial.println("======================================================");

    // BME680 reading

    if (bme.performReading()) {
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
    }

    Serial.println();

    // TMG3993 reading

    // Proximity
    if (tmg3993.getSTATUS() & STATUS_PVALID) {
        uint8_t proximity_raw = tmg3993.getProximityRaw();  // read the Proximity data will clear the status bit

        Serial.print("Proximity Raw : ");
        Serial.println(proximity_raw);

        // don't forget to clear the interrupt bits
        tmg3993.clearProximityInterrupts();
    }

    Serial.println();

    // Color
    if (tmg3993.getSTATUS() & STATUS_AVALID) {
        uint16_t r, g, b, c;
        int32_t lux, cct;
        tmg3993.getRGBCRaw(&r, &g, &b, &c);
        lux = tmg3993.getLux(r, g, b, c);
        // the calculation of CCT is just from the `Application Note`,
        // from the result of our test, it might have error.
        cct = tmg3993.getCCT(r, g, b, c);
        Serial.print("RGBC Data: ");
        Serial.print(r);
        Serial.print(" ");
        Serial.print(g);
        Serial.print(" ");
        Serial.print(b);
        Serial.print(" ");
        Serial.println(c);
        Serial.print("Lux: ");
        Serial.print(lux);
        Serial.print(" ");
        Serial.print("CCT: ");
        Serial.println(cct);
        // don't forget to clear the interrupt bits
        tmg3993.clearALSInterrupts();
    }

    Serial.println();

    int result = analogReadMilliVolts(HB_PIN);
    Serial.print("Heart Beat (mV) : ");
    Serial.println(result);
}
