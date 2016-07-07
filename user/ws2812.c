#include "ets_sys.h"
#include "osapi.h"
#include "c_types.h"
#include "ws2812.h"

#define WSGPIO 4
#define IO_MUX PERIPHS_IO_MUX_GPIO4_U
#define IO_FUNC FUNC_GPIO4

void __attribute__((optimize("O2"))) send_ws_0(uint8_t gpio){
    uint8_t i;
    i = 4; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
    i = 9; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}
void __attribute__((optimize("O2"))) send_ws_1(uint8_t gpio){
    uint8_t i;
    i = 8; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
    i = 6; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

void WS2812OutBuffer( uint8_t * buffer, uint16_t length)
{
	PIN_FUNC_SELECT(IO_MUX, IO_FUNC);
	PIN_PULLUP_EN(IO_MUX);
	GPIO_REG_WRITE(GPIO_ENABLE_W1TS_ADDRESS, (1 << WSGPIO));
	GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, (1 << WSGPIO));

	// Ignore incomplete Byte triples at the end of buffer:
	length -= length % 3;
 
	 // Do not remove these:
	os_delay_us(1);
	os_delay_us(1);
	
	uint16_t i;
//    GPIO_OUTPUT_SET(GPIO_ID_PIN(WSGPIO), 0);
	for( i = 0; i < length; i++ )
	{
		system_soft_wdt_feed();

		uint8_t byte = buffer[i];
		if( byte & 0x80 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x40 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x20 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x10 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x08 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x04 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x02 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
		if( byte & 0x01 ) send_ws_1(WSGPIO); else send_ws_0(WSGPIO);
	}
}

void led_reset(void *arg)
{
	memset(outbuffer, 0, LEDS*3); 
	WS2812OutBuffer(outbuffer, LEDS*3); 
}

void set_led(int r, int g, int b, long delay)
{
	memset(outbuffer, 0, LEDS*3); 
	outbuffer[0] = g; // green
	outbuffer[1] = r; // red
	outbuffer[2] = b; // blue
	WS2812OutBuffer(outbuffer, LEDS*3); 
	if(delay > 0)
	{
		os_timer_disarm(&led_timer);
		os_timer_setfn(&led_timer, (os_timer_func_t *)led_reset, NULL);
		os_timer_arm(&led_timer, delay, 0);
	}
}
