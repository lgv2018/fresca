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

This program will correctly display temperature on the LCD and 7-seg displays for up to 8 sensors
It will also monitor all the sensors temperature and activate cooling according to user settings
Pinout can be lookedup/changed in this file where it says " PINOUT_LINE "
The menu can be navigated by pressing the 'Select' key, which will cycle through:
temperature display, CoolOn threshold setting, CoolOff threshold setting and Offset calibration setting
The CoolOn and CoolOff settings can be modified by pressing the right/left keys,
Which will in turn activate the relay when the temperature falls below CoolOn,
and will turn the relay off when the temperature rises above CoolOff
Current sensor selected can be changed in main menu by pressing up/down keys
Stores Cooling thresholds and other data in EEPROM
To know if it there is valid data in the EEPROM (e.g. arduino was used for other projects)
the code will search for a magic number '0x5A' in the EEPROM at address 0, if it doesn't find it
it will write the magic number, and the default values for the thresholds
To recover the EEPROM default state, keep SELECT key pressed at boot up until the main menu is reached

*********************************************************************************************
*/

#include <LiquidCrystal.h>
#include <TM1637Display.h>
#include <OneWire.h>
#include <DFR_Key.h>
#include <EEPROM.h>
#include "utils.h"
#include "fresca.h"

////////////////////////////////////////
// ****** DEFINE PINOUT HERE ****** PINOUT_LINE
const byte gc_7seg_dio_pins[MAX_NUM_DS1820_SENSORS]   = {7,14,15,16,17,18,19,20};   //All TM1637 DIO are specified here
const byte gc_7seg_clk_pins                           = {8};                        //One clock for all TM1637 displays
const byte g_CoolSwitch[MAX_NUM_DS1820_SENSORS]       = {9,21,22,23,24,25,26,27};   //Contains pin number for relay index
const byte gc_ds1820_pins[MAX_NUM_DS1820_SENSORS]     = {6,28,29,30,31,32,33,34};   //DS18B20 Digital temperature sensor
const byte gc_lcd_pins[6]                             = {12, 11, 5, 4, 3, 2};       //LCD 16x2 based on the Hitachi HD44780 controller (rs, enable, d4, d5, d6, d7)
const byte gc_keypad_pins                             = 0;                          //Analog Keypad on the LCD PCB (has to be an analog pin)
////////////////////////////////////////

////////////////////////////////////////
//Global variables
byte     g_showtempLCD = 0; //Set to the Sensor number you want to display on the LCD (0: sensor0)
int16_t  g_TempReading[NUM_DS1820_SENSORS]   = {0};
int16_t  g_CoolOnThresh[NUM_DS1820_SENSORS]  = {24*16,24*16,24*16,24*16,24*16,24*16,24*16,24*16};
int16_t  g_CoolOffThresh[NUM_DS1820_SENSORS] = {25*16,25*16,25*16,25*16,25*16,25*16,25*16,25*16};
int16_t  g_OffsetSensor[NUM_DS1820_SENSORS]  = {0};
////////////////////////////////////////

////////////////////////////////////////
//Define objects
TM1637Display * g_disp7seg[NUM_DS1820_SENSORS];      //7segment displays
OneWire *       g_ds1820[NUM_DS1820_SENSORS];   //DS18B20 Digital temperature sensor
LiquidCrystal   lcd(gc_lcd_pins[0], gc_lcd_pins[1], gc_lcd_pins[2], gc_lcd_pins[3], gc_lcd_pins[4], gc_lcd_pins[5]); //(rs, enable, d4, d5, d6, d7) //LCD 16x2 based on the Hitachi HD44780 controller
DFR_Key         keypad(gc_keypad_pins);         //Analog Keypad on the LCD
////////////////////////////////////////

////////////////////////////////////////
//Start main code
////////////////////////////////////////

enum state_type {st_show_temp,st_change_CoolOn, st_change_CoolOff, st_calib_sensor}; //State machine states

