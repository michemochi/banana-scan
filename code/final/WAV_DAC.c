/*********************************************************************
 *
 *  Multichannel synth to SPI to  MCP4822 dual channel 12-bit DAC
 *
 *********************************************************************
 * Bruce Land Cornell University
 * October 2015
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
/* ====== MCP4822 control word ==============
bit 15 A/B: DACA or DACB Selection bit
1 = Write to DACB
0 = Write to DACA
bit 14 ? Don?t Care
bit 13 GA: Output Gain Selection bit
1 = 1x (VOUT = VREF * D/4096)
0 = 2x (VOUT = 2 * VREF * D/4096), where internal VREF = 2.048V.
bit 12 SHDN: Output Shutdown Control bit
1 = Active mode operation. VOUT is available. ?
0 = Shutdown the selected DAC channel. Analog output is not available at the channel that was shut down.
VOUT pin is connected to 500 k???typical)?
bit 11-0 D11:D0: DAC Input Data bits. Bit x is ignored.
*/
// A-channel, 1x, active
#define DAC_config_chan_A 0b0011000000000000
#define DAC_config_chan_B 0b1011000000000000
#define Fs 8000.0
#define two32 4294967296.0 // 2^32 

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2_1.h"
// for sine
#include <math.h>
// the speech amplitudes
#include "Digits_no_compress.h"

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_cmd, pt_tick;
// uart control threads
static struct pt pt_input, pt_output, pt_DMA_output ;

// system 1 second interval tick
int sys_time_seconds ;

// === 16:16 fixed point macros ==========================================
typedef signed int fix16 ;
#define multfix16(a,b) ((fix16)(((( signed long long)(a))*(( signed long long)(b)))>>16)) //multiply two fixed 16:16
#define float2fix16(a) ((fix16)((a)*65536.0)) // 2^16
#define fix2float16(a) ((float)(a)/65536.0)
#define fix2int16(a)    ((int)((a)>>16))
#define int2fix16(a)    ((fix16)((a)<<16))
#define divfix16(a,b) ((fix16)((((signed long long)(a)<<16)/(b)))) 
#define sqrtfix16(a) (float2fix16(sqrt(fix2float16(a)))) 
#define absfix16(a) abs(a)
#define onefix16 0x00010000 // int2fix16(1)
#define sustain_constant float2fix16(256.0/20000.0) ; // seconds per decay update

//== Timer 2 interrupt handler ===========================================
// actual scaled DAC 
volatile unsigned int DAC_data;
// SPI
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this DAC

// the DDS units:
// #define NUMSINE 25 defined in header
volatile unsigned int phase_accum[NUMSINE], phase_incr[NUMSINE];
//volatile unsigned int F[NUMSINE]={100, 113, 128, 145, 165, 187, 211, 239, 271, 
//                                  307, 348, 394, 447, 506, 573, 649, 736, 833,
 //                                 944, 1069, 1211, 1372, 1555, 1761, 1995}; // defined in header
// DDS sine table
#define sine_table_size 256
volatile fix16 sine_table[sine_table_size];
volatile fix16 wave ;
volatile fix16 amplitude[NUMSINE];
volatile int sample_count, array_count, end_count ;

// profiling of ISR
volatile int isr_time;
//=============================
void __ISR(_TIMER_2_VECTOR, ipl2) Timer2Handler(void)
{
    mPORTBSetBits(BIT_1);
    mT2ClearIntFlag();
    int i;
    
    // read the coefficients 
    // and convert 8-bits to fix16 fraction
    if ((sample_count++ & table_interval) == 0){
        for(i=0; i<NUMSINE; i++){
            if (array_count<end_count){
                amplitude[i] = (AllDigits[array_count++])<<10 ;}
            else amplitude[i] = 0;
        }
    }
    // array of sine waves
    wave = 0;
    for(i=0; i<NUMSINE; i++){
        phase_accum[i] += phase_incr[i] ; 
        wave = wave + multfix16(amplitude[i], sine_table[phase_accum[i]>>24]);
    }
     
    // === Channel A =============
    // CS low to start transaction
     mPORTBClearBits(BIT_0); // start transaction
    // test for ready
     //while (TxBufFullSPI2());
     // write to spi2
     WriteSPI2(DAC_data); 
    
    // truncate to 12 bits, read table, convert to int and add offset
    DAC_data = DAC_config_chan_A | ((fix2int16(wave)) + 2048) ; 
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
     mPORTBSetBits(BIT_0); // end transaction
     mPORTBClearBits(BIT_1);
     // === Channel B =============
     /*
     // CS low to start transaction
     mPORTBClearBits(BIT_0); // start transaction
    // test for ready
     //while (TxBufFullSPI2());
     // write to spi2
     WriteSPI2(DAC_data);
     
     wave_2 = multfix16(sine_table[phase_accum_2>>24], env_2);
    // truncate to 12 bits, phase-shift 90 degrees, read table, convert to int and add offset
    DAC_data = DAC_config_chan_B | (fix2int16(wave_2) + 2048) ; 
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
     // CS high
     mPORTBSetBits(BIT_0); // end transaction
     */
     isr_time = max(ReadTimer2(), isr_time) ; // - isr_time;
     //isr_time = ReadTimer2();
} // end ISR TIMER2

