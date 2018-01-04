#include <Arduino.h>
#include <ArduinoJson.h>
#include <CommonConfig.h>
#include <MqttClient.h>
#include <FotaClient.h>
#include <ESPWifiClient.h>
#include <RemotePrint.h>
#include "settings.h"


MqttClient *mqttClient    = NULL;
FotaClient *fotaClient    = new FotaClient(DEVICE_NAME);
ESPWifiClient *wifiClient = new ESPWifiClient(WIFI_SSID, WIFI_PASS);
long lastStatusMsgSentAt  = 0;
long lastClickTime        = 0;

bool heaterPowerOn       = false;
bool heaterHeating       = false;
uint8_t heaterTemp       = HEATER_MIN_TEMP;
uint8_t heaterTempTarget = HEATER_INITIAL_TEMP;

void setHeaterPowerStatus(bool powerOn) {
  if (heaterPowerOn != powerOn) {
    PRINT("Heater: Power Status set to: ");
    heaterPowerOn = powerOn;

    if (heaterPowerOn) {
      PRINTLN("ON");
    } else {
      PRINTLN("OFF");
    }
  }
}

uint8_t validateTemp(uint8_t temp) {
  if (temp < HEATER_MIN_TEMP) {
    return HEATER_MIN_TEMP;
  }

  if (heaterTempTarget > HEATER_MAX_TEMP) {
    return HEATER_MAX_TEMP;
  }
  return temp;
}

void heaterPublishStatus(bool        forcePublish = false,
                         const char *messageId    = NULL) {
  long now = millis();

  if ((forcePublish) or (now - lastStatusMsgSentAt >
                         MQTT_PUBLISH_STATUS_INTERVAL)) {
    lastStatusMsgSentAt = now;

    const size_t bufferSize = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3);
    DynamicJsonBuffer jsonBuffer(bufferSize);
    JsonObject& root   = jsonBuffer.createObject();
    JsonObject& status = root.createNestedObject("status");

    if (messageId != NULL) {
      root["messageId"] = messageId;
    }

    status["powerOn"] = heaterPowerOn;
    status["temp"]    = heaterTempTarget;
    status["heating"] = heaterHeating;

    // convert to String
    String outString;
    root.printTo(outString);

    // publish the message
    mqttClient->publish(MQTT_TOPIC_GET, outString);
  }
}

void heaterSend(String payload) {
  PRINTLN("HEATER: Send to heater.");

  const size_t bufferSize = JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(3) + 50;
  DynamicJsonBuffer jsonBuffer(bufferSize);
  JsonObject& root   = jsonBuffer.parseObject(payload);
  JsonObject& status = root.get<JsonObject&>("status");

  if (!status.success()) {
    PRINTLN_E("HEATER: JSON with \"status\" key not received.");
    PRINTLN_E(payload);
    return;
  }

  const char *powerOn = status.get<const char *>("powerOn");

  if (powerOn) {
    setHeaterPowerStatus(strcasecmp(powerOn, "true") == 0);
  }

  uint8_t tempDelta = status.get<uint8_t>("tempDelta");

  if (tempDelta) {
    heaterTempTarget = validateTemp(heaterTempTarget + tempDelta);
  }

  uint8_t temp = status.get<uint8_t>("temp");

  if ((temp) and (temp != heaterTempTarget)) {
    heaterTempTarget = validateTemp(temp);
  }
  const char *messageId = root.get<const char *>("messageId");
  heaterPublishStatus(true, messageId);
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  PRINT("MQTT Message arrived [");
  PRINT(topic);
  PRINTLN("] ");

  String payloadString = String((char *)payload);

  // Do something according the topic
  if (strcmp(topic, MQTT_TOPIC_SET) == 0) {
    heaterSend(payloadString);
  }
  else {
    PRINT("MQTT: Warning: Unknown topic: ");
    PRINTLN(topic);
  }
}

void heaterButtonClick(int     pin,
                       uint8_t clicksCount          = 1,
                       bool    performInitialConfig = false) {
  if (performInitialConfig or ((clicksCount > 0) and (millis() >=
                                                      lastClickTime +
                                                      HEATER_BUTTON_DELAY_BETWEEN_CLICKS)))
  {
    while (true) {
      digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(pin,         HIGH);
      delay(HEATER_BUTTON_CLICK_TIME);
      digitalWrite(pin,         LOW);
      digitalWrite(LED_BUILTIN, HIGH);
      PRINT("HEATER: Button click on: ");

      switch (pin) {
        case PIN_TEMP_UP: {
          PRINTLN("TEMP UP");
          heaterTemp++;
          break;
        }
        case PIN_TEMP_DOWN: {
          PRINTLN("TEMP DOWN");

          if (!performInitialConfig) {
            heaterTemp--;
          }
          break;
        }
        case PIN_FUNC: {
          PRINTLN("FUNC");
          break;
        }
      }
      clicksCount--;
      lastClickTime = millis();

      if (performInitialConfig) {
        delay(HEATER_BUTTON_DELAY_BETWEEN_CLICKS);
      }

      if (clicksCount <= 0) {
        break;
      }
    }
  }
}

void heaterInitialConfiguration() {
  // Initial temp configuration
  PRINT("HEATER: Initial temperature configuration");
  PRINTLN(heaterTempTarget);
  heaterButtonClick(PIN_TEMP_DOWN, HEATER_MAX_TEMP - HEATER_MIN_TEMP, true);
  delay(HEATER_BUTTON_DELAY_BETWEEN_UP_DOWN);
  heaterButtonClick(PIN_TEMP_UP, HEATER_INITIAL_TEMP - HEATER_MIN_TEMP, true);
}

void setup() {
  pinMode(PIN_TEMP_UP, OUTPUT);
  digitalWrite(PIN_TEMP_UP, LOW);
  pinMode(PIN_TEMP_DOWN, OUTPUT);
  digitalWrite(PIN_TEMP_DOWN, LOW);
  pinMode(PIN_FUNC,    OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  wifiClient->init();
  mqttClient = new MqttClient(MQTT_SERVER,
                              MQTT_SERVER_PORT,
                              DEVICE_NAME,
                              MQTT_USERNAME,
                              MQTT_PASS,
                              MQTT_TOPIC_SET,
                              MQTT_SERVER_FINGERPRINT,
                              mqttCallback);
  fotaClient->init();
  heaterInitialConfiguration();
}

void setHeaterTempIfNeeded() {
  if (heaterTemp != heaterTempTarget) {
    // PRINT("HEATER: Set temperature to ");
    // PRINTLN(heaterTempTarget);
    int degreesDelta = heaterTemp - heaterTempTarget;

    // PRINTLN(degreesDelta);
    if (degreesDelta > 0) {
      heaterButtonClick(PIN_TEMP_DOWN);
    } else {
      heaterButtonClick(PIN_TEMP_UP);
    }

    // PRINT("HEATER: Temp is: ");
    // PRINTLN(heaterTemp);
  }
}

void loop() {
  RemotePrint::instance()->handle();
  fotaClient->loop();
  mqttClient->loop();
  setHeaterTempIfNeeded();
  heaterPublishStatus();
}
