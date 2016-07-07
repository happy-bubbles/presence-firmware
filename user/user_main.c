#include <esp8266.h>
#include <config.h>
#include "osapi.h"
#include "ets_sys.h"

#include "espfs.h"
#include "ws2812.h"

#include "stdout.h"
#include "gpio16.h"

#include "httpclient.h"
#include "jsmn.h"
#include "hmac_sha1.h"

#include "uart.h"

#define PERIPHS_IO_MUX_PULLDWN          BIT6
#define PIN_PULLDWN_DIS(PIN_NAME)             CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)
#define sleepms(x) os_delay_us(x*1000);

//0 is lowest priority
#define procTaskPrio        0
#define procTaskQueueLen    1

static os_timer_t mqtt_timer;

static os_timer_t startup_connect_timer;

bool led_toggle = false;

static char json_ble_send[500];
static char ble_mac_addr[13];
static char ble_rssi [5];
static char ble_is_scan_resp [2];
static char ble_type [2];
static char ble_data[100];

char beacon_uuid [33];
char beacon_major [5];
char beacon_minor [5];
char tx_power [3];
char instance_id [13];

#include "mqtt.h"
#include "mqtt_client.h"
extern MQTT_Client mqttClient;

uint32_t atoint32(char *instr)
{
  uint32_t retval;
  int i;

  retval = 0;
  for (; *instr; instr++) {
    retval = 10*retval + (*instr - '0');
  }
  return retval;
}

extern void mqtt_client_init(void);
static void ICACHE_FLASH_ATTR mqtt_check(void *args) 
{
  if (mqttClient.connState != MQTT_CONNECTED)
	{
		os_printf("====================== MQTT BAD, restarting\r\n");
		//force a watchdog timeout and reset by doing something stupid like this here loop
		while(1){};
	}
}

char topic [130];
static void ICACHE_FLASH_ATTR sendMQTTibeacon(char * mac, char * uuid, char * major, char * minor, char * tx_power, char * rssi) 
{
  if (mqttClient.connState != MQTT_CONNECTED)
	{
		//os_printf("mqtt not connected, returning\r\n");
    return;
	}
	memset(json_ble_send, 0, 500);
	os_sprintf(json_ble_send, "{\"hostname\": \"%s\",\r\n\"beacon_type\": \"ibeacon\",\r\n\"mac\": \"%s\",\r\n\"rssi\": %s,\r\n\"uuid\": \"%s\",\r\n\"major\": \"%s\",\r\n\"minor\": \"%s\",\r\n\"tx_power\": \"%s\"}", flashConfig.hostname, mac, rssi, uuid, major, minor, tx_power);
	memset(topic, 0, 130);
	os_sprintf(topic, "happy-bubbles/ble/%s/ibeacon/%s", flashConfig.hostname, uuid);
	
  bool status = MQTT_Publish(&mqttClient, topic, json_ble_send, os_strlen(json_ble_send), 0, 0);
	//os_printf("sent mqtt message result: %d\r\n", status);
}
static void ICACHE_FLASH_ATTR sendMQTTeddystone(char * mac, char * namespace, char * instance_id, char * tx_power, char * rssi)
{
  if (mqttClient.connState != MQTT_CONNECTED)
	{
		//os_printf("mqtt not connected, returning\r\n");
    return;
	}
	memset(json_ble_send, 0, 500);
	os_sprintf(json_ble_send, "{\"hostname\": \"%s\",\r\n\"beacon_type\": \"eddystone\",\r\n\"mac\": \"%s\",\r\n\"rssi\": %s,\r\n\"tx_power\": \"%s\",\r\n\"namespace\": \"%s\",\r\n\"instance_id\": \"%s\"}", flashConfig.hostname, mac, rssi, tx_power, namespace, instance_id);
	memset(topic, 0, 130);
	os_sprintf(topic, "happy-bubbles/ble/%s/eddystone/%s", flashConfig.hostname, namespace);
	
  bool status = MQTT_Publish(&mqttClient, topic, json_ble_send, os_strlen(json_ble_send), 0, 0);
	//os_printf("sent mqtt message result: %d\r\n", status);
}

static void ICACHE_FLASH_ATTR sendMQTTraw(char * mac, char * rssi, char * is_scan, char * type, char * data) 
{
  if (mqttClient.connState != MQTT_CONNECTED)
	{
		//os_printf("mqtt not connected, returning\r\n");
    return;
	}

	memset(json_ble_send, 0, 500);
	os_sprintf(json_ble_send, "{\"hostname\": \"%s\",\r\n\"mac\": \"%s\",\r\n\"rssi\": %s,\r\n\"is_scan_response\": \"%s\",\r\n\"type\": \"%s\",\r\n\"data\": \"%s\"}", flashConfig.hostname, mac, rssi, is_scan, type, data);
	memset(topic, 0, 130);
	os_sprintf(topic, "happy-bubbles/ble/%s/raw/%s", flashConfig.hostname, mac);
  bool status = MQTT_Publish(&mqttClient, topic, json_ble_send, os_strlen(json_ble_send), 0, 0);
}

