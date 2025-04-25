
// Include FreeRTOS Libraries
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Include xilinx Libraries
#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"

// Other miscellaneous libraries
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "pmodkypd.h"
#include "sleep.h"
#include "PmodOLED.h"
#include "OLEDControllerCustom.h"


#define BTN_DEVICE_ID  XPAR_INPUTS_DEVICE_ID
#define BTN_CHANNEL    1
#define KYPD_DEVICE_ID XPAR_KEYPAD_DEVICE_ID
#define KYPD_BASE_ADDR XPAR_KEYPAD_BASEADDR


#define FRAME_DELAY 50000

// keypad key table
#define DEFAULT_KEYTABLE 	"0FED789C456B123A"

// Declaring the devices
XGpio       btnInst;
PmodOLED    oledDevice;
PmodKYPD 	KYPDInst;

// Function prototypes
void initializeScreen();
static void oledTask( void *pvParameters );

// To change between PmodOLED and OnBoardOLED is to change Orientation
const u8 orientation = 0xF; // Set up for Normal PmodOLED(false) vs normal
                            // Onboard OLED(true)
const u8 invert = 0x0; // true = whitebackground/black letters
                       // false = black background /white letters
u8 keypad_val = 'x';

void initializeScreen();
void InitializeKeypad();
static void oledTask(void *pvParameters);
void resetBoard(char board[3][3]);
void drawBoardText(PmodOLED* oled, char board[3][3]);
int checkWinner(char board[3][3], char player);
int digitToBoardPosition(char digit, int *row, int *col);
char getKeypadDigit();
void celebrateWin(PmodOLED *oled);



int main()
{
	int status = 0;
	// Initialize Devices
	initializeScreen();
	InitializeKeypad();

	// Buttons
	status = XGpio_Initialize(&btnInst, BTN_DEVICE_ID);
	if(status != XST_SUCCESS){
		xil_printf("GPIO Initialization for SSD failed.\r\n");
		return XST_FAILURE;
	}
	XGpio_SetDataDirection (&btnInst, BTN_CHANNEL, 0x0f);


	xil_printf("Initialization Complete, System Ready!\n");


	xTaskCreate( oledTask					/* The function that implements the task. */
			   , "screen task"				/* Text name for the task, provided to assist debugging only. */
			   , configMINIMAL_STACK_SIZE	/* The stack allocated to the task. */
			   , NULL						/* The task parameter is not used, so set to NULL. */
			   , tskIDLE_PRIORITY			/* The task runs at the idle priority. */
			   , NULL
			   );

	vTaskStartScheduler();


   while(1);

   return 0;
}

void initializeScreen()
{
   OLED_Begin(&oledDevice, XPAR_PMODOLED_0_AXI_LITE_GPIO_BASEADDR,
         XPAR_PMODOLED_0_AXI_LITE_SPI_BASEADDR, orientation, invert);
}

void InitializeKeypad()
{
   KYPD_begin(&KYPDInst, KYPD_BASE_ADDR);
   KYPD_loadKeyTable(&KYPDInst, (u8*) DEFAULT_KEYTABLE);
}


void drawCrossHair(u8 xco, u8 yco)
{
	OLED_MoveTo(&oledDevice, xco, 0);
	OLED_DrawLineTo(&oledDevice, xco, OledRowMax - 1);
	OLED_MoveTo(&oledDevice, 0, yco);
	OLED_DrawLineTo(&oledDevice, OledColMax - 1, yco);
}


