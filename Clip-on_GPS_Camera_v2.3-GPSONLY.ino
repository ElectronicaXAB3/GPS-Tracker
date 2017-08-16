/***********************************************************************

GPS Module only version 1: 08/15/2017
--> All ArduCAM related parts are removed from the program.

************************************************************************/
// Libraries / Header files
#include <Wire.h> // I2C connection
#include <SPI.h> // SPI connection
#include <SoftwareSerial.h>
#include <SdFat.h> // SD card
#include <Adafruit_GPS.h>

// Set the pins used (#define preprocessor)
#define GPS_RX 2 // GPS RX SoftwareSerial Pin 2
#define GPS_TX 3 // GPS TX SoftwareSerial Pin 3
#define SD_CS 4 // SD Card uses Pin 4 for Adalogger
#define ledPinError A0 // Error LED pin
#define LED_Switch A1 // Digital LED for SW 1 FN 1 (GPS pause/standby)
#define LED_GPS A2 // On status for GPS
#define switch1 9 // GPS Switch physical PIN 9
// Default SPI connections
// MOSI - PIN 11
// MISO - PIN 12
// SCK - PIN 13
// SDA - PIN A4
// SCL - PIN A5

//* Other Definitions *//
#define LOG_FIXONLY false // set to true to only log to SD when GPS has a fix, for debugging, keep it false
#define GPSECHO  true // Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console

//* variables / Constants  *//
char sw1 = 0; // GPS Switch code
bool usingInterrupt = false; 
bool StandbyMode = true;
bool RecordMode = false;
bool powr = 0;
uint8_t i = 0; // counter used in generating Filename
char filename[15]; // Character sequences (array) for file name: 15 elements
File logfile;

//* Alias *//
SoftwareSerial mySerial(GPS_TX, GPS_RX); // GPS SoftwareSerials
Adafruit_GPS GPS(&mySerial); // Constructor when using SoftwareSerials
SdFat sd; // file system alias for SdFat

//* Function Declaration *//
//void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy
void flash (uint8_t pin, char times = 1); // Speficy defaults to be 500ms and 0

//*************************************************//
//***************** Function **********************//
//*************************************************//

//* Blinking *//
void flash (uint8_t pin, char times) { // interval 500 millisec
  for (uint8_t i = 0 ; i < times ; i++) { // increment happens after the code block is executed
    delay (500);
    digitalWrite(pin, HIGH);
    delay (500);
    digitalWrite(pin, LOW); // delay before so next command can continue right after
  }
}

//* Error Signal *//
void error() { // blink out an error code, return if serial command = 1
  Serial.print(F("** ERROR **"));
  while (Serial.read() != 1) { // no incoming data = -1
    flash(ledPinError, 1);
  }
  Serial.print(F("** Return **"));
  return;
}

//* Signal Handler *//
SIGNAL(TIMER0_COMPA_vect) {
  //Serial.print(F("Signal Handler..."));
  char c = GPS.read(); // Interrupt is called once a millisecond, looks for any new GPS data, and stores it
  if (GPSECHO && c) {
#ifdef UDR0
    UDR0 = c; // writing direct to UDR0 is much much faster than //Serial.print but only one character can be written at a time.
#endif
  }
}

//* GPS Interrupt(#0) Pin *//
void useInterrupt(boolean v) {
  //Serial.println(F("Use interrupt..."));
  if (v) {
    OCR0A = 0xAF; // Timer0 is already used for millis() - we'll just interrupt somewhere
    TIMSK0 |= _BV(OCIE0A); // in the middle and call the "Compare A" function above
    usingInterrupt = true;
  } else {
    TIMSK0 &= ~_BV(OCIE0A); // do not call the interrupt function COMPA anymore
    usingInterrupt = false;
  }
}

//* Timer Switch *//
char timerswitch(uint8_t nswitch) {
  bool sw_m_1 = digitalRead(nswitch);
  if (sw_m_1) {
    long SW_Ci = millis();  // Switch counter initiated
    long SW_C = 0; // Reset SW_C
    while (SW_C <= 10000) { // limit to 10 seconds
      sw_m_1 = digitalRead(nswitch);
      SW_C = millis() - SW_Ci;  // Switch counter finished
      if (!sw_m_1) { // upon botton release
        if (SW_C >= 1500) { // fn 2 1.5 sec: two blinks
          flash(LED_Switch, 2);
          Serial.println(F("2 sw1"));
          return 2;
        }
        else if (SW_C >= 500) { // fn 1 0.5 sec: one blink
          flash(LED_Switch, 1);
          Serial.println(F("1 sw1"));
          return 1;
        }
        else { // period too short
          Serial.println(F("0 sw1"));
          return 0;
        }
      }
    }
    // timeout
    flash(LED_Switch, 1);
  }
  return 0;
}

