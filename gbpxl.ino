/**
 * gbpxl (Game Boy Printer XL)
 * 
 * For use with Game Boy Camera and EPSON TM-T88 printer series
 * (and possibly other ESC/POS compatible printers)
 * 
 * Made by Vaclav Mach (www.xx0x.cz/gbpxl)
 * 
 * USE WITH:
 * 
 * a) custom gbpxl board (github.com/xx0x/gbpxl)
 * b) Arduino Nano Every + TTL->RS232 converter
 * 
 * 
 * DIP SWITCH SETTINGS:
 *
 * Scale     1x            2x            3x                
 *           DIP 1 = OFF   DIP 1 = ON    DIP 1 = (doesn't care)                     
 *           DIP 2 = OFF   DIP 2 = OFF   DIP 2 = ON                      
 * 
 * Cut       OFF           ON                   
 * paper     DIP 3 = OFF   DIP 3 = ON                       
 * 
 * Printer   9600          38400
 * baud      DIP 4 = OFF   DIP 4 = ON
 * rate 
 * 
 * 
 * HOW TO WIRE THE BOARD:
 * 
 * Game Boy Link Connector
 * (cable at the end which plugs into the Game Boy)
 *  ___________
 * |  6  4  2  |
 *  \_5__3__1_/ 
 * 
 * 1 = VCC (can't be used - doesn't provide enough "juice" for the board)
 * 2 = SO (serial output)
 * 3 = SI (serial input)
 * 4 = SC (serial clock)
 * 6 = GND
 * 
 * RJ-12 connector (power from printer's "DK" port)
 * (cable at the end which plugs into the printer) 
 *  ___________
 * |           |
 * |___________|
 *  | | | | | |
 *  1 2 3 4 5 6 
 *
 * 1 = GND 
 * 3 = VIN (24 V)
 * The gbpxl board supports 10 - 30 V as VIN voltage.
 * 
 * 
 * IF USING NANO EVERY:
 * 
 * SO = Arduino pin 4
 * SI = Arduino pin 3
 * SC = Arduino pin 2
 * 
 * Don't connect RJ-12 to Arduino Nano Every, it supports 21 V max!
 * 
 * Big thanks to Brian Khuu for decoding the GBP protocol
 * and making Arduino Game Boy Printer Emulator
 * https://github.com/mofosyne/arduino-gameboy-printer-emulator
 * 
 */

#include "gbp/gameboy_printer.cpp"
#include "test_image.h"

// PINS (Arduino Nano Every)
#define PIN_DIP_A 5
#define PIN_DIP_B 6
#define PIN_DIP_CUT 7
#define PIN_DIP_RATE 8
#define PIN_BTN 9
#define PIN_LED 13

// IMAGE DEFINITION
#define TILE_PIXEL_WIDTH 8
#define TILE_PIXEL_HEIGHT 8
#define TILES_PER_LINE 20
#define IMG_WIDTH 160
#define IMG_HEIGHT 144
#define EPSON_BYTES_PER_LINE 20

// BUFFER OPTIONS
#define BUFFER_SIZE 2880
byte printBuffer[BUFFER_SIZE];
int totalBytes = 0;
#define LINES_AT_ONCE_3X 48 // if your printer wont print the whole image, try values: 36, 24, 16

// SETTINGS
#define MAX_SCALE 3
#define DEFAULT_BAUD_RATE 9600
#define FAST_BAUD_RATE 38400
#define PARTIAL_CUT 66
#define FULL_CUT 65
#define PC_BAUD_RATE 115200

byte scale = 1;                             // DIP switches 1,2
byte cut = false;                           // DIP switch 3
unsigned long baudRate = DEFAULT_BAUD_RATE; // DIP switch 4
byte info = false;                          // prints text before and after print, specified by functions infoBefore, infoAfter
byte cutMode = PARTIAL_CUT;                 // full cut is not supported by TM88

// DEBUG STUFF
#define COPY_TEST_IMAGE_TO_BUFFER 1
#define SEND_TO_PC_AFTER_PRINTING 0

/**
 * Printed if info = true
 */
