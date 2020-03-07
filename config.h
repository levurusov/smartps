#define UART_RATE 4800
#define NMEA_BUFFER_LENGTH 85
#define MAX_NUMERIC_CHARS 10

#define CAMERA_EN_PIN 8 //8 on silk
#define MODEM_EN_PIN 7 //7 on silk

#define WDT_TIMEOUT 100
#define DRIFT_TOLERANCE 20 //if our clock deviated more than this, re-sync

#define MODEM_POWERUP_DELAY 7 //for buggy USB port, power up modem when USB controller already initialized
#define UART_USING_DELAY 60 //if linux board has only one UART, it will produce any output before $PNBLL messages will start.
                            //Also, if we will send $PNBLP during u-boot work, it can stop booting.
