/*
 * File:        TFT print routine
 * Author:      Bruce Land
 * For use with Sean Carroll's Big Board
 * Adapted from:
 *              main.c by
 * Author:      Syed Tahmid Mahbub
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2_1.h"

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
////////////////////////////////////
/* Demo code for interfacing TFT (ILI9340 controller) to PIC32
 * The library has been modified from a similar Adafruit library
 */
// Adafruit data:
/***************************************************
  This is an example sketch for the Adafruit 2.2" SPI display.
  This library works with the Adafruit 2.2" TFT Breakout w/SD card
  ----> http://www.adafruit.com/products/1480

  Check out the links above for our tutorials and wiring diagrams
  These displays use SPI to communicate, 4 or 5 pins are required to
  interface (RST is optional)
  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
// print a line on the TFT
// string buffer
char buffer[60];
void printLine(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 10 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 8, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(1);
    tft_writeString(print_buffer);
}

void printLine2(int line_number, char* print_buffer, short text_color, short back_color){
    // line number 0 to 31 
    /// !!! assumes tft_setRotation(0);
    // print_buffer is the string to print
    int v_pos;
    v_pos = line_number * 20 ;
    // erase the pixels
    tft_fillRoundRect(0, v_pos, 239, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_print ;

// system 1 second interval tick
static int sys_time_seconds ;

// === print Thread =================================================
// Predefined colors definitions (from tft_master.h)
//#define	ILI9340_BLACK   0x0000
//#define	ILI9340_BLUE    0x001F
//#define	ILI9340_RED     0xF800
//#define	ILI9340_GREEN   0x07E0
//#define ILI9340_CYAN    0x07FF
//#define ILI9340_MAGENTA 0xF81F
//#define ILI9340_YELLOW  0xFFE0
//#define ILI9340_WHITE   0xFFFF

static PT_THREAD (protothread_print(struct pt *pt))
{
    PT_BEGIN(pt);
    static int line_count=0;
    static unsigned short text_color, back_color ;
    
    text_color = ILI9340_YELLOW;
    back_color = ILI9340_BLUE;
     tft_setTextColor(text_color);  
     sprintf(buffer, "PRINT TEST   15 ");
     printLine2(0, buffer, text_color, back_color);
     
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1);
        if (line_count++ > 15) {
            PT_YIELD_TIME_msec(1000);
            line_count=1;
            if (text_color == ILI9340_YELLOW) text_color = ILI9340_WHITE;
            else if (text_color == ILI9340_WHITE) text_color = ILI9340_GREEN;
            else if (text_color == ILI9340_GREEN) text_color = ILI9340_RED;
            else if (text_color == ILI9340_RED) text_color = ILI9340_BLUE;
            else text_color = ILI9340_YELLOW;
        }
        if (line_count==1) back_color = ILI9340_BLACK;
        if (line_count==4) back_color = ILI9340_YELLOW;
        if (line_count==7) back_color = ILI9340_BLUE;
        if (line_count==10) back_color = ILI9340_GREEN;
        if (line_count==13) back_color = ILI9340_RED;
        tft_setTextColor(text_color); 
        
        sprintf(buffer, "Line #%d Time=%d", line_count, PT_GET_TIME());
        printLine2(line_count, buffer,  text_color, back_color);

        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // print thread

// === Main  ======================================================
void main(void) {
 //SYSTEMConfigPerformance(PBCLK);
  
  ANSELA = 0; ANSELB = 0; 

  // set up timer on big board
  // timer is used to profile math operations
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 0xffff);

  // === config threads ==========
  // turns OFF UART support and debugger pin, unless defines are set
  PT_setup();

  // === setup system wide interrupts  ========
  INTEnableSystemMultiVectoredInt();

  // init the threads
  PT_INIT(&pt_print);

  // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  // erase
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  
  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_print(&pt_print));
      }
  } // main

// === end  ======================================================

