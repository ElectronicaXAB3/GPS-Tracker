# GPS SD Logger :satellite:
### Adafruit Metro + Adafruit Ultimate GPS

Uses the same circuit as the Clip-on GPS CAM project. The button contain 2 modes depending on the amount of time pressed: 1.5 sec to start/stop recording, 0.5 sec to pause or resume the recording. All the location data was recorded to SD card at 1 Hz.

##Debug Notes

### 08/15/2017 (1) 

  In some cases, GPS.read() will print out the following
  
    $PGACK,103*40  -->  getting ready for a cold-start (flushing all memory of previous fixes)
  
    $PGACK,105*46  -->  GPS initialization routine is done
  
    $PMTK011,MTKGPS*08  -->  MTKGPS is a general chip identifier
  
    $PMTK010,001*2E  -->  configuration is completed
  
    $PMTK010,002*2D  -->  module is awake and waiting for input on the Serial connection
  
  All of these indicate startup signals.
 
### 08/15/2017 (2)

SD is a RAM hungry module. Following some recommendations on forums, I decided to use SdFat.h instead of SD.h Although functions are more limited, it could save SRAM. However, I saw no difference in dynamic memory i.e. 82% before and after.
  
### 08/15/2017 (3)

Interrupt is basically you are trying to do something while in the middle of other functions in this case main loop(). useInterrupt allows the GPS to Serial.print the NMEA sentence (lastNMEA) even while loop is running. Thus, this is mainly used for debugging to confirm what GPS is reading. If not used, SIGNAL and useInterrupt can be deleted.
 
### 08/15/2017 (4)

I'm trying more to break out the codes in the main loop into private functions. The two main reasons are that I'm trying to organize the code and trying to save memories esp. SRAM. Since when the function closes, the memory allocated for stack (local variables) will be 100% reclaimed as free space; thus, freeing up space and prevent RAM crashes. The new functions include: 
  
    void flash (uint8_t pin, char times) --> replace manual LED blinking
    File createFile(File & logfile, char *prefix) --> generate file
    void logGPSNMEA (File & logfile) --> log GPS to specified file
    char timerswitch(uint8_t nswitch) --> switch operator by time pressed
    void error() --> error signal
  
  Some learning notes here:
  
  * default values for function parameters can be set at function declaration (e.g. void flash (uint8_t pin, char times = 0);)

  * uint8_t is used to specify that int is defined to be 8 bit which means uint16_t and uint32_t are 16 and 32 bits accordingly.

  * instead of true header name, you can specify alias e.g. SdFat sd; Although it might be better to use "namespace" or "typedef" idk, have to read more.

  * SRAM allocation contains (1) Global and Static variables (2) Local variables (stack) (3) and Dynamic Allocations in between. In creating functions, I can use the reclaimable stack space. This could be made better by de-allocating dynamic space. But, remember to watch out for "fragmented heap" which could prevent us from really claiming back the dynamic space.

  * I actually struggle a lot with the function that returns file type. The solution seems to be that instead of carrying the file name to be opened in the function, I can just reference the logfile over to the function (using & operator) i.e. declare file in setup, refer in and return from createFile, then refer in logGPSNMEA. Now everything is consistent using the same file register.

  * Another is when I tried to create file name by passing along the string from input parameter when using function in the main loop. The problem was solved simply by using the pointer (*) in the function parameter i.e. pointer points to the input when recalled in the main loop.

  * In C/C++, array or strings work very similarly i.e. they are not real array or string but act as pointer to a specific memory where the values get stored. Thus, they are just continuous lines of data with "0" at the end in the case of string. Meaning: *(Array+7) = Array[7] since pointer Array (without specified position []) is just a pointer to the start of the array/chunk of memory, so +7 means the next 7 positions.

  * Now, the most confusing of all is the GPS library. I initally use: GPS.waitForSentence (PMTK_AWAKE) which is defined in the header, but that sentence was never cleared so I had to print out the NMEA to see what sentence actually came up. It appears that the response defined as PMTK_AWAKE rarely appear. Therefore, I changed the sentence to: GPS.waitForSentence("$PMTK011,MTKGPS*08") which is basically the chip identifier which always come up when GPS starts/restarts. The standby and wakeup works now because of this. PS I did not use .wakeup()since the header uses waitForSentence (PMTK_AWAKE) in the func, so I just use .sendCommand("") since any bit sent to GPS will wake it up.

  I also found out that .lastNMEA() actually returns the char in .read() meaning I always need to read everytime before checking last NMEA or use .newNMEAreceived().

  * Sometimes, the complier/controller just act weird. Try using the sample sketches e.g. one of the cases, the GPS only showed response noted in (1) even when loading in echo example. Somehow, I re-downloaded the library and switch between PIN 5 power source to main line, then things seem to work.

  * Also, don't forget to put break after each switch case...just a bug sometimes

### 08/15/2017 Upload Result
 
  Sketch uses 20984 bytes (65%) of program storage space. Maximum is 32256 bytes.
  Global variables use 1693 bytes (82%) of dynamic memory, leaving 355 bytes for local variables. Maximum is 2048 bytes.
  Low memory available, stability problems may occur.


## Apparently, SD library took up 38% of the SRAM !!!

### 08/16/2017 (1)

A problem with parsing NMEA again. I tried moving power source from PIN 5 to main line during the program and it seem to work well. This might be the problem(?) Or it could just be that my free RAM is not enough because sometimes that couldn't solve it and sometimes it just work by itself. 
  

