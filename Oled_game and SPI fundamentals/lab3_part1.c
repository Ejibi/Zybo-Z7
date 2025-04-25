/*****************************  FreeRTOS includes. ****************************/
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
/***************************** Include Files *********************************/

#include "xparameters.h"	/* SDK generated parameters */
#include "xspips.h"			/* SPI device driver */
#include "xgpio.h"
#include "xil_printf.h"
#include <ctype.h>

/********************** User defined Include Files **************************/
#include "initialization.h"
#include "spi_driver.h"

/************************** Constant Definitions *****************************/
#define CHAR_PERCENT			0x25	// '%'  character is used as termination sequence
#define CHAR_CARRIAGE_RETURN	0x0D	// '\r' character is used to detect 'enter'
#define CHAR_DOLLAR 			0x24	// '$'  character is used as a dummy character
#define TRANSFER_SIZE_IN_BYTES  1 		// 1 byte is transferred between SPI 0 and SPI 1 in the provided template every time
#define QUEUE_LENGTH			500
#define UART_DEVICE_ID_1		XPAR_PS7_UART_1_DEVICE_ID
#define UART_BASE_ADDRESS       XPAR_PS7_UART_1_BASEADDR
#define SPI_0_DEVICE_ID			XPAR_XSPIPS_0_DEVICE_ID
#define SPI_1_DEVICE_ID			XPAR_XSPIPS_1_DEVICE_ID
#define RGB_DEVICE_ID           XPAR_AXI_LEDS_DEVICE_ID
#define RGB_CHANNEL             2

/************************* Task Function definitions *************************/
static void vUartManagerTask( void *pvParameters );
static void vSpiMainTask( void *pvParameters );
static void vSpiSubTask( void *pvParameters );
static TaskHandle_t xTask_uart;
static TaskHandle_t xTask_spi0;
static TaskHandle_t xTask_spi1;

/************************* Queue Function definitions *************************/
static QueueHandle_t uart_to_spi = NULL;//queue between task1 and task2
static QueueHandle_t spi_to_uart = NULL;//queue between task1 and task2

/************************* Function prototypes *************************/
void checkCommand(char buffer[]);
void updateRollingBuffer(char buffer[], u8 byte);
void terminateInput(void);
u8 terminationSequence(char buffer[]);
void checkTerminationSequence(u8 *sequence_flag, u8 *uart_byte);
/************************* Global Variables *********************************/
XGpio RgbLed;
XUartPs_Config *Config;

u32 flag = 0; 					// enables sending dummy char in SPI1-SPI0 mode
u8 spi_loopback_enabled = 0;	// GLOBAL variable to enable/disable loopback for spi main task
u8 uart_loopback_enabled = 0;   // GLOBAL variable to enable/disable loopback for uart task
u8 active_command = 1;			// determines the loopback mode
u8 spi_data;	// contains last data received when spi loopback was enabled

int message_counter=0;



int main(void)
{
	int status;
	Config = XUartPs_LookupConfig(UART_DEVICE_ID_1);

	xTaskCreate( vUartManagerTask
			   , (const char *) "UART TASK"
			   , configMINIMAL_STACK_SIZE*10
			   , NULL
			   , tskIDLE_PRIORITY+4
			   , &xTask_uart
			   );

	xTaskCreate( vSpiMainTask
			   , (const char *) "Main SPI TASK"
			   , configMINIMAL_STACK_SIZE*10
			   , NULL
			   , tskIDLE_PRIORITY+3
			   , &xTask_spi0
			   );

	xTaskCreate( vSpiSubTask
			   , (const char *) "Sub SPI TASK"
			   , configMINIMAL_STACK_SIZE*10
			   , NULL
			   , tskIDLE_PRIORITY+3
			   , &xTask_spi1
			   );


	uart_to_spi = xQueueCreate(QUEUE_LENGTH, sizeof(u8)); //connects vUartManagerTask -> vSpiMainTask
	spi_to_uart = xQueueCreate(QUEUE_LENGTH, sizeof(u8)); //connects vSpiMainTask -> vUartManagerTask


	/* Check the uart_to_spi and spi_to_uart if they were created. */
	configASSERT(uart_to_spi);
	configASSERT(spi_to_uart);

	status = XGpio_Initialize(&RgbLed, RGB_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}
	XGpio_SetDataDirection(&RgbLed, RGB_CHANNEL, 0x00);


/* TODO: Complete the UART and SPI initialization routines
 *
 */
	if(intializeUART(UART_DEVICE_ID_1) == XST_SUCCESS){
		xil_printf("\n Successful UART initialization \n");
	}

	if(initializeSPI(SPI_0_DEVICE_ID,SPI_1_DEVICE_ID) == XST_SUCCESS){
		xil_printf("\n Successful SPI initialization \n");
	}

	vTaskStartScheduler();

	while(1);

	return 0;
}


