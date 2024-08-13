
#if defined(CONFIG_PLATFORM_8711B)
#include "rtl8710b_ota.h"
#include <FreeRTOS.h>
#include <task.h>
#elif defined(CONFIG_PLATFORM_8195A)
#include <ota_8195a.h>
#elif defined(CONFIG_PLATFORM_8195BHP)
#include <ota_8195b.h>
#elif defined(CONFIG_PLATFORM_8710C)
#include <ota_8710c.h>
#elif defined(CONFIG_PLATFORM_8721D)
#include <platform/platform_stdlib.h>
#include "rtl8721d_ota.h"
#include <FreeRTOS.h>
#include <task.h>
#endif
#include <wifi_constants.h>
#include "wifi_conf.h"
#include "gpio_api.h"

#define LED_BLUE   _PB_22 	//Blue LED
#define PUSH_BTN   _PB_23 	//Push button

gpio_t led_blue;
gpio_t push_btn;

#define PORT	8080
#define HOST	"192.168.0.119"  //"m-apps.oss-cn-shenzhen.aliyuncs.com"
#define RESOURCE "OTA_All.bin"     //"051103061600.bin"


#ifdef HTTP_OTA_UPDATE
void http_update_ota_task(void *param){
	(void)param;

	// Init blue LED
	gpio_init(&led_blue, LED_BLUE);
	gpio_dir(&led_blue, PIN_OUTPUT);    // Direction: Output
	gpio_mode(&led_blue, PullNone);     // No pull

	// Initial Push Button pin
    gpio_init(&push_btn, PUSH_BTN);
    gpio_dir(&push_btn, PIN_INPUT);     // Direction: Input
    gpio_mode(&push_btn, PullNone);       // No pull
	
#if defined(configENABLE_TRUSTZONE) && (configENABLE_TRUSTZONE == 1)
	rtw_create_secure_context(configMINIMAL_SECURE_STACK_SIZE);
#endif
	
	printf("\n\r\n\r\n\r\n\r<<<<<< OTA HTTP Example >>>>>>>\n\r\n\r\n\r\n\r");

	vTaskDelay(5000);

	int counter = 0;

	while(1) {

        if (gpio_read(&push_btn)) {
			if (counter > 0) {
            	gpio_write(&led_blue, 0);
				counter = 0;
			}
			vTaskDelay(1000);
            continue;
        } 
		
		if (counter < 3) {
			// turn on LED
            gpio_write(&led_blue, 1);
			counter++;
			printf("button pressed, counting to 3 ...\n");
			vTaskDelay(1000);
			continue;
		}

		printf("let's start ota update ...\n");
		break;
	}

	while(wifi_is_ready_to_transceive(RTW_STA_INTERFACE) != RTW_SUCCESS){
		printf("Wait for WIFI connection ...\n");
		vTaskDelay(1000);
	}

	int ret = -1;
	ret = http_update_ota(HOST, PORT, RESOURCE);


	printf("\n\r[%s] Update task exit", __FUNCTION__);
	if(!ret){
		printf("\n\r[%s] Ready to reboot", __FUNCTION__);	
		ota_platform_reset();
	}
	vTaskDelete(NULL);	
}


void example_ota_http(void){
		if(xTaskCreate(http_update_ota_task, (char const *)"http_update_ota_task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS){
		printf("\n\r[%s] Create update task failed", __FUNCTION__);
	}
}
#endif

