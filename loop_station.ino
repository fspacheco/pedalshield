/****************************************************************************
   Pedal shield + SD board
   Reading/writing to the SD card

   Fernando S. Pacheco
   2016

   // Licensed under a Creative Commons Attribution 3.0 Unported License.
   // Based on rcarduino.blogspot.com previous work.
   // www.electrosmash.com/pedalshield

 ****************************************************************************/

#include <SPI.h>
#include <SD.h>

int in_ADC0, in_ADC1;  //variables for 2 ADCs values (ADC0, ADC1) //*********************TO DO: conferir resolução (12 bits? 16?)
int POT0, POT1, POT2, out_DAC0, out_DAC1; //variables for 3 pots (ADC8, ADC9, ADC10)
const uint8_t LED = 13;
const uint8_t FOOTSWITCH = 7;
const uint8_t TOGGLE = 2;
const char FILENAME[] = "teste.txt";

uint8_t foot_state;

const int chipSelect = 10; //for SD card
File myFile;

//state machine
enum states {IDLE, FOOT_PRESSED, WAIT_FOOT_TIME, CLEAR_CARD, CHECK_CARD, READ_CARD, WRITE_CARD};
uint8_t curr_state = IDLE;
uint32_t current_time;
uint32_t start_time;

void setup()
{

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println("Initializing SD card...");

  if (!SD.begin(chipSelect)) {
    Serial.println("ERROR: SD initialization failed! ##########");
    return;
  }
  Serial.println("OK: SD initialization done.");

  //ADC Configuration
  ADC->ADC_MR |= 0x80;   // DAC in free running mode.
  ADC->ADC_CR = 2;       // Starts ADC conversion.
  ADC->ADC_CHER = 0x1CC0; // Enable ADC channels 0 and 1.

  //DAC Configuration
  analogWrite(DAC0, 0); // Enables DAC0
  analogWrite(DAC1, 0); // Enables DAC0

  //pedalSHIELD pin configuration
  pinMode(LED, OUTPUT);
  pinMode(FOOTSWITCH, INPUT_PULLUP);
  pinMode(TOGGLE, INPUT_PULLUP);
  Serial.print("OK: DAC and pins initialization done.");
  Serial.print(" LED:");
  Serial.print(LED);
  Serial.print(" FOOTSWITCH: ");
  Serial.print(FOOTSWITCH);
  Serial.print(" TOGGLE: ");
  Serial.println(TOGGLE);
}

void loop()
{
  //Read the ADCs
  while ((ADC->ADC_ISR & 0x1CC0) != 0x1CC0); // wait for ADC 0, 1, 8, 9, 10 conversion complete.
  in_ADC0 = ADC->ADC_CDR[7];             // read data from ADC0
  in_ADC1 = ADC->ADC_CDR[6];             // read data from ADC1
  POT0 = ADC->ADC_CDR[10];               // read data from ADC8
  POT1 = ADC->ADC_CDR[11];               // read data from ADC9
  POT2 = ADC->ADC_CDR[12];               // read data from ADC10

  //Add volume feature with POT2
  out_DAC0 = map(in_ADC0, 0, 4095, 1, POT2);
  out_DAC1 = map(in_ADC1, 0, 4095, 1, POT2);

  //Write the DACs
  dacc_set_channel_selection(DACC_INTERFACE, 0);       //select DAC channel 0
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC0);//write on DAC
  dacc_set_channel_selection(DACC_INTERFACE, 1);       //select DAC channel 1
  dacc_write_conversion_data(DACC_INTERFACE, out_DAC1);//write on DAC

  foot_state = digitalRead(FOOTSWITCH);

  Serial.print("ST:");
  Serial.print(curr_state);
  Serial.print("  FOOT:");
  Serial.println(foot_state);

  current_time = millis();
  //Turn on the LED if the effect is ON and write in SD card or Play
  switch (curr_state) {
    case IDLE:
      if (foot_state == HIGH) {
        digitalWrite(LED, HIGH);
        curr_state = FOOT_PRESSED;
      }
      else {
        digitalWrite(LED, LOW);
        curr_state = CHECK_CARD;
      }
      break;

    case FOOT_PRESSED:
      start_time = current_time;
      curr_state = WAIT_FOOT_TIME;
      break;

    case WAIT_FOOT_TIME:
      if (current_time - start_time > 4000) { //foot_switch pressed for 4 seconds to erase SD card
        start_time = current_time;
        curr_state = CLEAR_CARD;
      }
      break;

    case CLEAR_CARD:
      if (SD.exists(FILENAME)) {
        // delete the file:
        Serial.print("Removing ");
        Serial.println(FILENAME);
        SD.remove(FILENAME);
        Serial.println("OK. Removed.");
      }
      else {
        Serial.print("WARNING: ");
        Serial.print(FILENAME);
        Serial.println(" NOT found while trying to delete it.");
      }
      curr_state = IDLE;
      break;

    case CHECK_CARD:
      if (SD.exists(FILENAME)) {
        curr_state = READ_CARD;
      }
      else {
        curr_state = WRITE_CARD;
      }
      break;

    case READ_CARD:
      myFile = SD.open(FILENAME, FILE_READ);
      if (!myFile) {
        Serial.println("ERROR: file could not be opened while trying to READ");
      }
      else {
        // read from the file until there's nothing else in it:
        while (myFile.available()) {
          out_DAC0 = Serial.write(myFile.read());
          out_DAC0 = map(in_ADC0, 0, 4095, 1, POT2);
          dacc_set_channel_selection(DACC_INTERFACE, 0);       //select DAC channel 0
          dacc_write_conversion_data(DACC_INTERFACE, out_DAC0);//write on DAC
          //TO DO: Check this. No DAC1?***********
        }
        // close the file:
        myFile.close();
      }
      curr_state = IDLE;
      break;

    case WRITE_CARD:
      myFile = SD.open(FILENAME, FILE_WRITE);
      // if the file opened okay, write to it:
      if (myFile) {
        myFile.println(out_DAC0); //TO DO: Check this. No DAC1? ***********
        Serial.print("OK: writing file ");
        Serial.print(FILENAME);
        Serial.print("  value: ");
        Serial.println(out_DAC0);
        // close the file:
        myFile.close();
        Serial.print("OK: DONE writing file ");
        Serial.println(FILENAME);
      }
      else {
        Serial.print("ERROR: while trying to open for WRITing ");
        Serial.println(FILENAME);
      }
      curr_state = IDLE;
      break;

    default:
      curr_state = IDLE;//just in case there is some problem
      Serial.println("ERROR: Reached default state in switch ###########################");
      break;
  }
}
