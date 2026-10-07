#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>

void MQTT_Init();
void MQTT_Loop();          // non-blocking; call every loop()
bool MQTT_IsConnected();
bool MQTT_Publish(const char *topic, const char *message, bool retain = false);

#endif
