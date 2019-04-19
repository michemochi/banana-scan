/*********************************************************************
 *  DDS demo code
 *  sine synth to SPI to  MCP4822 dual channel 12-bit DAC
 *********************************************************************
 * Bruce Land Cornell University
 * Sept 2017
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
/* ====== MCP4822 control word ==============
bit 15 A/B: DACA or DACB Selection bit
1 = Write to DACB
0 = Write to DACA
bit 14 : Don't Care
bit 13 GA: Output Gain Selection bit
1 = 1x (VOUT = VREF * D/4096)
0 = 2x (VOUT = 2 * VREF * D/4096), where internal VREF = 2.048V.
bit 12 SHDN: Output Shutdown Control bit
1 = Active mode operation. VOUT is available. 
0 = Shutdown the selected DAC channel. Analog output is not available at the channel that was shut down.
bit 11-0 D11:D0: DAC Input Data bits. 
*/
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
#define DAC_config_chan_B 0b1011000000000000
#define Fs 200000.0
#define two32 4294967296.0 // 2^32 
#define Finit 220.0

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2_1.h"
// for sine
#include <math.h>

volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
// for 60 MHz PB clock use divide-by-3
volatile int spiClkDiv = 2 ; // 20 MHz DAC clock

////////////////////////////////////
// graphics libraries
// SPI channel 1 connections to TFT
#include "tft_master.h"
#include "tft_gfx.h"
////////////////////////////////////

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_param ;

// DDS sine table
#define sine_table_size 256
volatile int sin_table[sine_table_size];

//== Timer 2 interrupt handler ===========================================
// actual scaled DAC 
volatile  int DAC_data;
// the DDS units:
volatile unsigned int phase_accum_main, phase_incr_main=Finit*two32/Fs ;//
// profiling of ISR
volatile int isr_enter_time, isr_exit_time;
// demo float to test isr timing
volatile float test_isr_float = 0.5 ;
volatile int test_isr_int = 2 ;
//=============================
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    // 74 cycles to get to this point from timer event
    isr_enter_time = ReadTimer2() ; // - isr_time;
    
    mT2ClearIntFlag();
    
    // the led
    mPORTAToggleBits(BIT_0);
    
    // main DDS phase
    phase_accum_main += phase_incr_main  ;
    //sine_index = phase_accum_main>>24 ;
    DAC_data = sin_table[phase_accum_main>>24]  ;
    
    // The following section is purely for testing mult speed!
    // Optimization set at level 1
    // comment out one or both to see effect on ISR time!
    // With neither: ISR total time is 123 cycles (enter, compute, exit)
    // With float mult, ISR total time is 269  cycles 
    // -- entry time doubles because more reg need to be saved
    // With int mult and shift, ISR total time is 128  cycles 
    // now the test multiply
    // comment out to see float mult time
    //DAC_data = (int)(DAC_data*test_isr_float);
    //DAC_data = (int)(DAC_data*test_isr_float);
    // comment out to see int shift/multiply time
    //DAC_data = (DAC_data*test_isr_int)>>1;
    // end test section
    
    // === Channel A =============
    // CS low to start transaction
     mPORTBClearBits(BIT_4); // start transaction
    // test for ready
     //while (TxBufFullSPI2());
    // write to spi2 
    WriteSPI2( DAC_config_chan_A | (DAC_data + 2048));
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
    mPORTBSetBits(BIT_4); // end transaction
    isr_exit_time = ReadTimer2() ; // - isr_time;
     
} // end ISR TIMER2

//****************************************************/
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
    tft_fillRoundRect(0, v_pos, 319, 16, 1, back_color);// x,y,w,h,radius,color
    tft_setTextColor(text_color); 
    tft_setCursor(0, v_pos);
    tft_setTextSize(2);
    tft_writeString(print_buffer);
}
// === set parameters ======================================================
// update a 1 second tick counter
static float Fout = Finit;
static PT_THREAD (protothread_param(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            // 
            PT_YIELD_TIME_msec(500) ;

            // step the frequency between 400 and 4000 Hz
            Fout = Fout * 1.05946;
            if (Fout > 2000) Fout = Finit; // Hz
            phase_incr_main = (int)(Fout*(float)two32/Fs);
            // NEVER exit while
            
            sprintf(buffer,"Tin=%d Tout=%d ISR=%d", isr_enter_time, isr_exit_time, isr_enter_time+isr_exit_time);
            printLine2(2, buffer, ILI9340_WHITE, ILI9340_BLACK);
      } // END WHILE(1)
  PT_END(pt);
} // thread 4

// === Main  ======================================================
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{
    
  /// timer interrupt //////////////////////////
    // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
    // 400 is 100 ksamples/sec at 30 MHz clock
    // 200 is 200 ksamples/sec
    
    OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 400);
    // set up the timer interrupt with a priority of 2
    ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
    mT2ClearIntFlag(); // and clear the interrupt flag

    /// SPI setup //////////////////////////////////////////
    // SCK2 is pin 26 
    // SDO2 is in PPS output group 2, could be connected to RB5 which is pin 14
    PPSOutput(2, RPB5, SDO2);
    // control CS for DAC
    mPORTBSetPinsDigitalOut(BIT_4);
    mPORTBSetBits(BIT_4);
    // divide Fpb by 2, configure the I/O ports. Not using SS in this example
    // 16 bit transfer CKP=1 CKE=1
    // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
    // For any given peripherial, you will need to match these
    SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
    
    // the LED
    mPORTASetPinsDigitalOut(BIT_0);
    mPORTASetBits(BIT_0);
    
    // === config the uart, DMA, vref, timer5 ISR =============
    PT_setup();

    // === setup system wide interrupts  ====================
    INTEnableSystemMultiVectoredInt();
  
  // === now the threads ====================

  // init the threads
  PT_INIT(&pt_param);

  // build the sine lookup table
   // scaled to produce values between 0 and 4096
   int i;
   for (i = 0; i < sine_table_size; i++){
         sin_table[i] = (int)(2047*sin((float)i*6.283/(float)sine_table_size));
    }
  
   // init the display
  // NOTE that this init assumes SPI channel 1 connections
  tft_init_hw();
  tft_begin();
  // erase
  tft_fillScreen(ILI9340_BLACK);
  //240x320 vertical display
  tft_setRotation(1); // Use tft_setRotation(1) for 320x240
  
  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread_param(&pt_param));
  }
} // main
