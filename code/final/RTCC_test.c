/*********************************************************************
 *
 *  RTCC test
 *
 *********************************************************************
 * Bruce Land Cornell University
 * November 2015
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config.h"
// threading library
#include "pt_cornell_1_2.h"

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_cmd, pt_tick;
// uart control threads
static struct pt pt_input, pt_output, pt_DMA_output ;

// system 1 second interval tick
int sys_time_seconds ;
// RTCC time, date
rtccTime	tm ;			// time structure BCD!!!
rtccDate	dt ;			// date structure

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

// === the RTCC uses BCD ==================================================
#define BCD2int(bcd) (bcd&0xf)+((bcd>>4)*10)

// === Serial Thread ======================================================
static PT_THREAD (protothread_cmd(struct pt *pt))
{
    // The serial interface
    static char cmd[16]; 
    static float value;
   
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
     
                 case 'x': // clear the RTC
                     RtccSetTimeDate(0x0, 0x0);
                     break;
                case 't': // attack rate constant  1=FM 2=main
                    tm.l=RtccGetTime();
                     sprintf(PT_send_buffer, "RTCC hr:min:sec=%d %d %d\n\r",
                             BCD2int(tm.hour), BCD2int(tm.min), BCD2int(tm.sec));
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
                 case 'd':
                     dt.l=RtccGetDate();
                     sprintf(PT_send_buffer, "RTCC year:month:day=%d %d %d\n\r",
                             BCD2int(dt.year), BCD2int(dt.mon), BCD2int(dt.mday));
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
    
  // === now the clock ====================
  // init the clock  -- IN BCD format!!! 
  tm.hour = 0x04;
  tm.min = 0;
  tm.sec = 0;
  dt.mday = 0x20;
  dt.mon = 0x11;
  dt.year = 0x15;
  RtccInit();
  RtccSetTimeDate(tm.l, dt.l) ;
  RtccSelectPulseOutput(1);		// select the seconds clock pulse as the function of the RTCC output pin
  RtccOutputEnable(1);			// enable the Output pin of the RTCC (PIN 7 on 28 pin PDIP)
   
  // === init the threads ===================
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_tick);
  
  
  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_tick(&pt_tick));
  }
} // main
