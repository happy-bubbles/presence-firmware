// Copyright 2015 by Thorsten von Eicken, see LICENSE.txt

#include <esp8266.h>
#include "config.h"
//#include "serled.h"
#include "cgiwifi.h"

#ifdef MQTT
#include "mqtt.h"
#include "mqtt_client.h"
extern MQTT_Client mqttClient;

//===== MQTT Status update

// Every minute...
#define MQTT_STATUS_INTERVAL (60*1000)

static ETSTimer mqttStatusTimer;

int ICACHE_FLASH_ATTR
mqttStatusMsg(char *buf) {
  sint8 rssi = wifi_station_get_rssi();
  if (rssi > 0) rssi = 0; // not connected or other error
  //os_printf("timer rssi=%d\n", rssi);

  // compose MQTT message
  return os_sprintf(buf,
    "{\"rssi\":%d, \"heap_free\":%ld}",
    rssi, (unsigned long)system_get_free_heap_size());
}

// Timer callback to send an RSSI update to a monitoring system
static void ICACHE_FLASH_ATTR mqttStatusCb(void *v) {
  if (!flashConfig.mqtt_status_enable || os_strlen(flashConfig.mqtt_status_topic) == 0 ||
    mqttClient.connState != MQTT_CONNECTED)
    return;

  char buf[128];
  mqttStatusMsg(buf);
  MQTT_Publish(&mqttClient, flashConfig.mqtt_status_topic, buf, os_strlen(buf), 1, 0);
}



#endif // MQTT


//===== Init status stuff

void ICACHE_FLASH_ATTR statusInit(void) {

#ifdef MQTT
  os_timer_disarm(&mqttStatusTimer);
  os_timer_setfn(&mqttStatusTimer, mqttStatusCb, NULL);
  os_timer_arm(&mqttStatusTimer, MQTT_STATUS_INTERVAL, 1); // recurring timer
#endif // MQTT
}


