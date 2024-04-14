#include <RTClib.h>
#include <Wire.h>
#include "pitches.h"
#include <LowPower.h>
RTC_DS3231 rtc;

#define led_pin_1 4
#define led_pin_2 5
#define led_pin_3 6
#define led_pin_4 7
#define buzzer 8
#define CLOCK_INTERRUPT_PIN 2
#define KONTAKTRON_INTERRUPT_PIN 3

// indeks alarmu
int i = 0;

// pierwsze uruchomienia
int start=0;

// ustawienie alarmu
bool set_alarm=false;

// liczba alarmów 
int const alarm_num=4;

// tabela z alarmami {godzina,minuta,led}
int Alarms[alarm_num][3]={
  {19,18,led_pin_1},
  {19,18,led_pin_2},
  {19,19,led_pin_3},
  {19,20,led_pin_4}
};
//melodia buzzera
int melody[] = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};
int noteDurations[] = {
  4, 8, 8, 4, 4, 4, 4, 4
};

void setup() {
    Serial.begin(9600);
    Serial.println(start);
    // inicjalizacja rtc
    if(!rtc.begin()) {
        Serial.println("Nie można znaleźć RTC!");
        Serial.flush();
        while (1) delay(10);
    }
    rtc.disable32K();

    pinMode(led_pin_1,OUTPUT);
    pinMode(led_pin_2,OUTPUT);
    pinMode(led_pin_3,OUTPUT);
    pinMode(led_pin_4,OUTPUT);

    // ustawienie przerwań
    pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
    pinMode(KONTAKTRON_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onAlarm, FALLING);
    attachInterrupt(digitalPinToInterrupt(KONTAKTRON_INTERRUPT_PIN),kontaktron,FALLING);

    // zarządzanie alarmami DS3231
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    rtc.writeSqwPinMode(DS3231_OFF);
    rtc.disableAlarm(2);

    // ustawienie czasu zegara
    // rtc.adjust(DateTime(2024,4,11,20,15,0));

    // ustawienie pierwszego alarmu testowego na 10 sekund w przyszłość
    DateTime now=rtc.now();
    DateTime alarmTime(rtc.now()+TimeSpan(10));
    if(!rtc.setAlarm1(alarmTime,DS3231_A1_Day)) {
        Serial.println("Błąd, alarm nie został ustawiony!");
    } else {
        Serial.println("Alarm wystąpi za 10 sekund!");
    }
}

void loop() {
    // drukuj aktualny czas
    char date[10] = "hh:mm:ss";
    rtc.now().toString(date);
    Serial.print(date);
    // pobierz wartość alarmu + tryb
    DateTime alarm1 = rtc.getAlarm1();
    Ds3231Alarm1Mode alarm1mode = rtc.getAlarm1Mode();
    char alarm1Date[12] = "DD hh:mm:ss";
    alarm1.toString(alarm1Date);
    Serial.print(" [Alarm1: ");
    Serial.print(alarm1Date);
    Serial.print(", Tryb: ");
    switch (alarm1mode) {
      case DS3231_A1_PerSecond: Serial.print("PerSecond"); break;
      case DS3231_A1_Second: Serial.print("Second"); break;
      case DS3231_A1_Minute: Serial.print("Minute"); break;
      case DS3231_A1_Hour: Serial.print("Hour"); break;
      case DS3231_A1_Date: Serial.print("Date"); break;
      case DS3231_A1_Day: Serial.print("Day"); break;
    }

    // wartość na pinie SQW (z powodu podciągnięcia 1 oznacza brak alarmu)
    Serial.print("] SQW: ");
    Serial.print(digitalRead(CLOCK_INTERRUPT_PIN));

    // czy wystąpił alarm
    Serial.print(" Fired: ");
    Serial.print(rtc.alarmFired(1));

    // operacje po przerwaniu i ustawieniu alarm_set
    DateTime now=rtc.now();
    Serial.println(start);
    if(set_alarm==true){
      // włączenie ustawionych diód 
      for (int j=0;j<alarm_num;j++){
        if(now.hour()==Alarms[j][0] && now.minute()==Alarms[j][1]){
          digitalWrite(int(Alarms[j][2]), HIGH);
          Serial.println("Alarm wystąpił!");
        }
      }

      // operacje do wykonania pierwszego testowego alarmu
      if(start==0){
        start++;
        for(int k=0;k<alarm_num;k++){
        digitalWrite(Alarms[k][2],HIGH);
        delay(1000);
        digitalWrite(Alarms[k][2],LOW);
        }
        set_alarm=false;
      }
      // dalsze operacje przy wywołaniu alarmu
      else{
      beep();
      set_alarm=false;
      }
    }
    // ustawianie kolejnego alarmu
    if (rtc.alarmFired(1)) {

      // czyszczenie poprzedniego alarmu
        rtc.clearAlarm(1);
        Serial.println(" - Alarm cleared");

      // sprawdzanie kolejnego alarmu
      while(i<alarm_num){
        DateTime now=rtc.now();
        if(now.hour() > Alarms[i][0] || (now.hour() == Alarms[i][0] && now.minute() >= Alarms[i][1])){
          i++;
          }
        else {
          break;
        }
      }

      // ustawianie alarmu na kolejny dzien
      if(i>alarm_num-1){
        i=0;
        DateTime now=rtc.now();
        DateTime alarmTime(now.year(), now.month(), now.day()+1, Alarms[i][0], Alarms[i][1],0);
        if(!rtc.setAlarm1(alarmTime,DS3231_A1_Day)) {
            Serial.println("Błąd, alarm nie został ustawiony!");
        } else {
            Serial.println("Alarm wystąpi jutro!");
            delay(1000);
            LowPower.powerDown(SLEEP_FOREVER,ADC_OFF,BOD_OFF);
        }  

      }
      // ustawianie kolejnego alarmu tego samego dnia
      else{
        DateTime now=rtc.now();
        DateTime alarmTime(now.year(), now.month(), now.day(), Alarms[i][0], Alarms[i][1],0);
        if(!rtc.setAlarm1(alarmTime,DS3231_A1_Day)) {
            Serial.println("Błąd, alarm nie został ustawiony!");
        } 
        else {
            Serial.println("Alarm wystąpi dzisiaj!");
        }  
     }
    }
    Serial.println();
    delay(1000);
}

// funkcja z przerwania wybudza system, ustawia set_alarm
void onAlarm() {
    Serial.println("Alarm wystąpił!");
    set_alarm=true;
    i=0;
}

// funkcja z przerwania wyłącza diody
void kontaktron(){
   Serial.println("Wyłączono diody");
   digitalWrite(led_pin_1,LOW);
   digitalWrite(led_pin_2,LOW);
   digitalWrite(led_pin_3,LOW);
   digitalWrite(led_pin_4,LOW);
   LowPower.powerDown(SLEEP_FOREVER,ADC_OFF,BOD_OFF);
}

// funkcja z melodią do buzzera
void beep(){
  for(int j=0;j<10;j++){
    for (int thisNote = 0; thisNote < 8; thisNote++) {
        int noteDuration = 1000 / noteDurations[thisNote];
        tone(buzzer, melody[thisNote], noteDuration);
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
        noTone(buzzer);
        if(digitalRead(KONTAKTRON_INTERRUPT_PIN)==HIGH){ 
          return;
        }
      }
  }
}