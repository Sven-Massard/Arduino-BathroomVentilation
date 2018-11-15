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
//Variables
float hum;  //Stores humidity value
float temp; //Stores temperature value
int lastButtonPlusState = LOW;
int lastButtonMinusState = LOW;
int buttonPlusReading;
int buttonMinusReading;

//Delayzeugs
unsigned long minLoopDuration = 100;        // So lange soll eine Loop minimal dauern
unsigned long timeLoopStart = 0;            // Zeit, bei der die Loop gestartet ist
unsigned long loopduration = 0;             // So lange hat die Loop gedauert
unsigned long dhtLastMeasure = 0;           // Zeit, bei der DHT das letzte Mal gemessen hat
unsigned long dhtMeasureDelay = 1000;       // DHT soll alle so viel Millisekunden messen
unsigned long timeToStopVentilation = 0;    // Zeit, ab der die Lüftung abschalten soll
unsigned long timeVentilationRemaining = 0; // Zeit, die die Lüftung noch an sein soll
unsigned long timeLoopDelay = 0;
unsigned long debounceDelay = 150; // Sollte weniger als die waittime sein.
unsigned long lastDebounceTimePlus = 0;
unsigned long lastDebounceTimeMinus = 0;

void printDisplay()
{
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Humidity: ");
  display.print(hum);
  display.println("%");
  display.print("Temp: ");
  display.print(temp);
  display.println(" Celsius");
  display.print("Restzeit: ");
  display.print(timeVentilationRemaining / 1000);
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
  timeLoopStart = millis();                          // We are using this variable instead of millis() to save cycles
  if ((timeLoopStart - dhtLastMeasure) > dhtMeasureDelay) // Measure at most once per second
  {
    Serial.print("Measuring!");
    Serial.println(timeLoopStart - dhtLastMeasure);
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    dhtLastMeasure = timeLoopStart;
  }

  // Buttonzeugs mit Debouncing
  // Plusschalter
  buttonPlusReading = digitalRead(SCHALTERPLUS); //LOW = Pressed

  if (buttonPlusReading == HIGH)
  {
    lastButtonPlusState = HIGH;
  }

  else if (buttonPlusReading == LOW && lastButtonPlusState == HIGH)
  {
    lastDebounceTimePlus = timeLoopStart;
    lastButtonPlusState = LOW;
  }

  else if ((timeLoopStart - lastDebounceTimePlus) > debounceDelay && buttonPlusReading == LOW && lastButtonPlusState == LOW)
  {
    timeToStopVentilation = timeToStopVentilation + 60000;
    lastButtonPlusState = HIGH;
  }

  // Minusschalter
  buttonMinusReading = digitalRead(SCHALTERMINUS);

  if (buttonMinusReading == HIGH)
  {
    lastButtonMinusState = HIGH;
  }

  else if (buttonMinusReading == LOW && lastButtonMinusState == HIGH)
  {
    lastDebounceTimeMinus = timeLoopStart;
    lastButtonMinusState = LOW;
  }

  else if ((timeLoopStart - lastDebounceTimeMinus) > debounceDelay && buttonMinusReading == LOW && lastButtonMinusState == LOW)
  {
    timeToStopVentilation = timeToStopVentilation - 60000;
    lastButtonMinusState = HIGH;
  }
  // Ende Buttonzeugs


  // Verbleibende Zeit ausrechnen.
  if (timeToStopVentilation > timeLoopStart && timeVentilationRemaining < 10000000) // Sometimes, minus button can cause overflow. We correct this here.
  {
    timeVentilationRemaining = timeToStopVentilation - timeLoopStart;
  }
  else
  {
    timeToStopVentilation = timeLoopStart;
    timeVentilationRemaining = 0;
  }

  // Auswerten und reagieren
  if (hum > 80 && timeVentilationRemaining < 180000)
  { // Wenn Feuchtigkeit über 80 Prozent und Zeit weniger als 3 Minuten ist, Zeit auf 3 Minuten setzen.
    timeToStopVentilation = timeLoopStart + 180000;
  }
  if (timeVentilationRemaining > 0)
  { //Lueftung anschalten
    digitalWrite(RELAIS, LOW);
  }
  else
  {
    digitalWrite(RELAIS, HIGH);
  }
  loopduration = millis() - timeLoopStart;        //loopduration = Millisekunden, die die Loop gedauert hat.
  timeLoopDelay = minLoopDuration - loopduration; // Bufferoverflow wenn loopduration > minLoopDuration
  if (timeLoopDelay > minLoopDuration)            // Falls Bufferoverflow, reset
  {
    timeLoopDelay = 0;
  }
  printDisplay();

  delay(timeLoopDelay); //minLoopDuration - loopduration = Anzahl Millisekunden, die noch gewartet werden muss.
}
