#include <arduinoFFT.h>
#include <Adafruit_NeoPixel.h>

// configuration

// the number of samples (must be a power of 2)
#define SAMPLES 256
// the number of leds (max: SAMPLES / 2)
#define N_LEDS 128
// The Pin where the data for the led strip is connected
#define LED_STRIP_PIN 4
// flash the strip after calibrating the analog reference
#define FLASH_AFTER_CALIBRATING true

// end of configuration

double vReal[SAMPLES];
double vImag[SAMPLES];
char data_avgs[N_LEDS];

int value;
int peaks[N_LEDS];
int maxVol = 255; // when sampling, the min and max are taken and the difference used as the volume, this is the max difference
int analogOffset = 0;

arduinoFFT FFT = arduinoFFT();

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


typedef struct {
    double r;       // a fraction between 0 and 1
    double g;       // a fraction between 0 and 1
    double b;       // a fraction between 0 and 1
} rgb;

// returns an adafruit color
rgb hsv2rgb(int h, int s, int v) {
    double      hh, p, q, t, ff;
    long        i;
    rgb         out;

    if (s <= 0.0) {      // < is bogus, just shuts up warnings
        out.r = v;
        out.g = v;
        out.b = v;
        return out;
    }
    hh = h;
    if (hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = v * (1.0 - s);
    q = v * (1.0 - (s * ff));
    t = v * (1.0 - (s * (1.0 - ff)));

    switch (i) {
    case 0:
        out.r = v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = v;
        out.b = t;
        break;
    case 3:
        out.r = p;
        out.g = q;
        out.b = v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = v;
        break;
    case 5:
    default:
        out.r = v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;
}


void setup() {
    // Serial.begin(115200);

    // clear the strip
    strip.begin();
    strip.clear();
    strip.show();

    // wait to get voltage stabilized
    delay(50);

    // calibrating analog reference (music has to be off for this)
    for (int i = 0; i < 64; i++) {
        analogOffset += analogRead(A0);
        delay(1);
    }
    analogOffset = round(analogOffset / 64.0);

    // flash after calibrating
    if (FLASH_AFTER_CALIBRATING) {
        for (int i = 0; i < N_LEDS; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 255));
            if (i != 0) {
                strip.setPixelColor(i-1, 0);
            }
            strip.show();
            delay(1);
        }
        strip.clear();
        strip.show();
    }
}

void loop() {
    // sampling
    int min = 0;
    int max = 0;
    for(int i=0; i<SAMPLES; i++) {
        // read the value and subtract offset
        int value = analogRead(A0) - analogOffset ;
        vReal[i] = value / 8;
        vImag[i] = 0;
        min = value < min ? value : min;
        max = value > max ? value : max;
    }
    float volFactor = (float) (max - min) / maxVol;

    // FFT
    FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

    // match the values with the number of leds
    int step = (SAMPLES / 2) / N_LEDS;
    int c = 0;
    for(int i = 0; i < (SAMPLES / 2); i += step) {
        data_avgs[c] = 0;
        for (int k = 0; k < step; k++) {
            data_avgs[c] = data_avgs[c] + vReal[i+k];
        }
        data_avgs[c] = data_avgs[c] / step;
        c++;
    }

    // show on strip
    rgb color;
    for(int i = 0; i < N_LEDS; i++) {
        // set max & min values
        data_avgs[i] = constrain(data_avgs[i], 0, 80);
        // remap averaged values to brightness
        data_avgs[i] = map(data_avgs[i], 0, 80, 0, 255);
        value = data_avgs[i];

        // decay from last state by one light
        peaks[i] = peaks[i] - 1;
        if (value > peaks[i])
            peaks[i] = value ;
        value = peaks[i];
        value = value >= 0 ? value : 0;

        // color = hsv2rgb(value, 1, 1); // to 255 not 360
        // strip.setPixelColor(i , strip.Color(color.r * value, color.g * value, color.b * value));
        color = hsv2rgb(i * 360.0 / N_LEDS, 1, 1);
        strip.setPixelColor(i, strip.Color(color.r * value * volFactor, color.g * value * volFactor, color.b * value * volFactor));
    }
    strip.show();
}
