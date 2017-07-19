#include <EEPROM.h>
#include <Bounce.h>
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <mcmd.h>

//Function Signatures for my_commands table
void show_signon_cmd(int argc, char **argv);
void set_percentage_cmd(int argc, char *argv[]);
void list_sd_cmd(int argc, char *argv[]);
void set_sample_cmd(int argc, char *argv[]);
void report_cmd(int argc, char *argv[]);
void trigger_cmd(int argc, char **argv);
void calibrate_cmd(int argc, char **argv);
void take_reading_cmd (int argc, char *argv[]);
void help_cmd(int argc, char **argv);
void pause_cmd(int argc, char **argv);

//Lidar Lite V3 control Pins
#define lidarCtl 21
#define pwmRtn 20

//EEPROM Addresses for Thresh Percent and Sample Size
#define percentAddr 0
#define sampleAddr 100

//Contol lines for mute and cal
#define swMute 18
#define btnCal 19

//VS1053 Control Lines
#define BREAKOUT_RESET  9      // VS1053 reset pin (output)
#define BREAKOUT_CS     10     // VS1053 chip select pin (output)
#define BREAKOUT_DCS    8      // VS1053 Data/command select pin (output)
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin
#define CARDCS 4     // Card chip select pin

//Status LED line
#define stsLED 22

//VS1053 variables
//Number of tracks for loop indexing
int numtracks = 0;
//string placeholders for cancatinating
const String prefix = "track";
const String suffix = ".mp3";

//VS1053 Object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(BREAKOUT_RESET, BREAKOUT_CS, BREAKOUT_DCS, DREQ, CARDCS);

//Bounce object for mute switch
Bounce mute = Bounce(swMute, 10);

//Variable for Lidar Reading
unsigned long pulseWidth;
//Calibrated threshold
int calThresh = 0;
int CalTemp = 0;
//Percentage for threshold calculation
float threshPercentage = .15;
//sample size for calibration readings
int sampleSize = 100;
//loop counter for audio playback
int counter = 1;
//bool varible for detecting if sensor has cycled
bool lock = false;
//pause variable for pause_cmd
bool pause = false;


//List of Serial Monitor Commands Available to user
cmd_t my_commands[] = {

  { "percent",   "        Set threshold percentage : 0-100 ",        set_percentage_cmd,    my_commands + 1   },
  { "sd",        "        List the files on the SD Card",       list_sd_cmd,   my_commands + 2   },
  { "sample",    "        Set sample size for threshold calibration : 1-1000",      set_sample_cmd,      my_commands + 3   },
  { "report",    "        Show current values",      report_cmd,         my_commands + 4   },
  { "go",        "        Simulate triggering lidar",  trigger_cmd,    my_commands + 5  },
  { "reading",   "        Take reading from sensor (opt - number of readings)", take_reading_cmd, my_commands + 6},
  { "calibrate", "Calibrate threshold",    calibrate_cmd,     my_commands + 7   },
  { "pause" ,    "        Pause trigger events", pause_cmd, my_commands + 8 },

  { "signon",    "        Show signon text",         show_signon_cmd,    my_commands + 9   },
  { "help",      "        Show available commands",  help_cmd,           my_commands + 10   },
  { "?",         "        Show available commands",  help_cmd,           NULL            },
};

//Displays available commands to user
void help_cmd(int argc, char **argv)
{
  cmd_t *p = my_commands;

  while (p) {
    Serial.print(p->cmd);
    Serial.print("\t");
    Serial.println(p->hlp);
    p = p->next;
  }
}

//Set new threshold percentage
//Updates the threshold in EEPROM
//Command input : number from 0-100
void set_percentage_cmd(int argc, char *argv[])
{
  if (argc != 2) {
    Serial.println("### Must specify percentage. 0 - 100 ###");
  }
  else if ( strtol(argv[1], NULL, 10) < 0 || strtol(argv[1], NULL, 10) > 100 ) {
    Serial.println("### Value must be between 0 and 100 ###");
  }

  else {
    threshPercentage = strtof(argv[1], NULL) / 100;
    EEPROM.put(percentAddr, threshPercentage);
    Serial.println ("Value Committed");

  }
}

