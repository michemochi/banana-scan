/*********************************************************************
 * Fruit Ripeness Detector - “Banana Scan”
 * Group Members: Christina Chang - cc2294, 
 * Michelle Feng - mf568, 
 * Russell Silva - rms438 
 *********************************************************************
 * Modified from DDS Demo Code
 * Bruce Land Cornell University
 * Sept 2017
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_2_3.h"
// threading library
#include "pt_cornell_1_2_3.h"
// for sine
#include <math.h>

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
////////////////////////////////////

//Headers for String Manipulation 
#include "string.h"
#include "stdlib.h"

//PORT B
#define EnablePullDownB(bits) CNPUBCLR=bits; CNPDBSET=bits;
#define DisablePullDownB(bits) CNPDBCLR=bits;
#define EnablePullUpB(bits) CNPDBCLR=bits; CNPUBSET=bits;
#define DisablePullUpB(bits) CNPUBCLR=bits;

//PORT A
#define EnablePullDownA(bits) CNPUACLR=bits; CNPDASET=bits;
#define DisablePullDownA(bits) CNPDACLR=bits;
#define EnablePullUpA(bits) CNPDACLR=bits; CNPUASET=bits;
#define DisablePullUpA(bits) CNPUACLR=bits;

//String buffer used for writing to the TFT
char buffer[60];
//Array used to store reflectance data from the spectrometer's six channels
char delimited[6][15];

//Single thread used in this program
static struct pt pt_param ;
static char character;
//variable used to determine a reset after scanning complete
static int reset;
//values outputted by the ADC connected to the potentiometer for the
//user interface
static int adc_9;
static int prevadc_9;
static int left;
static int up;
//Keep track of movements right, left, and down during scanning
static int steps_right, steps_left, max_right, max_left, max_down;
static int total_steps_down;
//Variable for state machine
static int state;
static int setting;
//color_reading for mapping reflectance values to colors formatted
//for the TFT color map
static unsigned short color_reading;
static short blue_tingent;
static struct pt pt1, pt2, pt3, pt4, pt_input, pt_output ;
char buffer[60];
char fruit[20];
char banana[20];
char orange[20];
char avocado[20];

//Thresholds used for determining if the spectral sensor is under a blue sheet,
//a pink sheet, or neither during the reset procedure
#define MAX_BLUE_ch1 2300
#define MIN_BLUE_ch1 1700
#define MAX_BLUE_ch2 320
#define MIN_BLUE_ch2 180

#define MAX_PINK_ch1 3350
#define MIN_PINK_ch1 2400
#define MAX_PINK_ch2 420
#define MIN_PINK_ch2 320

static PT_THREAD (protothread_param(struct pt *pt))
{
    PT_BEGIN(pt);
    static int pattern;
    //RB3 and RB4 command bits for sending movement commands to the Raspberry Pi
    //RB3 and RB4: up - 00, down - 01, left - 10, right - 11
    //RB7 Ready Bit
    //RB8 Acknowledgment Bit
    //RB5 connected to external button
    mPORTBSetPinsDigitalOut(BIT_3 | BIT_4 | BIT_7);    //Set port as output
    mPORTASetPinsDigitalIn(BIT_5 | BIT_8);    //Set port as input
    //Pull down for external button
    EnablePullDownB(BIT_5);
   
    //Terminate character and time for UART connection with spectral sensor
    PT_terminate_char = '\n' ;
    PT_terminate_time = 1000;
    
    static int q;
    static int state;
    char *delim;
    
    //Cursors to keep track of color map on the TFT when scanning
    static int cursor_x = 0;
    static int cursor_y = 0;
    static int incr = 12;
    
    tft_fillRoundRect(30, 280, 80, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
    
    ////////////////////////////////////
    //USER INTERFACE
    ////////////////////////////////////
    //Does not end until the user specifies either a banana, apple, or avocado
    //to scan. The user input does not change the nature of the program however,
    //but is included for a future iteration of the system to include different
    //color mapping for different fruits.
    while (strcmp(fruit, banana) && strcmp(fruit, orange) && strcmp(fruit, avocado))
    {      
        tft_setTextColor(ILI9340_RED); tft_setTextSize(1.5);
        //White background highlights current setting
        tft_fillRoundRect(30, (290 + (setting * 10)), 80, 10, 1, ILI9340_WHITE);// x,y,w,h,radius,color
        tft_setCursor(30, 280);
        sprintf(buffer, "Which fruit to detect ripeness?");
        tft_writeString(buffer);
        tft_setCursor(30, 290);
        //Three settings: Banana, Apple, or Avocado
        sprintf(buffer, "Banana");
        tft_writeString(buffer);
        tft_setCursor(30, 300);
        sprintf(buffer, "Apple");
        tft_writeString(buffer);
        tft_setCursor(30, 310);
        sprintf(buffer, "Avocado");
        tft_writeString(buffer);
    
        // read the ADC AN11 
        adc_9 = ReadADC10(0);  
        AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below
        
        //If the user moved the pot sufficiently to the right, move down a setting.
        if (adc_9 > prevadc_9 + 80)
        {
            setting++;
            prevadc_9 = adc_9;
            tft_fillRoundRect(30, 280, 80, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        }
            
        //If the user moved the pot sufficiently to the left, move up a setting.
        else if (adc_9 < prevadc_9 - 80)
        {
            setting--;
            prevadc_9 = adc_9;
            tft_fillRoundRect(30, 280, 80, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        }
            
        if (setting == 3)
            setting = 0;
        if (setting == -1)
            setting = 2;
       
        //If the button is pressed, load the setting to "fruit"
        if (mPORTBReadBits(BIT_5))
        {
            if (setting == 0)
                strcpy(fruit, banana);
            if (setting == 1)
                strcpy(fruit,orange);
            if (setting == 2)
                strcpy(fruit,avocado);
        }
        
        PT_YIELD_TIME_msec(20);
    }

    //Turn on Spectrometer LED
    sprintf(PT_send_buffer,"ATLED1=100\r");
    PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output));
    PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input));
    
    //Turn on Spectrometer LED a second time (this is done because the first
    //UART command to the sensor sometimes fails to send, however we have not
    //encountered a failed command send after the first transmission)
    sprintf(PT_send_buffer,"ATLED1=100\r");
    PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output));
    PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input));
    
    //RB3 - Left/Right or Up/Down 
    //RB4 - Direction
    //RB7 - Ready Bit
    //RB8 - Acknowledgment Bit
	while (1) {
		//We want the sensor to finish moving before transitioning to the
        //next state. PT_YIELD_UNTIL yields the thread until the acknowledgement
        //bit from the Raspberry Pi is set high, acknowledging that the plotter
        //completed its movement.
        if (state != 0)
            PT_YIELD_UNTIL(pt, mPORTBReadBits(BIT_8) == 256);
        //Set all the outputs to low before sending another move command
        mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
        
        //Take Readings from the Spectrometer
        sprintf(PT_send_buffer,"ATCDATA\r");
        //Send command to take readings
        PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output));
        //Wait until the data from all six channels is received
        PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input));

        //Data from each channel is separated by commas
        delim = strtok(PT_term_buffer, ", ");

        //Load the reflectance values from each channel into the corresponding 
        //elements in delimited array. Element 0 represents 610 nm and element
        //1 represents 680 nm
        for (q = 0; q < 6; q++)
        {
            strcpy(delimited[q], delim);
            if (q != 5)
                delim = strtok(NULL, ",");
        }
        
        ////////////////////////////////////
        //COLOR READING CALCULATION
        ////////////////////////////////////
        //Color Reading Calculation for Color Map of Spectrometer Readings on
        //The TFT Display
        
        //From the 4760 webpage - The Adafruit TFT color format is 16 bit:
        //Top 5 bits encoding red intensity, mask is 0xf800
        //Middle 6 bits green, mask is 0x07e0
        //Low 5 bits blue, mask is 0x001f
        
        //From Spectrometer - 680 nm channel has values observed from 0-512
        
        //Color map is a combination of blue and red tingents
        //The following code provides the framework for implementing
        //a different color map for each fruit (although the color map 
        //implementation is the same for each fruit here).
        if (!strcmp(fruit, banana))
        {
            //Convert 0-512 range into 0-31 range
            tft_writeString(buffer);
            color_reading = (unsigned short)atoi(delimited[1]);
            //multiply by 32
            color_reading = color_reading << 5;
            //divide by 512
            color_reading = color_reading >> 9;
            //Blue tingent has a max of 13
            blue_tingent = 31 - color_reading - 18; //blue tingent only values 
            //0-13 to avoid bright blue or bright purple colored blocks
            if (blue_tingent < 0)
                blue_tingent = 0;
            //Left shift 11 bits for red intensity
            color_reading = color_reading << 11;
            //Combine red and blue intensities
            color_reading = color_reading + blue_tingent;
        }
        if (!strcmp(fruit, orange))
        {
            color_reading = (unsigned short)atoi(delimited[1]);
            //multiply by 32
            color_reading = color_reading << 5;
            //divide by 512
            color_reading = color_reading >> 9;
            blue_tingent = 31 - color_reading - 18; //blue tingent only values 0-13
            if (blue_tingent < 0)
                blue_tingent = 0;
            //Left shift 11 bits for red intensity
            color_reading = color_reading << 11;
            color_reading = color_reading + blue_tingent;
        }
        if (!strcmp(fruit, avocado))
        {
            color_reading = (unsigned short)atoi(delimited[1]);
            //multiply by 32
            color_reading = color_reading << 5;
            //divide by 512
            color_reading = color_reading >> 9;
            blue_tingent = 31 - color_reading - 18; //blue tingent only values 0-13
            if (blue_tingent < 0)
                blue_tingent = 0;
            //Left shift 11 bits for red intensity
            color_reading = color_reading << 11;
            color_reading = color_reading + blue_tingent;
        }
        
        tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(1.5);
        //Clear Corresponding Section of the TFT Display
        //Outputs the current state of the state machine, the color reading,
        //and the spectrometer readings to user while scanning
        tft_fillRoundRect(30, 280, 320, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(30, 280);
        sprintf(buffer, "State: %d, Color_reading: %d", state, color_reading);
        tft_writeString(buffer);
        tft_setCursor(30, 290);
        sprintf(buffer, "610nm Value: %s", delimited[0]);
        tft_writeString(buffer);
        tft_setCursor(30, 300);
        sprintf(buffer, "680nm Value: %s", delimited[1]);
        tft_writeString(buffer);
        
        ////////////////////////////////////
        //STATE MACHINE
        ////////////////////////////////////
        //States 0 - 3: Reset Procedure
        //States 4 - 6: Scanning Procedure
        //State 7: End Scan
        
        //Reset Procedure - Inspects whether the sensor is located under the 
        //blue sheet, the pink sheet, or neither for the sensor to locate itself
        //and move to the startup position.
        
        //Scanning Procedure - Moves in a grid-like fashion (all the way right,
        //one step down, all the way left, one step down, etc.) to scan the
        //entire grid
        
        //End Scan - Displays the color map and prompts the user to scan again
        switch (state) {
            //Init State
            case 0:  // init / reset
                
                //Detect if under the blue sheet
                if (atoi(delimited[0]) < MAX_BLUE_ch1 && atoi(delimited[0]) > MIN_BLUE_ch1
                    && atoi(delimited[1]) < MAX_BLUE_ch2 && atoi(delimited[1]) > MIN_BLUE_ch2) {
                    state = 2; //go blue
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_7);
                }
                
                //Detect if under the pink sheet
                else if (atoi(delimited[0]) < MAX_PINK_ch1 && atoi(delimited[0]) > MIN_PINK_ch1
                        && atoi(delimited[1]) < MAX_PINK_ch2 && atoi(delimited[1]) > MIN_PINK_ch2){
                    state = 3;
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3 | BIT_4);
                    mPORTBSetBits(BIT_7);
                }
                
                //Under neither the blue nor pink sheet
                else {
                    state = 1;
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3);
                    mPORTBSetBits(BIT_7);
                }

                break;

            //move left until blue detected
            case 1: // neither
                
                //If blue detected, switch to blue state
                if (atoi(delimited[0]) < MAX_BLUE_ch1 && atoi(delimited[0]) > MIN_BLUE_ch1
                && atoi(delimited[1]) < MAX_BLUE_ch2 && atoi(delimited[1]) > MIN_BLUE_ch2){
                    state = 2; //go blue
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_7);
                }

                //If pink detected, switch to pink state
                else if (atoi(delimited[0]) < MAX_PINK_ch1 && atoi(delimited[0]) > MIN_PINK_ch1
                        && atoi(delimited[1]) < MAX_PINK_ch2 && atoi(delimited[1]) > MIN_PINK_ch2){
                    state = 3; // go pink
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3 | BIT_4);
                    mPORTBSetBits(BIT_7);
                }
                  
                //else keep on moving left
                else{
                    state = 1;
                    
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3);
                    mPORTBSetBits(BIT_7);
                }
                
                break;
                
            //move up until pink detected
            case 2: // blue             
                
                //If pink detected, switch to pink state
                if (atoi(delimited[0]) < MAX_PINK_ch1 && atoi(delimited[0]) > MIN_PINK_ch1
                        && atoi(delimited[1]) < MAX_PINK_ch2 && atoi(delimited[1]) > MIN_PINK_ch2){
                    state = 3;
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3 | BIT_4);
                    mPORTBSetBits(BIT_7);
                }
                
                //else keep on moving up
                else 
                {
                    state = 2;
                    
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_7);
                }

                break;
                
            case 3: // pink ; begin moving right out of pink           
                
                //Keep on moving right until pink is not detected anymore
                if (atoi(delimited[0]) < MAX_PINK_ch1 && atoi(delimited[0]) > MIN_PINK_ch1
                        && atoi(delimited[1]) < MAX_PINK_ch2 && atoi(delimited[1]) > MIN_PINK_ch2){
                    state = 3; // go pink
                    
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3 | BIT_4);
                    mPORTBSetBits(BIT_7);
                }
                
                //If pink is no longer detected, begin scanning (state 4)
                else
                {
                    state = 4;
                    //MOVE
                    mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                    mPORTBSetBits(BIT_3 | BIT_4);
                    mPORTBSetBits(BIT_7);   
                }     
                
                break;
                
            case 4: // continue moving right; start drawing pixels
                
                //Move right 18 steps 
                //MOVE
                mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                mPORTBSetBits(BIT_3 | BIT_4);
                mPORTBSetBits(BIT_7);                 
                
                //Fill a block on the color map for each movement
                tft_fillRect(cursor_x, cursor_y, 12, 12, color_reading);
                //Sensor moved right, so next block is right of the previous
                cursor_x+=incr;
                
                steps_right++; 
                                   
                //If the sensor stepped horizontally 18 times, time to move down
                if ( steps_right == max_right ){
                    state = 6;
                }
                
                else 
                    state = 4;
                
                break;
                
            case 5: // continue moving left; start drawing pixels
                
                //Move left 18 steps 
                //MOVE
                mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                mPORTBSetBits(BIT_3);
                mPORTBSetBits(BIT_7);
                    
                //Fill a block on the color map for each movement
                tft_fillRect(cursor_x, cursor_y, 12, 12, color_reading);
                //Sensor moved left, so next block is left of the previous
                cursor_x-=incr;
                
                steps_left++;
               
                //If the sensor stepped horizontally 18 times, time to move down
                if ( steps_left >= max_left ){
                    state = 6;
                }
                
                else 
                    state = 5;
                
                break;
                
            case 6: // move down
                
                //Move down 20 steps
                //MOVE
                mPORTBClearBits(BIT_3 | BIT_4 | BIT_7);
                mPORTBSetBits(BIT_4);
                mPORTBSetBits(BIT_7);
                        
                total_steps_down++;
                
                //Fill a block on the color map for each movement
                tft_fillRect(cursor_x, cursor_y, 12, 12, color_reading);
                //Sensor moved down, so next block is below the previous
                cursor_y+=incr;          
                
                //If sensor just moved right 18 times, clear steps_right to 0
                //And set the next state to moving left
                if (steps_right >= max_right){
                    steps_right = 0;
                    state = 5;
                }
                
                //If sensor just moved left 18 times, clear steps_left to 0
                //And set the next state to moving right
                else if (steps_left >= max_left){
                    steps_left = 0;
                    state = 4;
                }
                
                //If the sensor stepped vertically 20 times, scanning complete
                if (total_steps_down >= max_down){
                    state = 7;
                }        
                
                break;
            
            //end case
            case 7:
                 //Turn off Spectrometer LED
                sprintf(PT_send_buffer,"ATLED1=0\r");
                PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output));
                PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input));
    
                //Clear the text portion of the screen, but not the color map
                tft_fillRoundRect(30, 280, 320, 42, 1, ILI9340_BLACK);// x,y,w,h,radius,color
                while (!reset)
                {
                    //Brief explanation of the colors on the color map and 
                    //the ripeness levels they represent
                    tft_setCursor(0, 280);
                    sprintf(buffer, "Blue - No Fruit");
                    tft_writeString(buffer);
                    tft_setCursor(0, 290);
                    sprintf(buffer, "Dark Red - Fruit, Less Ripe");
                    tft_writeString(buffer);
                    tft_setCursor(0, 300);
                    sprintf(buffer, "Bright Red - Fruit, More Ripe");
                    tft_writeString(buffer);
                    tft_setCursor(0, 310);
                    //Prompts the user to start a new scan
                    sprintf(buffer, "Push Button to Scan Again");
                    tft_writeString(buffer);
                    PT_YIELD_TIME_msec(20);
                    //If button pressed, reset
                    if (mPORTBReadBits(BIT_5))
                        reset = 1;
                }
                
                ////////////////////////////////////
                //STATE MACHINE
                ////////////////////////////////////
                //Clears the TFT screen, reinitializes all the variables used in 
                //the program to their starting values, and displays the 
                //starting user interface again in preparation for the next
                //scan.

                //reinitialize variables
                cursor_x = 0;
                cursor_y = 0;
                adc_9 = 0;
                prevadc_9 = 0;
                setting = 0;
                sprintf(fruit, "");

                //Display user interface again in preparation for new scan.
                tft_fillScreen(ILI9340_BLACK);// x,y,w,h,radius,color
                while (!strcmp(fruit, banana) && !strcmp(fruit, orange) && !strcmp(fruit, avocado))
                {  
                    tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(1.5);
                    tft_fillRoundRect(30, (290 + (setting * 10)), 320, 10, 1, ILI9340_WHITE);// x,y,w,h,radius,color
                    tft_setCursor(30, 280);
                    sprintf(buffer, "Which fruit to detect ripeness?");
                    tft_writeString(buffer);
                    tft_setCursor(30, 290);
                    sprintf(buffer, "Banana");
                    tft_writeString(buffer);
                    tft_setCursor(30, 300);
                    sprintf(buffer, "Apple");
                    tft_writeString(buffer);
                    tft_setCursor(30, 310);
                    sprintf(buffer, "Avocado");
                    tft_writeString(buffer);

                    // read the ADC AN11 
                    adc_9 = ReadADC10(0);  
                    AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below

                    if (adc_9 > prevadc_9 + 80)
                    {
                        setting++;
                        prevadc_9 = adc_9;
                        tft_fillRoundRect(30, 280, 320, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
                    }

                    else if (adc_9 < prevadc_9 - 80)
                    {
                        setting--;
                        prevadc_9 = adc_9;
                        tft_fillRoundRect(30, 280, 320, 80, 1, ILI9340_BLACK);// x,y,w,h,radius,color
                    }

                    if (setting == 3)
                        setting = 0;
                    if (setting == -1)
                        setting = 2;

                    if (mPORTBReadBits(BIT_5))
                    {
                        if (setting == 0)
                            strcpy(fruit, banana);
                        if (setting == 1)
                            strcpy(fruit, orange);
                        if (setting == 2)
                            strcpy(fruit, avocado);
                    }

                    PT_YIELD_TIME_msec(20);
                }

                //Turn on Spectrometer LED
                sprintf(PT_send_buffer,"ATLED1=100\r");
                PT_SPAWN(pt, &pt_output, PutSerialBuffer(&pt_output));
                PT_SPAWN(pt, &pt_input, PT_GetMachineBuffer(&pt_input));

                //Clear Corresponding Section of the TFT Display
                tft_fillRoundRect(30, 280, 320, 42, 1, ILI9340_BLACK);// x,y,w,h,radius,color
                tft_setCursor(30, 280);
                tft_setTextColor(ILI9340_YELLOW); tft_setTextSize(1.5);
                tft_writeString(PT_term_buffer);
    
                //reinitialize more variables
                state = 0;
                steps_left = 0;
                steps_right = 0;
                total_steps_down = 0;
                reset = 0;
                
        }
	} // END WHILE(1)
	PT_END(pt);
} // thread 4



// === Main  ======================================================
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{
    state = 0;
    total_steps_down = 0;
    setting = 0;
    max_right = 18;
    max_left = 18;
    max_down = 20;  
    sprintf(fruit, "");
    sprintf(banana, "banana");
    sprintf(orange, "orange");
    sprintf(avocado, "avocado");
    adc_9 = 0;
    prevadc_9 = 0;
    reset = 0;
    steps_left = 0;
    steps_right = 0;
    
    ANSELA = 0; ANSELB = 0; 
 
    PT_terminate_char = '\n' ;
    //PT_terminate_count = 1 ;
    PT_terminate_time = 1000;
  
// the ADC ///////////////////////////////////////
	// configure and enable the ADC
	CloseADC10();	// ensure the ADC is off before setting the configuration

					// define setup parameters for OpenADC10
					// Turn module on | ouput in integer | trigger mode auto | enable autosample
					// ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
					// ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
					// ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
    #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_OFF //

                        // define setup parameters for OpenADC10
                        // ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
    #define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
                        //
                        // Define setup parameters for OpenADC10
                        // use peripherial bus clock | set sample time | set ADC clock divider
                        // ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
                        // ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
    #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

                        // define setup parameters for OpenADC10
                        // set AN11 and  as analog inputs
    #define PARAM4	ENABLE_AN11_ANA // pin 24

                        // define setup parameters for OpenADC10
                        // do not assign channels to scan
    #define PARAM5	SKIP_SCAN_ALL

					// use ground as neg ref for A | use AN11 for input A     
					// configure to sample AN11 
	SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11); // configure to sample AN11 
	OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5); // configure ADC using the parameters defined above

	EnableADC10(); // Enable the ADC
				   ///////////////////////////////////////////////////////

    // === setup system wide interrupts  ====================
    INTEnableSystemMultiVectoredInt();
  
  // === now the threads ====================

  // init the threads
  PT_INIT(&pt_param);
  
  AcquireADC10(); // not needed if ADC_AUTO_SAMPLING_ON below
  
   // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  // erase
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Vertical Display for Larger Color Map
  
  // === config the uart, DMA, vref, timer5 ISR =============
  PT_setup();
  sprintf(PT_send_buffer,"ATLED1=100\r");

  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread_param(&pt_param));
  }
} // main