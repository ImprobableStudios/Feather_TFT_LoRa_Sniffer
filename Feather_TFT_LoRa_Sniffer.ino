/*
 Name:      Feather_TFT_LoRa_Sniffer.ino
 Created:   7/31/2017 6:04:42 PM
 Author:    joe
*/

#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <RH_RF95.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef ESP8266
#define STMPE_CS  16
#define TFT_CS    0
#define TFT_DC    15
#define SD_CS     2

#define LED       x

#define RFM95_CS  x
#define RFM95_RST x
#define RFM95_INT x
#endif
#ifdef __AVR_ATmega32U4__
#define STMPE_CS  6
#define TFT_CS    9
#define TFT_DC    10
#define SD_CS     5

#define LED       13

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 7
#endif
#ifdef ARDUINO_SAMD_FEATHER_M0
#define STMPE_CS  6
#define TFT_CS    9
#define TFT_DC    10
#define SD_CS     5

#define LED       13

#define RFM95_CS  8
#define RFM95_RST 4
#define RFM95_INT 3
#endif
#ifdef TEENSYDUINO
#define TFT_DC    10
#define TFT_CS    4
#define STMPE_CS  3
#define SD_CS     8

#define LED       x

#define RFM95_CS  x
#define RFM95_RST x
#define RFM95_INT x
#endif
#ifdef ARDUINO_STM32_FEATHER
#define TFT_DC    PB4
#define TFT_CS    PA15
#define STMPE_CS  PC7
#define SD_CS     PC5

#define LED       x

#define RFM95_CS  x
#define RFM95_RST x
#define RFM95_INT x
#endif
#ifdef ARDUINO_NRF52_FEATHER /* BSP 0.6.5 and higher! */
#define TFT_DC    11
#define TFT_CS    31
#define STMPE_CS  30
#define SD_CS     27

#define LED       x

#define RFM95_CS  x
#define RFM95_RST x
#define RFM95_INT x
#endif

#define TEXT_HEIGHT_UNSCALED    8
#define PRINTFLN_MAXLEN         256

#define RH_FLAGS_ACK            0x80

#define LOG_HEADER              "MS,Freq,FreqPacketCount,RSSI,From,To,MsgId,Flags,Ack,DataLen,DataBytes,Data"

// Define the frequencies to scan
#define FREQ_COUNT 19
float _frequencies[] =
{
    868.0, 915.0,                                                   // From Adafruit Learn
    903.9, 904.1, 904.3, 904.5, 904.7, 904.9, 905.1, 905.3, 904.6,  // TTN - US Uplink
    923.3, 923.9, 924.5, 925.1, 925.7, 926.3, 926.9, 927.5          // TTN - US Downlink
};

// How long should a frequency be monitored
#define FREQ_TIME_MS            5000

// Used to track how many packets we have seen on each frequency above
uint16_t _packetCounts[FREQ_COUNT] = { 0 };

// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

Adafruit_ILI9341 _tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

uint8_t  _rxBuffer[RH_RF95_MAX_MESSAGE_LEN];                // receive buffer
uint8_t  _rxRecvLen;                                        // number of bytes actually received
char     _printBuffer[512]  = "\0";                         // to send output to the PC
uint32_t _freqExpire        = 0;                            // Millisecond at which frequency should change
uint32_t _freqIndex         = 0;                            // Which frequency is currently monitored
bool     _sdReady           = false;                        // Is a file open and ready for writing?
File     _logfile;                                          // Log file on SD card

void tft_printfln(uint8_t size, uint16_t fg_color, uint16_t bg_color, char *fmt, ...)
{
    static uint16_t line = 0;
    static bool scroll = false;

    // Resulting string limited to PRINTFLN_MAXLEN chars
    char buf[PRINTFLN_MAXLEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, PRINTFLN_MAXLEN, fmt, args);
    va_end(args);

    _tft.setTextColor(fg_color, bg_color);
    _tft.setTextSize(size);

    // Handle repositioning in the framebuffer
    if (line >= _tft.height() / TEXT_HEIGHT_UNSCALED)
    {
        scroll = true;
        line = 0;
        _tft.setCursor(0, 0);
    }

    if (scroll)
    {
        _tft.fillRect(0, (TEXT_HEIGHT_UNSCALED * line), _tft.width(), (TEXT_HEIGHT_UNSCALED * size), bg_color);
        _tft.scrollTo((TEXT_HEIGHT_UNSCALED * line) + (TEXT_HEIGHT_UNSCALED * size));
    }

    _tft.println(buf);

    line += size;
}

void rf95_setFrequency(uint32_t index)
{
    if (!rf95.setFrequency(_frequencies[index]))
    {
        tft_printfln(2, ILI9341_BLACK, ILI9341_RED, "setFrequency failed ");
        while (1);
    }

    snprintf(_printBuffer, sizeof(_printBuffer), "Freq: %04u (%04u)   ", (uint32_t)(_frequencies[index] * 10), _packetCounts[index]);
    tft_printfln(2, ILI9341_BLACK, ILI9341_BLUE, _printBuffer);
    Serial.println(_printBuffer);
}

