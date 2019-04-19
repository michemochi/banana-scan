/*
 * File:        ADC > FFT > TFT
 * Author:      Bruce Land
 * 
 * Target PIC:  PIC32MX250F128B
 */

////////////////////////////////////
// clock AND protoThreads configure!
// You MUST check this file!
#include "config60.h"
// threading library
#include "pt_cornell_1_2_1.h"

////////////////////////////////////
// graphics libraries
#include "tft_master.h"
#include "tft_gfx.h"
// need for rand function
#include <stdlib.h>
#include <math.h>
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

// === the fixed point macros ========================================
typedef signed short fix14 ;
#define multfix14(a,b) ((fix14)((((long)(a))*((long)(b)))>>14)) //multiply two fixed 2.14
#define float2fix14(a) ((fix14)((a)*16384.0)) // 2^14
#define fix2float14(a) ((float)(a)/16384.0)
#define absfix14(a) abs(a) 
// === input arrays ==================================================
#define nSamp 512
#define nPixels 256
fix14 v_in[nSamp] ;

// === thread structures ============================================
// thread control structs
// note that UART input and output are threads
static struct pt pt_fft ;

// system 1 second interval tick
int sys_time_seconds ;

// === print line for TFT ============================================
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

//=== FFT ==============================================================
// FFT
#define N_WAVE          512    /* size of FFT 512 */
#define LOG2_N_WAVE     9     /* log2(N_WAVE) 0 */
#define begin {
#define end }

fix14 Sinewave[N_WAVE]; // a table of sines for the FFT
fix14 window[N_WAVE]; // a table of window values for the FFT
fix14 fr[N_WAVE], fi[N_WAVE];
int pixels[nPixels] ;

void FFTfix(fix14 fr[], fix14 fi[], int m)
//Adapted from code by:
//Tom Roberts 11/8/89 and Malcolm Slaney 12/15/94 malcolm@interval.com
//fr[n],fi[n] are real,imaginary arrays, INPUT AND RESULT.
//size of data = 2**m
// This routine does foward transform only
begin
    int mr,nn,i,j,L,k,istep, n;
    int qr,qi,tr,ti,wr,wi;

    mr = 0;
    n = 1<<m;   //number of points
    nn = n - 1;

    /* decimation in time - re-order data */
    for(m=1; m<=nn; ++m)
    begin
        L = n;
        do L >>= 1; while(mr+L > nn);
        mr = (mr & (L-1)) + L;
        if(mr <= m) continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
        //ti = fi[m];   //for real inputs, don't need this
        //fi[m] = fi[mr];
        //fi[mr] = ti;
    end

    L = 1;
    k = LOG2_N_WAVE-1;
    while(L < n)
    begin
        istep = L << 1;
        for(m=0; m<L; ++m)
        begin
            j = m << k;
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];
            //wr >>= 1; do not need if scale table
            //wi >>= 1;

            for(i=m; i<n; i+=istep)
            begin
                j = i + L;
                tr = multfix14(wr,fr[j]) - multfix14(wi,fi[j]);
                ti = multfix14(wr,fi[j]) + multfix14(wi,fr[j]);
                qr = fr[i] >> 1;
                qi = fi[i] >> 1;
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            end
        end
        --k;
        L = istep;
    end
end

// === FFT Thread =============================================
    
// DMA channel busy flag
#define CHN_BUSY 0x80
#define log_min 0x20 //0x20   
static PT_THREAD (protothread_fft(struct pt *pt))
{
    PT_BEGIN(pt);
    static int sample_number ;
    static fix14 zero_point_4 = float2fix14(0.4) ;
    static fix14 frr, fii ;
    // approx log calc ;
    static int sx, y, ly, temp ;
    
    while(1) {
        // yield time 1 second
        PT_YIELD_TIME_msec(300);
        
        // enable ADC DMA channel and get
        // 512 samples from ADC
        DmaChnEnable(0);
        // yield until DMA done: while((DCH0CON & Chn_busy) ){};
        PT_WAIT_WHILE(pt, DCH0CON & CHN_BUSY);
        // 
        // profile fft time
        WriteTimer4(0);
        
        // compute and display fft
        // load input array
        for (sample_number=0; sample_number<nSamp-1; sample_number++){
            // window the input and perhaps scale it
            fr[sample_number] = multfix14(v_in[sample_number], window[sample_number]); 
            fi[sample_number] = 0 ;
        }
        
        // do FFT
        FFTfix(fr, fi, LOG2_N_WAVE);
        // get magnitude and log
        // The magnitude of the FFT is approximated as: 
        //   |amplitude|=max(|Re|,|Im|)+0.4*min(|Re|,|Im|). 
        // This approximation is accurate within about 4% rms.
        // https://en.wikipedia.org/wiki/Alpha_max_plus_beta_min_algorithm
        for (sample_number = 0; sample_number < nPixels; sample_number++) {  
            // get the approx magnitude
            //fr[sample_number] = abs(fr[sample_number]); //>>9
            //fi[sample_number] = abs(fi[sample_number]);
            frr = abs(fr[sample_number]); //>>9
            fii = abs(fi[sample_number]);
            // reuse fr to hold magnitude
            //fr[sample_number] = max(fr[sample_number], fi[sample_number]) + 
            //        multfix14(min(fr[sample_number], fi[sample_number]), zero_point_4); 
            frr = max(frr, fii) + 
                    multfix14(min(frr, fii), zero_point_4); 
            // reuse fr to hold log magnitude
            // shifting finds most significant bit
            // then make approxlog  = ly + (fr-y)./(y) + 0.043;
            // BUT for an 8-bit approx (4 bit ly and 4-bit fraction)
            // ly 1<=ly<=14
            // omit the 0.043 because it is too small for 4-bit fraction
            if (sample_number==128) temp = frr;
            sx = frr;
            y=1; ly=0;
            while(sx>1) {
               y=y*2; ly=ly+1; sx=sx>>1;
            }
            // shift ly into upper 4-bits as integer part of log
            // take bits below y and shift into lower 4-bits
            frr = ((ly)<<4) + ((frr-y)>>(ly-4) ) ;
            // bound the noise at low amp
            if(frr<log_min) frr = log_min;
            fr[sample_number] = frr;
        }
        
        // timer 4 set up with prescalar=8, 
        // hence mult reading by 8 to get machine cycles
        sprintf(buffer, "FFT cycles %d", (ReadTimer4())*8);
        printLine2(0, buffer, ILI9340_WHITE, ILI9340_BLACK);
        sprintf(buffer, "%4d %4x", temp, fr[128]);
        printLine2(1, buffer, ILI9340_WHITE, ILI9340_BLACK);
        
        // Display on TFT
        // erase, then draw
        for (sample_number = 0; sample_number < nPixels; sample_number++) {
            tft_drawPixel(sample_number, pixels[sample_number], ILI9340_BLACK);
            pixels[sample_number] = -fr[sample_number] + 235 ;
            tft_drawPixel(sample_number, pixels[sample_number], ILI9340_WHITE);
            // reuse fr to hold magnitude 
        }    
        // NEVER exit while
      } // END WHILE(1)
  PT_END(pt);
} // FFT thread