void infoBefore()
{
    epson_println("gbpxl 0.1");
}

/**
 * Printed if info = true
 */
void infoAfter()
{
    epson_println("print ok");
}

/**
 * Initial setup
 */
void setup()
{
    Serial.begin(PC_BAUD_RATE);

    pinMode(PIN_DIP_A, INPUT_PULLUP);
    pinMode(PIN_DIP_B, INPUT_PULLUP);
    pinMode(PIN_DIP_CUT, INPUT_PULLUP);
    pinMode(PIN_DIP_RATE, INPUT_PULLUP);
    pinMode(PIN_BTN, INPUT_PULLUP);
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    gameboy_printer_setup();
    delay(100);

    updateDipSwitches();
    delay(100);

    epson_start();
    delay(100);
    epson_feed(5);

    Serial.println("Device loaded, waiting for print data...");
    digitalWrite(PIN_LED, HIGH);

#if COPY_TEST_IMAGE_TO_BUFFER
    copyTestImageToBuffer();
#endif
}

/**
 * Loops and waits for the data and commands
 */
void loop()
{
    updateDipSwitches();
    bool schedulePrint = false;

    // Packet received from gameboy
    if (gbp_printer.packet_ready_flag)
    {
        digitalWrite(PIN_LED, LOW);

        // Indicate To The Byte Scanner and the Message Parser to scan for new packet
        gbp_rx_tx_byte_reset(&(gbp_printer.gbp_rx_tx_byte_buffer));
        gbp_parse_message_reset(&(gbp_printer.gbp_packet_parser));

        // Process this packet
        switch (gbp_printer.gbp_packet.command)
        {
        case GBP_COMMAND_INIT:
        { // This clears the printer status register (and buffers etc... in the real printer)
            gbp_printer.gbp_printer_status = {0};
            clearBuffer();
            break;
        }
        case GBP_COMMAND_DATA:
        { // This is called when new data is recieved.
            recieveData();
            break;
        }
        case GBP_COMMAND_PRINT:
        { // This would usually indicate to the GBP to start printing.
            schedulePrint = true;
            break;
        }
        case GBP_COMMAND_INQUIRY:
        { // This is usally called multiple times to check if ready to print or has finished printing.

            break;
        }
        }

        digitalWrite(PIN_LED, HIGH);
        gbp_printer.packet_ready_flag = false; // Packet Processed
    }

    // Trigger Timeout and reset the printer if byte stopped being recieved.
    if ((gbp_printer.gbp_rx_tx_byte_buffer.syncronised))
    {
        if ((0 != gbp_printer.uptime_til_timeout_ms) && (millis() > gbp_printer.uptime_til_timeout_ms))
        { // reset printer byte and packet processor
            Serial.println("# ERROR: Timed Out");
            gbp_rx_tx_byte_reset(&(gbp_printer.gbp_rx_tx_byte_buffer));
            gbp_parse_message_reset(&(gbp_printer.gbp_packet_parser));
        }
    }
    else
    { // During scanning phase timeout is not required.
        gbp_printer.uptime_til_timeout_ms = 0;
    }

    if (schedulePrint || !digitalRead(PIN_BTN))
    {
        print();
    }
}

/**
 * Prints the data in the buffer
 */
void print()
{
    if (totalBytes == 0)
    {
        return;
    }

    epson_center();

    if (info)
    {
        if (!cut)
        {
            epson_feed(2);
        }
        infoBefore();
        epson_feed(2);
    }

    Serial.println("Data recieved, begin print!");

    if (scale > 2)
    {
        fly_print();
    }
    else
    {
        beginPrint();
        sendBuffer();
        finishPrint();
    }

    epson_feed(2);

    if (info)
    {
        infoAfter();
        epson_feed(2);
    }
    if (cut)
    {
        if (!info)
        {
            epson_feed(1);
        }
        epson_cut();
    }

    Serial.println("Print finished");
    gbp_printer.uptime_til_pretend_print_finish_ms = 0;
    gbp_printer.gbp_printer_status.printer_busy = false;
    digitalWrite(PIN_LED, HIGH);

#if SEND_TO_PC_AFTER_PRINTING
    sendBufferToPc();
#endif
}