static void vUartManagerTask( void *pvParameters )
{
	const u8 dummy = CHAR_DOLLAR;   // dummy char to send to the slave as a control character
	u8 uart_byte = 0, spi_data=0;				// contains the last byte received through UART
	printMenu();
	char rollingBuffer[3]={'\0', '\0', CHAR_CARRIAGE_RETURN}; // rolling buffer to detect commands and termination sequence

	while(1){
		if(flag==1){ 						// flag is set to 1 when the user enters the end sequence on SPI1-SPI0 mode
			while(1){
			/* TODO: Implement the following sequence of operations:
			 * 1. Send a "dummy" control character to uart_spi to initiate communication.
			 * 2. Await incoming bytes from the SPIMain task via spi_uart using xQueueReceive with portMAX_DELAY.
			 * 3. If a Dummy character ('$') is received, break out of the loop.
			 * 4. Check if the UART is ready to send data (XUartPs_IsTransmitFull). If it's full, wait until there is space.
			 * 5. Send the received byte to the UART until a null character ('\0') is received. Refer to the XUartPs_WriteReg
			 *    function with the appropriate parameters.
			 */

				xQueueSend(uart_to_spi,&dummy,0);
				if(xQueueReceive(spi_to_uart,&spi_data,portMAX_DELAY) == pdTRUE){
					if( spi_data == '$'){
						break;
					}
					if(XUartPs_IsTransmitFull(UART_BASE_ADDRESS) == TRUE ){
										vTaskDelay(pdMS_TO_TICKS(10));
					}
					else{
						if(spi_data != '\0'){
						XUartPs_WriteReg(UART_BASE_ADDRESS,XUARTPS_FIFO_OFFSET,spi_data);
						}
					}

				}

			}
			terminateInput();
		} else {
			while (XUartPs_IsReceiveData(Config->BaseAddress)){

				uart_byte = XUartPs_ReadReg(Config->BaseAddress, XUARTPS_FIFO_OFFSET);
				updateRollingBuffer(rollingBuffer, uart_byte);
				if(uart_loopback_enabled == 1 && active_command == 1){
					XUartPs_WriteReg(Config->BaseAddress, XUARTPS_FIFO_OFFSET, uart_byte);
					if(terminationSequence(rollingBuffer)){
						terminateInput();
					}
				} else if(active_command == 2){
					xQueueSendToBack(uart_to_spi, &uart_byte, 0UL);
					xQueueReceive(spi_to_uart, &spi_data, portMAX_DELAY);
					while (XUartPs_IsTransmitFull(Config->BaseAddress) == TRUE);

					if(spi_loopback_enabled){
						XUartPs_WriteReg(Config->BaseAddress, XUARTPS_FIFO_OFFSET, spi_data);
						if(terminationSequence(rollingBuffer)){
							terminateInput();
						}
					} else if(!spi_loopback_enabled){
						if (spi_data != '\0') {
							XUartPs_WriteReg(Config->BaseAddress, XUARTPS_FIFO_OFFSET, spi_data);
						}
                    }
				}

				checkCommand(rollingBuffer);
			}
		}
	vTaskDelay(1);
	}
}