//* Create File in SD card *//
File createFile(File & logfile, char * prefix) { // make a pointer for "prefix"
  //* SD Initialization //
  if (!sd.begin(SD_CS)) { // see if the card is present and can be initialized:
    Serial.println(F("Card init. failed!"));
    error();
  }
  Serial.println(F("Card initialized"));
  //* Generate File Name //
  Serial.println(F("Generating file name..."));
  char filename[10]; // Character sequences (array) for file name: 15 elements
  strcpy(filename, prefix); // Copy string since filename = NULL, prefix goes to the first positions
  strcat(filename, "000.TXT"); // Concatenate strings = join two strings together
  //Serial.println (filename); // test filename before edit
  for (uint8_t i = 0; i < 1000; i++) { // Changing file name to the subsequent numbers
    filename[3] = '0' + i / 100; // #element 6 = 7th letter i.e. first 0
    filename[4] = '0' + (i % 100)/10; // #element 7 = 8th letter i.e. second 0 (% = Modulo)
    filename[5] = '0' + (i % 100)/10 % 10; // #element 7 = 8th letter i.e. second 0 (% = Modulo)
    // create if does not exist, do not open existing, write, sync after write
    if (! sd.exists(filename)) {
      break;
    }
  }

  //* Generate File Writable //
  logfile = sd.open(filename, FILE_WRITE);
  if (logfile) { // File open. Ready to write.
    Serial.print("Writing to ");
    Serial.println(filename);
  } else { // File did not open
    Serial.print(F("Couldnt create "));
    Serial.println(filename);
    error();
  }
  return;
}


//* Log GPS NMEA sentences *//
void logGPSNMEA (File & logfile) {
  // ---- > GPS record function
  if (GPS.newNMEAreceived()) { // if a sentence is received, we can check the checksum, parse it...
    if (!GPS.parse(GPS.lastNMEA())) {  // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
    }
    Serial.write(GPS.lastNMEA());
    Serial.println();
    Serial.println(F("Parsed"));
    if (LOG_FIXONLY && !GPS.fix) {
      Serial.print(F("No Fix"));
      return;
    }
    char *stringptr = GPS.lastNMEA();
    uint8_t stringsize = strlen(stringptr);
    delayMicroseconds(15);
    if (stringsize != logfile.write((uint8_t *)stringptr, stringsize)) { //write the string to the SD file
      error();
    }
    if (strstr(stringptr, "RMC")) {
      logfile.flush();
    }
    Serial.println(F("Log"));
  }
  // ----------> Function ends
}

//*************************************************//
//****************** Setup ************************//
//*************************************************//

void setup() {
  Wire.begin();
  Serial.begin(19200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println(F("Setting up..."));    // Setup Starts
  pinMode(SD_CS, OUTPUT);// make sure the default SD_CS pin is set to output, even if you don't use it.
  pinMode(switch1, INPUT); // set GPS button input
  pinMode(LED_Switch, OUTPUT); // LED switch
  pinMode(LED_GPS, OUTPUT); // LED GPS
  pinMode(ledPinError, OUTPUT); // LED error

  // GPS is powered on PIN 5 (just to keep circuit consistent with the main project)
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH); // GPS power: 20mA current draw (PIN has 40mA max)

  digitalWrite(ledPinError, HIGH); // Setting up

  //--------------------//
  //* Setup SD Module *//
  //--------------------//

  Serial.println(F("SD starts...")); // Setup GPS
  while (!sd.begin(SD_CS)) {
    Serial.println(F("SD Card Error"));
    error();
  }
  Serial.println(F("SD Card detected."));

  //--------------------//
  //* Setup GPS Module *//
  //--------------------//

  Serial.println(F("GPS starts..."));    // Setup GPS
  GPS.begin(9600); // GPS Baud rate
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);
  //GPS.sendCommand(PMTK_API_SET_FIX_CTL_1HZ);
  //useInterrupt(true);
  Serial.print(F("Initial GPS Standby..."));
  Serial.println(GPS.standby()); // 1 = yes, 0 = no
  Serial.println(F("Setup completed"));
  digitalWrite(ledPinError, LOW);
}

