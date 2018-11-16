/* How to use the DHT-22 sensor with Arduino uno
   Temperature and humidity sensor
*/

//Libraries
#include <DHT.h>             // Feuchtigkeitsensor
#include <Adafruit_Sensor.h> // Dependance DHT.h
//#include <SPI.h>
//#include <Wire.h>
//#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h> // OLED Display

//Constants
#define DHTPIN 5 // what pin we're connected to
#define OLED_RESET 4
#define RELAIS 7
#define DHTTYPE DHT22 // DHT 22  (AM2302)
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH 16

// Schalter
#define SCHALTERPLUS 8
#define SCHALTERMINUS 9

Adafruit_SSD1306 display(OLED_RESET);

static const unsigned char PROGMEM logo16_glcd_bmp[] =
    {B00000000, B11000000,
     B00000001, B11000000,
     B00000001, B11000000,
     B00000011, B11100000,
     B11110011, B11100000,
     B11111110, B11111000,
     B01111110, B11111111,
     B00110011, B10011111,
     B00011111, B11111100,
     B00001101, B01110000,
     B00011011, B10100000,
     B00111111, B11100000,
     B00111111, B11110000,
     B01111100, B11110000,
     B01110000, B01110000,
     B00000000, B00110000};

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

// Delay and debouncing variables
#define MINLOOPDURATION 100       // So lange soll eine Loop minimal dauern
#define DHTMEASUREDELAY 1000      // DHT soll alle so viel Millisekunden messen
#define DEBOUNCEDELAY 150         // Sollte weniger als die waittime sein.
#define HUMIDITYTHRESHOLD 80      //above this threshold, ventilation will switch on
#define PASTTHRESHOLDDELAY 180000 // time in ms that ventilation will stay on after going below threshold
#define BUTTONTIME 60000          // time one plusbutton or minusbutton will add or substract

// Variables
float humidity;
float temperature;
unsigned long ventilationTimestampStop = 0; // timestamp at which ventilation switches off
unsigned long VentilationTimeRemaining = 0; // remaining time ventilation stays on in seconds

unsigned long loopDelayTime = 0;           // delay end of loop so loop duration is at least MINLOOPDURATION
unsigned long loopStartTimestamp = 0;      // timestamp at which loop started
unsigned long loopduration = 0;            // time the loop took before delaying
unsigned long dhtLastMeasureTimestamp = 0; // timestamp at which temperature and humidity was last measured

// Buttons: HIGH = not pressed, LOW = pressed
int plusbuttonLastState = HIGH; // Button state on previous loop
int minusbuttonLastState = HIGH;
int plusbuttonReading = HIGH;
int minusbuttonReading = HIGH;

//timestamp at which buttons started being pressed for debouncing purposes
unsigned long plusbuttonLastPressedTimestamp = 0;
unsigned long minusbuttonLastPressedTimestamp = 0;

void printDisplay()
{
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Humidity: ");
  display.print(humidity);
  display.println("%");
  display.print("Temp: ");
  display.print(temperature);
  display.println(" Celsius");
  display.print("Restzeit: ");
  display.print(VentilationTimeRemaining / 1000);
  display.println(" Sekunden");
  display.display();
  display.clearDisplay();
}

void setup()
{
  pinMode(SCHALTERPLUS, INPUT);
  digitalWrite(SCHALTERPLUS, HIGH);
  pinMode(SCHALTERMINUS, INPUT);
  digitalWrite(SCHALTERMINUS, HIGH);
  pinMode(RELAIS, OUTPUT);
  dht.begin();
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop()
{
  // We are often using loopStartTimestamp instead of millis() to save cycles
  loopStartTimestamp = millis();
  // Measure humidity and temperature only every DHTMEASUREDELAY
  if ((loopStartTimestamp - dhtLastMeasureTimestamp) > DHTMEASUREDELAY)
  {
    Serial.print("Measuring!");
    Serial.println(loopStartTimestamp - dhtLastMeasureTimestamp);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    dhtLastMeasureTimestamp = loopStartTimestamp;
  }

  // Buttonzeugs mit Debouncing
  // Plusschalter
  plusbuttonReading = digitalRead(SCHALTERPLUS); //LOW = Pressed

  if (plusbuttonReading == HIGH)
  {
    plusbuttonLastState = HIGH;
  }

  else if (plusbuttonReading == LOW && plusbuttonLastState == HIGH)
  {
    plusbuttonLastPressedTimestamp = loopStartTimestamp;
    plusbuttonLastState = LOW;
  }

  else if ((loopStartTimestamp - plusbuttonLastPressedTimestamp) > DEBOUNCEDELAY && plusbuttonReading == LOW && plusbuttonLastState == LOW)
  {
    ventilationTimestampStop = ventilationTimestampStop + BUTTONTIME;
    plusbuttonLastState = HIGH;
  }

  // Minusschalter
  minusbuttonReading = digitalRead(SCHALTERMINUS);

  if (minusbuttonReading == HIGH)
  {
    minusbuttonLastState = HIGH;
  }

  else if (minusbuttonReading == LOW && minusbuttonLastState == HIGH)
  {
    minusbuttonLastPressedTimestamp = loopStartTimestamp;
    minusbuttonLastState = LOW;
  }

  else if ((loopStartTimestamp - minusbuttonLastPressedTimestamp) > DEBOUNCEDELAY && minusbuttonReading == LOW && minusbuttonLastState == LOW)
  {
    ventilationTimestampStop = ventilationTimestampStop - BUTTONTIME;
    minusbuttonLastState = HIGH;
  }
  // Ende Buttonzeugs

  // Verbleibende Zeit ausrechnen.
  if (ventilationTimestampStop > loopStartTimestamp && VentilationTimeRemaining < 10000000) // Sometimes, minus button can cause overflow. We correct this here.
  {
    VentilationTimeRemaining = ventilationTimestampStop - loopStartTimestamp;
  }
  else
  {
    ventilationTimestampStop = loopStartTimestamp;
    VentilationTimeRemaining = 0;
  }

  // Auswerten und reagieren
  if (humidity > HUMIDITYTHRESHOLD && VentilationTimeRemaining < PASTTHRESHOLDDELAY)
  { // Wenn Feuchtigkeit über HUMIDITYTHRESHOLD Prozent und Zeit weniger als 3 Minuten ist, Zeit auf 3 Minuten setzen.
    ventilationTimestampStop = loopStartTimestamp + PASTTHRESHOLDDELAY;
  }
  if (VentilationTimeRemaining > 0)
  { //Lueftung anschalten
    digitalWrite(RELAIS, LOW);
  }
  else
  {
    digitalWrite(RELAIS, HIGH);
  }
  loopduration = millis() - loopStartTimestamp;   //loopduration = Millisekunden, die die Loop gedauert hat.
  loopDelayTime = MINLOOPDURATION - loopduration; // Bufferoverflow wenn loopduration > MINLOOPDURATION
  if (loopDelayTime > MINLOOPDURATION)            // Falls Bufferoverflow, reset
  {
    loopDelayTime = 0;
  }
  printDisplay();

  delay(loopDelayTime); //MINLOOPDURATION - loopduration = Anzahl Millisekunden, die noch gewartet werden muss.
}