void show_signon_cmd(int argc, char **argv)
{
  //show_signon();
  Serial.println("COSI WOSU Welcome Mat");
  Serial.println("Compiled: " __DATE__ ", " __TIME__ ", " __VERSION__);
  Serial.print(F("Arduino IDE version: "));
  Serial.println(ARDUINO, DEC);

}


//Displays the file tree on SD card
void list_sd_cmd(int argc, char *argv[])
{
  printDirectory(SD.open("/"), 0);
}

//Set the sample size for calabration average
//Updates value in EEPROM
//Command input: number from 0 - 1000
void set_sample_cmd(int argc, char *argv[])
{
  long int  i = 0;
  if (argc != 2) {
    Serial.println("### Must specify sample size. 1 - 1000 ###");
  }
  else if ( strtol(argv[1], NULL, 10) < 1 || strtol(argv[1], NULL, 10) > 1000)  {
    Serial.println("### Value must be between 1 and 1000 ###");
  }

  else {
    i = strtol(argv[1], NULL, 10);
    sampleSize = i;
    EEPROM.put(sampleAddr, sampleSize);
    Serial.println ("Value Committed");

  }
}

//Dispalys a list of current values. Includes percentage, sample, and more.
void report_cmd(int argc, char *argv[])
{
  Serial.print("Threshold Percentage: ");
  Serial.println(threshPercentage);
  Serial.print("Sample Size: ");
  Serial.println(sampleSize);
  Serial.print("Number of Tracks: ");
  Serial.println(numtracks);
  Serial.print("Calibrated Threshold: ");
  Serial.println(calThresh);
  Serial.print("Current Reading: ");
  Serial.println(takeReading());
}

//Triggers Playback
void trigger_cmd(int argc, char **argv)
{
  PlayTrack();
}

//recalibrates threshold
void calibrate_cmd(int argc, char **argv)
{
  CalibrateThreshold();
}

//takes a reading from lidar sensor
//Optional - number of samples
void take_reading_cmd (int argc, char *argv[])
{
  if (argc == 1) {
    Serial.print("Reading: ");
    Serial.println(takeReading());
  }

  if (argc  == 2) {
    for (int i = 0; i < strtol(argv[1], NULL, 10); i++) {
      Serial.print("Reading: ");
      Serial.println(takeReading());
    }
  }
}

//Pauses triggering of lidar. Proves useful when playback spams the serial console.
void pause_cmd(int argc, char **argv) {
  pause = ! pause;

  if (pause == true) {
    Serial.println("Paused");
  }
  else {
    Serial.println("Resumed");
  }
}

//Calibrates the threshold for kids to cross. Takes 100 readings, averages them, then applies a percentage
//to determine the threshold.
//Param: None
//Return: None
void CalibrateThreshold () {
  //Turn LED on to notify user that system is calibrating
  digitalWrite(stsLED, HIGH);
  //take set number of sample and add them together
  for (int i = 0; i < sampleSize; i++) {

    CalTemp += takeReading();
  }
  //Find average of sample
  CalTemp = (CalTemp / sampleSize);
  Serial.print("Average Reading: ");
  Serial.println(CalTemp);
  //Apply the percentage to come up with threshold that must be crossed
  calThresh = CalTemp - (CalTemp * threshPercentage);
  Serial.print("Calibrated Threshold: ");
  Serial.println(calThresh);
  //Turn LED off when finished
  digitalWrite(stsLED, LOW);
}

//Takes single reading from Lidar Sensor
//Param : None
//Return : Distance reading from Sensor
int takeReading () {
  pulseWidth = pulseIn(pwmRtn, HIGH); // Count how long the pulse is high in microseconds
  pulseWidth = pulseWidth / 10; // 10usec = 1 cm of distance
  return pulseWidth;
  //Serial.println(pulseWidth); // Print the distance
}

// Prints the SD Card Directory to the Serial Console
//Param : directory to scan, number of tabs
//Return: Number of files found in root directory
int printDirectory(File dir, int numTabs) {
  int k = 0;
  while (true) {

    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      //Serial.println("**nomorefiles**");
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
      k++;
    }
    entry.close();
  }
  return k;
}

