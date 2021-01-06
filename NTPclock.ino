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

//Global variables
bool failSafe = false;
strDateTime dateTime;
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
  if (failSafe)
  {
    return false;
  }
  dateTime = NTPhu.getNTPtime(1.0, 1);
  if ((dateTime.year < YEAR_MIN) || (dateTime.year > YEAR_MAX))
  {
    return false;
  }
  else
  {
    return dateTime.valid;
  }
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

void setup() {
  bool initRefresh = false;
  unsigned int refreshCounter = 0;

  display.clear();
  display.setBrightness(BRIGHTNESS, true);
  WiFi_Config();
  if (!failSafe)
  {
    while (!initRefresh)
    {
      initRefresh = RefreshDateTime();
      delay(1000);
      refreshCounter++;
      if (refreshCounter > 59)
      {
        break;
      }
    }
    RefreshDisplay(dateTime.hour, dateTime.minute);
  }
  else
  {
    display.setSegments(SEG_FAIL);
  }
}

void loop() {
  static unsigned long lastRefresh;
  unsigned int refreshCnt = 0;
  bool validRefresh = false;
  byte storedHour;

  if ((millis() - lastRefresh) > REFRESH)
  {
    dateTime.minute++;
    if (dateTime.minute > 59)
    {
      storedHour = dateTime.hour;
      while (!validRefresh)
      {
        validRefresh = RefreshDateTime();
        delay(1000);
        refreshCnt++;
        if (refreshCnt > 59)
        {
          dateTime.minute = 1;
          dateTime.hour = storedHour + 1;
          if (dateTime.hour > 23)
          {
            dateTime.hour = 0;
          }
          break;
        }
      }
    }
    RefreshDisplay(dateTime.hour, dateTime.minute);
    lastRefresh = millis();
    validRefresh = false;
  }
}
