/**
 * gbpxl (Game Boy Printer XL)
 * 
 * Interface between Game Boy Camera and ESC/POS compatible printers
 * 
 * Made by Vaclav Mach (www.xx0x.cz/gbpxl)
 * This fork of gbpxl is modified for outputting
 * multitone prints using an Epson TM-T88V/VI
 * 
 * USE WITH:
 * 
 * a) custom gbpxl board (github.com/xx0x/gbpxl)
 * b) Arduino Nano Every + TTL->RS232 converter
 * 
 * DIP SWITCH SETTINGS:
 *
 * 1: SCALE
 * OFF: 1x
 * ON:  2x
 * 
 * 2: CUT after each print (or when margin is 3)
 * OFF: No
 * ON:  Yes
 * 
 * 3: HI-DENSITY
 * OFF: Normal density light tone, best for normal receipt paper
 * ON:  High density light tone, best for adhesive label paper
 *
 * 4: MARGINS
 * OFF: No
 * ON:  Yes
 * 
 * For board/wiring details check the main project
 * 
 * Main project made by Vaclav Mach (www.xx0x.cz/gbpxl)
 * https://github.com/xx0x/gbpxl
 * 
 * Big thanks to Brian Khuu for decoding the GBP protocol
 * and making Arduino Game Boy Printer Emulator
 * https://github.com/mofosyne/arduino-gameboy-printer-emulator
 * 
 */

#include "gbp/gameboy_printer.cpp"
#include "test_image.h"

// PINS (Arduino Nano Every)
#define PIN_DIP_SCALE 8
#define PIN_DIP_CUT 7
#define PIN_DIP_DENSITY 6
#define PIN_DIP_MARGINS 5
#define PIN_LED 13

// IMAGE DEFINITION
#define TILE_PIXEL_WIDTH 8
#define TILE_PIXEL_HEIGHT 8
#define TILES_PER_LINE 20
#define IMG_WIDTH 160
#define IMG_HEIGHT 16
#define BYTES_PER_LINE IMG_WIDTH * IMG_HEIGHT / 8

// BUFFER OPTIONS
#define MAX_BUFFER_SIZE BYTES_PER_LINE * 5 // max lines per buffer send is 5 before timeout occurs
byte printBufferDark[MAX_BUFFER_SIZE]; // dark tone buffer stores data as raster bitmap, in rows
byte printBufferMid[MAX_BUFFER_SIZE]; // mid tone buffer stores data as raster bitmap, in rows
byte printBufferLight[MAX_BUFFER_SIZE]; // light tone buffer stores data as raster bitmap, in rows
unsigned int linesPerBuffer = 5; // Can print a 5 line buffer using 115200 baud rate
unsigned int totalBytes = 0; // bytes received from GameBoy

// SETTINGS
// Settings assume your printer is set to 115200 baud rate. TM-T88V can be configure to use
// this higher baud rate from mode selection menu (hold feed while powering on).
#define BAUD_RATE 115200

// OUTPUT OPTIONS
byte scale = 2;                       // DIP switch 1
byte cut = true ;                     // DIP switch 2
byte margins = true;                  // DIP switch 3
byte hidensity = false;               // DIP switch 4
unsigned int printMargin = 0;

// DEBUG STUFF
#define STARTUP_PRINTER_TEST 1

/**
 * Initial setup
 */
void setup()
{
    Serial.begin(BAUD_RATE);

    pinMode(PIN_DIP_SCALE, INPUT_PULLUP);
    pinMode(PIN_DIP_CUT, INPUT_PULLUP);
    pinMode(PIN_DIP_MARGINS, INPUT_PULLUP);
    pinMode(PIN_DIP_DENSITY, INPUT_PULLUP);

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

    gameboy_printer_setup();
    delay(100);

    updateDipSwitches();
    delay(100);

    epson_start();
    delay(100);

#if STARTUP_PRINTER_TEST
    Serial.println("Sending test print");
    printerTest();
#endif

    Serial.println("Device loaded, waiting for print data...");
}

/**
 * Loops and waits for the data and commands
 */
