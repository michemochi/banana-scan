/**
 * This is a very small example that shows how to use
 * protothreads.
 * Thread 1 and 2 exchange semaphore locks to alternate execution and blink LEDS
 * Thread 3 is a command line control
 * Thread 4 toggles an i/o pin when spawned by thread 3
 * Thread 5 maintaions a 1 second tick
 * Thread 6  toggles an i/o pin when signaled by thread 3
 * -- Pin 2,3,4 are connected to LEDs and 300 ohm resitors to ground
 * -- Pin 25 is the voltage debug output
 * -- Uart connections explained elsewhere
 * Modified by Bruce Land 
 * Sept 23, 2015
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config_1_2_2.h"
// threading library
#include "pt_cornell_1_2_2.h"

// === thread structures ============================================
// semaphores for controlling two threads
// for guarding the UART and for allowing stread blink control
static struct pt_sem control_t1, control_t2, control_t6  ;
// thread control structs
// note that UART input and output are threads
static struct pt pt1, pt2, pt3, pt4, pt5, pt6, pt_input, pt_output, pt_DMA_output ;
// turn threads 1 and 2 on/off and set thread timing
static int wait_t1 = 200 ;// mSec
// delay time for thread 4 and thread 6
static int wait_t2 = 1000 ;// mSec
// system 1 second interval tick
int sys_time_seconds ;

// === Thread 1 ======================================================
/**
 * The first protothread function. A protothread function must always
 * return an integer, but must NEVER explicitly return - returning is
 * performed inside the protothread statements.
 *
 * The protothread function is driven by the main loop further down in
 * the code.
 */
static PT_THREAD (protothread1(struct pt *pt))
{
    // mark beginning of thread
    PT_BEGIN(pt);

    /* We loop forever here. */
    while(1) {

        //stop until thread 2 signals
        PT_SEM_WAIT(pt, &control_t1);
        // put a 2 microsec pulse on the debug pin with amplitude 3
        PT_DEBUG_VALUE(3, 2) ;
        
        // toggle a port pin
        mPORTAToggleBits(BIT_0);

         // tell thread 2 to go
        PT_SEM_SIGNAL(pt, &control_t2);

        // This is a locally written macro using  timer5 to count millisec
        // to program a yield time
        PT_YIELD_TIME_msec(wait_t1) ;
        
        // NEVER exit while
    } // END WHILE(1)

  // mark end the thread
  PT_END(pt);
} // thread 1

// === Thread 2 ======================================================
//
static PT_THREAD (protothread2(struct pt *pt))
{
    PT_BEGIN(pt);

      while(1) {
            
            //stop until thread 1 signals
            PT_SEM_WAIT(pt, &control_t2);

            // put a 5 microsec pulse on the debug pin with amplitude 6
            PT_DEBUG_VALUE(6, 5) ;

            // toggle a port
            mPORTAToggleBits(BIT_1);

            // tell thread 1 to go
            PT_SEM_SIGNAL(pt, &control_t1);
            
            // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 2

// === Thread 4 ======================================================
//  blinks on command 
static PT_THREAD (protothread4(struct pt *pt))
{
    PT_BEGIN(pt);
    // put a 10 microsec pulse on the debug pin with amplitude 1
    PT_DEBUG_VALUE(1, 10) ;
    // make a single pulse of length wait_t2
    mPORTBSetBits(BIT_0);
    PT_YIELD_TIME_msec(wait_t2) ;
    mPORTBClearBits(BIT_0);
    PT_EXIT(pt);      
    PT_END(pt);
} // thread 4

// === Thread 6 ======================================================
//  blinks on command 
static PT_THREAD (protothread6(struct pt *pt))
{
    PT_BEGIN(pt);
    while(1){
        PT_SEM_WAIT(pt, &control_t6);
        // make a single pulse of length wait_t2
        mPORTBSetBits(BIT_0);
        PT_YIELD_TIME_msec(wait_t2) ;
        mPORTBClearBits(BIT_0);
    }
    PT_END(pt);
} // thread 6

// === Thread 3 ======================================================
// The serial interface
static char cmd[16]; 
static int value;

static PT_THREAD (protothread3(struct pt *pt))
{
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
             sscanf(PT_term_buffer, "%s %d", cmd, &value);

             switch(cmd[0]){
                 case 't': // command form: t 1000
                     // number entered becomes the thread 4 and 6 blink time
                     wait_t2 = value ; 
                     break;
                 case 'b': // command form: b
                     // blocks this thread while thread 4 makes pulse,
                     // but does not block other threads
                     PT_SPAWN(pt, &pt4, protothread4(&pt4)); 
                     break;
                 case 's': // command form: s
                     // does NOT block this thread while thread 6 makes pulse
                     PT_SEM_SIGNAL(pt, &control_t6);
                     break;
                 case 'w': // command form: w
                     // blocks THIS thread for 2 seconds
                     PT_YIELD_TIME_msec(2000);
                     break;
                 case 'p':
                     printf("%d\t%d\n\r", PT_GET_TIME(), sys_time_seconds);
                     break;
             }
             
            // never exit while
      } // END WHILE(1)
  PT_END(pt);
} // thread 3

// === Thread 5 ======================================================
// update a 1 second tick counter
static PT_THREAD (protothread5(struct pt *pt))
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
    
  // === set up i/o port pin ===============
  mPORTASetBits(BIT_0 );	//Clear bits to ensure light is off.
  mPORTASetPinsDigitalOut(BIT_0  );    //Set port as output
 // mPORTBSetBits(BIT_0 );	//Clear bits to ensure light is off.
  //mPORTBSetPinsDigitalOut(BIT_0 );    //Set port as output

  // === now the threads ====================
  // init  the thread control semaphores
  PT_SEM_INIT(&control_t1, 0); // start blocked
  PT_SEM_INIT(&control_t2, 1); // start unblocked
  PT_SEM_INIT(&control_t6, 0); // start blocked

  // init the threads
  PT_INIT(&pt1);
  PT_INIT(&pt2);
  PT_INIT(&pt3);
  PT_INIT(&pt4);
  PT_INIT(&pt5);
  PT_INIT(&pt6);

  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread1(&pt1));
    PT_SCHEDULE(protothread2(&pt2));
    PT_SCHEDULE(protothread3(&pt3)); 
    PT_SCHEDULE(protothread5(&pt5)); 
    PT_SCHEDULE(protothread6(&pt6)); 
  }
} // main
