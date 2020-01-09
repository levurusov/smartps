/*                                          WHAT DOES THIS FIRMWARE DO
 *                   1. Measures currents, voltages and temperature and sends it to Linux board
 *                      via the UART using NMEA0183-like syntax.
 *                   2. Receives real time and commands from Linux board via the UART.
 *                   3. Turns on and off Linux board and modem when needed. Acts as watchdog.
 * 
 *                      Warning! UART works at 4800 bps, don't forget to set it 
 *                               on linux board and serial monitor
 *
 *                                           NMEA0183-like sentences:
 *    From PS to Linux:
 *    
 *           1      2      3 4     5       6     7       8
 *           |      |      | |     |       |     |       |
 *    $PNBLP,hhmmss,ddmmyy,A,xx.xx,xxxx.xx,xx.xx,xxxx.xx*hh
 *    
 *    1) Time (UTC)
 *    2) Date
 *    3) Time&Date status: A - synchronized with NTP, V - not syncronized
 *    4) ACC voltage, V
 *    5) ACC current, mA
 *    6) Solar panel voltage, V
 *    7) Solar panel current, mA
 *    8) Checksum
 *    From Linux to PS:
 *    
 *           1      2      3 4      5      6
 *           |      |      | |      |      |
 *    $PNBLL,hhmmss,ddmmyy,A,hhmmss,hhmmss*hh
 *    
 *    
 *    1) Time (UTC)
 *    2) Date
 *    3) Time&Date status: A - synchronized with NTP, V - not syncronized
 *    4) Turn on time (UTC)
 *    5) Turn off time (UTC)
 *    6) Checksum
 *    
 *    
 *    Code used:
 *    -own code;
 *    -microNMEA Arduino library - hardly modified and moved inside of the sketch
 */

//timekeeping functions library
#include <Time.h>
#include <TimeLib.h>

//INA219 sensor library
#include <Adafruit_INA219.h>
#include <stdio.h>
#include <avr/wdt.h>

//Local includes
#include "config.h"
#include "myNMEA.h"

Adafruit_INA219 sensor_acc;
Adafruit_INA219 sensor_solar(0x41);//additional address jumper soldered to use 2 sensors

float acc_busvoltage = 0;
char s_acc_busvoltage[MAX_NUMERIC_CHARS];
float acc_current_mA = 0;
char s_acc_current_mA[MAX_NUMERIC_CHARS];
float solar_busvoltage = 0;
char s_solar_busvoltage[MAX_NUMERIC_CHARS];
float solar_current_mA = 0;
char s_solar_current_mA[MAX_NUMERIC_CHARS];
unsigned char timeIsInSync = 0;

char txNMEABuffer[NMEA_BUFFER_LENGTH];
char rxNMEABuffer[NMEA_BUFFER_LENGTH];
myNMEA nmea(rxNMEABuffer, sizeof(rxNMEABuffer));

void rminitialspaces(char* arr) {
  char tmp [MAX_NUMERIC_CHARS];
  unsigned char j=0;
  for(unsigned char i=0; i<MAX_NUMERIC_CHARS;++i) {
    if(arr[i]!=' ') {
      tmp[j]=arr[i];
      ++j;
      if(tmp[j]=='\0') break;
    }
  }
  strncpy(arr,tmp,MAX_NUMERIC_CHARS-1);
}

void powerModemOff() {
  digitalWrite(MODEM_EN_PIN, LOW);
}

void powerModemOn() {
  digitalWrite(MODEM_EN_PIN, HIGH);
}

void powerLinuxBoardOff() {
  //Set UART pins to low to avoid powering linux board through them
  Serial.end();
  pinMode(0, OUTPUT);
  digitalWrite(0, LOW);
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  digitalWrite(CAMERA_EN_PIN, LOW);
}

void powerLinuxBoardOn() {
  digitalWrite(CAMERA_EN_PIN, HIGH);
  pinMode(0, INPUT);//maybe not needed
  pinMode(1, INPUT);//maybe not needed
  Serial.begin(UART_RATE);
}

void restartSystem() {
  powerModemOff();
  powerLinuxBoardOff();
  delay(1000);
  powerLinuxBoardOn();
  powerModemOn();
}

