/*
 * File:        Fixed point testing
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

// string buffer
char buffer[60];

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_fixed14, pt_fixed16, pt_float ;

// system 1 second interval tick
static int sys_time_seconds ;
// elapsed time cycle counters
static int t, zero_t, dt ;

// === fixed14 Thread =================================================
// 
typedef signed short fix14 ;
#define multfix14(a,b) ((fix14)((((int)(a))*((int)(b)))>>14)) //multiply two fixed 2.14
#define float2fix14(a) ((fix14)((a)*16384.0)) // 2^14
#define fix2float14(a) ((float)(a)/16384.0)
#define fix2int14(a)    ((int)((a)>>14))
#define int2fix14(a)    ((fix14)((a)<<14))
#define absfix14(a) abs(a) 

static fix14 in14_1, in14_2, out14 ;

static PT_THREAD (protothread_fixed14(struct pt *pt))
{
    PT_BEGIN(pt);
     tft_setCursor(0, 0);
     tft_setTextColor(ILI9340_WHITE);  tft_setTextSize(1);
     tft_writeString("Operation timing\n");

      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        sys_time_seconds++ ;
        
        // 14 bit fix ///////////////////
        // all numbers less than 2!
        in14_1 = float2fix14((float)sys_time_seconds/10.) ; //(float)sys_time_seconds/100
        in14_2 = float2fix14(0.1) ;
        // read the time
        t = TMR2;
        // do the test
        out14 =  multfix14(in14_1,in14_2) ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,10, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 10);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "Multfix14 dt value  %d  %5.3f", dt, fix2float14(out14));
        tft_writeString(buffer);
        
        // MAC
        in14_1 = float2fix14((float)sys_time_seconds/20.) ; //(float)sys_time_seconds/100
        in14_2 = float2fix14(0.2) ;
        out14 = float2fix14(-0.5);
        t = TMR2;
        // do the test
        out14 =  multfix14(in14_1,in14_2) + out14 ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,20, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 20);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "Mult+add dt value  %d  %5.3f", dt, fix2float14(out14));
        tft_writeString(buffer);
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // timer thread

// === fixed16 Thread =================================================
// 
typedef signed int fix16 ;
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b)))) 
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a)))) 
#define absfix16(a) abs(a)

static fix16 in16_1, in16_2, out16 ;

static PT_THREAD (protothread_fixed16(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
         // yield time 1 second
        PT_YIELD_TIME_msec(1000) ;
        
        // 16 bit fix ///////////////////
        // all numbers less than 2!
        in16_1 = float2fix16((float)sys_time_seconds) ; //(float)sys_time_seconds/100
        in16_2 = float2fix16(0.1) ;
        // read the time
        t = TMR2;
        // do the test
        out16 =  multfix16(in16_1,in16_2) ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,40, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 40);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "Multfix16 dt value  %d  %5.3f", dt, fix2float16(out16));
        tft_writeString(buffer);
        
        // read the time
        out16 = float2fix16(-0.5);
        t = TMR2;
        // do the test
        out16 =  multfix16(in16_1,in16_2) + out16 ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,50, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 50);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "Mult+add dt value  %d  %5.3f", dt, fix2float16(out16));
        tft_writeString(buffer);

        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // color thread

// === float Thread =============================================
//  
static float infloat_1, infloat_2, outfloat ;

static PT_THREAD (protothread_float(struct pt *pt))
{
    PT_BEGIN(pt);
      while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(1000);
        
        // float ///////////////////
        // all numbers less than 2!
        infloat_1 = (float)sys_time_seconds ; 
        infloat_2 = 0.1 ;
        // read the time
        t = TMR2;
        // do the test
        outfloat =  infloat_1 * infloat_2 ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,70, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 70);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "float   dt value  %d  %5.3f", dt, outfloat);
        tft_writeString(buffer);
        
        // MAC
        outfloat = -0.5;
        t = TMR2;
        // do the test
        outfloat =  infloat_1 * infloat_2 + outfloat ;
        // compute elapsed time - 5 cycles of overhead
        dt = TMR2 - t - zero_t;
        // print it to TFT LCD
        tft_fillRoundRect(0,80, 230, 14, 1, ILI9340_BLACK);// x,y,w,h,radius,color
        tft_setCursor(0, 80);
        tft_setTextColor(ILI9340_WHITE); tft_setTextSize(1);
        sprintf(buffer, "Mult+add dt value  %d  %5.3f", dt, outfloat);
        tft_writeString(buffer);

        
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // animation thread

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
  PT_INIT(&pt_fixed14);
  PT_INIT(&pt_fixed16);
  PT_INIT(&pt_float);

  // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(0); // Use tft_setRotation(1) for 320x240

  // get zero offset reading for timer
   t = TMR2;
  // timer reading overhead 
  zero_t = TMR2 - t;
  
  // round-robin scheduler for threads
  while (1){
      PT_SCHEDULE(protothread_fixed16(&pt_fixed16));
      PT_SCHEDULE(protothread_fixed14(&pt_fixed14));
      PT_SCHEDULE(protothread_float(&pt_float));
      }
  } // main

// === end  ======================================================

