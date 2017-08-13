#include <Wire.h>
#include "TM1637.h"
#include "LedControlMS.h"

// Кнопки настроек
#define SETTINGS_MODE A0  // Выбор режима
#define SETTINGS_UP A1    // Вверх
#define SETTINGS_DOWN  A2 // Вниз

// Адрес модуля ds3231 на шине I2C
#define DS3231_I2C_ADDRESS 0x68

// Объект для работы со светодиодной матрицей
/*
 * Подключение
 * DIN - 12
 * CS  - 11
 * CLK - 10
 */
LedControl lc = LedControl(12,10,11,1);

// Режимы настроек
enum Setting 
{
  sSET_DAY = 1,
  sSET_MONTH = 2,
  sSET_HOUR = 3,
  sSET_MINUTE = 4,
  sSET_SHOW_SECONDS = 5,
  sSET_BRIGHTNESS = 6
};

// Столбцы на светодиодной матрице для отображения
// того или иного параметра времени
enum LcRow
{
  lrSECOND = 1,
  lrMINUTE = 2,
  lrHOUR = 3,
  lrMONTH = 5,
  lrDAY = 6
};

boolean settingsMode = false;   // Находимся ли в режиме настроек
byte currentSetting = 0;        // Текущий режим настроек (enum Setting)
byte showSeconds = 0xFF;        // Показывать ли секунды
byte brightness = 5;            // Яркость, от 0 до 8

// Текущее время
struct Time
{
  byte second;        // 0-59
  byte minute;        // 0-59
  byte hour;          // 1-23
  byte dayOfWeek;     // 1-7
  byte dayOfMonth;    // 1-28/29/30/31
  byte month;         // 1-12
  byte year;
} time_;

// Функции перевода чисел для работы с модулем RTC
byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val){
  return ( (val/16*10) + (val%16) );
}

// Установка времени в модуль RTC DS3231
void setDateDs3231(Time t)
{
   Wire.beginTransmission(DS3231_I2C_ADDRESS);
   Wire.write(0);
   Wire.write(decToBcd(t.second));    
   Wire.write(decToBcd(t.minute));
   Wire.write(decToBcd(t.hour));     
   Wire.write(decToBcd(t.dayOfWeek));
   Wire.write(decToBcd(t.dayOfMonth));
   Wire.write(decToBcd(t.month));
   Wire.write(decToBcd(t.year));
   Wire.endTransmission();
}

// Получение текущего времени из модуля RTC
void getDateDs3231(Time& t)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0);
  Wire.endTransmission();

  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);

  t.second     = bcdToDec(Wire.read() & 0x7f);
  t.minute     = bcdToDec(Wire.read());
  t.hour       = bcdToDec(Wire.read() & 0x3f); 
  t.dayOfWeek  = bcdToDec(Wire.read());
  t.dayOfMonth = bcdToDec(Wire.read());
  t.month      = bcdToDec(Wire.read());
  t.year       = bcdToDec(Wire.read());
}

//включает выход SQW в DS3231, который вроде выключен по умолчанию
void setINT()
{    
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0x0E);
  Wire.write(0x0);
  Wire.endTransmission();
}

// Первоначальная настройка светодиодной матрицы
void setupMatrix() {
  lc.shutdown(0, false);          // Включаем матрицу
  lc.setIntensity(0, brightness); // Устанавливаем яркость
  lc.clearDisplay(0);             // Очищаем дисплей
}

// Обновлять дисплей будем раз в секунду по прерыванию с ds3231
volatile bool needToUpdate = true;

void changeUpdate() {
  needToUpdate = true;
}

// Отображаем текущее время на матрице
void showDateTime(Time t)
{
  lc.setRow(0, lrSECOND, t.second & showSeconds);
  lc.setRow(0, lrMINUTE, t.minute);
  lc.setRow(0, lrHOUR, t.hour);

  lc.setRow(0, lrMONTH, t.month);
  lc.setRow(0, lrDAY, t.dayOfMonth);
}

// Обновление времени на матрице
void updateTime() 
{
  // Выключим обновление на следующий такт цикла - будем ждать прерывания
  needToUpdate = false;
  // Читаем время из модуля
  getDateDs3231(time_);
  // И показываем его 
  showDateTime(time_);
}

