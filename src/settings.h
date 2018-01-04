#ifndef SETTING_H
#define SETTING_H

#define DEVICE_NAME "LargeBedroomHeater2"

#define MQTT_TOPIC_GET "get/apartment/largeBedroom/heater/2"
#define MQTT_TOPIC_SET "set/apartment/largeBedroom/heater/2"

#define HEATER_INITIAL_TEMP (uint8_t)6U
#define HEATER_MIN_TEMP (uint8_t)5U
#define HEATER_MAX_TEMP (uint8_t)35U
#define HEATER_BUTTON_DELAY_BETWEEN_UP_DOWN 2000 // 2000
#define HEATER_BUTTON_DELAY_BETWEEN_CLICKS 750   // 750
#define HEATER_BUTTON_CLICK_TIME 200             // 200

#define PIN_TEMP_UP D1                           // White wire
#define PIN_TEMP_DOWN D5                         // Black wire
#define PIN_FUNC D2                              // Yellow wire

#endif // ifndef SETTING_H
