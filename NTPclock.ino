//Includes
#include <TM1637Display.h>
#include <NTPtimeESP.h>
#include <credentials.h>

// Defines
#define CLK         14
#define DIO         12
#define NTPSERVER   "hu.pool.ntp.org"
#define YEAR_MIN    2020
#define YEAR_MAX    2035
#define TIMEOUT     5000    //5 sec
#define BRIGHTNESS  2       //brightness
#define REFRESH     60000   //1min
#define MAXREFRESH  59
#define MAXMIN      59
#define MAXHOUR     23

//Global variables
bool failSafe = false;
strDateTime dateTime;
unsigned long lastRefresh;
const uint8_t SEG_FAIL[] = {
  SEG_A | SEG_E | SEG_F ,                          // F
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,   // A
  SEG_E | SEG_F,                                   // I
  SEG_D | SEG_E | SEG_F                            // L
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

void WaitForMinute(unsigned long waitUntilNextMinute)
{
  while (waitUntilNextMinute > millis())
  {
    delay(1000);
  }
  lastRefresh = millis();
  dateTime.minute++;
  if (dateTime.minute > MAXMIN)
  {
    dateTime.minute = 0;
    dateTime.hour++;
    if (dateTime.hour > MAXHOUR)
    {
      dateTime.hour = 0;
    }
  }
  RefreshDisplay(dateTime.hour, dateTime.minute);
}

void setup() {
  bool initRefresh = false;
  unsigned long waitForExactMinute;

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
  waitForExactMinute = millis() + REFRESH - dateTime.second * 1000;
  WaitForMinute(waitForExactMinute);
}

void loop() {
  bool validRefresh = false;
  byte storedHour;
  unsigned long waitForNextMinute;

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
        waitForNextMinute = millis() + REFRESH - dateTime.second * 1000;
        WaitForMinute(waitForNextMinute);
        return;
      }
    }
    RefreshDisplay(dateTime.hour, dateTime.minute);
    lastRefresh = millis();
  }
}
