#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <arduinoFFT.h>

#define BTN_PIN 4
#define LCD_ADDR 0x27
// ^^ is wired like an I2C device, you should explain what I2C and address is,
// wire SDA -> A4 + SCL -> A5 VCC-> 5V
// the backlight display pins should have a jumper
#define HBS_PIN A0

// ----- for Day 2
// program CTRL
#define LCD_ROWS 4
#define LCD_COLS 16

#define SAMP_TME 16667 // in uS for analog smoothing
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// delay is 2ms, we need a 5s window
#define SAMP_N 300
#define threshold 350
// vv needs tuning, was 10
#define SLOPE_MIN 3

/**
 * System -
 *   OS:  [Linux Mint 22.1 x86 Cinnamon]
 *   IDE: [CLion + PlatformIO]
 * Author: Zain Ali
 *
 * APS ReadySetCode HeartBeat Kit Example Code
 * Showcases 3 different algorithms for different ways to detect heartbeats
 *
 * Unit 1:
 * use a button to figure out hardware
 *
 * Unit 2:
 * use an Analog PulseSensor to do things
 *
 * Unit 3:
 * TROIKA ALGO, please see Docs
 *
 * Docs + links: labeled by skim or read or if you still want more
 * https://link.springer.com/article/10.1007/s11831-021-09597-4 << Cite this for TROIKA Algo, FRFR READ TS
 * https://learn.adafruit.com/scanning-i2c-addresses/arduino << I2C device scan, READ
 * https://gist.github.com/tfeldmann/5411375 << another I2C san (not the one I used but it's cool), if you want more
 * https://www.adafruit.com/product/1093?srsltid=AfmBOoqwTVR6AGR2bwP1o3GK9-nqLg_Dyd-FgV7eGp7fDjfm3NMKhWae << heartBeat Sensor, if you want more
 * https://medium.com/@lnandanapalli/efficient-array-wrapping-the-modulo-trick-every-developer-should-know-7ee614272100 << Modulo operator, READ mention to kids
 * https://docs.arduino.cc/built-in-examples/digital/Debounce/ << how the button SHOULD be wired, << for Zain for later, maybe you could add this in Unit 1 info
 * our code doesn't debounce, but that will be a later addition but it's cool to help the kids understand digital/analog signals
 *
 * https://www.norwegiancreations.com/2017/09/arduino-tutorial-using-millis-instead-of-delay/ << Millis vs Delay, READ
 * this is seriously a much more complex problem in modern x86 which would be cool to talk about to kids
 * but i don't really got links for how complex time sharing is from schedulers to timer chirp interrupts, gnu linux, fork(), Cores + threads
 * but I can explain what to look up
 *
 *** understanding Fourier Transform**,
 * this is not Fast Fourier transform which is what we are using and is quite different but this gives a mathematical foundation for it
 * show one of these to the kids eventually
 * https://www.youtube.com/watch?v=spUNpyF58BY&list=PL4VT47y1w7A1-T_VIcufa7mCM3XrSA5DD&index=1
 * https://www.youtube.com/watch?v=MBnnXbOM5S4&list=PL4VT47y1w7A1-T_VIcufa7mCM3XrSA5DD&index=2
 * https://www.youtube.com/watch?v=ToIXSwZ1pJU&list=PL4VT47y1w7A1-T_VIcufa7mCM3XrSA5DD&index=3
 * https://www.youtube.com/watch?v=r6sGWTCMz2k&list=PL4VT47y1w7A1-T_VIcufa7mCM3XrSA5DD&index=4
 */

// ---- Configuration ---------------------------------------------------------
// Frequency resolution = SAMPLE_RATE / SAMPLES = 50/128 = 0.39 Hz = 23.4 BPM
// Parabolic interpolation around the peak gets us sub-bin (~1-2 BPM) accuracy.
const uint16_t SAMPLES = 128;
const float SAMPLE_RATE = 50.0f;
const unsigned long SAMPLE_US = (unsigned long) (1000000.0f / SAMPLE_RATE);

// ---- FFT buffers -----------------------------------------------------------
float vReal[SAMPLES];
float vImag[SAMPLES];
ArduinoFFT<float> FFT(vReal, vImag, SAMPLES, SAMPLE_RATE);

struct Biquad {
	float b0, b1, b2, a1, a2, z1, z2;
};

Biquad bp = {0.1795f, 0.0f, -0.1795f, -1.6151f, 0.6409f, 0.0f, 0.0f};

static inline float biquadStep(Biquad &f, float x) {
	float y = f.b0 * x + f.z1;
	f.z1 = f.b1 * x - f.a1 * y + f.z2;
	f.z2 = f.b2 * x - f.a2 * y;
	return y;
}

float trackedBPM = 75.0f;
bool hrLocked = false;
const float MIN_BPM = 40.0f;
const float MAX_BPM = 200.0f;
const float MAX_BPM_JUMP = 15.0f;

float runMean = 0.0f;
float runVar = 1.0f;
const float STATS_ALPHA = 0.01f; // EMA rate for mean/var
const float THRESH_K = 0.5f; // threshold = mean + K*std
bool aboveThresh = false;
unsigned long lastBeatMs = 0;
const unsigned long REFRACTORY_MS = 280; // 214 BPM ceiling

bool ledOn = false;
unsigned long ledOffAtMs = 0;
const unsigned long LED_FLASH_MS = 120;

static inline void serviceLED() {
	if (ledOn && (long) (millis() - ledOffAtMs) >= 0) {
		lcd.setCursor(1,1);
		lcd.write(0);
		digitalWrite(LED_BUILTIN, LOW);
		ledOn = false;
	}
}