// === Main  ======================================================

void main(void) {
    //SYSTEMConfigPerformance(PBCLK);

    ANSELA = 0;
    ANSELB = 0;

    // === config threads ==========
    // turns OFF UART support and debugger pin, unless defines are set
    PT_setup();

    // === setup system wide interrupts  ========
    INTEnableSystemMultiVectoredInt();

    // the ADC ///////////////////////////////////////
    
    // timer 3 setup for adc trigger  ==============================================
    // Set up timer3 on,  no interrupts, internal clock, prescalar 1, compare-value
    // This timer generates the time base for each ADC sample. 
    // works at ??? Hz
    #define sample_rate 800000 // 500000 works
    // 30 MHz PB clock rate
    #define timer_match pb_clock/sample_rate
    OpenTimer3(T3_ON | T3_SOURCE_INT | T3_PS_1_1, timer_match); 
    
    //=== DMA Channel 0 transfer ADC data to array v_in ================================
    // Open DMA Chan1 for later use sending video to TV
    #define DMAchan0 0
	DmaChnOpen(DMAchan0, 0, DMA_OPEN_DEFAULT);
    DmaChnSetTxfer(DMAchan0, (void*)&ADC1BUF0, (void*)v_in, 2, nSamp*2, 2); //512 16-bit integers
    DmaChnSetEventControl(DMAchan0, DMA_EV_START_IRQ(28)); // 28 is ADC done
    // ==============================================================================
    
    // configure and enable the ADC
    CloseADC10(); // ensure the ADC is off before setting the configuration

    // define setup parameters for OpenADC10
    // Turn module on | ouput in integer | trigger mode auto | enable autosample
    // ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
    // ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
    // ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
    #define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_TMR | ADC_AUTO_SAMPLING_ON //

    // define setup parameters for OpenADC10
    // ADC ref external  | disable offset test | disable scan mode | do 1 sample | use single buf | alternate mode off
    #define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_1 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_OFF
    //
    // Define setup parameters for OpenADC10
    // for a 30 MHz PB clock rate
    // use peripherial bus clock | set sample time | set ADC clock divider
    // ADC_CONV_CLK_Tcy should work at 40 MHz.
    // ADC_SAMPLE_TIME_6 seems to work with a source resistance < 1kohm
    #define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_5 | ADC_CONV_CLK_Tcy2 //ADC_SAMPLE_TIME_5| ADC_CONV_CLK_Tcy2

    // define setup parameters for OpenADC10
    // set AN11 and  as analog inputs
    #define PARAM4	ENABLE_AN11_ANA // pin 24

    // define setup parameters for OpenADC10
    // do not assign channels to scan
    #define PARAM5	SKIP_SCAN_ALL

    // use ground as neg ref for A | use AN11 for input A     
    // configure to sample AN11 
    SetChanADC10(ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN11); // configure to sample AN4 
    OpenADC10(PARAM1, PARAM2, PARAM3, PARAM4, PARAM5); // configure ADC using the parameters defined above

    EnableADC10(); // Enable the ADC
    ///////////////////////////////////////////////////////

    // timer 4 setup for profiling code ===========================================
    // Set up timer2 on,  interrupts, internal clock, prescalar 1, compare-value
    // This timer generates the time base for each horizontal video line
    OpenTimer4(T4_ON | T4_SOURCE_INT | T4_PS_1_8, 0xffff);
        
    // === init FFT data =====================================
    // one cycle sine table
    //  required for FFT
    int ii;
    for (ii = 0; ii < N_WAVE; ii++) {
        Sinewave[ii] = float2fix14(sin(6.283 * ((float) ii) / N_WAVE)*0.5);
        window[ii] = float2fix14(1.0 * (1.0 - cos(6.283 * ((float) ii) / (N_WAVE - 1))));
        //window[ii] = float2fix(1.0) ;
    }
    // ========================================================
    
    // init the threads
    PT_INIT(&pt_fft);

    // init the display
    tft_init_hw();
    tft_begin();
    tft_fillScreen(ILI9340_BLACK);
    //240x320 vertical display
    tft_setRotation(1); // Use tft_setRotation(1) for 320x240

    // round-robin scheduler for threads
    while (1) {
        PT_SCHEDULE(protothread_fft(&pt_fft));
    }
} // main

// === end  ======================================================

