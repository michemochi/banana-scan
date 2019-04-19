/*********************************************************************
 *
 *                  SPI to  23LC1024 serial RAM & DAC
 * Uses interactive commands from the uart to test SPI serial RAM
 * Same SPI channel is used to drive DAC
 *
 *********************************************************************
 * Bruce Land -- Cornell University
 * Dec 2015
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

//=== clock AND protoThreads configure =============================
#include "config.h"
// threading library
#include "pt_cornell_1_2.h"

// === thread structures ============================================
// semaphores for controlling two threads
// for guarding the UART and for allowing stread blink control
static struct pt_sem control_t1, control_t2, control_t6  ;
// thread control structs
// note that UART input and output are threads
static struct pt pt_cmd, pt_tick;
// uart control threads
static struct pt pt_input, pt_output, pt_DMA_output ;
// system 1 second interval tick
int sys_time_seconds ;

// === RAM test data ====================================================
// some data to write
char test_data[]={2, 4, 6, 8, 10, 100, 200, 255}; // 8
char test_data2[8];

// === SPI setup ========================================================
volatile SpiChannel spiChn = SPI_CHANNEL2 ;	// the SPI channel to use
volatile int spiClkDiv = 2 ; // 20 MHz max speed for this RAM
//volatile int spiClkDiv = 4 ;

// === RAM commands ======================================================
// from 23LC1024 datasheet
// http://ww1.microchip.com/downloads/en/DeviceDoc/20005142C.pdf
// Default setup includes data streaming from current address
// This is used in the array read/write functions
#define RAM_WRITE_CMD (0x2000000) // top 8 bits -- 24 bits for address
#define RAM_READ_CMD  (0x3000000) // top 8 bits -- 24 bits for address
// command format consists of a read or write command ORed with a 24-bit
// address, even though the actual address is never more than 17 bits
// general procedure will be:
// Drop chip-select line
// Do a 32-bit SPI transfer with command ORed with address
// switch to 8-bit SPI mode
// Send one or more data bytes
// Raise chip-select line
//
// At 20 MHz SPI bus rate:
// Command ORed with address, plus mode change, takes 2.2 microsec
// Each byte takes 0.75 microseconds 

/* ====== MCP4822 control word =========================================
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

// === spi bit widths ====================================================
// hit the SPI control register directly
inline void Mode16(void){  // configure SPI1 for 16-bit mode
    SPI2CONSET = 0x400;
    SPI2CONCLR = 0x800;
}
// ========
inline void Mode8(void){  // configure SPI1 for 8-bit mode
    SPI2CONCLR = 0x400;
    SPI2CONCLR = 0x800;
}
// ========
inline void Mode32(void){  // configure SPI1 for 8-bit mode
    SPI2CONCLR = 0x400;
    SPI2CONSET = 0x800;
}
// === DAC byte write =====================================================
// address between 0 and 2^17-1
// data bytes
void dac_write_byte(int data){
    int junk;
    // Channel config ORed with data
    Mode16();
    mPORTBClearBits(BIT_1);
    // write to spi2 and convert 8 bits to 12 bits
    WriteSPI2(DAC_config_chan_A | (data));
    // test for done
    while (SPI2STATbits.SPIBUSY); // wait for end of transaction
    junk = ReadSPI2(); // must always read, even if nothing useful is returned
    // set 8-bit transfer for each byte
    mPORTBSetBits(BIT_1);
    return ;
}

// === RAM byte write =====================================================
// address between 0 and 2^17-1
// data bytes
void ram_write_byte(int addr, char data){
    int junk;
    // set 32-bit transfer for read/write command ORed with
    // actual address
    Mode32();
    mPORTBClearBits(BIT_0);
    WriteSPI2(RAM_WRITE_CMD | addr); // addr not greater than 17 bits
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    junk = ReadSPI2(); // must always read, even if nothing useful is returned
    // set 8-bit transfer for each byte
    Mode8();
    WriteSPI2(data); // data write
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    junk = ReadSPI2();
    mPORTBSetBits(BIT_0);
    return ;
}

// === RAM array write =====================================================
// address, pointer to an array containing the data, number of BYTES to store
void ram_write_byte_array(int addr, char* data, int count){
    int junk, i;
    Mode32();
    mPORTBClearBits(BIT_0);
    WriteSPI2(RAM_WRITE_CMD | addr); // addr not greater than 17 bits
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    junk = ReadSPI2();
    Mode8();
    for(i=0; i<count; i++){
        WriteSPI2(data[i]); // data write
        while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
        junk = ReadSPI2();
    }
    mPORTBSetBits(BIT_0);
    return ;
}

// === RAM byte read ======================================================
int ram_read_byte(int addr){
    int junk, data;
    Mode32();
    mPORTBClearBits(BIT_0);
    WriteSPI2(RAM_READ_CMD | addr); // addr not greater than 17 bits
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    junk = ReadSPI2();
    Mode8();
    WriteSPI2(junk); // force the read
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    data = ReadSPI2();
    mPORTBSetBits(BIT_0);
    return data;
}

// === RAM array read ======================================================
// address, pointer to an array receiving the data, number of BYTES to read
int ram_read_byte_array(int addr, char* data, int count){
    int junk, i;
    Mode32();
    mPORTBClearBits(BIT_0);
    WriteSPI2(RAM_READ_CMD | addr); // addr not greater than 17 bits
    while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
    junk = ReadSPI2();
    Mode8();
    for(i=0; i<count; i++){
        WriteSPI2(junk); // force the read
        while (SPI2STATbits.SPIBUSY); // wait for it to end of transaction
        data[i] = ReadSPI2();
    }
    mPORTBSetBits(BIT_0);
    return ;
}

// === Serial Thread ======================================================
static PT_THREAD (protothread_cmd(struct pt *pt))
{
    PT_BEGIN(pt);
    // The serial interface
    static char cmd[16]; 
    static int i, value1, value2, value3;
    int ram_addr, ram_data, ram_count;
    
    for(i=0; i<8;i++) {
        ram_write_byte(i, 0); // zero some memory
        test_data2[i]=0;
    }
    
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
             sscanf(PT_term_buffer, "%s %d %d %d", cmd, &value1, &value2, &value3);

             switch(cmd[0]){
                 case 'w': // write RAM, command form: w addr byte
                     ram_addr = value1;
                     ram_data = value2;
                     ram_write_byte(ram_addr, (char)ram_data);
                     break;
                 case 'r': // read RAM, command form: r addr
                     value2 = ram_read_byte(value1);
                     sprintf(PT_send_buffer, "RAM(%d) = %d\n\r", value1, value2);
                     // spawn a print thread
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
                 case 'm': // write RAM array, command form: m addr, item count
                     ram_addr = value1;
                     ram_count = value2;
                     // array name is pointer to array 'test_data'
                     ram_write_byte_array(ram_addr, test_data, ram_count);
                     break;
                 case 'z': // clear a chunk of memory and receiving array for testing
                     for(i=0; i<8;i++) {
                        ram_write_byte(i, 0); // zero some memory
                        test_data2[i]=0;
                     }
                     break;
                 case 'd': // move a byte from memory to DAC: d ram_addr
                     value2 = ram_read_byte(value1);
                     dac_write_byte(value2<<4);
                     sprintf(PT_send_buffer, "RAM(%d) to DAC= %d\n\r", value1, value2);
                     // spawn a print thread
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
                case 'a': // command form: a address count
                     ram_addr = value1;
                     ram_count = value2;
                     // test_data2 is receiving array pointer
                     ram_read_byte_array(ram_addr, test_data2, ram_count);
                     // read out a few values for testing
                     sprintf(PT_send_buffer, "%d %d %d \n\r", test_data2[0], test_data2[1], test_data2[2]);
                     PT_SPAWN(pt, &pt_DMA_output, PT_DMA_PutSerialBuffer(&pt_DMA_output) );
                     break;
                 case 't': // just prints system time
                     sprintf(PT_send_buffer, "%d\n\r", sys_time_seconds);
                     // by spawning a print thread
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
// set up UART, threads
// then schedule them as fast as possible

int main(void)
{
  // === config the uart, DMA, vref, timer5 ISR =====
  PT_setup();

   // === setup system wide interrupts  =======
  INTEnableSystemMultiVectoredInt();
    
  // === set up SPI =======================
  // SCK2 is pin 26 
  // SDO2 (MOSI) is in PPS output group 2, could be connected to RPB5 which is pin 14
  PPSOutput(2, RPB5, SDO2);
  // SDI2 (MISO) is PPS output group 3, could be connected to RPA2 which is pin 9
  PPSInput(3,SDI2,RPA2);

  // control CS for RAM (bit 0) and for DAC (bit 1)
  mPORTBSetPinsDigitalOut(BIT_0 | BIT_1);
  //and set both bits to turn off both enables
  mPORTBSetBits(BIT_0 | BIT_1);
        
  // divide Fpb by 2, configure the I/O ports. Not using SS in this example
  // 8 bit transfer CKP=1 CKE=1
  // possibles SPI_OPEN_CKP_HIGH;   SPI_OPEN_SMP_END;  SPI_OPEN_CKE_REV
  // For any given peripherial, you will need to match these
  SpiChnOpen(spiChn, SPI_OPEN_ON | SPI_OPEN_MODE8 | SPI_OPEN_MSTEN | SPI_OPEN_CKE_REV , spiClkDiv);
  
  // === now the threads ====================

  // init the threads
  PT_INIT(&pt_cmd);
  PT_INIT(&pt_tick);
        
  // schedule the threads
  while(1) {
    // round robin
    PT_SCHEDULE(protothread_cmd(&pt_cmd));
    PT_SCHEDULE(protothread_tick(&pt_tick));
  }
} // main