/**
 * Begins the print (scale: 1x, 2x)
 * https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=99#gs_lparen_cl_fn112
 */
void beginPrint()
{
    unsigned int payload = BUFFER_SIZE + 10; // pL and pH specify the number of bytes following m as (pL + pH × 256).
    epson_write(29);                         // GS
    epson_write(40);                         // (
    epson_write(76);                         // L
    epson_write(payload & 0xFF);             // pL
    epson_write(payload >> 8 & 0xFF);        // pH
    epson_write(48);                         // m
    epson_write(112);                        // fn
    epson_write(48);                         // a
    epson_write(scale);                      // bx
    epson_write(scale);                      // by
    epson_write(49);                         // c
    epson_write(IMG_WIDTH & 0xFF);           // xL
    epson_write(IMG_WIDTH >> 8 & 0xFF);      // xH
    epson_write(IMG_HEIGHT & 0xFF);          // yL
    epson_write(IMG_HEIGHT >> 8 & 0xFF);     // yH
}

/**
 * Sends the whole buffer to the printer
 */
void sendBuffer()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        digitalWrite(PIN_LED, ((i % 60) < 10) ? HIGH : LOW);
        epson_write(printBuffer[i]);
    }
}

/** 
 * Sends a command to print all data in printer's buffer
 *https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=98
 */
void finishPrint()
{
    epson_write(29); // GS
    epson_write(40); // (
    epson_write(76); // L
    epson_write(2);  // pL
    epson_write(0);  // pH
    epson_write(48); // m
    epson_write(50); // fn
}

/**
 * Starts "on-the-fly" print batch
 */
void fly_beginPrint()
{
    unsigned int w = IMG_WIDTH * scale;
    unsigned int h = LINES_AT_ONCE_3X * scale;
    unsigned int payload = w / 8 * h + 10;
    epson_write(29);                  // GS
    epson_write(40);                  // (
    epson_write(76);                  // L
    epson_write(payload & 0xFF);      // pL
    epson_write(payload >> 8 & 0xFF); // pH
    epson_write(48);                  // m
    epson_write(112);                 // fn
    epson_write(48);                  // a
    epson_write(1);                   // bx
    epson_write(1);                   // by
    epson_write(49);                  // c
    epson_write(w & 0xFF);            // xL
    epson_write(w >> 8 & 0xFF);       // xH
    epson_write(h);                   // yL
    epson_write(0);                   // yH
}

/**
 * Finishes "on-the-fly" print batch
 */
void fly_endPrint()
{
    finishPrint();
}

/*
 * Provides "on-the-fly" print
 * since TM-T88 doesn't have big enough buffer for 3x scaled image, it must be sent in batches
 */
void fly_print()
{
    Serial.println("Begin fly_print");
    for (byte base = 0; base < IMG_HEIGHT; base += LINES_AT_ONCE_3X)
    {
        fly_beginPrint();

        for (byte l = 0; l < LINES_AT_ONCE_3X; l++)
        {
            byte line = base + l;
            for (byte y = 0; y < scale; y++) // copy line 3x (scale = 3)
            {
                for (byte x = 0; x < EPSON_BYTES_PER_LINE; x++) // for each pixel in line
                {
                    int i = line * EPSON_BYTES_PER_LINE + x;
                    digitalWrite(PIN_LED, ((i % 60) < 10) ? HIGH : LOW);
                    uint32_t out = bitscale(printBuffer[i], scale);
                    epson_write(out >> 16 & 0xFF);
                    epson_write(out >> 8 & 0xFF);
                    epson_write(out & 0xFF);
                }
            }
        }

        fly_endPrint();
    }
}

/**
 * Clears the print buffer
 */
void clearBuffer()
{
    totalBytes = 0;
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        printBuffer[i] = 0;
    }
}

/** 
 * Recieves the data from the Game Boy and transforms it for the printer
 * 2-bit depth 8*8 tiles -> 1-bit depth in line pixels
 */
