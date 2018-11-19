#include <Adafruit_ST7735.h>
#include <Adafruit_mfGFX.h>
#include <SPI.h>
#include <MAX30105.h>
#include <heartRate.h>
#include "heartRate.h"
#include "MQTT.h"

// Color definition for the TFT-LCD
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF

// TFT-LCD connections and definitions
#define cs   2
#define rst  3
#define dc   4

// Button pins
#define enter 5
#define down 6
#define up 7

// BPM
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
long irValue;
float BPM = 0.00;
float averageBpm;

const unsigned long sampleDuration = 1 * 60 * 1000UL;
unsigned long lastSampled = 0;

// Pin connections
const int xPin = A2;     //Connected to A2 X-axis
const int yPin = A1;     //Connected to A1 Y-axis
const int zPin = A0;     //Connected to A0 Z-axis

// Accelerometer
int xRead = 0;
int yRead = 0;
int zRead = 0;
// If 0 there is no movement, if 1 there is movement.
int movementDetected = 0;

// Temporary storage for check
int prevX = 0;
int prevY = 0;
int prevZ = 0;

// Buttons main menu
int currentMenu = 0;
int menuValueDB = 0;

// BPM menu selection
int sendOrQuit = 0;
bool sendData = false;

// Menu switch
bool isActive = false;

Adafruit_ST7735 tft = Adafruit_ST7735(cs, dc, rst); // hardware spi
MAX30105 maxSensor; // Pulse sensor
MQTT client("demo.thingsboard.io", 1883, callback); // MQTT client

const char* topic = "v1/devices/me/telemetry";

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting all components...");

	pinMode(up, INPUT_PULLUP);
	pinMode(down, INPUT_PULLUP);
	pinMode(enter, INPUT_PULLUP);

	tft.initR( INITR_GREENTAB );
	tft.fillScreen(ST7735_BLACK);

	client.connect("photon", "2Yjyr8lL0P6qeSOyD41Q", NULL); // MQTT access token

	if (!maxSensor.begin(Wire, I2C_SPEED_FAST)) {
      Serial.println("Heartrate sensor was not found.");
      while (1);
  }
	maxSensor.setup();
}

void loop()
{
	if (client.isConnected()) {
    client.loop();
  }
	// Main menu
	while(!isActive){
		printMainMenu();
		checkUpDown();
		selectMainMenu();
	}

	// Seconddary menu
	printSecondaryMenu();
	selectSecondaryMenu();
	checkUpDown();
	checkForMovement();
	irValue = maxSensor.getIR();
	Serial.println(irValue);

	// BPM check
	if (irValue > 60000) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    unsigned long now = millis();
    BPM = 60 / (delta / 1000.0);

    if (BPM < 200 && BPM > 35 && irValue > 50000) {
      rates[rateSpot++] = (byte)BPM;
      rateSpot %= RATE_SIZE;
      if (now - lastSampled >= sampleDuration) {
          averageBpm = 0;
          for (byte x = 0 ; x < RATE_SIZE ; x++)
          averageBpm += rates[x];
          averageBpm /= RATE_SIZE;
					Serial.println(averageBpm);
          lastSampled = millis();
					Particle.publish("Average BPM: ", String(averageBpm));
      }
    } else {
      if (now - lastSampled >= sampleDuration) {
				lastSampled = millis();
      }
    }
  }

	//Checks if the finger is properly placed on MAX30100 sensor.
	if(irValue < 60000){
		averageBpm = 0;
		BPM = 0;
	} else if(sendData == true){ // If test person has started the test.
		Particle.publish("BPM: ", String(BPM)); // Sends BPM for firebase webook.
		String payload = "{"; // Payload for MQTT
		payload += "\"Movement\":"; payload += String(movementDetected);
		payload += ",";
		payload += "\"BPM\":"; payload += String(BPM);
		payload += ",";
		payload += "\"AverageBPM\":"; payload += String(averageBpm);
		payload += "}";

		const char* buffer = payload.c_str();

		Serial.println(buffer);
		client.publish(topic, buffer);
	}
	movementDetected = 0;

	delay(800);
}

// Callback function
void callback(char* topic, byte* payload, unsigned int length) {
	char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
}

