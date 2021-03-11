//Includes
#include <TM1637Display.h>
#include <NTPtimeESP.h>
#include <credentials.h>

// Defines
#define BUT1        4
#define BUT2        5
#define CLK         14
#define DIO         12
#define NTPSERVER   "hu.pool.ntp.org"
#define YEAR_MIN    2020
#define YEAR_MAX    2035
#define TIMEOUT     5000    //5 sec
#define BRIGHTNESS  7       //brightness during day
#define NIGHTBRIGHT 2       //brightness during night
#define REFRESH     60000   //1min
#define MAXREFRESH  59
#define MAXMIN      59
#define MAXHOUR     23
#define MAXBYTE     254
#define NIGHTBEFORE 7
#define NIGHTAFTER  21
#define SETUPDELAY  500


//Enum
enum SETUP_SM {
  INIT				    = 0,
  MAIN				    = 1,
  WAITFORSETUP		= 2,
  SETUP				    = 3,
  WAITFORRESTART	= 4,
  RESTART			    = 5,
  RESERVED			  = 9,
};


//Global variables
bool failSafe = false;
strDateTime dateTime;
unsigned long lastRefresh;
const uint8_t SEG_FAIL[] = {
  SEG_A | SEG_E | SEG_F | SEG_G,                   // F
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,   // A
  SEG_E | SEG_F,                                   // I
  SEG_D | SEG_E | SEG_F                            // L
};

const uint8_t SEG_SET[] = {
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,           // S
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,		       // E
  SEG_D | SEG_E | SEG_F | SEG_G,                   // t
  0
};


//Init services
TM1637Display display(CLK, DIO);
NTPtime NTPhu(NTPSERVER);

void RefreshDisplay(byte dispH, byte dispM)
{
  display.showNumberDecEx(dispH, 0x40, false, 2, 0);
  display.showNumberDecEx(dispM, 0x00, true,  2, 2);
}

bool RefreshDateTime()
{
  bool timeIsValid = false;
  unsigned int refreshCounter = 0;

  if (failSafe)
  {
    return false;
  }
  while (!timeIsValid)
  {
    refreshCounter++;
    if (refreshCounter > MAXREFRESH)
    {
      break;
    }
    dateTime = NTPhu.getNTPtime(1.0, 1);
    if ((dateTime.year < YEAR_MIN) || (dateTime.year > YEAR_MAX))
    {
      timeIsValid = false;
    }
    else
    {
      timeIsValid = dateTime.valid;
    }
    if (!timeIsValid)
    {
      delay(1000);
    }
  }
  return timeIsValid;
}

void WiFi_Config()
{
  unsigned long wifitimeout = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin (ssid, password);
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (millis() - wifitimeout > TIMEOUT)
    {
      failSafe = true;
      break;
    }
  }
}

void WaitForMinute(unsigned long waitfromPrevMinute)
{
  while (millis() - waitfromPrevMinute < REFRESH )
  {
    delay(1000);
  }
  lastRefresh = millis();
  dateTime.minute++;
  TimeLimiter();
  RefreshDisplay(dateTime.hour, dateTime.minute);
}

void AutoBrightness()
{
  static byte actualBrightness = BRIGHTNESS;
  byte newBrightness;

  if (failSafe)
  {
    return;
  }
  if ((dateTime.hour < NIGHTBEFORE) || (dateTime.hour > NIGHTAFTER))
  {
    newBrightness = NIGHTBRIGHT;
  }
  else
  {
    newBrightness = BRIGHTNESS;
  }
  if (newBrightness != actualBrightness)
  {
    display.setBrightness(newBrightness, true);
    actualBrightness = newBrightness;
  }
}

