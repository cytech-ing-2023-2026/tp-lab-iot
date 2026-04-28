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

// Pins KY-023 joystick
#define JOY_X_PIN 7
#define JOY_Y_PIN 6
#define JOY_SW_PIN 5

// Pins TMG3993 en I2C
#define TMG_SDA 41
#define TMG_SCL 42
#define TMG_I2C_FREQ 100000UL

// WebSocket settings
#define WS_PORT 81

SemaphoreHandle_t dataMutex;

struct SensorData {
    // BME
    bool bme_reading;
    float bme_temp, bme_hum, bme_press, bme_gas;
    // TMG Proximity
    bool tmg_proximity;
    uint8_t tmg_proximity_raw;
    // TMG Color
    bool tmg_color;
    uint16_t tmg_r, tmg_g, tmg_b, tmg_c;
    int32_t tmg_lux, tmg_cct;
    // Analog
    int heartbeat;
    int joystick_x, joystick_y;
    bool joystick_pressed;
} sensorData;

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

// ----------------------- FREERTOS TASKS --------------------

void taskBME680(void *pvParameters) {
    for (;;) {
        bool reading = bme.performReading();
        
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            sensorData.bme_reading = reading;
            if (reading) {
                sensorData.bme_temp = bme.temperature;
                sensorData.bme_hum = bme.humidity;
                sensorData.bme_press = bme.pressure / 100.0;
                sensorData.bme_gas = bme.gas_resistance / 1000.0;
            }
            xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void taskTMG3993(void *pvParameters) {
    for (;;) {
        bool prox_valid = tmg3993.getSTATUS() & STATUS_PVALID;
        uint8_t prox_raw = 0;
        if (prox_valid) {
            prox_raw = tmg3993.getProximityRaw();
            tmg3993.clearProximityInterrupts();
        }

        bool color_valid = tmg3993.getSTATUS() & STATUS_AVALID;
        uint16_t r = 0, g = 0, b = 0, c = 0;
        int32_t lux = 0, cct = 0;
        if (color_valid) {
            tmg3993.getRGBCRaw(&r, &g, &b, &c);
            lux = tmg3993.getLux(r, g, b, c);
            cct = tmg3993.getCCT(r, g, b, c);
            tmg3993.clearALSInterrupts();
        }

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            sensorData.tmg_proximity = prox_valid;
            if (prox_valid) sensorData.tmg_proximity_raw = prox_raw;

            sensorData.tmg_color = color_valid;
            if (color_valid) {
                sensorData.tmg_r = r; sensorData.tmg_g = g;
                sensorData.tmg_b = b; sensorData.tmg_c = c;
                sensorData.tmg_lux = lux; sensorData.tmg_cct = cct;
            }
            xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

void taskAnalogInputs(void *pvParameters) {
    for (;;) {
        int hb = analogReadMilliVolts(HB_PIN);
        int jx = analogRead(JOY_X_PIN);
        int jy = analogRead(JOY_Y_PIN);
        bool jsw = digitalRead(JOY_SW_PIN) == LOW;

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            sensorData.heartbeat = hb;
            sensorData.joystick_x = jx;
            sensorData.joystick_y = jy;
            sensorData.joystick_pressed = jsw;
            xSemaphoreGive(dataMutex);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
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

    // --- KY-023 initialization
    pinMode(JOY_SW_PIN, INPUT_PULLUP);

    // --- Wi-Fi + embedded web server initialization
	
	// Wi-Fi

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

	// Web server

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

    // --- FreeRTOS Tasks Start
    dataMutex = xSemaphoreCreateMutex();
    
    xTaskCreatePinnedToCore(taskBME680, "BME_Task", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskTMG3993, "TMG_Task", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(taskAnalogInputs, "Analog_Task", 2048, NULL, 1, NULL, 1);

    // --- End of setup
    Serial.println("SETUP_COMPLETE");
}

void loop() {
    if (webServerEnabled) {
        server.handleClient();
        webSocket.loop();
    }

    // Lire les donnees protegees par le mutex et faire une copie locale
    SensorData localData;
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        localData = sensorData;
        xSemaphoreGive(dataMutex);
    }

    display.clear();

    // ----------------------- DISPLAY READINGS -----------------------
    
    // Left side

    display.setTextAlignment(TEXT_ALIGN_LEFT);

    // BME680
    if (localData.bme_reading) {
        display.drawString(0, 0, "Tp: " + String(localData.bme_temp) + " C");
        display.drawString(0, 10, "Hum: " + String(localData.bme_hum) + " %");
        display.drawString(0, 20, "Press: " + String(localData.bme_press) + " hPa");
        display.drawString(0, 30, "Gas: " + String(localData.bme_gas) + " kOhm");
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
    if (localData.tmg_proximity) {
        display.drawString(0, 50, "Proximity: " + String(localData.tmg_proximity_raw));
    } else {
        display.drawString(0, 50, "Proximity reading failed");
    }

    // Right side

    display.setTextAlignment(TEXT_ALIGN_RIGHT);

    // TMG3993 Color
    if (localData.tmg_color) {
        display.drawString(127, 0, "rgb(" + String(localData.tmg_r) + "," + String(localData.tmg_g) + "," + String(localData.tmg_b) + ")");
        display.drawString(127, 10, "C=" + String(localData.tmg_c));
        display.drawString(127, 20, String(localData.tmg_lux) + " lux");
        display.drawString(127, 30, String(localData.tmg_cct) + " K");
    } else {
        display.drawString(127, 0, "Color reading failed");
    }

    // Web server status
    if (webServerEnabled) {
        display.drawString(127, 40, "WS: ON");
    } else {
        display.drawString(127, 40, "WS: OFF");
    }

    // Heartbeat + joystick button
    display.drawString(127, 50, "HB: " + String(localData.heartbeat) + " " + (localData.joystick_pressed ? "J:P" : "J:R"));

    display.display();

    // ----------------------- JSON GENERATION -----------------------
    String json;
    json.reserve(560);

    json += "{";
    json += "\"uptime\":" + String(millis());
    json += ",\"heartbeat\":" + String(localData.heartbeat);
    json += ",\"joystick\":{";
    json += "\"x\":" + String(localData.joystick_x);
    json += ",\"y\":" + String(localData.joystick_y);
    json += ",\"pressed\":" + String(localData.joystick_pressed ? "true" : "false");
    json += "}";

    json += ",\"bme\":";
    if (localData.bme_reading) {
        json += "{";
        json += "\"temperature\":" + String(localData.bme_temp, 2);
        json += ",\"humidity\":" + String(localData.bme_hum, 2);
        json += ",\"pressure\":" + String(localData.bme_press, 2);
        json += ",\"gas\":" + String(localData.bme_gas, 2);
        json += "}";
    } else {
        json += "null";
    }

    json += ",\"tmg\":{";
    json += "\"proximity\":";
    if (localData.tmg_proximity) {
        json += String(localData.tmg_proximity_raw);
    } else {
        json += "null";
    }

    json += ",\"color\":";
    if (localData.tmg_color) {
        json += "{";
        json += "\"red\":" + String(localData.tmg_r);
        json += ",\"green\":" + String(localData.tmg_g);
        json += ",\"blue\":" + String(localData.tmg_b);
        json += ",\"clear\":" + String(localData.tmg_c);
        json += ",\"lux\":" + String(localData.tmg_lux);
        json += ",\"cct\":" + String(localData.tmg_cct);
        json += "}";
    } else {
        json += "null";
    }

    json += "}}";

    // ------------------------ SERIAL OUTPUT -----------------------
	Serial.println("JSON:" + json);

    // ------------------------ WEB OUTPUT -----------------------
    latestJson = json;

    if (webServerEnabled) {
        webSocket.broadcastTXT(latestJson);
    }

    delay(10);
}
