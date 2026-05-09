#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// # define LED_BUILTIN 13 defined by arduino.h
#define BTN_PIN 4
#define LCD_ADDR 0x27
// ^^ is wired like an I2C device, you should explain what I2C and address is,
// wire SDA -> A4 + SCL -> A5 VCC-> 5V
// the backlight display pins should have a jumper
#define HBS_PIN A0

#define LCD_ROWS 4
#define LCD_COLS 20

#define SAMP_TME 16667 // in uS for analog smoothing
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

#define SAMP_N 20
#define threshold 350
#define SLOPE_MIN 10

int i = 0;

// for debugging, detects connected I2C devices (LCD display in this case)
void I2C_Scan() {
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

float reads[SAMP_N];

void setup() {
	Serial.begin(9600);
	Wire.begin();

	// INPUT_PULLUP
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(BTN_PIN, INPUT_PULLUP);
	// figure out how exactly

	// I2C_scan needs serial to be awake
	while (!Serial);
	//I2C_Scan();

	lcd.init();
	lcd.backlight();
	// Set cursor to column 0, line 0
	lcd.setCursor(0, 0);
	lcd.print("Hello, HeartEaterBeater");
}

// filters all 60hz signals entirely (mains power)
int readFiltered(int pin) {
	unsigned int sampleCount = 0;
	unsigned long total = 0;

	for ( unsigned long start = micros(); micros() - start < SAMP_TME;) {
		total += analogRead(pin);
		sampleCount++;
	}

	return (int)(total / sampleCount);
}

bool rising = true;
unsigned long peakTime = 0;
long delta = 1000;
void loop() {
	// // day 1 reading from a button and display to LCD
	// if (digitalRead(BTN_PIN) == LOW) {
	// 	lcd.clear();
	// 	// digitalWrite(LED_BUILTIN, LOW);
	// } else if (digitalRead(BTN_PIN) == HIGH) {
	// 	// digitalWrite(LED_BUILTIN, HIGH);
	// 	lcd.setCursor(0, 0);
	// 	lcd.print("Button Pressed");
	// }

	// Serial.print("Signal: ");
	// Serial.println(signal);

	/* this algorithm is intentionally designed to fail btw
	 * its simple to understand
	 * however, the signal is noisy and we are looking for a spike from the baseline which could
	 * potentially move depending on what finger you use and how hard you press
	 * to solve this, we use a few different algorithms stacked
	*/


	/*
	if (signal <= threshold) {
		lcd.clear();
		digitalWrite(LED_BUILTIN, LOW);
	} else if (digitalRead(BTN_PIN) == HIGH) {
		digitalWrite(LED_BUILTIN, HIGH);
		lcd.setCursor(0, 0);
		lcd.print("BEAT");
	}
	*/
	// final algo

	int signal = readFiltered(HBS_PIN);
	digitalWrite(LED_BUILTIN, LOW);
	reads[i] = signal;

	long slope = reads[i] - reads[(i +1) % SAMP_N];

	if (!rising && slope > SLOPE_MIN) {
		rising = true;
	} else if (rising && slope < -SLOPE_MIN) {

		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("BEAT");
		long tmp = millis();
		delta = tmp - peakTime;
		peakTime = tmp;
		digitalWrite(LED_BUILTIN, HIGH);
		rising = false;
	}

	lcd.setCursor(0, 1);
	lcd.print("HR: ");
	double BPM = double(1)/ delta * 1000 * 60;

	lcd.print(BPM);

	Serial.print("signal: ");
	Serial.print(signal);
	Serial.print(",slope: ");
	Serial.print(slope);
	Serial.print(",");
	Serial.print(delta);

	i++;
	i%=SAMP_N;
}