static void vSpiMainTask( void *pvParameters )
{

	u8 received_from_uart;
	u8 send_to_uart;
	u32 received_bytes=0;
	u8 send_buffer[1];

	while(1){

		if(xQueueReceive( uart_to_spi
					 , &received_from_uart	//queue to receive the data from UART Manager task
					 , portMAX_DELAY
					 ) == pdTRUE){

		if(active_command == 2){

			if(spi_loopback_enabled){
				xQueueSend(spi_to_uart, &received_from_uart, 0UL); // byte is read from the FIFO1
			} else if (!spi_loopback_enabled){
			/*******************************************/
			// TODO 1: Implement the logic for handling SPI communication.
			// This process involves sending, yielding for SPI processing, reading back, and forwarding through spi_to_uart.
			// 1. Copy the received data from the uart_to_spi into "send_buffer" variable. The "send_buffer" variable is declared for you.
			// 2. Increment the received_bytes counter for each byte received.
			// 3. Check if received_bytes matches TRANSFER_SIZE_IN_BYTES.
			//    a. If true, transmit the collected bytes via SPI.
			//    b. Yield the task to allow for SPI communication processing.
			//    c. Read the response back from SPI.
			//    d. Send the received byte back through FIFO2.
			//    e. Reset received_bytes counter to prepare for the next message.
				send_buffer[0] = received_from_uart;
				received_bytes++;
				if (received_bytes == TRANSFER_SIZE_IN_BYTES){
					spiMasterWrite(send_buffer,received_bytes );
					taskYIELD();
					spiMasterRead(received_bytes);
					xQueueSend(spi_to_uart, & RxBuffer_Master, 0);
					received_bytes = 0;


				}
			/*******************************************/

			}
		}
		}
		vTaskDelay(1);
	}
}


