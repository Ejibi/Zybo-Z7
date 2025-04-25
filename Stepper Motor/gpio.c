/*
 * gpio.c
 * ----------------------------------------
 * GPIO Task Implementations for Button and LED Control
 *
 * Created by: Antonio Andara Lara
 * Modified by: Antonio Andara Lara | Mar 19, 2024; Mar 15, 2025
 *
 * Description:
 * This file defines FreeRTOS tasks for:
 * - pushbutton_task: Reads button states and sends them to the appropriate queues.
 * - led_task: Displays an LED animation based on the motor step mode.
 */

#include "stepper.h"
#include "gpio.h"


void pushbutton_task(void *p)
{
    u8 curr_val, prev_val = 0;
    const u8 BTN0_MASK = 1;
    const u8 BTN1_MASK = 2;
    int counter = 0;
 while (1)
    {
        curr_val = XGpio_DiscreteRead(&buttons, BUTTONS_CHANNEL);
        if ( (curr_val & BTN0_MASK) )
        {
        	xil_printf("enter\r\n");
        	vTaskDelay(DELAY_50_MS);
        	counter++;
        	xil_printf("counter %d",counter);
        	if (counter == 3) {
            	xil_printf("Sending emergency event to queue\r\n");
                xQueueSend(emergency_queue, &curr_val, 0);
                counter = 0;
                vTaskDelay(DELAY_50_MS);
            }
        }
        if ( (curr_val & BTN1_MASK) )
        {
            xQueueSend(button_queue, &curr_val, 0);
        }
        prev_val = curr_val;
        vTaskDelay(DELAY_50_MS);
    }
}


void led_task(void *p)
{
    u8 step_mode = 3;
    int index = 0; // To keep track of the current step in the animation sequence

    while(1) {
	/* --------------------------------------------------*/
    	// TODO: receive from led_queue into step_mode
    	if(xQueueReceive(led_queue, &step_mode, 0) == pdPASS){
    		xil_printf("%d",step_mode);
    	}
	/* --------------------------------------------------*/
        if ( step_mode > 2 ) {
        	index = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            switch (step_mode) {
                case WAVE_DRIVE:
                    // Cycle through Wave Drive patterns
                    switch (index++ % 4) {
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, WAVE_DRIVE_4); break;
                    } break;
                case FULL_STEP:
                    // Cycle through Full Step patterns
                    switch (index++ % 4) {
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, FULL_STEP_4); break;
                    } break;
                case HALF_STEP:
                    // Cycle through Half Step patterns
                    switch (index++ % 8) {
                        case 0: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_1); break;
                        case 1: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_2); break;
                        case 2: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_3); break;
                        case 3: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_4); break;
                        case 4: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_5); break;
                        case 5: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_6); break;
                        case 6: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_7); break;
                        case 7: XGpio_DiscreteWrite(&green_leds, 1, HALF_STEP_8); break;
                    } break;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
}
