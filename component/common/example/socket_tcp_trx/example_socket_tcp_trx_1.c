#include "FreeRTOS.h"
#include "task.h"
#include <platform/platform_stdlib.h>
#include <lwip/sockets.h>
//#include <osdep_api.h>
#include <osdep_service.h>
#include "serial_api.h"
#include "serial_ex_api.h"

#define SERVER_PORT     80
#define LISTEN_QLEN     2

#define UART_TX    	_PA_18	//UART0  TX
#define UART_RX    	_PA_19	//UART0  RX
#define UART_RTS  	_PA_16	//UART0  RTS
#define UART_CTS   	_PA_17	//UART0  CTS

static int tx_exit = 0, rx_exit = 0;
//static _Sema tcp_tx_rx_sema;
static _sema tcp_tx_rx_sema;

// i`m not sure if this is needed
// maybe UART can work in separate tasks
// maybe later i`ll check
static _sema uart_tx_rx_sema;

//#define Z_UART_DEBUG
serial_t sobj;


void uart_send_data(serial_t *sobj, void *data, int count)
{
    unsigned int i=0;
    rtw_down_sema(&uart_tx_rx_sema);
    for (i=0;i<count;i++) {
        serial_putc(sobj, ((char *)data)[i]);
    }
    rtw_up_sema(&uart_tx_rx_sema);
}


static void tx_thread(void *param)
{
	int client_fd = * (int *) param;
	unsigned char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	printf("\n%s start\n", __FUNCTION__);

	while(1) {
		int ret = 0;


        // simple buffering
        // to avoid sending frame with few bytes
        // if there is more in the pipe
        int i = 0;
        int j = 0;
        for (i=0;i<sizeof(buffer);i++){
            if (serial_readable(&sobj))
            {
                rtw_down_sema(&uart_tx_rx_sema);
                buffer[j++] = serial_getc(&sobj);
                rtw_up_sema(&uart_tx_rx_sema);
            }
        }

        if (j>0)
        {
#ifdef Z_UART_DEBUG
            printf("%d bytes UART->TCP: ", j);
            int i = 0;
            for (i = 0; i < j ; i++)
            {
                printf("0x%x ",buffer[i]);
            }
            printf("\n");
#endif
            if (j<sizeof(buffer)){
                rtw_down_sema(&tcp_tx_rx_sema);
                ret = send(client_fd, buffer, j, 0);
                rtw_up_sema(&tcp_tx_rx_sema);
            }
            else
            {
                printf("buff corrupted");
            }
        }
        else
        {
            if (rx_exit){
                goto exit;
            }
            continue; // avoid quit
        }

		if(ret <= 0)
			goto exit;

		vTaskDelay(100);
	}

exit:
	printf("\n%s exit\n", __FUNCTION__);
	tx_exit = 1;
	vTaskDelete(NULL);
}

static void rx_thread(void *param)
{
	int client_fd = * (int *) param;
	unsigned char buffer[1024];
	printf("\n%s start\n", __FUNCTION__);

	while(1) {
		int ret = 0, sock_err = 0;
		size_t err_len = sizeof(sock_err);

		rtw_down_sema(&tcp_tx_rx_sema);
		ret = recv(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT);
		getsockopt(client_fd, SOL_SOCKET, SO_ERROR, &sock_err, &err_len);
		rtw_up_sema(&tcp_tx_rx_sema);
        if (ret > 0){
#ifdef Z_UART_DEBUG
            printf("%d bytes TCP->UART: ", ret);
            int i=0;
            for (i=0;i<ret;i++)
            {
                printf("0x%x ",buffer[i]);
            }
            printf("\n");
#endif
            uart_send_data( &sobj, buffer, ret);
        }

		// ret == -1 and socket error == EAGAIN when no data received for nonblocking
		if((ret == -1) && ((sock_err == EAGAIN)
#if LWIP_VERSION_MAJOR >= 2
			||(sock_err == 0)
#endif
		))
			continue;
		else if(ret <= 0)
			goto exit;

		vTaskDelay(10);
	}

exit:
	printf("\n%s exit\n", __FUNCTION__);
	rx_exit = 1;
	vTaskDelete(NULL);
}

static void example_socket_tcp_trx_thread(void *param)
{
	int server_fd = -1, client_fd = -1;
	struct sockaddr_in server_addr, client_addr;
	size_t client_addr_size;

	// Delay to wait for IP by DHCP
	vTaskDelay(10000);
	printf("\nExample: socket tx/rx 1\n");

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;

	if(bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
		printf("ERROR: bind\n");
		goto exit;
	}

	if(listen(server_fd, LISTEN_QLEN) != 0) {
		printf("ERROR: listen\n");
		goto exit;
	}

	while(1) {
		client_addr_size = sizeof(client_addr);
		client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_size);

		if(client_fd >= 0) {
			tx_exit = 1;
			rx_exit = 1;
			rtw_init_sema(&tcp_tx_rx_sema, 1);

			if(xTaskCreate(tx_thread, ((const char*)"tx_thread"), 512, &client_fd, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
				printf("\n\r%s xTaskCreate(tx_thread) failed", __FUNCTION__);
			else
				tx_exit = 0;

			// vTaskDelay(10);

			if(xTaskCreate(rx_thread, ((const char*)"rx_thread"), 512, &client_fd, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
				printf("\n\r%s xTaskCreate(rx_thread) failed", __FUNCTION__);
			else
				rx_exit = 0;

			while(1) {
				if(tx_exit && rx_exit) {
					close(client_fd);
					break;
				}
				else
					vTaskDelay(1000);
			}

			rtw_free_sema(&tcp_tx_rx_sema);
		}
	}

exit:
	close(server_fd);
	vTaskDelete(NULL);
}

void example_socket_tcp_trx_1(void)
{
	sobj.uart_idx = 0;

    // mbed uart test
    rtw_init_sema(&uart_tx_rx_sema, 1);
    serial_init(&sobj,UART_TX,UART_RX);
    serial_baud(&sobj,115200);
    serial_format(&sobj, 8, ParityNone, 1);
	serial_rx_fifo_level(&sobj, FifoLvHalf);
	serial_set_flow_control(&sobj, FlowControlRTSCTS, UART_RTS, UART_CTS);    // Pin assignment is ignored
	if(xTaskCreate(example_socket_tcp_trx_thread, ((const char*)"example_socket_tcp_trx_thread"), 1024, NULL, tskIDLE_PRIORITY + 1, NULL) != pdPASS)
		printf("\n\r%s xTaskCreate(example_socket_tcp_trx_thread) failed", __FUNCTION__);
	rtw_free_sema(&tcp_tx_rx_sema);
}