static void oledTask( void *pvParameters )
{
	   char board[3][3];
	    char currentPlayer = 'X';
	    char keyPressed;
	    int row, col;
	    int movesMade = 0;
	    int gameOver = 0;
	    int buttonval = 0;
	    int k = 0;

	    resetBoard(board);
	    OLED_ClearBuffer(&oledDevice);
	    OLED_SetCursor(&oledDevice, 0,1);
	    OLED_PutString(&oledDevice, "Game Start press 2");
	    OLED_Update(&oledDevice);




	    while(1) {
	    	 buttonval = XGpio_DiscreteRead(&btnInst,BTN_CHANNEL);
	    	 if( buttonval == 2){
	    		 k=1;
	    	 }
	    	if(k==1){
	        drawBoardText(&oledDevice, board);
	        OLED_Update(&oledDevice);
	        keyPressed = getKeypadDigit();
	        if (digitToBoardPosition(keyPressed, &row, &col)) {
	            if (board[row][col] == ' ') {
	                board[row][col] = currentPlayer;
	                movesMade++;
	                if (checkWinner(board, currentPlayer)) {
	                    OLED_ClearBuffer(&oledDevice);
	                    drawBoardText(&oledDevice, board);
	                    OLED_SetCursor(&oledDevice, 0, 3);
	                    OLED_PutString(&oledDevice, " Win");
	                    OLED_PutChar(&oledDevice, currentPlayer);
	                    OLED_Update(&oledDevice);
	                    vTaskDelay(70);
	                    celebrateWin(&oledDevice);
	                    vTaskDelay(20);
	                    gameOver = 1;
	                } else if (movesMade == 9) {
	                    OLED_ClearBuffer(&oledDevice);
	                    drawBoardText(&oledDevice, board);
	                    OLED_SetCursor(&oledDevice, 0, 3);
	                    OLED_PutString(&oledDevice, "It's a tie!");
	                    OLED_Update(&oledDevice);
	                    gameOver = 1;
	                } else {
	                    currentPlayer = (currentPlayer == 'X') ? 'O' : 'X';
	                }
	            }
	        }
	        if (gameOver) {
	            vTaskDelay(pdMS_TO_TICKS(200));
	            resetBoard(board);
	            currentPlayer = 'X';
	            movesMade = 0;
	            gameOver = 0;
	        }
	    }
	    }
}



void resetBoard(char board[3][3])
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            board[i][j] = ' ';
        }
    }
}

void drawBoardText(PmodOLED* oled, char board[3][3])
{
    OLED_ClearBuffer(oled);
    for (int i = 0; i < 3; i++) {
        OLED_SetCursor(oled, 0, i);
        for (int j = 0; j < 3; j++) {
            OLED_PutChar(oled, board[i][j]);
            if (j < 2) OLED_PutString(oled, "|");
        }
    }
    vTaskDelay(5);
}

int checkWinner(char board[3][3], char player)
{
    for (int i = 0; i < 3; i++) {
        if ((board[i][0] == player && board[i][1] == player && board[i][2] == player) ||
            (board[0][i] == player && board[1][i] == player && board[2][i] == player)) {
            return 1;
        }
    }
    if ((board[0][0] == player && board[1][1] == player && board[2][2] == player) ||
        (board[0][2] == player && board[1][1] == player && board[2][0] == player)) {
        return 1;
    }
    return 0;
}

int digitToBoardPosition(char digit, int *row, int *col)
{
    if (digit >= '1' && digit <= '9') {
        int num = digit - '1';
        *row = num / 3;
        *col = num % 3;
        return 1;
    }
    return 0;
}

char getKeypadDigit()
{
    u16 keyState = KYPD_getKeyStates(&KYPDInst);
    u8 key;
    if (KYPD_getKeyPressed(&KYPDInst, keyState, &key) == KYPD_SINGLE_KEY) {
        return key;
    }
    return ' ';
}

void flashWinText(PmodOLED *oled, int flashes, int onTime, int offTime){
	for (int i = 0; i < flashes; i++) {
		OLED_ClearBuffer(oled);
		OLED_SetCursor(oled, 20,15);
		OLED_PutString(oled, "     Win");
		OLED_Update(oled);
		vTaskDelay(pdMS_TO_TICKS(onTime));

		OLED_ClearBuffer(oled);
		OLED_Update(oled);
		vTaskDelay(pdMS_TO_TICKS(offTime));
	}
}


void expandingRectangle(PmodOLED *oled, int centerX, int centerY, int maxRadius, int stepSize) {
    for (int size = 0; size <= maxRadius; size += stepSize) {
        int halfSize = size / 2;


        int x1 = centerX - halfSize;
        int y1 = centerY - halfSize;
        int x2 = centerX + halfSize;
        int y2 = centerY + halfSize;

        OLED_MoveTo(oled, x1, y1);
        OLED_RectangleTo(oled, x2, y2);

        OLED_Update(oled);

        OLED_ClearBuffer(oled);


        vTaskDelay(pdMS_TO_TICKS(100));


        if (x1 <= 0 && y1 <= 0 && x2 >= OledColMax && y2 >= OledRowMax) {
            break;
        }
    }
}


void celebrateWin(PmodOLED *oled) {
    int width = OledColMax;
    int height =  OledRowMax;
    int centerX = width / 2;
    int centerY = height / 2;
    int maxRadius = (width > height ? width : height);


    expandingRectangle(oled, centerX, centerY, maxRadius, 10);
    vTaskDelay(10);
    flashWinText(oled,10,50,100);
    vTaskDelay(5);
}