void processSensors() {
    acc_busvoltage = sensor_acc.getBusVoltage_V();
    dtostrf(acc_busvoltage,5, 2, s_acc_busvoltage);
    rminitialspaces(s_acc_busvoltage);
    acc_current_mA = sensor_acc.getCurrent_mA();
    dtostrf(acc_current_mA,7, 2, s_acc_current_mA);
    rminitialspaces(s_acc_current_mA);
    solar_busvoltage = sensor_solar.getBusVoltage_V();
    dtostrf(solar_busvoltage,5, 2, s_solar_busvoltage); 
    rminitialspaces(s_solar_busvoltage);
    solar_current_mA = sensor_solar.getCurrent_mA();
    dtostrf(solar_current_mA,7, 2, s_solar_current_mA);
    rminitialspaces(s_solar_current_mA);
}

void buildPNBLPSentence() {//Place actual data into txNMEABuffer
  char checksum[5];
  snprintf(txNMEABuffer,(NMEA_BUFFER_LENGTH-5),"$PNBLP,%02d%02d%02d,%02d%02d%02d,%s,%s,%s,%s,%s*",hour(),minute(),second(),
            day(),month(),year(),(timeStatus()== timeSet ? "A" : "V"),s_acc_busvoltage,s_acc_current_mA,
            s_solar_busvoltage,s_solar_current_mA);
  myNMEA::generateChecksum(txNMEABuffer, checksum);
  checksum[2] = '\r';
  checksum[3] = '\n';
  checksum[4]='\0';
  snprintf(txNMEABuffer+strlen(txNMEABuffer),4,"%s",checksum);
}


void setup() {
  pinMode(CAMERA_EN_PIN, OUTPUT);
  pinMode(MODEM_EN_PIN, OUTPUT);
  restartSystem();
  wdt_enable(WDTO_8S);
  sensor_acc.begin();
  sensor_solar.begin();
  pinMode(13, OUTPUT);//Red LED
}

void loop() {
  static unsigned int actionCounter=0;//for LED blinking and some periodical actions
  static enum _numOfBlinks {CAMERA_OFF=1,CAMERA_ON,NOTIME} numOfBlinks = NOTIME;
  static unsigned long secNow, secPowerOn, secPowerOff;
  static bool cameraEnabledByTimetable;
  static bool cameraIsPowered=true;
  if(timeStatus()!= timeSet)
    numOfBlinks = NOTIME;
  else {
    if(digitalRead(CAMERA_EN_PIN)==HIGH)
      numOfBlinks = CAMERA_ON;
    else
      numOfBlinks = CAMERA_OFF;
  }
    
  wdt_reset();
  delay(100);//100ms main loop delay


  if(nmea.isValid() && (timeStatus()!= timeSet) ) {
    setTime(nmea.getHour(), nmea.getMinute(), nmea.getSecond(), nmea.getDay(), nmea.getMonth(), nmea.getYear());
  }
  
  if( (actionCounter%4 == 0) && (actionCounter/4 < numOfBlinks) )
    digitalWrite(LED_BUILTIN, HIGH);
  else
    digitalWrite(LED_BUILTIN, LOW);
  actionCounter=( actionCounter<20 ? actionCounter+1 : 0 );//2s total period
  if(actionCounter==0) {//generate NMEA string
    buildPNBLPSentence();
    Serial.println(txNMEABuffer);
    //decide camera and modem power status
    secNow=hour()*3600+minute()*60+second();
    secPowerOn=nmea.getPowerOnHour()*3600+nmea.getPowerOnMinute()*60+nmea.getPowerOnSecond();
    secPowerOff=nmea.getPowerOffHour()*3600+nmea.getPowerOffMinute()*60+nmea.getPowerOffSecond();
    cameraEnabledByTimetable=(secPowerOn>secPowerOff)?( secNow>secPowerOn || secNow<secPowerOff )
                                                      :( secNow>secPowerOn && secNow<secPowerOff );
    if(timeStatus()==timeSet)//we can turn camera off only after sync time
    {
      if(cameraEnabledByTimetable!=cameraIsPowered)
      {
        if(cameraEnabledByTimetable)
        {
          powerLinuxBoardOn();
          powerModemOn();
        }
        else
        {
          powerModemOff();
          powerLinuxBoardOff();
        }
        cameraIsPowered=cameraEnabledByTimetable;
      }
    }
  }
  if(actionCounter==10) processSensors();


  while (Serial.available())
  {
    //Fetch the character one by one
    char c = Serial.read();
    //Pass the character to the library
    nmea.process(c);
  }




}
