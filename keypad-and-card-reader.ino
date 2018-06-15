/*
 * keypad-and-card-reader.ino
 * Input device for eg an access control system.
 * 
 * Author: Søren Thing <soeren@thing.dk>
 * Released under MIT license - please see the file LICENSE.
 * 
 * Inputs:
 *  - A RFID-RC522 card reader
 *  - A matrix keypad
 *  
 * Output:
 *  - Simple lines to the serial port.
 *    Each line has the format <TYPE>:<VALUE>\r\n
 *    Examples:
 *      - DEBUG:Reader restarted.
 *      - NFC:EA35892E
 *      - KEYS:1234
 *      
 * The RFID-RC522 card reader is used to read the UID from an ISO14443 card. 
 * 
 * When using the keypad, a line is only sent when they key # is pressed - ie press 1234# to get "KEY:1234" as output.
 * A maximum of 20 digits can be input.
 * When a key is pressed the keypad backlight will flash.
 * the input buffer is cleared if no new keys are pressed for 10 seconds.
 *
 * The individual inputs can be disabled by the defines below.
 *
 * Hardware:
 *  - A 3.3V/8MHz Arduino Pro Mini clone
 *    Beware: The card reader is NOT 5V tolerant.
 *  - A RFID-RC522 card reader (Buy at eBay or AliExpress)
 *  - A matrix keypad - whatever quality suits you.
 *    I am using a nice IP65 rated Accord ACCORD AK-304-N-SSL-WP-MM-BL from TME:
 *    https://www.tme.eu/en/details/kb304-mns-wp-b/metal-keypads/accord/ak-304-n-ssl-wp-mm-bl/
 * 
 * Connections for the MFRC522 board:
 * Signal     Pin                    Pin
 *            Arduino Pro Mini       MFRC522 board
 * -----------------------------------------
 * SPI SS     10                     SDA (The pin's I2C dunction)
 * SPI MOSI   11                     MOSI
 * SPI MISO   12                     MISO
 * SPI SCK    13                     SCK
 * Reset      14   (marked A0)       RST
 * 
 * Connections for the keypad:
 * (Adapt to your specific keypad)
 * Signal     Pin                    Pin
 *            Arduino Pro Mini       Keypad
 * -----------------------------------------
 * Ground     GND                    -LED
 * Backlight  2                      +LED
 * Column 2   3                      1
 * Row 1      4                      2
 * Column 1   5                      3
 * Row 4      6                      4
 * Column 3   7                      5
 * Row 3      8                      6
 * Row 2      9                      7
 */

//#define DISABLE_CARD_READER
//#define DISABLE_KEYPAD

#define SERIAL_SPEED 115200

#ifndef DISABLE_CARD_READER
#include <SPI.h>
#include <MFRC522.h>
#define SS_PIN  10
#define RST_PIN A0
MFRC522 mfrc522(SS_PIN, RST_PIN);	// Create MFRC522 instance.
#endif

#ifndef DISABLE_KEYPAD
#include <Keypad.h>
#define BACKLIGHT_PIN 2
#define MAX_KEY_COUNT 20
#define MAX_KEY_DELAY 10
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {4, 9, 8, 6};
byte colPins[COLS] = {5, 3, 7};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
#endif

void setup() {
  Serial.begin(SERIAL_SPEED);
  Serial.println("DEBUG:keypad-and-card-reader restarted.");

  #ifndef DISABLE_CARD_READER
  // Init card reader
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("DEBUG:Card reader enabled.");
  #endif
  
  #ifndef DISABLE_KEYPAD
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  Serial.println("DEBUG:Keypad enabled.");
  #endif
}

#ifndef DISABLE_CARD_READER
void readCard() {
  static MFRC522::Uid lastUid;     // UID of last scanned card
  static unsigned long lastMillis; // value of millis() when last card was scanned.

  // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  // Check that it is not a bounce of the same card
  if (memcmp(&lastUid, &mfrc522.uid, mfrc522.uid.size) == 0 // Same UID
        && (millis() - lastMillis) < 1000) { // Scanned again within on second 
    // Halt the card so it does not respond again while in the field
    mfrc522.PICC_HaltA();
    return;
  }

  // Remember the time
  lastMillis = millis();

  // Output the UID followed by Enter.
  Serial.print("NFC:");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] >> 4, HEX);
    Serial.print(mfrc522.uid.uidByte[i] & 0x0F, HEX);
  } 
  Serial.println();

  // Halt the card so it does not respond again while in the field
  mfrc522.PICC_HaltA();

  // Remember the UID
  memcpy(&lastUid, &mfrc522.uid, sizeof(lastUid));
}
#endif

#ifndef DISABLE_KEYPAD
void readKeypad() {
  static char key;
  static char buffer[MAX_KEY_COUNT + 1];
  static uint8_t count = 0;
  static unsigned long lastMillis; // value of millis() when last key was pressed.

  // Flush old keypresses?
  if (count && (millis() - lastMillis) > 1000 * MAX_KEY_DELAY) { // More than MAX_KEY_DELAY seconds since last key 
    count = 0;
  }
  
  key = keypad.getKey();
  if (key == NO_KEY) {
    return;
  }
  if (key == '#') { // Send a line
    digitalWrite(BACKLIGHT_PIN, LOW);
    buffer[count] = 0;
    Serial.print("KEYS:");
    Serial.println(buffer);
    count = 0;
    delay(250);
    digitalWrite(BACKLIGHT_PIN, HIGH);
  }
  else if (count < MAX_KEY_COUNT) { // Add to buffer
    digitalWrite(BACKLIGHT_PIN, LOW);
    buffer[count] = key;
    count++;
    delay(50);
    digitalWrite(BACKLIGHT_PIN, HIGH);
  }

  // Remember the time
  lastMillis = millis();  
}
#endif

void loop() {
  #ifndef DISABLE_CARD_READER
  readCard();
  #endif
  
  #ifndef DISABLE_KEYPAD
  readKeypad();
  #endif
}