//Plays next track in loop of audio tracks
//Param : None
//Return: None
void PlayTrack() {
  //Turn status LED on to tell user track is playing
  digitalWrite(stsLED, HIGH);
  //MAke sure counter is not at end of loop
  if (counter  == numtracks + 1) {
    //reset if it is
    counter = 1;
  }
  else

    //Let I/O pin settle
    delay(10);
  //Cancatinate string to get filename
  String track = (prefix + counter + suffix);
  //Let user know which track is playing
  Serial.print("Playing: ");
  //Conversion that is required for VS1053 library
  char Track[track.length()];
  track.toCharArray(Track, track.length() + 1);
  //Print track name
  Serial.println(Track);
  //Play track through VS1053
  musicPlayer.playFullFile(Track);
  //Turn LED off when track is done
  digitalWrite(stsLED, LOW);
  //Progress track counter
  counter ++;

}

//Setup Code
void setup() {
  // put your setup code here, to run once:
  //Init Serial port for communication
  Serial.begin(9600);

  //Teensy is too fast for PC. So we wait
  delay(2000);

  //Start command system
  cmdInit(my_commands, 9600);

  //Welcome the user
  Serial.println("WOSU Welcome Mat - ' ? ' for help");

  //Update values from EEPROM
  EEPROM.get(percentAddr, threshPercentage);
  EEPROM.get(sampleAddr, sampleSize);

  //Set PinModes for input and output
  pinMode(swMute, INPUT);
  pinMode(btnCal, INPUT_PULLUP);
  pinMode(stsLED, OUTPUT);
  digitalWrite(stsLED, LOW);
  pinMode(lidarCtl, OUTPUT); // Set pin 2 as trigger pin
  digitalWrite(lidarCtl, LOW); // Set trigger LOW for continuous read
  pinMode(pwmRtn, INPUT); // Set pin 3 as monitor pin

  //if VS1053 can be found, begin communication
  if (! musicPlayer.begin()) { // initialise the music player
    Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
    //otherwise, end program and notifiy user through status LED
    while (1) {
      digitalWrite(stsLED, HIGH);
      delay(1000);
      digitalWrite(stsLED, LOW);
      delay(1000);
    }

  }
  Serial.println(F("VS1053 found"));

  //If SD card can be found, begin communication
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    //otherwise, end program and notify user through status LED
    while (1) {
      digitalWrite(stsLED, HIGH);
      delay(250);
      digitalWrite(stsLED, LOW);
      delay(250);
      digitalWrite(stsLED, HIGH);
      delay(1000);
      // don't do anything more
    }

  }
  Serial.println("SD Card Found");

  //Find out how many tracks are on the sd card
  numtracks = printDirectory(SD.open("/"), 0);
  Serial.print ("Number of tracks on SD Card: ");
  Serial.println(numtracks);

  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20, 20);

  // Timer interrupts are not suggested, better to use DREQ interrupt!
  //musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  //Calibrate the trigger threshold
  CalibrateThreshold();
}

void loop() {
  // put your main code here, to run repeatedly:

  //required to use command library
  cmdPoll();
  //look for change in mute switch
  mute.update();

  //Next two if statements ensure the mute message is only displayed once
  if (mute.fallingEdge()) {
    Serial.println("Unmuted");
  }
  if (mute.risingEdge()) {
    Serial.println("Muted");
  }

  // Re-runs calibration if the button is pressed
  if (digitalRead(btnCal) == LOW) {
    CalibrateThreshold();
  }

  //Multiple conditions must be met for playback to happen
  //paused via cmd must not be true
  //system cannot be muted via switch
  //the threshold must be crossed
  //and the sensor must have gone through cycle to trigger
  //one cyle = not triggered -> triggered -> not triggered
  if (pause != true) {
    if (digitalRead(swMute) != HIGH) {
      if (takeReading() < calThresh && lock != true) {
        //PlayAudio
        lock = true;
        PlayTrack();
      }

      if (takeReading() > calThresh && lock == true) {
        lock = false;
      }
    }

    //Notify user that system is muted through LED
    else {
      digitalWrite(stsLED, HIGH);
      delay(250);
      digitalWrite(stsLED, LOW);
      delay(250);
    }
  }
}

