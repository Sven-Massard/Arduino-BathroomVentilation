
//Libraries
#include <DHT.h>              // Feuchtigkeitsensor
#include <Adafruit_Sensor.h>  // Dependance DHT.h
#include <Adafruit_SSD1306.h> // OLED Display

// Pins
#define OLED_RESET -1 // Pseudo value, display has no reset pin
#define DHTPIN 5
#define RELAIS 7 // Switches ventilation on or off
#define PLUSBUTTON 8
#define MINUSBUTTON 9
#define OFFBUTTON 10

// Other variables
#define DHTTYPE DHT11
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH 16

Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

DHT dht(DHTPIN, DHTTYPE); //// Initialize DHT sensor for normal 16mhz Arduino

// Delay and debouncing constants
// #define MINLOOPDURATION 50        // So lange soll eine Loop minimal dauern
#define DHTMEASUREDELAY 1000       // DHT sensor is slow. 1s yields good results
#define DEBOUNCEDELAY 200          //
#define HUMIDITYTHRESHOLD 80       //above this threshold, ventilation will switch on
#define PASTTHRESHOLDDELAY 180000  // time in ms that ventilation will stay on after going below threshold
#define VENTILATIONMAXTIME 3600000 // Max time in milliseconds the ventilation can run. Above, timer will reset to 0
#define BUTTONTIME 60000           // time one plusbutton or minusbutton will add or substract

// Variables
float humidity;
float temperature;
bool turnedOff = false;
unsigned long ventilationStopTimestamp = 0; // timestamp at which ventilation switches off
unsigned long ventilationRemainingTime = 0; // remaining time ventilation stays on in seconds
// unsigned long loopDelayTime = 0;            // delay end of loop so loop duration is at least MINLOOPDURATION
unsigned long loopStartTimestamp = 0; // timestamp at which loop started
// unsigned long loopduration = 0;             // time the loop took before delaying
unsigned long dhtLastMeasureTimestamp = 0; // timestamp at which temperature and humidity was last measured
unsigned long minutes = 0;
unsigned long seconds = 0;

// Buttons: HIGH = not pressed, LOW = pressed
int plusbuttonLastState = HIGH; // Button state on previous loop
int minusbuttonLastState = HIGH;
int offbuttonLastState = HIGH;

int plusbuttonReading = HIGH;
int minusbuttonReading = HIGH;
int offbuttonReading = HIGH;

//timestamp at which buttons started being pressed for debouncing purposes
unsigned long plusbuttonLastPressedTimestamp = 0;
unsigned long minusbuttonLastPressedTimestamp = 0;
unsigned long offbuttonLastPressedTimestamp = 0;

void printDisplay()
{
  minutes = ventilationRemainingTime / 60000;
  seconds = (ventilationRemainingTime % 60000) / 1000;
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Humidity: ");
  display.print(humidity);
  display.println("%");
  display.print("Temp: ");
  display.print(temperature);
  display.println(" Celsius");
  if (turnedOff)
  {
    display.print("OFF");
  }
  else
  {
    display.print("Restzeit: ");
    display.print(minutes);
    display.print("m");
    display.print(seconds);
    display.println("s");
  }
  display.display();
  display.clearDisplay();
}

void setup()
{
  pinMode(PLUSBUTTON, INPUT);
  digitalWrite(PLUSBUTTON, HIGH);
  pinMode(MINUSBUTTON, INPUT);
  digitalWrite(MINUSBUTTON, HIGH);
  pinMode(OFFBUTTON, INPUT);
  digitalWrite(OFFBUTTON, HIGH);
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

  // Off button
  offbuttonReading = digitalRead(OFFBUTTON);

  if (offbuttonReading == HIGH)
  {
    offbuttonLastState = HIGH;
  }

  else if (offbuttonReading == LOW && offbuttonLastState == HIGH)
  {
    offbuttonLastPressedTimestamp = loopStartTimestamp;
    offbuttonLastState = LOW;
  }
  else if ((loopStartTimestamp - offbuttonLastPressedTimestamp) > DEBOUNCEDELAY)
  {
    turnedOff = !turnedOff;
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
  if (turnedOff)
  {
    ventilationStopTimestamp = loopStartTimestamp;
    ventilationRemainingTime = 0;
  }

  if (ventilationRemainingTime > 0)
  {
    digitalWrite(RELAIS, LOW); // Switches ventilation on
  }
  else
  {
    digitalWrite(RELAIS, HIGH);
  }

  // Make loop last at least MINLOOPDURATION
  /* loopduration = millis() - loopStartTimestamp;
  loopDelayTime = MINLOOPDURATION - loopduration; // Bufferoverflow if loopduration > MINLOOPDURATION
  if (loopDelayTime > MINLOOPDURATION)            // Catches bufferoverflow
  {
    loopDelayTime = 0;
  } */
  printDisplay();

  // delay(loopDelayTime);
}