void loop(void)
{
    int tempKey      = 0;
    int lastKey      = 0;
    int currSensor   = 0;
    state_type state = st_show_temp;
    char print_buf[MAX_BUF_CHARS];
    
    lcd.clear();                //Wipe the screen
    interrupts();               // enable all interrupts
    TIMSK1 |= (1 << OCIE1A);    // enable timer compare interrupt
    
    //MAIN Infinite loop
    while(true)
    {
        ////////////////////////////////////////
        //Poll keys
        do
        {
            tempKey  = keypad.getKey();
        } while (tempKey == SAMPLE_WAIT);
        
        if (tempKey != NO_KEY)
        {
            lastKey  = tempKey;
            if (DO_DEBUG)
            {
                sprintf(print_buf,"Key press: %d - Analog value: %d\n", lastKey, analogRead(gc_keypad_pins));
                Serial.print(print_buf);
            }
        }
        
        ////////////////////////////////////////
        //Menu state machine
        switch (state)
        {
            case st_show_temp:
                switch (tempKey)
                {
                    case SELECT_KEY:
                        state = st_change_CoolOn;
                    break;
                    
                    case UP_KEY:
                        currSensor = SensorNext(currSensor);
                        lcd.clear(); //Wipe the screen
                    break;
                    
                    case DOWN_KEY:
                        currSensor = SensorPrev(currSensor);
                        lcd.clear(); //Wipe the screen
                    break;
                    
                    default:
                        // state = st_change_CoolOn;
                    break;
                }
                g_showtempLCD=currSensor;
                //Print first row only
                snprintf(print_buf, LCD_WIDTH+1, "Temp sensor %d         ",currSensor);
                lcd.setCursor(0,0);
                lcd.print(print_buf);
            break;
            
            case st_change_CoolOn:
                g_showtempLCD=-1; //Dont show temperature for any sensor
                switch (tempKey)
                {
                    case SELECT_KEY:
                        state = st_change_CoolOff;
                        //Write value in EEPROM
                        EEPROM.put(currSensor*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+0, g_CoolOnThresh[currSensor]);
                    break;
                    case RIGHT_KEY:
                        //Increment
                        g_CoolOnThresh[currSensor]=UpdateCoolOn(g_CoolOnThresh[currSensor],g_CoolOffThresh[currSensor],true); //0.5deg Steps
                    break;
                    case LEFT_KEY:
                        //Decrement
                        g_CoolOnThresh[currSensor]=UpdateCoolOn(g_CoolOnThresh[currSensor],g_CoolOffThresh[currSensor],false); //0.5deg Steps
                    break;
                    default:
                        //Unsupported key
                        // state = st_show_temp;
                    break;
                }
                //First row
                snprintf(print_buf, LCD_WIDTH+1, "CoolOn sensor %d",currSensor);
                lcd.setCursor(0,0);   //First row
                lcd.print(print_buf);
                //Second row
                PrintTempLCD(g_CoolOnThresh[currSensor],false);
            break;
        
            case st_change_CoolOff:
                g_showtempLCD=-1; //Dont show temperature for any sensor
                switch (tempKey)
                {
                    case SELECT_KEY:
                        state = st_calib_sensor;
                        //Write value in EEPROM
                        EEPROM.put(currSensor*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t), g_CoolOffThresh[currSensor]);
                    break;
                    case RIGHT_KEY:
                        //Increment
                        g_CoolOffThresh[currSensor]=UpdateCoolOff(g_CoolOffThresh[currSensor],g_CoolOnThresh[currSensor],true); //0.5deg Steps
                    break;
                    case LEFT_KEY:
                        //Decrement
                        g_CoolOffThresh[currSensor]=UpdateCoolOff(g_CoolOffThresh[currSensor],g_CoolOnThresh[currSensor],false); //0.5deg Steps
                    break;
                    default:
                        //Unsupported key
                        // state = st_show_temp;
                    break;
                }
                snprintf(print_buf, LCD_WIDTH+1, "CoolOff sensor %d",currSensor);
                lcd.setCursor(0,0);   //First row
                lcd.print(print_buf);
                //Second row
                PrintTempLCD(g_CoolOffThresh[currSensor],false);
            break;
            
            case st_calib_sensor: //For offset calibration
                g_showtempLCD=-1; //Dont show temperature for any sensor
                switch (tempKey)
                {
                    case SELECT_KEY:
                        state = st_show_temp;
                        //Write value in EEPROM
                        EEPROM.put(currSensor*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t)*2, g_OffsetSensor[currSensor]);
                    break;
                    case RIGHT_KEY:
                        //Increment
                        g_OffsetSensor[currSensor]+=4; //0.25deg Steps
                    break;
                    case LEFT_KEY:
                        //Decrement
                        g_OffsetSensor[currSensor]-=4; //0.25deg Steps
                    break;
                    default:
                        //Unsupported key
                        // state = st_show_temp;
                    break;
                }
                snprintf(print_buf, LCD_WIDTH+1, "Off calib sensor %d",currSensor);
                lcd.setCursor(0,0);   //First row
                lcd.print(print_buf);
                //Second row
                PrintTempLCD(g_OffsetSensor[currSensor],false);
            break;
            
            default:
                state = st_show_temp;
            break;
        }
        
        ////////////////////////////////////////
        if (tempKey != NO_KEY) delay_noInterrupts(500); //Don't change state too quickly
    }
    
}

