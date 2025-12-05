#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <timers.h>
#include <SoftwareSerial.h>
#include "pitches.h"

const byte txPin = 8;
const byte rxPin = 7;

const int doorSwitchPin = 4;
const int buzzerPin = 12;
const int LED_PIN = 10;
const int BTN_PIN = 2;

const unsigned long DEBOUNCE_DELAY = 20;
int lastBtnState = HIGH;
int currBtnState = HIGH;
unsigned long lastDebounceTime = 0;

const int PAUSE_TIME = 10000;
const int NOTE = NOTE_C5;

bool doorOpen;
bool scaleWeight;
bool alarmPause = false;
bool postAlarmPause = false; //tracks whether the door has been closed post alarm pause or not
bool connected = false;

char incByte;
char bytePack;

SemaphoreHandle_t xSerialMutex = NULL;

TimerHandle_t xPauseTimer;

//Callback function for xPauseTimer
void unpauseCallback() {
  Serial.println("Alarms re-enabled. If door is open, they will be re-enabled upon close");
  alarmPause = false;
  if (doorOpen){
    postAlarmPause = true;
  } else {
    postAlarmPause = false;
  }
}

SoftwareSerial HC12(txPin, rxPin); // HC-12 TX Pin, HC-12 RX Pin

// put your setup code here, to run once:
void setup() {
  //initialize serial communication
  Serial.begin(9600);
  //initialize HC12 software serial communication
  HC12.begin(9600);

  //initialize input pin for door contact switch
  pinMode(doorSwitchPin, INPUT_PULLUP);
  //buzzer pin
  pinMode(buzzerPin, OUTPUT);
  //button pin
  pinMode(BTN_PIN, INPUT_PULLUP);

  doorOpen = digitalRead(doorSwitchPin); //determine initial state of door

  xSerialMutex = xSemaphoreCreateMutex();
  if (xSerialMutex == NULL) {
    Serial.println("Mutex creation failed");
  }

  xPauseTimer = xTimerCreate(
    "UnpauseTimer",
    pdMS_TO_TICKS(PAUSE_TIME),
    pdFALSE,
    (void *)0,
    unpauseCallback
  );

  xTaskCreate(
    TaskDoor,
    "Door",
    128,
    NULL,
    1,
    NULL );

  xTaskCreate(
    TaskButton,
    "Button",
    128,
    NULL,
    1,
    NULL );


  xTaskCreate(
    TaskTxRx, 
    "Communication", 
    128, 
    NULL, 
    1, 
    NULL );

  //task scheduler automatically starts
}

void loop() 
{
  //empty, because tasks
}


/*----------------- TASKS ------------------*/

void TaskDoor(void *pvParameters) {
  for (;;) {
    if (digitalRead(doorSwitchPin) != doorOpen) { //if door state changed
      if (digitalRead(doorSwitchPin)) { //if switch is open
        doorOpen = true;
        if (xSemaphoreTake(xSerialMutex, (TickType_t) 10) == pdTRUE) {
          Serial.println("Door was opened!");
          bytePack = 'O'; //we don't need to send this to the scale technically, but for future functionality will be useful
          xSemaphoreGive(xSerialMutex); }
      } else if (!digitalRead(doorSwitchPin)) {
        doorOpen = false;
        postAlarmPause = false; //so we can re-enable alarms on door close after alarm pause
        if (xSemaphoreTake(xSerialMutex, (TickType_t) 10) == pdTRUE) {
          Serial.println("Door was closed!");
          bytePack = 'C';
          xSemaphoreGive(xSerialMutex); }
      }
    }

    if (scaleWeight && !alarmPause && !postAlarmPause) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    if (doorOpen && scaleWeight && !alarmPause && !postAlarmPause) {
      tone(buzzerPin, NOTE);
    } else {
      noTone(buzzerPin);
    }
  }
}

void TaskButton(void *pvParameters) {
  for (;;) {
    int reading = digitalRead(BTN_PIN);

    if (reading != lastBtnState) {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      if (reading != currBtnState) {
        currBtnState = reading;
        if (currBtnState != LOW) {
          Serial.println("Button press detected, pausing alarms for 10 seconds");
          alarmPause = true;
          xTimerStart(xPauseTimer, 0); //start software timer
        }
      }
    }
  }
}

void TaskTxRx(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(xSerialMutex, (TickType_t) 10) == pdTRUE) {
      while (HC12.available()) { //RX, HC12 has data to receive
        incByte = HC12.read();
        Serial.println(incByte);
        if (incByte == 'W') {
          scaleWeight = true;
        } else {
          scaleWeight = false;
          digitalWrite(LED_PIN, LOW);
        }
      }
      while (bytePack != 0) { //TX, bytePack has data to send
        HC12.write(bytePack);
        bytePack = 0;
      }
      xSemaphoreGive(xSerialMutex);
    }
  }
}
