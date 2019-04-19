
// the ADC ///////////////////////////////////////
// configure and enable the ADC
CloseADC10(); // ensure the ADC is off before setting the configuration

// define setup parameters for OpenADC10
// Turn module on | ouput in integer | trigger mode auto | enable autosample
// ADC_CLK_AUTO -- Internal counter ends sampling and starts conversion (Auto convert)
// ADC_AUTO_SAMPLING_ON -- Sampling begins immediately after last conversion completes; SAMP bit is automatically set
// ADC_AUTO_SAMPLING_OFF -- Sampling begins with AcquireADC10();
#define PARAM1  ADC_FORMAT_INTG16 | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON //

// define setup parameters for OpenADC10
// ADC ref external  | disable offset test | disable scan mode | do 2 sample | use single buf | alternate mode on
#define PARAM2  ADC_VREF_AVDD_AVSS | ADC_OFFSET_CAL_DISABLE | ADC_SCAN_OFF | ADC_SAMPLES_PER_INT_2 | ADC_ALT_BUF_OFF | ADC_ALT_INPUT_ON
        //
// Define setup parameters for OpenADC10
// use peripherial bus clock | set sample time | set ADC clock divider
// ADC_CONV_CLK_Tcy2 means divide CLK_PB by 2 (max speed)
// ADC_SAMPLE_TIME_5 seems to work with a source resistance < 1kohm
// SLOW it down a little
#define PARAM3 ADC_CONV_CLK_PB | ADC_SAMPLE_TIME_15 | ADC_CONV_CLK_Tcy //ADC_SAMPLE_TIME_15| ADC_CONV_CLK_Tcy2

// define setup parameters for OpenADC10
// set AN11 and  as analog inputs
#define PARAM4 ENABLE_AN1_ANA | ENABLE_AN5_ANA // 

// define setup parameters for OpenADC10
// do not assign channels to scan
#define PARAM5 SKIP_SCAN_ALL //|SKIP_SCAN_AN5 //SKIP_SCAN_AN1 |SKIP_SCAN_AN5  //SKIP_SCAN_ALL

// use ground as neg ref for A | use AN11 for input A     
// // configure to sample AN5 and AN1
SetChanADC10( ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEA_AN1 | ADC_CH0_NEG_SAMPLEB_NVREF | ADC_CH0_POS_SAMPLEB_AN5 );
    
OpenADC10( PARAM1, PARAM2, PARAM3, PARAM4, PARAM5 ); // configure ADC using the parameters defined above

EnableADC10(); // Enable the ADC

// TO READ the two channels
// adc1 = ReadADC10(0);
// adc5 = ReadADC10(1); 
//
///////////////////////////////////////////////////////