//Setup function run after POR
void setup(void)
{
    byte i;
    
    // Disable all interrupts
    noInterrupts();
    
    //////////////////////////////////////////////////
    //Initialize Serial
    Serial.begin(9600);
    Serial.print("---------------");Serial.println();
    Serial.print("'fresca' test");Serial.println();
    Serial.print("Please wait ...");Serial.println();
    Serial.print("---------------");Serial.println();
    //////////////////////////////////////////////////

    //////////////////////////////////////////////////
    //Initialize LCD
    Serial.print("Initializing LCD...");
    lcd.begin(LCD_WIDTH, LCD_HEIGHT,1);
    lcd.setCursor(0,0);
    lcd.print("'fresca' test");
    lcd.setCursor(0,1);
    lcd.print("Please wait ...");
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////

    //////////////////////////////////////////////////
    //Initialize Displays
    Serial.print("Initializing 7-segment displays...");
    for (i = 0; i < NUM_DS1820_SENSORS; i++)
    {
        //Create object
        g_disp7seg[i] = new TM1637Display(gc_7seg_clk_pins,gc_7seg_dio_pins[i]);
        //
        g_disp7seg[i]->setBrightness(0x0f,true); //Turn-on
        g_disp7seg[i]->showNumberDec(1305,true);
    }
    //Blink them
    delay_noInterrupts(400);
    for (i = 0; i < NUM_DS1820_SENSORS; i++)
    {
        g_disp7seg[i]->setBrightness(0x00,false);//Turn-off
        g_disp7seg[i]->showNumberDec(1305,true); //Write the number again to turn off
    }
    delay_noInterrupts(400);
    for (i = 0; i < NUM_DS1820_SENSORS; i++)
    {
        g_disp7seg[i]->setBrightness(0x0f,true); //Turn-on again (end blinking)
        g_disp7seg[i]->showNumberDec(1305,true);
    }
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////

    //////////////////////////////////////////////////
    //Initialize temperature sensors
    Serial.print("Initializing DS18B20 temperature sensors...");
    for (i=0;i<NUM_DS1820_SENSORS;i++)
    {
        //Create object
        g_ds1820[i] = new OneWire(gc_ds1820_pins[i]);
        //Default is 12-bit resolution
        g_ds1820[i]->reset();
        // g_ds1820[i]->select(addr[sensor]); //Select a particular sensor
        g_ds1820[i]->skip();         //SkipROM command, we only have one sensor per wire
        g_ds1820[i]->write(0x44,0);  // Command 0x44, start temperature conversion
    }
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////
    
    //////////////////////////////////////////////////
    //Initialize keypad
    Serial.print("Initializing keypad...");
    keypad.setRate(KEYPAD_REFRESH_RATE); //Sets the sample rate at once every x milliseconds.
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////
    
    //////////////////////////////////////////////////
    //Initialize Relays
    Serial.print("Initializing relays...");
    for (i=0;i<NUM_DS1820_SENSORS;i++)
    {
        pinMode(g_CoolSwitch[i], OUTPUT);
        digitalWrite(g_CoolSwitch[i], (RELAY_ACTIVE==1) ? HIGH : LOW);
    }
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////
    
    g_showtempLCD=0; //Show temperature for sensor0
    
    //////////////////////////////////////////////////
    //Recall EEPROM values
    Serial.print("Recalling EEPROM values ... ");
    if ( (EEPROM.read(EEPROM_MAGIC_VAR_ADDR) != EEPROM_MAGIC_VAR_VALUE) || SelectKeyPressed() )
    {
        //We haven't written this EEPROM before, or user is indicating a reset of the EEPROM
        //Store new default values
        Serial.print("***NOT found magic number, writing default values ...");
        EEPROM.put(EEPROM_MAGIC_VAR_ADDR, (byte) EEPROM_MAGIC_VAR_VALUE); //Write magic number
        
        //Write default values
        for (i=0; i<NUM_DS1820_SENSORS; i++)
        {
            EEPROM.put(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+0, g_CoolOnThresh[i]);
            EEPROM.put(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t), g_CoolOffThresh[i]);
            EEPROM.put(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t)*2, g_OffsetSensor[i]);
        }
    }
    else
    {
        //We have written this EEPROM before, recall the values
        Serial.print("Successfully found magic number, recalling values ...");
        for (i=0; i<NUM_DS1820_SENSORS; i++)
        {
            EEPROM.get(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+0, g_CoolOnThresh[i]);
            EEPROM.get(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t), g_CoolOffThresh[i]);
            EEPROM.get(i*EEPROM_BLOCKSIZE+EEPROM_START_ADDR+sizeof(int16_t)*2, g_OffsetSensor[i]);
        }
    }
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////
    
    //////////////////////////////////////////////////
    // Initialize Timer1 interrupt
    Serial.print("Initializing timer1 ...");
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0; //Timer count register
    OCR1A  = (unsigned) ((16.0e6/256.0)*TEMP_POLL_SEC); // Output compare match register (16MHz/256)*Segs = (16e6/256)*0.8Seg, if using 256 prescaler
    TCCR1B |= (1 << WGM12) | (1 << CS12);               // Clear Timer on Compare (CTC) mode ; 256 prescaler
    Serial.print("Done!");Serial.println();
    //////////////////////////////////////////////////
    
    Serial.print("***************************");Serial.println();
    Serial.print("***Starting main program***");Serial.println();
    Serial.print("***************************");Serial.println();
    
    delay_noInterrupts(3000);
}