void sd_inititalize(void)
{
    if (SD.begin(SD_CS))
    {
        char filename[15];

        strcpy(filename, "LORA__00.TXT");

        for (uint8_t i = 0; i < 100; i++)
        {
            filename[6] = '0' + i / 10;
            filename[7] = '0' + i % 10;

            // create if does not exist, do not open existing, write, sync after write
            if (!SD.exists(filename))
            {
                break;
            }
        }

        _logfile = SD.open(filename, FILE_WRITE);

        if (!_logfile)
        {
            Serial.print("Couldnt create ");
            Serial.println(filename);
            tft_printfln(2, ILI9341_BLACK, ILI9341_RED, "File create failed  ");
        }
        else
        {
            Serial.print("Writing to ");
            Serial.println(filename);
            tft_printfln(2, ILI9341_BLACK, ILI9341_GREEN, "File: %12s  ", filename);

            // Set flag to indicate we can write to SD
            _sdReady = true;
        }
    }
    else
    {
        tft_printfln(2, ILI9341_BLACK, ILI9341_RED, "SD Init failed      ");
        Serial.println("SD.begin() failed!");
    }
}

void setup()
{
    // Configure pins
    pinMode(LED, OUTPUT);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    // The RFM95 has a pulldown on this pin, so the radio
    // is technically always selected unless you set the pin low
    // this will cause other SPI devices to fail to function as 
    // expected because CS (active-low) will be selected for 
    // the RFM95 at the same time.
    digitalWrite(RFM95_CS, HIGH);

    // Wait for serial to initialize before proceeding, so all output can be seen
    // This will block stand-alone operations
//    while (!Serial);

    // Initialize Serial
    Serial.begin(115200);
    delay(100);

    // Initialize TFT
    _tft.begin();
    _tft.fillScreen(ILI9341_BLACK);
    _tft.setRotation(0);
    _tft.setTextWrap(false);

    tft_printfln(2, ILI9341_BLACK, ILI9341_GREEN, "LoRa Network Probe  ");

    // Initialize SD
    sd_inititalize();

    // Initialize Radio
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    while (!rf95.init())
    {
        tft_printfln(2, ILI9341_BLACK, ILI9341_RED, "LoRa init failed    ");
        while (1);
    }

    tft_printfln(2, ILI9341_BLACK, ILI9341_GREEN, "LoRa init OK        ");

    rf95_setFrequency(_freqIndex);
    rf95.setPromiscuous(true);

    // Update time for changing frequency
    _freqExpire = millis() + FREQ_TIME_MS;

    // Log the header for CSV output
    Serial.println(LOG_HEADER);
    if (_sdReady)
    {
        _logfile.println(LOG_HEADER);
        _logfile.flush();
    }
}

void loop()
{
    _rxRecvLen = sizeof(_rxBuffer);

    digitalWrite(LED, LOW);

    rf95.setModeRx();

    // Handle incoming packet if available
    if (rf95.recv(_rxBuffer, &_rxRecvLen))
    {
        char isAck[4] = { "   " };

        digitalWrite(LED, HIGH);

        _packetCounts[_freqIndex]++;

        if (rf95.headerFlags() & RH_FLAGS_ACK)
        {
            memcpy(isAck, "Ack\0", 3);
        }

        _rxBuffer[_rxRecvLen] = '\0';

        // Output to TFT
        tft_printfln(1, ILI9341_GREEN, ILI9341_BLACK, "Signal(RSSI)= %d (Freq: %d)", rf95.lastRssi(), (uint32_t)(_frequencies[_freqIndex] * 10));
        tft_printfln(1, ILI9341_GREEN, ILI9341_BLACK, " %d >> %d MsgId:%d Flags:%2x %s", rf95.headerFrom(), rf95.headerTo(), rf95.headerId(), rf95.headerFlags(), isAck);
        tft_printfln(1, ILI9341_GREEN, ILI9341_BLACK, " %d => %s", _rxRecvLen, _rxBuffer);

        // Prepare delimited output for log
        snprintf(_printBuffer, sizeof(_printBuffer), "%u,%u.%u,%04d,%d,%d,%d,%d,%02X,%s,%d,",
            millis(),
            (uint32_t)(_frequencies[_freqIndex] * 10) / 10,
            (uint32_t)(_frequencies[_freqIndex] * 10) % 10,
            _packetCounts[_freqIndex],
            rf95.lastRssi(),
            rf95.headerFrom(),
            rf95.headerTo(),
            rf95.headerId(),
            rf95.headerFlags(),
            isAck,
            _rxRecvLen
        );

        // Add bytes received as hex values
        for (int i = 0; i < _rxRecvLen; i++)
        {
            snprintf(_printBuffer, sizeof(_printBuffer), "%s %02X", _printBuffer, _rxBuffer[i]);
        }

        // Add bytes received as string - this is usually ugly and useless, but
        // it is here just in case. Maybe someone will send something in plaintext
        snprintf(_printBuffer, sizeof(_printBuffer), "%s,%s", _printBuffer, _rxBuffer);

        Serial.println(_printBuffer);

        if (_sdReady)
        {
            _logfile.println(_printBuffer);

            // Flushing adds overhead to the write time, but do it. FAT is VERY sensitive
            // to corruption.  If you don't flush regularly you WILL corrupt the File Allocation Table -
            // it is very difficult to avoid
            _logfile.flush();
        }
    }

    // Change frequency if it is time
    if (millis() > _freqExpire)
    {
        rf95.setModeIdle();

        _freqExpire = millis() + FREQ_TIME_MS;
        _freqIndex++;

        if (_freqIndex == FREQ_COUNT)
        {
            _freqIndex = 0;
        }

        rf95_setFrequency(_freqIndex);
    }
}