void checkUpDown(){
	// Logical checks for main menu
	if(digitalRead(up) == HIGH && currentMenu > 0 && isActive == false){
		currentMenu--;
		tft.fillScreen(ST7735_BLACK);
		// Main menu down Button
	}else if(digitalRead(down) == HIGH && currentMenu < 5 && isActive == false){
		currentMenu++;
		tft.fillScreen(ST7735_BLACK);
		// Main menu enter button logic
	} else if(digitalRead(enter) == HIGH && isActive == false){
		isActive = true;
		menuValueDB = currentMenu;
		currentMenu = 7;
		// Logical checks for BPM-menu
	} else if(digitalRead(enter) == HIGH && isActive == true && sendOrQuit == 0){
		sendData = true;
		Serial.println("Sending data...");
	} else if(digitalRead(enter) == HIGH && isActive == true && sendOrQuit == 1){
		sendData = false;
		Serial.println("Sending data paused...");
		Serial.println("To resume press: send data");
		// Returns to main menu
	}else if(digitalRead(enter) == HIGH && isActive == true && sendOrQuit == 2){
		if(sendData = true){
			sendData = false;
		}
		isActive = false;
		currentMenu = 0;
		sendOrQuit = 0;
		tft.fillScreen(ST7735_BLACK);
	} else if(digitalRead(down) == HIGH && isActive == true && sendOrQuit < 2){
		sendOrQuit++;
	} else if(digitalRead(up) == HIGH && isActive == true && sendOrQuit > 0){
		sendOrQuit--;
	}
}

void printMainMenu()
{
	tft.setCursor(20, 0);
	tft.setTextSize(2);
	tft.setTextColor(GREEN);
	tft.setTextWrap(true);
	tft.print("Select");

	tft.setCursor(20, 20);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("genre:");

	tft.setCursor(20, 40);
	tft.setTextSize(2);
	tft.setTextColor(WHITE);
	tft.setTextWrap(true);
	tft.print("Action");

	tft.setCursor(20, 60);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("Comedy");

	tft.setCursor(20, 80);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("Drama");

	tft.setCursor(20, 100);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("Horror");

	tft.setCursor(20, 120);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("Romance");

	tft.setCursor(20, 140);
	tft.setTextSize(2);
	tft.setTextWrap(true);
	tft.print("Thriller");
}

void selectMainMenu()
{
	tft.setTextColor(ST7735_WHITE, ST7735_BLACK);
	if(currentMenu == 0)
	{
		tft.setCursor(5, 40);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	}
	else if(currentMenu == 1){
		tft.setCursor(5, 60);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	} else if(currentMenu == 2){
		tft.setCursor(5, 80);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	}else if(currentMenu == 3){
		tft.setCursor(5, 100);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	} else if(currentMenu == 4){
		tft.setCursor(5, 120);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	} else if(currentMenu == 5){
		tft.setCursor(5, 140);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	}
}

void printSecondaryMenu()
{
		tft.fillScreen(ST7735_BLACK);
		tft.setCursor(45, 25);
		tft.setTextColor(ST7735_WHITE);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print("BPM:");

		tft.setCursor(20, 45);
		tft.setTextSize(3);
		tft.setTextColor(GREEN);
		tft.setTextWrap(true);
		tft.print(BPM);

		tft.setCursor(20, 100);
		tft.setTextColor(ST7735_WHITE);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print("Send data");

		tft.setCursor(20, 120);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print("Pause");

		tft.setCursor(20, 140);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print("Quit");
}

void selectSecondaryMenu()
{
	if(sendOrQuit == 0){
		tft.setCursor(5, 100);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	} else if(sendOrQuit == 1){
		tft.setCursor(5, 120);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	} else if(sendOrQuit == 2){
		tft.setCursor(5, 140);
		tft.setTextSize(2);
		tft.setTextWrap(true);
		tft.print(">");
	}
}

void checkForMovement()
{
	prevX = xRead;
	prevY = yRead;
	prevZ = zRead;

  xRead = analogRead(xPin);
  yRead = analogRead(yPin);
  zRead = analogRead(zPin);

	if((prevX - xRead >= 100) || (xRead - prevX >= 100)){
		Serial.println("There was movement in X-axis");
		movementDetected = 1;
	} else if((prevZ - zRead >= 100) || (zRead - prevZ >= 100)){
		Serial.println("There was movement in Z-axis");
		movementDetected = 1;
	} else if((prevY - yRead >= 100) || (yRead - prevY >= 100)){
		Serial.println("There was movement in X-axis");
		movementDetected = 1;
	}
}
