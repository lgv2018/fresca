/*
    'fresca' project, temperature control for making beer!
    Copyright (C) 2017  Leonardo M. Capossio

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
*********************************************************************************************
Author: Leonardo Capossio
Project: 'fresca'
Description:

*Header, constants and prototypes for 'fresca'
*Here some constants that control certain parts of the program can be changed
*Pinout is specified on the fresca.ino file
*Temperature display is defaulted to degrees celsius, but can be changed to fahrenheit with TEMP_FAHRENHEIT
 Celsius temperature display on 4-digit 7-segment LCD is two leftmost digits are whole part and two rightmost are fractional part
 Fahrenheit temperature display on 4-digit 7-segment LCD is three leftmost digits are whole part and the rightmost is fractional part
 All temperatures are represented internally with 16 bits signed 'Q11.4' fixed point format (celsius)
*********************************************************************************************
*/

#ifndef FRESCA_H
#define FRESCA_H

    ////////////////////////////////////////
    //MACROS
    #define TEMP_DATA_TYPE int16_t
    #define ROUND(x) ((x)>=0?(TEMP_DATA_TYPE)((x)+0.5):(TEMP_DATA_TYPE)((x)-0.5))
    //Convert to signed fixed point, max 16 bits, truncates result
    #define TEMPFLOAT2FIX(VAL_FP,SCALE) (TEMP_DATA_TYPE) ROUND(VAL_FP*(float)SCALE)
    ////////////////////////////////////////

    ////////////////////////////////////////
    //
    #define CRISTAL_FREQ_MHZ       16.0   //Arduino operating frequency Floating point
    #define TIMER1_PRESCALE        256.0  //Floating point, don't change
    #define TEMP_FRAC_BITS         4      //Fractional bits for temperature representation
    #define TEMP_SCALE             (1<<TEMP_FRAC_BITS) //Scaling factor to transform floating point to fixed point
    
    //Define steps and max/min temperature values, user can modify this
    #define THRESHOLD_STEP_FP      0.25   //In floating point
    #define OFFSET_STEP_FP         0.0625 //In floating point
    #define MAX_TEMP_FP            30.0   //Max Temp for CoolOn
    #define MIN_TEMP_FP            5.0    //Min Temp for CoolOff
    #define MAX_OFF_TEMP_FP        1.0    //Max Temp for OffsetCalib
    #define MIN_OFF_TEMP_FP        1.0    //Min Temp for OffsetCalib (will be interpreted as negative)
    
    
    //Now calculate fixed point values
    #define THRESHOLD_STEP          TEMPFLOAT2FIX(THRESHOLD_STEP_FP,TEMP_SCALE)   //
    #define OFFSET_STEP             TEMPFLOAT2FIX(OFFSET_STEP_FP,TEMP_SCALE)      //
    #define MAX_TEMP                TEMPFLOAT2FIX(MAX_TEMP_FP,TEMP_SCALE)         //
    #define MIN_TEMP                TEMPFLOAT2FIX(MIN_TEMP_FP,TEMP_SCALE)         //
    #define MAX_OFF_TEMP            TEMPFLOAT2FIX(MAX_OFF_TEMP_FP,TEMP_SCALE)     //
    #define MIN_OFF_TEMP            TEMPFLOAT2FIX(MIN_OFF_TEMP_FP,TEMP_SCALE)     //
    

    ////////////////////////////////////////
    //Constants, user can modify this
    #define MAX_NUM_DS1820_SENSORS 8
    #define NUM_DS1820_SENSORS     8       //One sensor per wire
    #define DS1820_CONFIG_REG      0x7F    //12-bit resolution, no more options
    
    #define TEMP_FAHRENHEIT 0            //!=0 temperature is displayed in fahrenheit
    #define DEBUG_SENSORS   0            //!=0 serial debug messages are enabled (Key press messages)
    #define DEBUG_KEYS      0            //!=0 serial debug messages are enabled (Sensor data and related messages)
    #define DEBUG_PERF      0            //!=0 serial debug messages are enabled (Performance and RAM usage)
    #define USE_CRC         1            //!=0 DS1820 CRC check is enabled
    #define MAX_BUF_CHARS  64            //Max. characters for print buffer
    #define TEMP_POLL_SEC 0.8            //Temperature polling in seconds
    #define LCD_WIDTH 16                 //LCD horizontal size
    #define LCD_HEIGHT 2                 //LCD vertical size
    #define RELAY_ACTIVE 0               //0: Active LOW relays, 1 active HIGH relays
    #define KEYPAD_REFRESH_RATE 20       //Sets the sample rate of the keypad at once every x milliseconds.
    #define TIMER_20MS          (((CRISTAL_FREQ_MHZ*1e6)/TIMER1_PRESCALE)*0.02)  //OCR1A value
    #define TIMER_100MS         (((CRISTAL_FREQ_MHZ*1e6)/TIMER1_PRESCALE)*0.1)   //OCR1A value
    #define TIMER_250MS         (((CRISTAL_FREQ_MHZ*1e6)/TIMER1_PRESCALE)*0.25)  //OCR1A value
    #define TIMER_500MS         (((CRISTAL_FREQ_MHZ*1e6)/TIMER1_PRESCALE)*0.5)   //OCR1A value
    #define INIT_DELAY          2000                   //Time to before starting main loop in milliseconds
    ////////////////////////////////////////

    ////////////////////////////////////////
    //EEPROM
    #define EEPROM_MAGIC_VAR_ADDR 0             //Byte variable stored in this location indicates EEPROM has been written previously
    #define EEPROM_MAGIC_VAR_VALUE 0x5A
    #define EEPROM_START_ADDR 1
    #define EEPROM_BLOCKSIZE  sizeof(int16_t)*2 //Each block contains: CoolOn, CoolOff
    #define EEPROM_ADDR_INCR  sizeof(int16_t)
    ////////////////////////////////////////
    
    
    ////////////////////////////////////////
    //Global variables
    extern byte     g_showtempLCD;                          //Set to the Sensor number you want to display on the LCD (0: sensor0)
    extern int16_t  g_TempReading[NUM_DS1820_SENSORS];      //TempReading for every sensor
    extern int16_t  g_CoolOnThresh[NUM_DS1820_SENSORS];     //CoolOnThreshold for every sensor
    extern int16_t  g_CoolOffThresh[NUM_DS1820_SENSORS];    //CoolOffThreshold for every sensor
    extern int16_t  g_OffsetSensor[NUM_DS1820_SENSORS];     //Offset calibration for every sensor
    
    //Objects (Peripherals)
    extern TM1637Display *g_disp7seg[NUM_DS1820_SENSORS];   //7segment displays (TM1637)
    extern OneWire *g_ds1820[NUM_DS1820_SENSORS];           //DS18B20 Digital temperature sensor
    extern const byte g_CoolSwitch[NUM_DS1820_SENSORS];     //Cooling actuators (Relays)
    extern LiquidCrystal lcd;                               //LCD 16x2 based on the Hitachi HD44780 controller
    extern DFR_Key keypad;                                  //Analog Keypad on the LCD
    ////////////////////////////////////////
    
    ////////////////////////////////////////
    //Function prototypes
    void read_temp_sensors(); //Read all temperature sensors and update global temperature variables
    inline void main_menu();  //Main menu
    ////////////////////////////////////////
    
#endif