// === the RTCC uses BCD ==================================================
#define BCD2int(bcd) (bcd&0xf)+((bcd>>4)*10)

// === Serial Thread ======================================================
static PT_THREAD (protothread_cmd(struct pt *pt))
{
    // The serial interface
    static char cmd[16]; 
    static float value;
    static float f_fm, f_main;
    static rtccTime	tm ;			// time structure BCD!!!
	static rtccDate	dt ;			// date structure
    // digit sample start samp  1     2      3     4     5     6     7     8    9     0
    static int digit_bound[] = {900, 1860, 2926, 4056, 5139, 6236, 7479, 8623, 9753, 10824};
    PT_BEGIN(pt);
    
    // clear port used by thread 4
    mPORTBClearBits(BIT_0);
    
      while(1) {
          
            // send the prompt via DMA to serial
            sprintf(PT_send_buffer,"cmd>");
            // by spawning a print thread
            PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
 
          //spawn a thread to handle terminal input
            // the input thread waits for input
            // -- BUT does NOT block other threads
            // string is returned in "PT_term_buffer"
            PT_SPAWN(pt, &pt_input, PT_GetSerialBuffer(&pt_input) );
            // returns when the thead dies
            // in this case, when <enter> is pushed
            // now parse the string
             sscanf(PT_term_buffer, "%s %f", cmd, &value);

             switch(cmd[0]){
                 case 'f': // frequency 1 is FM; freq 2 is fundamental
                     // freq*2^32/Fs
                    
                     break;
                 case 'p': // play
                     sample_count = 0;
                     array_count = 0;
                     end_count = table_count ; // play all the digits
                     break;
                 case 'd': // digit
                     array_count = digit_bound[(int)value-1];   
                     end_count = digit_bound[(int)value];
                     break;
                case 's': // sustain
                     sample_count = 0;
                     
                     break;
                 case 'x': // clear the RTC
                     RtccSetTimeDate(0x0, 0x0);
                     break;
                case 'r': // attack rate constant  1=FM 2=main
                    tm.l=RtccGetTime();
                     sprintf(PT_send_buffer, "RTCC hr:min:sec=%d %d %d\n\r",
                             BCD2int(tm.hour), BCD2int(tm.min), BCD2int(tm.sec));
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
                 case 't':
                     sprintf(PT_send_buffer, "isr time=%d cycles \n\r",  isr_time);
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
             }
             
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === Thread 5 ======================================================
// update a 1 second tick counter
static PT_THREAD (protothread_tick(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            // yield time 1 second
            PT_YIELD_TIME_msec(1000) ;
            sys_time_seconds++ ;
            // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 4

// === Main  ======================================================
// set up UART, timer2, threads
// then schedule them as fast as possible

int main(void)
{
    
    // === config the uart, DMA, vref, timer5 ISR =============
  PT_setup();

   // === setup system wide interrupts  ====================
  INTEnableSystemMultiVectoredInt();
    
  /// timer interrupt //////////////////////////
        // Set up timer2 on,  interrupts, internal clock, prescalar 1, toggle rate
        // at 30 MHz PB clock 60 counts is two microsec
        // 400 is 100 ksamples/sec
        // 2000 is 20 ksamp/sec
        // 1000 is 40 ksample/sec
        // 2000 is 20 ks/sec
        // 5000 is 8 ksamples/sec
        OpenTimer2(T2_ON | T2_SOURCE_INT | T2_PS_1_1, 5000);
        // set up the timer interrupt with a priority of 2
        ConfigIntTimer2(T2_INT_ON | T2_INT_PRIOR_2);
        mT2ClearIntFlag(); // and clear the interrupt flag

        // SCK2 is pin 26 
        // SDO2 (MOSI) is in PPS output group 2, could be connected to RB5 which is pin 14
        PPSOutput(2, RPB5, SDO2);

        // control CS for DAC
        mPORTBSetPinsDigitalOut(BIT_0);
        mPORTBSetBits(BIT_0);
        
        // profile  pin for ISR
        mPORTBSetPinsDigitalOut(BIT_1);
        
        // divide Fpb by 2, configure the I/O ports. Not using SS in this example
        // 16 bit transfer CKP=1 CKE=1
        // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
        // For any given peripherial, you will need to match these
        SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE16 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  // === now the threads ====================
  // init the clock      
   RtccInit();	
   RtccSetTimeDate(0x0, 0x0);	
   //RTCCONbits.RTCOE = 1; // turn on output pin

   RtccSelectPulseOutput(1);		// select the seconds clock pulse as the function of the RTCC output pin
   //RtccSelectPulseOutput(0);		// select the alarm pulse as the function of the RTCC output pin
   RtccOutputEnable(1);			// enable the Output pin of the RTCC
   
  // init the threads
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_tick);
  
  // build the sine lookup table
   // scaled to produce values between 0 and 4096
   int i;
   for (i = 0; i < sine_table_size; i++){
         sine_table[i] =  float2fix16(200*sin((float)i*6.283/(float)sine_table_size));
    }
   
   // init the sine wave generator frequencies
    #define incr_const two32/Fs
   for(i=0; i<NUMSINE; i++){
        phase_accum[i] = 0 ; 
        phase_incr[i] = (int) (F[i]*incr_const) ;
    }
   
   // turn off the sound
   array_count = table_count ;
   
  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_tick(&pt_tick));
  }
} // main