static void vSpiSubTask( void *pvParameters )
{
	u8 temp_store, *sequence_flag=0;
	int spi_rx_bytes = 0;
	int msg_bytes = 0;
	int copy_msg_bytes = 0;
	char buffer[150];
	int str_length; 				// length of number of bytes string
	while(1){
        if(spi_loopback_enabled==0 && active_command==2){
        /* TODO: Implement the logic for handling and monitoring SPI communication in this task.
         * This involves reading from SPI, monitoring for a specific sequence, and responding with a summary message.
         * the device should read a byte (ignoring all dummy'$' characters)
         * Detect the termination sequence (\r%\r) and set the termination_flag to 3 upon successful detection.
         * Continuously track:
         *     a. The number of characters received over SPI.
         *     b. The number of complete messages received so far.
         * if the byte is valid then all byte counts (spi_rx_bytes and msg_bytes) are updated
         * Use "RxBuffer_Slave" for reading data from the SPI slave node.
         * after receiving every byte it should check for the termination sequence.
		 * if the termination sequence is detected a string with this format must be created:
		 * Store the message string "\nNumber of bytes received over SPI:%d\nLast message byte count: %d\nTotal messages received: %d\n".
		 * where the placeholders represent spi_rx_bytes, msg_bytes and message_counter
		 * Before detecting the termination sequence, ensure bytes are consistently sent back to the SPI master.
		 * create a loop that will read dummy characters from the master and will send the bytes of the string
		 * Upon receiving the termination sequence:
		 *     a. Construct the message string with the total number of bytes and messages received.
		 *     b. Loop through the message string and send it back to the SPI master using the appropriate SpiSlave write function.
		 * when the loop is done reset the appropriate variables and continue
		 */

        	spiSlaveRead(TRANSFER_SIZE_IN_BYTES);
        	temp_store = RxBuffer_Slave[0];
        	if(temp_store == CHAR_DOLLAR){
        	    continue;
        	}

        	if(*sequence_flag == 3){
        	        		msg_bytes = copy_msg_bytes;
        	        		continue;
        	        	}
        	else if( temp_store == '\r' && *sequence_flag != 2){
        		msg_bytes = 0;
        	}


        	spi_rx_bytes++;
        	msg_bytes++;

        	spiSlaveWrite(&temp_store,TRANSFER_SIZE_IN_BYTES);
        	checkTerminationSequence(sequence_flag, &temp_store);


        	if (*sequence_flag == 3) {

        		message_counter++;


        		flag = 1;
        		snprintf(buffer, sizeof(buffer),
                         "\nNumber of bytes received over SPI:%d\n"
                         "Last message byte count: %d\n"
                         "Total messages received: %d\n",
                         (int)spi_rx_bytes - 1, msg_bytes, message_counter);

				str_length = strlen(buffer);

					for (int i = 0; i < str_length; i++) {
					spiSlaveWrite((u8 *)&buffer[i], 1);
				}
				*sequence_flag = 0;
        	}



		vTaskDelay(1);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
// FUNCTION DEFINITIONS
///////////////////////////////////////////////////////////////////////////

/**
 * This functions checks if the user input is a valid command or not. A valid command would be <ENTER><COMMAND><ENTER>.
 * <COMMAND> can only be 1 or 2
 * For a valid command, the active_command would be 1 or 2
 * Returns: None
 */
void checkCommand(char buffer[])
{

	if (buffer[2] == CHAR_CARRIAGE_RETURN && buffer[0] == CHAR_CARRIAGE_RETURN) {
		if (buffer[1] == '1') {
			uart_loopback_enabled = !uart_loopback_enabled;
			active_command = 1;
			if(uart_loopback_enabled){
				xil_printf("\n*** UART Loop-back ON ***\r\n");
				XGpio_DiscreteWrite(&RgbLed, RGB_CHANNEL, 4);
			} else {
				xil_printf("\n*** UART Loop-back OFF ***\r\n");
				XGpio_DiscreteWrite(&RgbLed, RGB_CHANNEL, 0);
			}
		} else if (buffer[1] == '2') {
			active_command = 2;
			spi_loopback_enabled = !spi_loopback_enabled;
			if(spi_loopback_enabled){
				xil_printf("\n*** SPI Loop-back ON (SPI Main enabled) ***\r\n");
				XGpio_DiscreteWrite(&RgbLed, RGB_CHANNEL, 1);
			} else {
				xil_printf("\n*** SPI Loop-back OFF (SPI Main-Sub enabled) ***\r\n");
				XGpio_DiscreteWrite(&RgbLed, RGB_CHANNEL, 2);
			}
		}
	}
}


void updateRollingBuffer(char buffer[], u8 byte)
{
	buffer[0] = buffer[1];
	buffer[1] = buffer[2];
	buffer[2] = byte;
}


u8 terminationSequence(char buffer[])
{
	return ( buffer[2] == CHAR_CARRIAGE_RETURN &&
	         buffer[1] == CHAR_PERCENT         &&
	         buffer[0] == CHAR_CARRIAGE_RETURN
			);
}


void terminateInput(void)
{
	active_command = 1;
	spi_loopback_enabled = 0;
	uart_loopback_enabled = 0;
	xil_printf("\n*** Text entry ended using termination sequence ***\r\n");
	XGpio_DiscreteWrite(&RgbLed, RGB_CHANNEL, 0);
}


/**
 * This functions checks if the user input is the termination sequence \r%\r
 * When the termination sequence is detected, sequence_flag would be 3
 * Returns: None
 */
void checkTerminationSequence(u8 *sequence_flag, u8 *uart_byte)
{
	/* TODO : Write the body of this function to take a sequence flag that stores the state of the machine
	 *       and a byte, using the state and the value of the received byte check that the last three received bytes are
	 *       <enter><%><enter>
	 *
	 */
	if (*uart_byte == '\r' && *sequence_flag == 0) {
			(*sequence_flag)++;

	}else if (*uart_byte == '%'&& *sequence_flag == 1) {
		(*sequence_flag)++;

	}else if (*uart_byte == '\r'&& *sequence_flag == 2) {
		(*sequence_flag)++;

	}else{
		*sequence_flag = 0;
	}

}