// Как будут отображаться настройки для того или иного режима
// В текущей реализации - показыватся точечка в самом верху
// над тем параметром, который изменяем
void showSettings( byte setting_mode )
{
  switch( setting_mode )
  {
    case sSET_DAY:
      lc.setRow(0, lrSECOND, time_.second & showSeconds);
      lc.setRow(0, lrMINUTE, time_.minute);
      lc.setRow(0, lrHOUR, time_.hour);
    
      lc.setRow(0, lrMONTH, time_.month);
      lc.setRow(0, lrDAY, time_.dayOfMonth | 0x80);
      break;
    case sSET_MONTH:
      lc.setRow(0, lrSECOND, time_.second & showSeconds);
      lc.setRow(0, lrMINUTE, time_.minute);
      lc.setRow(0, lrHOUR, time_.hour);
    
      lc.setRow(0, lrMONTH, time_.month | 0x80);
      lc.setRow(0, lrDAY, time_.dayOfMonth);
      break;
    case sSET_HOUR:
      lc.setRow(0, lrSECOND, time_.second & showSeconds);
      lc.setRow(0, lrMINUTE, time_.minute);
      lc.setRow(0, lrHOUR, time_.hour | 0x80);
    
      lc.setRow(0, lrMONTH, time_.month);
      lc.setRow(0, lrDAY, time_.dayOfMonth); 
      break;
    case sSET_MINUTE:
      lc.setRow(0, lrSECOND, time_.second & showSeconds);
      lc.setRow(0, lrMINUTE, time_.minute | 0x80);
      lc.setRow(0, lrHOUR, time_.hour);
    
      lc.setRow(0, lrMONTH, time_.month);
      lc.setRow(0, lrDAY, time_.dayOfMonth);  
      break;
    case sSET_SHOW_SECONDS:
      lc.setRow(0, lrSECOND, time_.second  & showSeconds | 0x80);
      lc.setRow(0, lrMINUTE, time_.minute);
      lc.setRow(0, lrHOUR, time_.hour);
    
      lc.setRow(0, lrMONTH, time_.month);
      lc.setRow(0, lrDAY, time_.dayOfMonth);  
      break;
    case sSET_BRIGHTNESS:
      // Для изменения яркости зальём все пиксели
      for( byte i = 0; i < 8; i++ )
        lc.setRow(0, i, 0xFF);
      break;
  }
}

// Увеличение значения параметра
void upSetting( byte setting )
{
  switch( setting )
  {
    case sSET_DAY:
      time_.dayOfMonth++;
      if( time_.dayOfMonth > 31 )
        time_.dayOfMonth = 1;
      break;
    case sSET_MONTH:
      time_.month++;
      if( time_.month > 12 )
        time_.month = 1;
      break;
    case sSET_HOUR:
      time_.hour++;
      if( time_.hour > 23 )
        time_.hour = 0;
      break;
    case sSET_MINUTE:
      time_.minute++;
      if( time_.minute > 59 )
        time_.minute = 0;
      break;
    case sSET_SHOW_SECONDS:
      if( showSeconds == 0xFF ) showSeconds = 0x00;
      else showSeconds = 0xFF;
      break;
    case sSET_BRIGHTNESS:
      if( brightness <= 8 ) brightness++;
      lc.setIntensity(0, brightness);
      break;
  }
}

// Уменьшение значения параметра
void downSetting( byte setting )
{
  switch( setting )
  {
    case sSET_DAY:
      time_.dayOfMonth--;
      if( time_.dayOfMonth < 1 )
        time_.dayOfMonth = 31;
      break;
    case sSET_MONTH:
      time_.month--;
      if( time_.month < 1 )
        time_.month = 12;
      break;
    case sSET_HOUR:
      time_.hour--;
      if( time_.hour > 0x90 )
        time_.hour = 23;
      break;
    case sSET_MINUTE:
      time_.minute--;
      if( time_.minute > 0x90 )
        time_.minute = 59;
      break;
    case sSET_SHOW_SECONDS:
      if( showSeconds == 0xFF ) showSeconds = 0x00;
      else showSeconds = 0xFF;
      break;
    case sSET_BRIGHTNESS:
      if( brightness > 0 ) brightness--;
      lc.setIntensity(0,brightness);
      break;
  }
}

// Настройка
void setup() {
  Wire.begin();
  pinMode(13, OUTPUT);
  pinMode(SETTINGS_MODE, INPUT_PULLUP);
  pinMode(SETTINGS_UP, INPUT_PULLUP);
  pinMode(SETTINGS_DOWN, INPUT_PULLUP);
  
  setINT();         // Настроим модуль RTC
  setupMatrix();    // Настроим матрицу  
  // Подключим обработчик прерывания с RTC модуля
  attachInterrupt(0, changeUpdate, RISING);
}

void loop(){
  if( needToUpdate && !settingsMode )
    updateTime();

  // Нажатие на кнопку выбора режима
  if( !digitalRead(SETTINGS_MODE) )
  {
    delay(200);
    settingsMode = true;
    // Переходим от одного пункта к другому
    if( currentSetting < sSET_BRIGHTNESS )
      currentSetting++;
    // Если прошли последний - запишем время в модуль DS3231 
    // (вдруг изменили в режиме настроек)
    // и выйдем из режима настроек
    else
    {
      setDateDs3231(time_);
      currentSetting = 0;
      settingsMode = false;
      lc.clearDisplay(0);
    }
  }

  // Режим настроек
  if( settingsMode )
  {
    showSettings( currentSetting );
    // Нажатие кнопки вверх
    if( !digitalRead(SETTINGS_UP) )
    {
      upSetting( currentSetting );
      delay(200);
    }
    // Нажатие кнопки вниз
    if( !digitalRead(SETTINGS_DOWN) )
    {
      downSetting( currentSetting );
      delay(200);
    }
  }
}