//int i = 0;

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
	lcd.createChar(0, SMALL_HEART);
	lcd.createChar(1, BIG_HEART);
	lcd.write(0);
	//lcd.print("Hello, HeartEaterBeater");
	delay(1000);
}

// filters all 60hz signals entirely (mains power)
/*
int readFiltered(int pin) {
	unsigned int sampleCount = 0;
	unsigned long total = 0;

	for (unsigned long start = micros(); micros() - start < SAMP_TME;) {
		total += analogRead(pin);
		sampleCount++;
	}

	return (int)(total / sampleCount);
}
*/
bool rising = true;
unsigned long peakTime = 0;
long delta = 1000;
float total = 0;

void loop() {
	// Unit 1 reading from a button and display to LCD
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

	/* Unit 2: code, rolling avg threshold
	 *
	*/
	/*
	reads[i] = analogRead(HBS_PIN);
	total += reads[i] - reads[(i + 1) % SAMP_N];

	float avg = total / SAMP_N;

	Serial.print("avg: ");
	Serial.print(avg);
	Serial.print(", read: ");
	Serial.print(reads[i]);
	Serial.print(", i: ");
	Serial.print(i);

	if (reads[i] > avg) {
		lcd.setCursor(0,0);
		lcd.write(1);
		lcd.print("BEAT");
		Serial.print("BEAT");
		digitalWrite(LED_BUILTIN, HIGH);
	} else {
		lcd.clear();
		lcd.write(0);
		digitalWrite(LED_BUILTIN, LOW);
	}

	++i %= SAMP_N;
	Serial.println("");

	delay(5);
	*/

	// final algo -- Unit 3
	unsigned long t = micros();
	for (uint16_t i = 0; i < SAMPLES; i++) {

		while ((long) (micros() - t) < 0) {
			serviceLED(); // turn LED off on schedule
		}
		t += SAMPLE_US;

		int raw = analogRead(HBS_PIN);
		float x = (float) raw - 512.0f;
		float y = biquadStep(bp, x);
		vReal[i] = y;
		vImag[i] = 0.0f;

		float delta = y - runMean;
		runMean += STATS_ALPHA * delta;
		runVar = (1.0f - STATS_ALPHA) * (runVar + STATS_ALPHA * delta * delta);
		float thresh = runMean + THRESH_K * sqrtf(runVar);

		unsigned long nowMs = millis();

		if (!aboveThresh && y > thresh && (nowMs - lastBeatMs) > REFRACTORY_MS) {
			aboveThresh = true;
			lastBeatMs = nowMs;

			lcd.setCursor(1,1);
			lcd.write(1);
			digitalWrite(LED_BUILTIN, HIGH);
			ledOn = true;
			ledOffAtMs = nowMs + LED_FLASH_MS;
		}
		if (aboveThresh && y < runMean) {
			aboveThresh = false;
		}
		serviceLED();
	}

	FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
	FFT.compute(FFTDirection::Forward);
	FFT.complexToMagnitude();
	serviceLED();

	const float bpmToBin = (float) SAMPLES / (SAMPLE_RATE * 60.0f);
	uint16_t binLo = (uint16_t) (MIN_BPM * bpmToBin);
	uint16_t binHi = (uint16_t) (MAX_BPM * bpmToBin);
	if (binLo < 1) binLo = 1;
	if (binHi > SAMPLES / 2 - 1) binHi = SAMPLES / 2 - 1;

	uint16_t sLo = binLo, sHi = binHi;
	if (hrLocked) {
		float lo = trackedBPM - MAX_BPM_JUMP;
		float hi = trackedBPM + MAX_BPM_JUMP;
		if (lo < MIN_BPM) lo = MIN_BPM;
		if (hi > MAX_BPM) hi = MAX_BPM;
		sLo = (uint16_t) (lo * bpmToBin);
		sHi = (uint16_t) (hi * bpmToBin);
		if (sLo < binLo) sLo = binLo;
		if (sHi > binHi) sHi = binHi;
	}

	uint16_t peakBin = sLo;
	float peakMag = 0.0f;
	for (uint16_t b = sLo; b <= sHi; b++) {
		if (vReal[b] > peakMag) {
			peakMag = vReal[b];
			peakBin = b;
		}
	}

	if (hrLocked) {
		float meanMag = 0.0f;
		for (uint16_t b = binLo; b <= binHi; b++) meanMag += vReal[b];
		meanMag /= (float) (binHi - binLo + 1);
		if (peakMag < 1.8f * meanMag) {
			peakMag = 0.0f;
			peakBin = binLo;
			for (uint16_t b = binLo; b <= binHi; b++) {
				if (vReal[b] > peakMag) {
					peakMag = vReal[b];
					peakBin = b;
				}
			}
		}
	}

	float refinedBin = (float) peakBin;
	if (peakBin > binLo && peakBin < binHi) {
		float a = vReal[peakBin - 1];
		float b = vReal[peakBin];
		float c = vReal[peakBin + 1];
		float denom = a - 2.0f * b + c;
		if (fabsf(denom) > 1e-6f) {
			refinedBin = (float) peakBin + 0.5f * (a - c) / denom;
		}
	}

	float bpm = refinedBin / bpmToBin;
	if (!hrLocked) {
		trackedBPM = bpm;
		hrLocked = true;
	} else { trackedBPM = 0.7f * bpm + 0.3f * trackedBPM; }

	Serial.print(F("BPM: "));
	Serial.println(trackedBPM, 1);
	lcd.clear();
	lcd.setCursor(0, 0);
	lcd.print("BPM: ");
	lcd.print(bpm);

	serviceLED();
}