void strip_newlines(char * inbuff, int len, char * outbuff)
{
	if( inbuff[len-1] == '\n' )
	{
		    inbuff[len-1] = 0;
	}
	strcpy(outbuff, inbuff);
}

bool is_connected()
{
	int x=wifi_station_get_connect_status();
	if (x != STATION_GOT_IP) {
		return false;
	}
	return true;
}

os_event_t    procTaskQueue[procTaskQueueLen];

static void ICACHE_FLASH_ATTR
procTask(os_event_t *events)
{
	system_os_post(procTaskPrio, 0, 0 );
	if( events->sig == 0 && events->par == 0 )
	{
		//Idle Event.
	}
}

void ICACHE_FLASH_ATTR hex2str(char *out, unsigned char *s, size_t len)
{
	char tmp_str[3];
	memset(out, 0, 40);
	memset(tmp_str, 0, 3);
	os_sprintf(tmp_str, "%02x", s[0]);
	strcpy(out, tmp_str);
	int ii;
	for(ii=1; ii<len; ii++)
	{
		memset(tmp_str, 0, 3);
		os_sprintf(tmp_str,"%02x", s[ii]);
		strcat(out, tmp_str);
	}
}

#define ibeacon_include_1 "0201"
#define ibeacon_include_2 "1aff4c0002"

// callback with a buffer of characters that have arrived on the uart
void ICACHE_FLASH_ATTR
bleBridgeUartCb(char *buf, short length)
{
	os_printf("********* got serial %d **  %s \n\r", length, buf);
	char * pch;
	pch = strtok(buf, "|");
	int i = 0;
					
	memset(ble_mac_addr, 0, 12);
	memset(ble_rssi, 0, 5);
	memset(ble_is_scan_resp, 0, 1);
	memset(ble_type, 0, 1);
	memset(ble_data, 0, 100);
	
	while(pch != NULL)
	{
		switch(i) 
		{
			case 0:
				{
					strcpy(ble_mac_addr, pch);
					break;
				}
			case 1:
				{
					strcpy(ble_rssi, pch);
					break;
				}
			case 2:
				{
					strcpy(ble_is_scan_resp, pch);
					break;
				}
			case 3:
				{
					strcpy(ble_type, pch);
					break;
				}
			case 4:
				{
					strcpy(ble_data, pch);
					break;
				}
		}
		i++;
		//os_printf("token: %s\n", pch);
		pch = strtok(NULL, "|");
	}
	
	//basic filters
	//check mac is 12 chars
	int should_send = 0;
	if(strlen(ble_mac_addr) != 12)
	{
		os_printf("bad mac, not 12, %d, %s\n", strlen(ble_mac_addr), ble_mac_addr);
		should_send = 1;
	}
	//check mac is all hex
	if (!ble_mac_addr[strspn(ble_mac_addr, "0123456789abcdefABCDEF")] == 0) 
	{
		os_printf("bad mac, not all hex %s\n", ble_mac_addr);
		should_send = 2;
	}
	//check data is all hex
	if (strlen(ble_data) < 8 || !ble_data[strspn(ble_data, "0123456789abcdefABCDEF")] == 0 ) 
	{
		os_printf("bad data %s\n", ble_data);
		should_send = 3;
	}
	//check rssi is all numbers
	if (strlen(ble_rssi) < 1 || !ble_rssi[strspn(ble_rssi, "-0123456789")] == 0) 
	{
		os_printf("bad rssi %s\n", ble_rssi);
		should_send = 4;
	}
	//check scan_response is 0 or 1
	if (strlen(ble_is_scan_resp) < 1 || !ble_is_scan_resp[strspn(ble_is_scan_resp, "01")] == 0) 
	{
		os_printf("bad scanresp %s\n", ble_is_scan_resp);
		should_send = 5;
	}
	//check ble_type is hex
	if (strlen(ble_type) < 1 || !ble_type[strspn(ble_type, "0123456789abcdefABCDEF")] == 0) 
	{
		os_printf("bad type %s\n", ble_type);
		should_send = 6;
	}
	if(should_send == 0) 
	{
		//os_printf("^^^^^^^^^GOT THROUGH FILTERS!\r\n");
	}
	else 
	{
		//os_printf("xxxxxxxxxxFAIL FILTERS! %d \r\n", should_send);
		//os_printf("=====%s || %s || %s || %s || %s ===DONE\n", ble_mac_addr, ble_rssi, ble_is_scan_resp, ble_type, ble_data);
		return;
	}
	//what type of beacon is this?
	int beacon_type = 99;
	if(strncmp(ibeacon_include_1, ble_data, strlen(ibeacon_include_1)) == 0 &&
    strstr(ble_data, ibeacon_include_2) != NULL)
	{
		beacon_type = 1; //ibeacon

		//now extract uuid
    memset(beacon_uuid, 0, 33);
    memcpy(beacon_uuid, &ble_data[18], 32);
    beacon_uuid[32] = '\0';

    //extract minor
    memset(beacon_minor, 0, 5);
    memcpy(beacon_minor, &ble_data[54], 4);
    beacon_minor[5] = '\0';
 
    //extract major
    memset(beacon_major, 0, 5);
    memcpy(beacon_major, &ble_data[50], 4);
    beacon_major[5] = '\0';
       
    //extract tx_power
    memset(tx_power, 0, 3);
    memcpy(tx_power, &ble_data[58], 2);
    tx_power[3] = '\0';
		
	//os_printf("^^^^^ ibeacon %s %s %s %s\n", beacon_uuid, beacon_major, beacon_minor, tx_power);
	}
	
	else if(ble_data[10] == 'a' &&
    ble_data[11] == 'a' &&
    ble_data[12] == 'f' &&
    ble_data[13] == 'e' &&

    ble_data[18] == 'a' &&
    ble_data[19] == 'a' &&
    ble_data[20] == 'f' &&
    ble_data[21] == 'e' &&
    
    //this is data type for UID per https://github.com/google/eddystone/blob/master/protocol-specification.md
    ble_data[22] == '0' &&
    ble_data[23] == '0' 
    )
  {
    beacon_type = 0; //eddystone

    //now extract instance_id
    memset(instance_id, 0, 13);
    memcpy(instance_id, &ble_data[46], 12);
    instance_id[13] = '\0';

    //now extract UUID
    memset(beacon_uuid, 0, 33);
    memcpy(beacon_uuid, &ble_data[26], 20);
    beacon_uuid[32] = '\0';

		//TODO: extract calibrated TX power
		//https://github.com/google/eddystone/tree/master/eddystone-uid
    memset(tx_power, 0, 3);
    memcpy(tx_power, &ble_data[24], 2);
    tx_power[3] = '\0';
  }

	//add some filters to
	if(beacon_type == 0) 
	{
		//os_printf("send some mqtt for eddystone %s ========\n", beacon_uuid);
		sendMQTTeddystone(ble_mac_addr, beacon_uuid, instance_id, tx_power, ble_rssi);
		if(flashConfig.led_output)
		{
			set_led(0, 0, 55, 130);
		}
	}
	else if(beacon_type == 1) 
	{
		//os_printf("send some mqtt for ibeacon %s ========\n", beacon_uuid);
		sendMQTTibeacon(ble_mac_addr, beacon_uuid, beacon_major, beacon_minor, tx_power, ble_rssi);
		if(flashConfig.led_output)
		{
			set_led(0, 55, 0, 130);
		}
	}
	else
	{
		if(flashConfig.led_output)
		{
			set_led(55, 0, 0, 130);
		}
	}

	//os_printf("send some mqtt for mac %s =====%d===\n", ble_mac_addr, strlen(ble_mac_addr));
	sendMQTTraw(ble_mac_addr, ble_rssi, ble_is_scan_resp, ble_type, ble_data);
}

