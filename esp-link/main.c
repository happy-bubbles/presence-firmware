/*
* ----------------------------------------------------------------------------
* "THE BEER-WARE LICENSE" (Revision 42):
* Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain
* this notice you can do whatever you want with this stuff. If we meet some day,
* and you think this stuff is worth it, you can buy me a beer in return.
* ----------------------------------------------------------------------------
* Heavily modified and enhanced by Thorsten von Eicken in 2015
* ----------------------------------------------------------------------------
*/

#include <esp8266.h>
#include "httpd.h"
#include "httpdespfs.h"
#include "cgi.h"
#include "cgiwifi.h"
#include "cgipins.h"
#include "cgitcp.h"
#include "cgimqtt.h"
#include "cgiflash.h"
#include "auth.h"
#include "espfs.h"
#include "uart.h"
#include "status.h"
#include "config.h"
#include "log.h"
#include "gpio.h"
#include "cgiservices.h"

#include "gpio16.h"

#define PERIPHS_IO_MUX_PULLDWN          BIT6
#define PIN_PULLDWN_DIS(PIN_NAME)             CLEAR_PERI_REG_MASK(PIN_NAME, PERIPHS_IO_MUX_PULLDWN)

/*
#define NOTICE(format, ...) do {	                                          \
	LOG_NOTICE(format, ## __VA_ARGS__ );                                      \
	os_printf(format "\n", ## __VA_ARGS__);                                   \
} while ( 0 )
*/
#define NOTICE(format, ...) do {	                                          \
	os_printf(format "\n", ## __VA_ARGS__);                                   \
} while ( 0 )

/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
HttpdBuiltInUrl builtInUrls[] = {
  { "/", cgiRedirect, "/home.html" },
  { "/menu", cgiMenu, NULL },
  { "/flash/next", cgiGetFirmwareNext, NULL },
  { "/flash/upload", cgiUploadFirmware, NULL },
  { "/flash/reboot", cgiRebootFirmware, NULL },
//  { "/pgm/sync", cgiOptibootSync, NULL },
//  { "/pgm/upload", cgiOptibootData, NULL },
//  { "/log/text", ajaxLog, NULL },
//  { "/log/dbg", ajaxLogDbg, NULL },
//  { "/log/reset", cgiReset, NULL },
//  { "/console/reset", ajaxConsoleReset, NULL },
//  { "/console/baud", ajaxConsoleBaud, NULL },
//  { "/console/text", ajaxConsole, NULL },
//  { "/console/send", ajaxConsoleSend, NULL },
  //Enable the line below to protect the WiFi configuration with an username/password combo.
  //    {"/wifi/*", authBasic, myPassFn},
  { "/wifi", cgiRedirect, "/wifi/wifi.html" },
  { "/wifi/", cgiRedirect, "/wifi/wifi.html" },
  { "/wifi/info", cgiWifiInfo, NULL },
  { "/wifi/scan", cgiWiFiScan, NULL },
  { "/wifi/connect", cgiWiFiConnect, NULL },
  { "/wifi/connstatus", cgiWiFiConnStatus, NULL },
  { "/wifi/setmode", cgiWiFiSetMode, NULL },
  { "/wifi/special", cgiWiFiSpecial, NULL },
  { "/wifi/apinfo", cgiApSettingsInfo, NULL },
  { "/wifi/apchange", cgiApSettingsChange, NULL },  
  { "/system/info", cgiSystemInfo, NULL },
  { "/system/update", cgiSystemSet, NULL },
  { "/services/info", cgiServicesInfo, NULL },
  { "/services/update", cgiServicesSet, NULL },

#ifdef MQTT
  { "/mqtt", cgiMqtt, NULL },
#endif  
  { "*", cgiEspFsHook, NULL }, //Catch-all cgi function for the filesystem

  { NULL, NULL, NULL }
};

static ETSTimer configModeTimer;

#ifdef SHOW_HEAP_USE
static ETSTimer prHeapTimer;
static void ICACHE_FLASH_ATTR prHeapTimerCb(void *arg) {
  os_printf("Heap: %ld\n", (unsigned long)system_get_free_heap_size());
}
#endif

# define VERS_STR_STR(V) #V
# define VERS_STR(V) VERS_STR_STR(V)
char* esp_link_version = VERS_STR(VERSION);