//*************************************************//
//******************* Loop ************************//
//*************************************************//

void loop() {

  //*********** Switch Operators ***********//
  sw1 = timerswitch(switch1); // GPS timer switch

  //*********** GPS Operator ***********//
  switch (sw1) {
    case 0: // No switch
      sw1 = 0;
      if (!RecordMode) {
        break;
      }
      else {
        //* Write GPS sentence to the file //
        if (!StandbyMode) { // if not standby
          GPS.read();
          logGPSNMEA(logfile);
          break;
        }
        else {
          Serial.print(F("*"));
          digitalWrite(LED_GPS, powr);
          powr = !powr;
          delay(250);
          break;
        }
      }
      
    case 1: // GPS Switch 1
      sw1 = 0;
      if (RecordMode) {
        Serial.println(F("GPS fn 1"));
        if  (!StandbyMode) {
          StandbyMode = true;
          GPS.standby(); // Standby GPS
          delay (100);
          Serial.println(F("GPS standby"));
          break;
        }
        else {
          StandbyMode = false;
          GPS.wakeup(); // Wake up GPS
          delay (100);
          Serial.println(F("GPS awake"));
          break;
        }
      }
      else {break;}

    case 2: // GPS Switch 2
      sw1 = 0;
      Serial.println(F("GPS fn 2"));
      if (RecordMode) { // If not standby/running: Stop logging and clear flash
        RecordMode = false;
        logfile.close();
        GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_OFF); // turn off GPS output
        Serial.println(F("Stop Logging"));
        //* Erase Flash memory and Standby //
        GPS.sendCommand(PMTK_LOCUS_ERASE_FLASH);
        Serial.println(F("GPS Flash cleared"));
        GPS.standby(); // Standby GPS
        //* Close logfile on SD //
        digitalWrite(LED_GPS, LOW); // GPS Status LED OFF
        break;
      }
      else { // if standby/not running: Wake up, start recording
        RecordMode = true;
        StandbyMode = false;
        //* Verifying GPS Status //
        GPS.sendCommand(""); // another beep to wake up before logging
        GPS.waitForSentence("$PMTK011,MTKGPS*08"); // wait till identifier instead of awake
        //* Generate File Writable //
        Serial.println("Passed");
        createFile(logfile, "GPS"); // Create file for new log
        Serial.println(F("Start Logging..."));
        digitalWrite(LED_GPS, HIGH); // GPS Status LED ON
        break;
      }
  }
}


//*************************************************//
//**************** Debug Notes ********************//
//*************************************************//