void ButtonCheck()
{
  byte pressedButtons;
  static bool firstPress = true;
  static SETUP_SM setupState = INIT;
  static SETUP_SM prevState = INIT;
  static unsigned long stateEnterTime;
  static unsigned long pressStartTime;
  static byte setupStep = 1;

  pressedButtons = ((!digitalRead(BUT1) << 1) | (!digitalRead(BUT2))); //3:BOTH, 2:BUT1, 1:BUT2, 0: NEITHER
  
  switch (setupState)
  {
    case INIT:
      {
        setupState = MAIN;
        break;
      }

    case MAIN:
      {
        if (3 == pressedButtons)
        {
          stateEnterTime = millis();
          setupState = WAITFORSETUP;
        }
        break;
      }

    case WAITFORSETUP:
      {
        if (pressedButtons < 3)
        {
          setupState = MAIN;
          break;
        }
        else if (millis() - stateEnterTime > 3000)
        {
          dateTime.hour = 0;
          dateTime.minute = 0;
          display.setSegments(SEG_SET);
          setupState = SETUP;
          prevState = WAITFORSETUP;
        }
        else
        {
          break;
        }
        break;
      }

    case SETUP:
      {
        if (3 == pressedButtons)
        {
          if (WAITFORSETUP == prevState)
          {
            stateEnterTime = millis();
            setupState = WAITFORRESTART;
            break;
          }
          else
          {
            firstPress = true;
            setupState = MAIN;
            break;
          }
        }
        if (1 == pressedButtons)
        {
          if (firstPress)
          {
            pressStartTime = millis();
            firstPress = false;
          }
          setupStep = CalculateStep(pressStartTime);
          dateTime.minute -= setupStep;
          TimeLimiter();
          RefreshDisplay(dateTime.hour, dateTime.minute);
          lastRefresh = millis();
          delay(SETUPDELAY);
          break;
        }
        if (2 == pressedButtons)
        {
          if (firstPress)
          {
            pressStartTime = millis();
            firstPress = false;
          }
          setupStep = CalculateStep(pressStartTime);
          dateTime.minute += setupStep;
          TimeLimiter();
          RefreshDisplay(dateTime.hour, dateTime.minute);
          lastRefresh = millis();
          delay(SETUPDELAY);
          break;
        }
        firstPress = true;
        break;
      }

    case WAITFORRESTART:
      {
        if (pressedButtons < 3)
        {
          setupState = SETUP;
          prevState = WAITFORRESTART;
          break;
        }
        else if (millis() - stateEnterTime > 3000)
        {
          RefreshDisplay(88, 88);
          setupState = RESTART;
        }
        else
        {
          break;
        }
        break;
      }

    case RESTART:
      {
        delay(1000);
        ESP.restart();
        break;
      }
  }
}

void TimeLimiter()
{
  if (dateTime.minute > MAXBYTE)
  {
    dateTime.minute = MAXMIN;
    dateTime.hour--;
    if (dateTime.hour > MAXBYTE)
    {
      dateTime.hour = MAXHOUR;
    }
  }
  else if (dateTime.minute > MAXMIN)
  {
    dateTime.minute = 0;
    dateTime.hour++;
    if (dateTime.hour > MAXHOUR)
    {
      dateTime.hour = 0;
    }
  }
  else
  {
    return;
  }
}

byte CalculateStep(unsigned long pressStart)
{
  unsigned long buttonPressed;

  buttonPressed = millis() - pressStart;

  if (buttonPressed > 16 * SETUPDELAY)
  {
    return (60);
  }
  if (buttonPressed > 10 * SETUPDELAY)
  {
    return (10);
  }
  return (1);
}

void setup() {
  bool initRefresh = false;
  unsigned long prevMinute;

  Serial.begin(115200);
  pinMode(BUT1, INPUT_PULLUP);
  pinMode(BUT2, INPUT_PULLUP);

  display.clear();
  display.setBrightness(BRIGHTNESS, true);
  WiFi_Config();
  initRefresh = RefreshDateTime();
  if (initRefresh)
  {
    RefreshDisplay(dateTime.hour, dateTime.minute);
  }
  else
  {
    display.setSegments(SEG_FAIL);
  }
  prevMinute = millis() - dateTime.second * 1000;
  WaitForMinute(prevMinute);
}

void loop() {
  bool validRefresh = false;
  byte storedHour;
  unsigned long prevMinute;

  ButtonCheck();

  if ((millis() - lastRefresh) > REFRESH)
  {
    dateTime.minute++;
    if (dateTime.minute > MAXMIN)
    {
      storedHour = dateTime.hour;
      validRefresh = RefreshDateTime();
      if (!validRefresh)
      {
        dateTime.minute = 1;
        dateTime.hour = storedHour + 1;
        if (dateTime.hour > MAXHOUR)
        {
          dateTime.hour = 0;
        }
      }
      else
      {
        RefreshDisplay(dateTime.hour, dateTime.minute);
        prevMinute = millis() - dateTime.second * 1000;
        WaitForMinute(prevMinute);
        return;
      }
    }
    AutoBrightness();
    RefreshDisplay(dateTime.hour, dateTime.minute);
    lastRefresh = millis();
  }
}
