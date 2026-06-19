#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>

#define LCD_ADDR 0x27
// for the LCD display, wire SDA -> A4, SCL -> A5, VCC-> 5V, GND -> GND
// the backlight display pins should have a jumper

#define HBS_PIN A0
// wire + to 3.3V, - to GND, S to A0

// prgrm ctrl defines
#define LCD_ROWS 2
#define LCD_COLS 16

#define BEAT_TIME_MS 100
#define SAMP_N 256

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

/**
 * System -
 *   OS:  [Linux Mint 22.1 x86 Cinnamon]
 *   IDE: [CLion + PlatformIO]
 * Author: Zain Ali
 *
 * APS ReadySetCode HeartBeat Kit Example Code
 * Showcases algorithms for for heartbeat detection and measurement
 *
 * Unit 1:
 *	Trigger some code when there is a heartbeat
 *	calculate The HeartRate of heartBeats
 *
 * Docs + links: labeled by skim or read or if you still want more
*  * https://link.springer.com/article/10.1007/s11831-021-09597-4 << Cite this for TROIKA Algo, FRFR READ TS
 * TODO: Tadgh Pull Fig 4. from here
 * https://learn.adafruit.com/scanning-i2c-addresses/arduino << I2C device scan, TODO: Tadgh pls read this for debugging info
 * https://gist.github.com/tfeldmann/5411375 << another I2C san (not the one I used but it's cool), if you want more
 * https://www.adafruit.com/product/1093?srsltid=AfmBOoqwTVR6AGR2bwP1o3GK9-nqLg_Dyd-FgV7eGp7fDjfm3NMKhWae << heartBeat Sensor, if you want more
 * https://medium.com/@lnandanapalli/efficient-array-wrapping-the-modulo-trick-every-developer-should-know-7ee614272100 << TODO: Tadgh build slides for the modulo operator based on here
 * https://docs.arduino.cc/built-in-examples/digital/Debounce/ << How the button should be wired, although we rely on not debouncing to detect things
 * https://www.norwegiancreations.com/2017/09/arduino-tutorial-using-millis-instead-of-delay/ << millis vs delay, nonblocking code
 */

byte SMALL_HEART[8] = {
	0b00000,
	0b00000,
	0b00000,
	0b01110,
	0b00100,
	0b00000,
	0b00000,
	0b00000
};

// Define the 5x8 custom heart character
// hashtags where the ones are so you can see how I drew it
byte BIG_HEART[8] = {
	0b00000, // 0
	0b01010, //  # #
	0b11111, // #####
	0b11111, // #####
	0b01110, //  ###
	0b00100, //   #
	0b00000, // 0
	0b00000  // 0
};

// this is one to ensure that ledService triggers once on startup
unsigned long lastBeat = 1;
unsigned long lastClear = 0;
float heartRate = 0;

void beat() {
	// if the lastClear happened last
	// to avoid looping lcd.clear()
	if (lastClear >= lastBeat) {
		// Step 4: write the code that reacts to a beat

		// Step 6: HB calculations and display here

		lastBeat = millis();
		digitalWrite(LED_BUILTIN, HIGH);

	}
}

void ledService() {
	// if lastBeat was at least BEAT_... ago
	// and the lastBeat happened last, to avoid looping lcd.clear()
	if ((millis() - lastBeat) > BEAT_TIME_MS && lastBeat > lastClear) {
		
		// Step 4: write the code that resets everything after a heartbeat

		lastClear = millis();

		// Step 6: print HB here, so it doesn't go away after clear
	}
}

// for debugging, detects connected I2C devices (LCD display in this case)
/** void I2C_Scan() {
	byte error, address;
	int nDevices;

	Serial.println("Scanning...");

	nDevices = 0;
	for(address = 1; address < 127; address++ ) {
		Wire.beginTransmission(address);
		error = Wire.endTransmission();

		if (error == 0) {
			Serial.print("I2C device found at address 0x");
			if (address<16) Serial.print("0");
			Serial.print(address,HEX);
			Serial.println("  !");
			nDevices++;
		}
		else if (error==4) {
			Serial.print("Unknown error at address 0x");
			if (address<16) Serial.print("0");
			Serial.println(address,HEX);
		}
	}
	if (nDevices == 0) Serial.println("No I2C devices found\n");
	else Serial.println("done\n");

	delay(5000); // Wait 5 seconds for next scan
}
*/

float reads[SAMP_N];

void setup() {
	Serial.begin(115200);
	Wire.begin();

	// START HERE, pinMode()


	// I2C_scan needs serial to be awake
	while (!Serial);
	// I2C_Scan(); // use to see if your LCD display is properly wired

	lcd.init();
	lcd.backlight();
	lcd.setCursor(0, 0);
	lcd.clear();
	lcd.createChar(0, SMALL_HEART);
	lcd.createChar(1, BIG_HEART);
	lcd.write(0);

	// Step 2
	lcd.print("Hello World");
	delay(1000);
}

float thresh = 570; // my tuned initial value for threshold
int i = 0;
long total = 0;

void loop() {

	// step 3 reading Data
	long raw = ;

	static unsigned long lastPrint = millis();
	if (millis() - lastPrint > 50) {
		lastPrint = millis();
		Serial.print("HBS: ");
		Serial.println(raw);
	}

	// step 4, tune thresh from graph, write beat, and ledService
	if (raw > thresh) {
		beat();
	}


	// step 5, thresh is a function of an AVG of all the reads for

	ledService();
}