void recieveData()
{
    Serial.println("Recieving data...");
    byte lines = gbp_printer.gbp_packet.data_length / 16 / 20; //2
    for (int line = 0; line < lines; line++)
    {
        for (byte tile = 0; tile < TILES_PER_LINE; tile++)
        {
            for (byte j = 0; j < TILE_PIXEL_HEIGHT; j++)
            {
                for (byte i = 0; i < TILE_PIXEL_WIDTH; i++)
                {
                    short offset = tile * 8 + j + 8 * TILES_PER_LINE * line;
                    byte hiBit = (byte)((gbp_printer.gbp_packet.data_ptr[offset * 2 + 1] >> (7 - i)) & 1);
                    byte loBit = (byte)((gbp_printer.gbp_packet.data_ptr[offset * 2] >> (7 - i)) & 1);
                    byte val = (byte)((hiBit << 1) | loBit); // 0-3
                    int index = (tile + TILES_PER_LINE * j) + totalBytes;
                    if (val > 1)
                    {
                        printBuffer[index] = printBuffer[index] | (1 << (7 - i));
                    }
                }
            }
        }
        totalBytes += TILES_PER_LINE * 8;
    }
}

/**
 * Updates the setting based on DIP switches
 */
void updateDipSwitches()
{
    bool a = !digitalRead(PIN_DIP_A);
    bool b = !digitalRead(PIN_DIP_B);
    scale = ((b << 1) | a) + 1;
    if (scale > MAX_SCALE)
    {
        scale = MAX_SCALE;
    }
    cut = !digitalRead(PIN_DIP_CUT);
    unsigned long newBaudRate = !digitalRead(PIN_DIP_RATE) ? FAST_BAUD_RATE : DEFAULT_BAUD_RATE;
    if (newBaudRate != baudRate)
    {
        baudRate = newBaudRate;
        epson_start();
    }
}

/**
 * Starts the printer
 */
void epson_start()
{
    if (Serial1)
    {
        Serial1.end();
        delay(200);
    }
    Serial1.begin(baudRate);
}

/**
 * Justifies content center
 * https://www.epson-biz.com/modules/ref_escpos/index.php?content_id=58
 */
void epson_center()
{
    Serial1.write(0x1B);
    Serial1.write(0x61);
    Serial1.write(1);
}

/**
 * Feeds a number of lines
 */
void epson_feed(byte lines)
{
    Serial1.write(0x1B);
    Serial1.write(0x64);
    Serial1.write(lines);
}

/**
 * Sends a CUT command
 * https://www.epson-biz.com/modules/ref_escpos/index.php?content_id=87
 */
void epson_cut()
{
    Serial1.write(0x1D);
    Serial1.write(0x56);
    Serial1.write(cutMode);
    Serial1.write(0xA);
}

/**
 * Sends a single byte
 */
void epson_write(byte c)
{
    Serial1.write(c);
}

/**
 * Sends a string
 */
size_t epson_println(const char *str)
{
    return Serial1.println(str);
}

/**
 * Copies each bit in source byte to the destination uint32 "by" times
 * by = 3 in this program
 * example:
 * src: 000000000000000010010011
 * dst: 111000000111000000111111
 */
uint32_t bitscale(uint8_t src, uint8_t by)
{
    uint32_t dst;
    for (int8_t i = 7; i >= 0; i--)
    {
        for (uint8_t j = 0; j < by; j++)
        {
            dst = dst << 1 | ((src & 1 << i) >> i);
        }
    }
    return dst;
}

/**
 * Sends the buffer contents to the PC via Serial
 */
void sendBufferToPc()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        if (i % 16 == 0)
        {
            Serial.println("");
        }
        digitalWrite(PIN_LED, ((i % 60) < 10) ? HIGH : LOW);
        Serial.print("0x");
        if (printBuffer[i] < 0x10)
        {
            Serial.print("0");
        }
        Serial.print(printBuffer[i], HEX);
        Serial.print(", ");
    }
}

/**
 * Copies test_image to buffer at the start
 */
void copyTestImageToBuffer()
{
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        printBuffer[i] = testImage[i];
    }
    totalBytes = BUFFER_SIZE;
    Serial.println("Test image copied to buffer");
}