//startup connect Timer event
static void ICACHE_FLASH_ATTR startup_connect_check(void *arg)
{
	led_toggle = !led_toggle;
	if(led_toggle)
	{
		gpio16_output_set(0);
	}
	else
	{
		gpio16_output_set(1);
	}
	
	if(is_connected())
	{
		//wifi_set_opmode(1);
		os_timer_disarm(&startup_connect_timer);
	  //httpdInit(success_connect_urls, 80);
		gpio16_output_set(0);
	}
}

// initialize the custom stuff that goes beyond esp-link
void app_init() {
	stdoutInit();

	os_printf("\nReady\n");
	os_printf("\nGOING TO TRY BLE\n");
	os_printf("\nGOING TO TRY BLE\n");
	os_printf("\nGOING TO TRY BLE\n");
	os_printf("\nGOING TO TRY BLE\n");

	//now pull D1 / GPIO 5 low
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO4);
	PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO4_U);
	GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, (1 << 5));
	GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << 5));

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

  os_printf("initializing MQTT");
  mqtt_client_init();

	//purplse
	set_led(155, 0, 155, 0);
	sleepms(100);
	set_led(0, 0, 0, 0);
	sleepms(100);
	set_led(155, 0, 155, 0);
	sleepms(100);
	set_led(0, 0, 0, 0);
	sleepms(100);
	set_led(155, 0, 155, 0);
	sleepms(100);
	set_led(0, 0, 0, 0);
	sleepms(100);
	set_led(155, 0, 155, 0);
	sleepms(100);
	set_led(0, 0, 0, 0);

	//MQTT Timer 
	os_timer_disarm(&mqtt_timer);
	os_timer_setfn(&mqtt_timer, (os_timer_func_t *)mqtt_check, NULL);
	os_timer_arm(&mqtt_timer, 60000, 1);
	
	uart_add_recv_cb(&bleBridgeUartCb);

	system_os_post(procTaskPrio, 0, 0 );
}