void loop()
{
    digitalWrite(PIN_LED, LOW);
    updateDipSwitches();
    bool schedulePrint = false;

    // Packet received from gameboy
    if (gbp_printer.packet_ready_flag)
    {
        // Indicate To The Byte Scanner and the Message Parser to scan for new packet
        gbp_rx_tx_byte_reset(&(gbp_printer.gbp_rx_tx_byte_buffer));
        gbp_parse_message_reset(&(gbp_printer.gbp_packet_parser));

        // Process this packet
        switch (gbp_printer.gbp_packet.command)
        {
            case GBP_COMMAND_INIT:
            { // This clears the printer status register (and buffers etc... in the real printer)
                gbp_printer.gbp_printer_status = {0};
                break;
            }
            case GBP_COMMAND_DATA:
            { // This is called when new data is received.
                receiveData();
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

        gbp_printer.packet_ready_flag = false; // Packet Processed
    }

    // Trigger Timeout and reset the printer if byte stopped being received.
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

    // If new data received or button pushed, start print
    if (schedulePrint)
    {
        receiveOptions();
        print();
    }
}

/**
 * Prints the data in the buffer
 */
void print()
{    
    // Send any stray lines in buffer before finishing print
    if (totalBytes != 0) {
        sendBuffers();
        finishPrint();
    }
    
    if (printMargin == 3) {
        if (cut) {
            epson_feed(7);
            epson_cut();
        }
    } else if (margins) {
        epson_feed(printMargin);
    }

    if (totalBytes != 0) {
        clearBuffer(totalBytes);
    }

    //Serial.println("Print finished");
    gbp_printer.uptime_til_pretend_print_finish_ms = 0;
    gbp_printer.gbp_printer_status.printer_busy = false;
    gbp_printer.gbp_printer_status.print_buffer_full = false;
}

/**
 * Begins the print (scale: 1x, 2x)
 * https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=99#gs_lparen_cl_fn112
 */
void printGsl(int currentColor)
{
    epson_center();
  
    // color data payload
    unsigned int payload = totalBytes + 10; // pL and pH specify the number of bytes following m as (pL + pH × 256).
    epson_write(29);                                // GS
    epson_write(40);                                // (
    epson_write(76);                                // L
    epson_write(payload & 0xFF);                    // pL
    epson_write(payload >> 8 & 0xFF);               // pH
    epson_write(48);                                // m
    epson_write(112);                               // fn
    epson_write(52);                                // a
    epson_write(scale);                             // bx
    epson_write(scale);                             // by
    epson_write(currentColor);                      // c
    epson_write(IMG_WIDTH & 0xFF);                  // xL
    epson_write(IMG_WIDTH >> 8 & 0xFF);             // xH
    epson_write((totalBytes / 20) & 0xFF);          // yL
    epson_write((totalBytes / 20) >> 8 & 0xFF);     // yH
    sendBuffer(currentColor);
}

/**
 * Sends the whole buffer to the printer
 */
void sendBuffer(int currentColor)
{
    if (currentColor == 49) {
        for (int i = 0; i < totalBytes; i++)
        {
            digitalWrite(PIN_LED, ((i % totalBytes/5) < 10) ? HIGH : LOW);
            epson_write(printBufferMid[i]);
        }
    } else if (currentColor == 50 || currentColor == 51) {
        for (int i = 0; i < totalBytes; i++)
        {
            digitalWrite(PIN_LED, ((i % totalBytes/5) == 0) ? HIGH : LOW);
            epson_write(printBufferLight[i]);
        }
    } else if (currentColor == 52) {
        for (int i = 0; i < totalBytes; i++)
        {
            digitalWrite(PIN_LED, ((i % totalBytes/5) == 0) ? HIGH : LOW);
            epson_write(printBufferDark[i]);
        }
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
 * Clears the print buffer
 */
void clearBuffer(int currentBufferSize)
{
    totalBytes = 0;
    printMargin = 0;
    memset(printBufferDark, 0, currentBufferSize);
    memset(printBufferMid, 0, currentBufferSize);
    memset(printBufferLight, 0, currentBufferSize);
    //Serial.println("Print buffer cleared");
}

/** 
 * Recieves the data from the Game Boy and transforms it for the printer
 * 2-bit depth 8*8 tiles -> 2-bit depth in line pixels
 */
void receiveData()
{
    //Serial.println("Recieving data...");
    byte lines = gbp_printer.gbp_packet.data_length / 16 / 20; //2
    if (lines != 0) {
        for (int line = 0; line < lines; line++)
        {
            digitalWrite(PIN_LED, HIGH);
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
                        if (val == 3)
                        {
                            printBufferDark[index] = printBufferDark[index] | (1 << (7 - i));
                            printBufferMid[index] = printBufferMid[index] | (1 << (7 - i));
                            printBufferLight[index] = printBufferLight[index] | (1 << (7 - i));
                        } else if (val == 2)
                        {
                            printBufferMid[index] = printBufferMid[index] | (1 << (7 - i));
                        } else if (val == 1)
                        {
                            if(hidensity) {
                                printBufferDark[index] = printBufferDark[index] | (1 << (7 - i));
                            }
                            printBufferLight[index] = printBufferLight[index] | (1 << (7 - i));
                        }
                    }
                }
            }
            totalBytes += TILES_PER_LINE * 8;
            digitalWrite(PIN_LED, LOW);
        }

        if (totalBytes == (linesPerBuffer * BYTES_PER_LINE)) {
            sendBuffers();
            finishPrint();
            clearBuffer(totalBytes);
        }
    }
}

/**
 * 
 */
void sendBuffers()
{
    //Serial.println("Printing lines buffer");
    // print lines as they are received. ATMEGA4809 doesn't have enough memory to store full image data
    
    for (int color = 49; color < 53; color++) {
        printGsl(color);
    }
}

/** 
 * Recieves print margin data from the Game Boy
 */
void receiveOptions()
{
    //Serial.println("Recieving options data...");
    printMargin = (byte)(gbp_printer.gbp_packet.data_ptr[1] & 0x0F);
}

/**
 * Updates the setting based on DIP switches
 */
void updateDipSwitches()
{
    scale = !digitalRead(PIN_DIP_SCALE) ? 2 : 1;
    cut = !digitalRead(PIN_DIP_CUT);
    margins = !digitalRead(PIN_DIP_MARGINS);
    hidensity = !digitalRead(PIN_DIP_DENSITY);
}

/**
 * Prints a test
 */
void printerTest()
{
    epson_center();
    Serial1.write(testImage, 1348);
    epson_feed(2);
    epson_linespacing(30);
    epson_println("gbpxl ready");
    epson_feed(1);
    epson_print("baud rate: ");
    epson_println(BAUD_RATE);
    epson_print("scale: ");
    epson_println(scale);
    epson_print("cut: ");
    epson_println(cut ? "ON" : "OFF");
    epson_print("margins: ");
    epson_println(margins ? "ON" : "OFF");
    epson_print("hidensity: ");
    epson_println(hidensity ? "HIGH" : "NORMAL");
    epson_feed(6);
    epson_cut();

}

/**
 * Starts the printer
 * https://reference.epson-biz.com/modules/ref_escpos/index.php?content_id=192#esc_atsign
 */
void epson_start()
{
    if (Serial1)
    {
        Serial1.end();
        delay(200);
    }
    Serial1.begin(BAUD_RATE);

    // reset printer
    Serial1.write(27); // ESC
    Serial1.write(64); // @
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
 * Espon line spacing
 */
void epson_linespacing(byte spacing)
{
    epson_write(27); // ESC
    epson_write(51); // 3
    epson_write(spacing);
}

/**
 * Feeds a number of lines
 */
void epson_feed(byte lines)
{
    Serial1.write(0x1B);
    Serial1.write(0x32);
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
    Serial1.write(49);
}

/**
 * Sends a single byte
 */
void epson_write(byte c)
{
    Serial1.write(c);
}

/**
 * Prints a text string
 */
size_t epson_println(const char *str)
{
    return Serial1.println(str);
}

/**
 * Prints a number
 */
size_t epson_println(unsigned long num)
{
    return Serial1.println(num);
}

/**
 * Prints a text string
 */
size_t epson_print(const char *str)
{
    return Serial1.print(str);
}

/**
 * Prints a number
 */
size_t epson_print(unsigned long num)
{
    return Serial1.print(num);
}