// address of espfs binary blob
extern uint32_t _binary_espfs_img_start;

extern void app_init(void);
extern void mqtt_client_init(void);

void user_rf_pre_init(void) {
  //default is enabled
  system_set_os_print(DEBUG_SDK);
}

//prod board
#define BUTTON_GPIO 12 //prod
#define PERIPHS_IO_MUX_GPIO12_U          (PERIPHS_IO_MUX + 0x4)


bool is_config_button_pressed(void) 
{
	if(!GPIO_INPUT_GET(BUTTON_GPIO))
	{
		return true;
	}
	return false;
}

void stupid_restart(void) 
{
	//force watchdog reset
	while(1){};
}

static int config_button_count = 0;
bool should_config(void) 
{
	if(is_config_button_pressed())
	{
		NOTICE("config button was pressed");
		if(config_button_count > 2)
		{
			//toggle config mode, on next restart
			flashConfig.configMode = !flashConfig.configMode;
			configSave();
			NOTICE("config button was pressed > 3, doing RESTART!");
			stupid_restart();
		}
		config_button_count++;
	}
	else
	{
		//restart counter if not pressed
		NOTICE("config COUNT RESET");
		config_button_count = 0;
	}
}

// Main routine to initialize esp-link.
void user_init(void) {
  // get the flash config so we know how to init things
  //configWipe(); // uncomment to reset the config for testing purposes
  bool restoreOk = configRestore();
  // Init gpio pin registers
  gpio_init();
  gpio_output_set(0, 0, 0, (1<<15)); // some people tie it to GND, gotta ensure it's disabled
  // init UART
  //uart_init(flashConfig.baud_rate, 115200);
  uart_init(115200, 115200);
  logInit(); // must come after init of uart
  // Say hello (leave some time to cause break in TX after boot loader's msg
  os_delay_us(10000L);
  os_printf("\n\n** %s\n", esp_link_version);
  os_printf("Flash config restore %s\n", restoreOk ? "ok" : "*FAILED*");

	// //turn off the red LED on the boards
	gpio16_output_conf();
	gpio16_output_set(1);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO12_U, FUNC_GPIO12);
	PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO12_U);
	PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO12_U);
	
  // Status LEDs
  statusInit();
  // Wifi
  wifiInit();

  os_timer_disarm(&configModeTimer);
	os_timer_setfn(&configModeTimer, should_config, NULL);
  os_timer_arm(&configModeTimer, 1000, 1);

	// init the flash filesystem with the html stuff
  espFsInit(&_binary_espfs_img_start);
	
	if(flashConfig.configMode)
	{
  	EspFsInitResult res = espFsInit(&_binary_espfs_img_start);
  	os_printf("espFsInit %s\n", res?"ERR":"ok");
  	// mount the http handlers
	  httpdInit(builtInUrls, 80);
#ifdef SHOW_HEAP_USE
  	os_timer_disarm(&prHeapTimer);
	  os_timer_setfn(&prHeapTimer, prHeapTimerCb, NULL);
  	os_timer_arm(&prHeapTimer, 10000, 1);
#endif
	}

  struct rst_info *rst_info = system_get_rst_info();
  NOTICE("Reset cause: %d=%s", rst_info->reason, rst_codes[rst_info->reason]);
  NOTICE("exccause=%d epc1=0x%x epc2=0x%x epc3=0x%x excvaddr=0x%x depc=0x%x",
    rst_info->exccause, rst_info->epc1, rst_info->epc2, rst_info->epc3,
    rst_info->excvaddr, rst_info->depc);
  uint32_t fid = spi_flash_get_id();
  NOTICE("Flash map %s, manuf 0x%02lX chip 0x%04lX", flash_maps[system_get_flash_size_map()],
      fid & 0xff, (fid&0xff00)|((fid>>16)&0xff));
  NOTICE("** esp-link ready");

	float brightness = 0.5;

	set_led(0, 0, 0, 0);
  os_delay_us(10);
	set_led((int)(255*brightness), (int)(69*brightness), 0, 0);

	if(flashConfig.configMode)
	{
		NOTICE("ENTERING CONFIG MODE");
		set_led((int)(255*brightness), (int)(69*brightness), 0, 0);
	  cgiServicesSNTPInit();

	}
	else
	{
		NOTICE("INITIALIZING USER APPLICATION");
	  app_init();
	}
}
