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

/* DHT11 sensor (TODO)
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
*/

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
  Serial.begin(UART_RATE);
  Serial.println(F("Started power supply."));
  wdt_enable(WDTO_8S);
//  dht.begin();
  sensor_acc.begin();
  sensor_solar.begin();
  //setTime(0);
  pinMode(13, OUTPUT);//Red LED
}

void loop() {
  static unsigned int actionCounter=0;//for LED blinking and some periodical actions
  static unsigned int numOfBlinks=4;//Heartbeat blinking: how many pulses will be occur every 2s (max up to 4)
  wdt_reset();
  delay(100);//100ms main loop delay
  
  if( (actionCounter%4 == 0) && (actionCounter/4 < numOfBlinks) ) digitalWrite(13, HIGH); else digitalWrite(13, LOW);
  actionCounter=( actionCounter<20 ? actionCounter+1 : 0 );
  if(actionCounter==0) {//generate NMEA string
    buildPNBLPSentence();
    Serial.println(txNMEABuffer);
  }
  if(actionCounter==10) processSensors();
  //myNMEA::sendSentence(Serial, "$PNBLP");
/*

  Serial.print("ACC Bus Voltage:   "); Serial.print(acc_busvoltage); Serial.println(" V");
  Serial.print("ACC Current:       "); Serial.print(acc_current_mA); Serial.println(" mA");
  Serial.print("ACC Power:         "); Serial.print(acc_power_mW); Serial.println(" mW");
  Serial.println("");
  Serial.print("SOLAR Bus Voltage:   "); Serial.print(solar_busvoltage); Serial.println(" V");
  Serial.print("SOLAR Current:       "); Serial.print(solar_current_mA); Serial.println(" mA");
  Serial.print("SOLAR Power:         "); Serial.print(solar_power_mW); Serial.println(" mW");
  Serial.println("");
*/
/*  
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  float hif = dht.computeHeatIndex(f, h);
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
 */
}