/* 08/15/2017 (1) 
 *  In some cases, GPS.read() will print out the following
 *  $PGACK,103*40  -->  getting ready for a cold-start (flushing all memory of previous fixes)
 *  $PGACK,105*46  -->  GPS initialization routine is done
 *  $PMTK011,MTKGPS*08  -->  MTKGPS is a general chip identifier
 *  $PMTK010,001*2E  -->  configuration is completed
 *  $PMTK010,002*2D  -->  module is awake and waiting for input on the Serial connection
 *  All of these indicate startup signals
 * 
 * 08/15/2017 (2)
 *  SD is a RAM hungry module. Following some recommendations on forums, 
 *  I decided to use SdFat.h instead of SD.h Although functions are more 
 *  limited, it could save SRAM. However, I saw no difference in dynamic 
 *  memory i.e. 82% before and after.
 *  
 * 08/15/2017 (3)
 *  Interrupt is basically you are trying to do something while in the 
 *  middle of other functions in this case main loop(). useInterrupt allows
 *  the GPS to Serial.print the NMEA sentence (lastNMEA) even while loop is
 *  running. Thus, this is mainly used for debugging to confirm what GPS is
 *  reading. If not used, SIGNAL and useInterrupt can be deleted.
 * 
 * 08/15/2017 (4)
 *  I'm trying more to break out the codes in the main loop into private
 *  functions. The two main reasons are that I'm trying to organize the code 
 *  and trying to save memories esp. SRAM. Since when the function closes,
 *  the memory allocated for stack (local variables) will be 100% reclaimed 
 *  as free space; thus, freeing up space and prevent RAM crashes. 
 *  The new functions include: 
 *  * void flash (uint8_t pin, char times) --> replace manual LED blinking
 *  * File createFile(File & logfile, char *prefix) --> generate file
 *  * void logGPSNMEA (File & logfile) --> log GPS to specified file
 *  * char timerswitch(uint8_t nswitch) --> switch operator by time pressed
 *  * void error() --> error signal
 *  
 *  Some learning notes here:
 *  
 *  - default values for function parameters can be set at function declaration 
 *    (e.g. void flash (uint8_t pin, char times = 0);)
 *  
 *  - uint8_t is used to specify that int is defined to be 8 bit which means 
 *    uint16_t and uint32_t are 16 and 32 bits accordingly.
 *  
 *  - instead of true header name, you can specify alias e.g. SdFat sd; Although 
 *    it might be better to use "namespace" or "typedef" idk, have to read more.
 *    
 *  - SRAM allocation contains (1) Global and Static variables (2) Local variables
 *    (stack) (3) and Dynamic Allocations in between. In creating functions, I 
 *    can use the reclaimable stack space. This could be made better by de-allocating
 *    dynamic space. But, remember to watch out for "fragmented heap" which could
 *    prevent us from really claiming back the dynamic space.
 *    
 *  - I actually struggle a lot with the function that returns file type. The
 *    solution seems to be that instead of carrying the file name to be opened
 *    in the function, I can just reference the logfile over to the function 
 *    (using & operator) i.e. declare file in setup, refer in and return from 
 *    createFile, then refer in logGPSNMEA. Now everything is consistent using
 *    the same file register.
 *    
 *  - Another is when I tried to create file name by passing along the string 
 *    from input parameter when using function in the main loop. The problem 
 *    was solved simply by using the pointer (*) in the function parameter
 *    i.e. pointer points to the input when recalled in the main loop.
 *    
 *  - In C/C++, array or strings work very similarly i.e. they are not real
 *    array or string but act as pointer to a specific memory where the values
 *    get stored. Thus, they are just continuous lines of data with "0" at 
 *    the end in the case of string. Meaning: *(Array+7) = Array[7] since 
 *    pointer Array (without specified position []) is just a pointer to 
 *    the start of the array/chunk of memory, so +7 means the next 7 positions.
 *    
 *  - Now, the most confusing of all is the GPS library. I initally use:
 *    GPS.waitForSentence (PMTK_AWAKE) which is defined in the header, but
 *    that sentence was never cleared so I had to print out the NMEA to see 
 *    what sentence actually came up. It appears that the response defined 
 *    as PMTK_AWAKE rarely appear. Therefore, I changed the sentence to:    
 *    GPS.waitForSentence("$PMTK011,MTKGPS*08") which is basically the chip 
 *    identifier which always come up when GPS starts/restarts. The standby 
 *    and wakeup works now because of this. PS I did not use .wakeup()since
 *    the header uses waitForSentence (PMTK_AWAKE) in the func, so I just use 
 *    .sendCommand("") since any bit sent to GPS will wake it up.
 *    
 *    I also found out that .lastNMEA() actually returns the char in
 *    .read() meaning I always need to read everytime before checking last 
 *    NMEA or use .newNMEAreceived().
 *  
 *  - Sometimes, the complier/controller just act weird. Try using the sample 
 *    sketches e.g. one of the cases, the GPS only showed response noted in (1)
 *    even when loading in echo example. Somehow, I re-downloaded the library 
 *    and switch between PIN 5 power source to main line, then things seem to work.
 *    
 *  - Also, don't forget to put break after each switch case...just a bug sometimes
 *    
 * 08/15/2017 Upload Note
 * 
 * Sketch uses 20984 bytes (65%) of program storage space. Maximum is 32256 bytes.
 * Global variables use 1693 bytes (82%) of dynamic memory, leaving 355 bytes for local variables. Maximum is 2048 bytes.
 * Low memory available, stability problems may occur.
 * 
 * 
 * ------------------------------------------------------------------------------------
 * 
 * 
 */ 

