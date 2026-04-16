#include <Adafruit_BME680.h>
#include <Adafruit_Sensor.h>
#include "Seeed_TMG3993_v2.hpp"
#include <HT_SSD1306Wire.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// Inclue Wi-Fi credentials
#include "src/wifi/wifi-credentials.h"
// Include web page
#include "src/html/web-root.h"

// Pins BME680 en SPI
#define BME_SCK 36   // 9
#define BME_MOSI 35  // 10
#define BME_MISO 37  // 4
#define BME_CS 34    // 11

// Pins heartbeat
#define HB_PIN 1

// Pins TMG3993 en I2C
#define TMG_SDA 41
#define TMG_SCL 42
#define TMG_I2C_FREQ 100000UL

// WebSocket settings
#define WS_PORT 81

Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);
TMG3993 tmg3993(&Wire1);
static SSD1306Wire display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Embedded web server
WebServer server(80);
WebSocketsServer webSocket(WS_PORT);
String latestJson = "{}";
bool webServerEnabled = false;

void handleRoot() {
    String html = FPSTR(WEB_ROOT_HTML);
    html.replace("__WS_PORT__", String(WS_PORT));
    server.send(200, "text/html; charset=utf-8", html);
}
void handleNotFound() {
    server.send(404, "application/json", "{\"error\":\"not_found\"}");
}

void handleWebSocketEvent(uint8_t clientNum, WStype_t type, uint8_t* payload, size_t length) {
    (void)payload;
    (void)length;

    if (type == WStype_CONNECTED) {
        IPAddress ip = webSocket.remoteIP(clientNum);
        Serial.printf("[WS] Client %u connecte (%s)\n", clientNum, ip.toString().c_str());
        webSocket.sendTXT(clientNum, latestJson);
    } else if (type == WStype_DISCONNECTED) {
        Serial.printf("[WS] Client %u deconnecte\n", clientNum);
    }
}

// ----------------------- SETUP & LOOP ----------------------

void setup() {
    // --- Serial initialization
    Serial.begin(115200);
    Serial.println("Serial is ready");

    // --- Display initialization
    display.init();
    display.setFont(ArialMT_Plain_10);

    // --- BME680 initialization
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

    // --- TMG3993 initialization
    Wire1.begin(TMG_SDA, TMG_SCL, TMG_I2C_FREQ);

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

    // --- Wi-Fi + embedded web server initialization
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connexion Wi-Fi");
    const unsigned long timeoutMs = 20000;
    const unsigned long startMs = millis();

    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs) {
        Serial.print(".");
        delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        server.on("/", HTTP_GET, handleRoot);
        server.onNotFound(handleNotFound);
        server.begin();

        webSocket.begin();
        webSocket.onEvent(handleWebSocketEvent);
        webServerEnabled = true;

        Serial.print("Wi-Fi connecte, IP: ");
        Serial.println(WiFi.localIP());
        Serial.println("Serveur web demarre sur le port 80");
        Serial.printf("WebSocket demarre sur le port %u\n", WS_PORT);
    } else {
        Serial.println("Wi-Fi non connecte, services web desactives");
    }

    // --- End of setup
    Serial.println("SETUP_COMPLETE");
}

