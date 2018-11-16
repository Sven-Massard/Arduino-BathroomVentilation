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

// Pins
#define DHTPIN 5 
#define OLED_RESET 4
#define RELAIS 7 // Switches ventilation on or off
#define PLUSBUTTON 8
#define MINUSBUTTON 9

// Other variables
#define DHTTYPE DHT22 // DHT 22  (AM2302)
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH 16

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

// Delay and debouncing constants
#define MINLOOPDURATION 100       // So lange soll eine Loop minimal dauern
#define DHTMEASUREDELAY 1000      // DHT soll alle so viel Millisekunden messen
#define DEBOUNCEDELAY 150         // Sollte weniger als die waittime sein.
#define HUMIDITYTHRESHOLD 80      //above this threshold, ventilation will switch on
#define PASTTHRESHOLDDELAY 180000 // time in ms that ventilation will stay on after going below threshold
#define VENTILATIONMAXTIME 3600000 // Max time in milliseconds the ventilation can run. Above, timer will reset to 0
#define BUTTONTIME 60000          // time one plusbutton or minusbutton will add or substract
// Variables
float humidity;
float temperature;
unsigned long ventilationStopTimestamp = 0; // timestamp at which ventilation switches off
unsigned long ventilationRemainingTime = 0; // remaining time ventilation stays on in seconds

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
  display.print(ventilationRemainingTime / 1000);
  display.println(" Sekunden");
  display.display();
  display.clearDisplay();
}

void setup()
{
  pinMode(PLUSBUTTON, INPUT);
  digitalWrite(PLUSBUTTON, HIGH);
  pinMode(MINUSBUTTON, INPUT);
  digitalWrite(MINUSBUTTON, HIGH);
  pinMode(RELAIS, OUTPUT);
  dht.begin();
  Serial.begin(9600);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
}

void loop()
{
  // We are often using loopStartTimestamp instead of millis() to save cycles
  loopStartTimestamp = millis();
  // Measure humidity and temperature only every DHTMEASUREDELAY ms
  if ((loopStartTimestamp - dhtLastMeasureTimestamp) > DHTMEASUREDELAY)
  {
    Serial.print("Measuring!");
    Serial.println(loopStartTimestamp - dhtLastMeasureTimestamp);
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    dhtLastMeasureTimestamp = loopStartTimestamp;
  }

  // Start buttonhandling
  // Plus button
  plusbuttonReading = digitalRead(PLUSBUTTON); //LOW = Pressed

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
    ventilationStopTimestamp = ventilationStopTimestamp + BUTTONTIME;
    plusbuttonLastState = HIGH;
  }

  // Minus button
  minusbuttonReading = digitalRead(MINUSBUTTON);

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
    ventilationStopTimestamp = ventilationStopTimestamp - BUTTONTIME;
    minusbuttonLastState = HIGH;
  }
  
  // Start time calculations
  if (humidity > HUMIDITYTHRESHOLD && ventilationRemainingTime < PASTTHRESHOLDDELAY)
  { 
    ventilationStopTimestamp = loopStartTimestamp + PASTTHRESHOLDDELAY;
  }

  //Sometimes, minus button can cause overflow. We correct this with clause "ventilationRemainingTime < VENTILATIONMAXTIME)"
  if (ventilationStopTimestamp > loopStartTimestamp && ventilationRemainingTime < VENTILATIONMAXTIME)
  {
    ventilationRemainingTime = ventilationStopTimestamp - loopStartTimestamp;
  }
  else
  {
    ventilationStopTimestamp = loopStartTimestamp;
    ventilationRemainingTime = 0;
  }


  
  // Start ventilation on or off handling
  if (ventilationRemainingTime > 0)
  {
    digitalWrite(RELAIS, LOW); // Switches ventilation on
  }
  else
  {
    digitalWrite(RELAIS, HIGH);
  }

  // Make loop last at least MINLOOPDURATION
  loopduration = millis() - loopStartTimestamp;
  loopDelayTime = MINLOOPDURATION - loopduration; // Bufferoverflow if loopduration > MINLOOPDURATION
  if (loopDelayTime > MINLOOPDURATION)            // Catches bufferoverflow
  {
    loopDelayTime = 0;
  }
  printDisplay();

  delay(loopDelayTime);
}