//////////////////////////////////////////
// Timer1 compare interrupt service routine
//////////////////////////////////////////
ISR(TIMER1_COMPA_vect)
{
    read_temp_sensors();
    //Reset count
    TCNT1  = 0; //Timer count register
}

//Read all temperature sensors and update global temperature variables
inline void read_temp_sensors()
{
    byte i, sensor;
    byte present = 0;
    byte data[9];
    bool crc_err;
    char print_buf[MAX_BUF_CHARS];
    int16_t HighByte, LowByte;
    static uint32_t end_time=0;
    uint32_t start;
        
    //Print debug stuff
    if (DO_DEBUG)
    {
        start = millis();
        sprintf(print_buf,"Time between calls: %ld\n", start - end_time);
        Serial.print(print_buf);
    }
    
    //////////////////////////////////////////////////////////////////
    //Loop all sensors
    //////////////////////////////////////////////////////////////////
    
    for (sensor=0;sensor<NUM_DS1820_SENSORS;sensor++)
    {
        //////////////////////////////////////////////////////////////////
        //Read temperature
        //////////////////////////////////////////////////////////////////
        present = g_ds1820[sensor]->reset();
        
        if (present)
        {
            //If sensor is present, read and update temperature
            g_ds1820[sensor]->skip();         //SkipROM command, we only have one sensor per wire
            g_ds1820[sensor]->write(0xBE);    //Read Scratchpad
            if (DO_DEBUG) { Serial.print("Data sensor ");Serial.print(sensor, DEC);Serial.print(" ="); }
            
            byte data_zero_cnt=0;
            for ( i = 0; i < 9; i++)
            {
                // Read 9 bytes
                data[i] = g_ds1820[sensor]->read();
                if (data[i] == 0) data_zero_cnt++;
                if (DO_DEBUG)
                {
                    Serial.print(" 0x");
                    Serial.print(data[i], HEX);
                }
            }

            //Immediately restart temperature conversion
            g_ds1820[sensor]->reset();
            // g_ds1820[sensor]->select(addr[sensor]); //Select a particular sensor
            g_ds1820[sensor]->skip();         // SkipROM command, we only have one sensor per wire
            g_ds1820[sensor]->write(0x44,0);  // Command 0x44, start temperature conversion

            crc_err=false;
            //Process data
            if (USE_CRC)
            {
                //Check CRC
                crc_err = (OneWire::crc8(data, 9)==0) ? false : true; //CRC equal 0 over the whole scratchpad memory means CRC is correct
                if (data_zero_cnt==9) crc_err=true; //If all bytes are zero, data is wrong
                if ( crc_err == false )
                {
                    if (DO_DEBUG) {Serial.print(" - CRC OK");Serial.println();}
                    //Update temperature
                    LowByte  = data[0]; //Temp low byte
                    HighByte = data[1]; //Temp high byte
                    g_TempReading[sensor] = (HighByte << 8) + LowByte;
                    g_TempReading[sensor] += g_OffsetSensor[sensor]; //Remove offset (if calibrated)
                }
                else
                {
                    //There was a CRC error, don't update temperature
                    Serial.print(" - CRC ERROR ");Serial.println();
                }
            }
            else
            {
                //Don't check CRC
                if (DO_DEBUG) {Serial.print(" - NO CHECK");Serial.println();}
                if (data_zero_cnt != 9) //If all bytes are zero, data is wrong
                {
                    //Update temperature
                    LowByte  = data[0]; //Temp low byte
                    HighByte = data[1]; //Temp high byte
                    g_TempReading[sensor] = (HighByte << 8) + LowByte;
                    g_TempReading[sensor] += g_OffsetSensor[sensor]; //Remove offset (if calibrated)
                }
                else
                {
                    //Most likely no sensor present
                    crc_err=true;
                    Serial.print("***CAN'T FIND SENSOR ");Serial.print(sensor, DEC);Serial.print(" ***");Serial.println();
                }
            }
        }
        else
        {
            //If no sensor present (or error), leave the temperature unchanged
            Serial.print("***CAN'T FIND SENSOR ");Serial.print(sensor, DEC);Serial.print(" ***");Serial.println();
        }

        //////////////////////////////////////////////////////////////////
        //Control Cooling
        //////////////////////////////////////////////////////////////////
        if (g_TempReading[sensor] <= g_CoolOnThresh[sensor])
        {
            //Turn on cooling
            SwitchCooling(sensor, true);
        }
        else if (g_TempReading[sensor] >= g_CoolOffThresh[sensor])
        {
            //Turn off cooling
            SwitchCooling(sensor, false);
        }
        
        //////////////////////////////////////////////////////////////////
        //Printing
        //////////////////////////////////////////////////////////////////

        bool SignBit;
        
        //7-segment
        int32_t DispTemp32;
        SignBit    = (g_TempReading[sensor] < 0) ? true : false;                     //test most sig bit
        DispTemp32 =( ((int32_t)g_TempReading[sensor]) * 100 ) >> 4;                 //Promote to 32-bits for the 7-seg display
        DispTemp32 = SignBit ? -DispTemp32 : DispTemp32;                             //Complement if negative
        g_disp7seg[sensor]->showNumberDecEx((unsigned)DispTemp32, 0x10, true, 4, 0); //Format to display in 4 7-segment chars (multiply by 100, then remove fractional bits)

        //LCD printing
        if (g_showtempLCD == sensor)
        {
            PrintTempLCD(g_TempReading[sensor],(crc_err)||(!present));
        }
    }
    
    //Print debug stuff
    if (DO_DEBUG)
    {
        //Print free ram
        sprintf(print_buf,"Free RAM [bytes]: %d\n", freeRam());
        Serial.print(print_buf);
        //Measure time
        end_time = millis();
        sprintf(print_buf,"Read sensors time: %ld\n", end_time - start);
        Serial.print(print_buf);
    }
}