void loop() {
    if (webServerEnabled) {
        server.handleClient();
        webSocket.loop();
    }

    display.clear();

    // ----------------------- GET READINGS -----------------------

    // BME680 reading

    bool bme_reading = bme.performReading();
    int bme_temperature = NULL;
    int bme_humidity = NULL;
    int bme_pressure = NULL;
    int bme_gas = NULL;

    if (bme_reading) {
        bme_temperature = bme.temperature; // °C
        bme_humidity = bme.humidity; // %
        bme_pressure = bme.pressure / 100.0; // hPa
        bme_gas = bme.gas_resistance / 1000.0; // kOhm
    }

    // TMG3993 reading

    // Proximity
    bool tmg3993_proximity = tmg3993.getSTATUS() & STATUS_PVALID;
    uint8_t tmg3993_proximity_raw = NULL;

    if (tmg3993_proximity) {
        tmg3993_proximity_raw = tmg3993.getProximityRaw();
        tmg3993.clearProximityInterrupts();
    }

    // Color
    bool tmg3993_color = tmg3993.getSTATUS() & STATUS_AVALID;
    uint16_t tmg3993_r = NULL;
    uint16_t tmg3993_g = NULL;
    uint16_t tmg3993_b = NULL;
    uint16_t tmg3993_c = NULL;
    int32_t tmg3993_lux = NULL;
    int32_t tmg3993_cct = NULL;

    if (tmg3993_color) {
        tmg3993.getRGBCRaw(&tmg3993_r, &tmg3993_g, &tmg3993_b, &tmg3993_c);
        tmg3993_lux = tmg3993.getLux(tmg3993_r, tmg3993_g, tmg3993_b, tmg3993_c);
        tmg3993_cct = tmg3993.getCCT(tmg3993_r, tmg3993_g, tmg3993_b, tmg3993_c);
        tmg3993.clearALSInterrupts();
    }

    // Heartbeat reading

    int heartbeat = analogReadMilliVolts(HB_PIN);

    // ----------------------- DISPLAY READINGS -----------------------
    
    // Left side

    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // BME680
    if (bme_reading) {
        display.drawString(0, 0, "Tp: " + String(bme_temperature) + " C");
        display.drawString(0, 10, "Hum: " + String(bme_humidity) + " %");
        display.drawString(0, 20, "Press: " + String(bme_pressure) + " hPa");
        display.drawString(0, 30, "Gas: " + String(bme_gas) + " kOhm");
    } else {
        display.drawString(0, 0, "BME680 reading failed");
    }

    // Wi-Fi status
    if (WiFi.status() == WL_CONNECTED) {
        display.drawString(0, 40, WiFi.localIP().toString());
    } else {
        display.drawString(0, 40, "Wi-Fi: OFF");
    }

    // TMG3993 Proximity
    if (tmg3993_proximity) {
        display.drawString(0, 50, "Proximity: " + String(tmg3993_proximity_raw));
    } else {
        display.drawString(0, 50, "Proximity reading failed");
    }

    // Right side

    display.setTextAlignment(TEXT_ALIGN_RIGHT);

    // TMG3993 Color
    if (tmg3993_color) {
        display.drawString(127, 0, "rgb(" + String(tmg3993_r) + "," + String(tmg3993_g) + "," + String(tmg3993_b) + ")");
        display.drawString(127, 10, "C=" + String(tmg3993_c));
        display.drawString(127, 20, String(tmg3993_lux) + " lux");
        display.drawString(127, 30, String(tmg3993_cct) + " K");
    } else {
        display.drawString(127, 0, "Color reading failed");
    }

    // Web server status
    if (webServerEnabled) {
        display.drawString(127, 40, "WS: ON");
    } else {
        display.drawString(127, 40, "WS: OFF");
    }

    // Heartbeat
    display.drawString(127, 50, "HB: " + String(heartbeat) + " mV");

    display.display();

    // ----------------------- JSON GENERATION -----------------------
    String json;
    json.reserve(420);

    json += "{";
    json += "\"uptime\":" + String(millis());
    json += ",\"heartbeat\":" + String(heartbeat);

    json += ",\"bme\":";
    if (bme_reading) {
        json += "{";
        json += "\"temperature\":" + String(bme.temperature, 2);
        json += ",\"humidity\":" + String(bme.humidity, 2);
        json += ",\"pressure\":" + String(bme.pressure / 100.0, 2);
        json += ",\"gas\":" + String(bme.gas_resistance / 1000.0, 2);
        json += "}";
    } else {
        json += "null";
    }

    json += ",\"tmg\":{";
    json += "\"proximity\":";
    if (tmg3993_proximity) {
        json += String(tmg3993_proximity_raw);
    } else {
        json += "null";
    }

    json += ",\"color\":";
    if (tmg3993_color) {
        json += "{";
        json += "\"red\":" + String(tmg3993_r);
        json += ",\"green\":" + String(tmg3993_g);
        json += ",\"blue\":" + String(tmg3993_b);
        json += ",\"clear\":" + String(tmg3993_c);
        json += ",\"lux\":" + String(tmg3993_lux);
        json += ",\"cct\":" + String(tmg3993_cct);
        json += "}";
    } else {
        json += "null";
    }

    json += "}}";

    // ------------------------ SERIAL OUTPUT -----------------------
    Serial.println(json);

    // ------------------------ WEB OUTPUT -----------------------
    latestJson = json;

    if (webServerEnabled) {
        webSocket.broadcastTXT(latestJson);
    }
}
