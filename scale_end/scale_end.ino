#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#include <SoftwareSerial.h>
#include <HX711.h>

const byte txPin = 10;
const byte rxPin = 11;

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 3;

HX711 scale;

float weight;
char bytePack;

SemaphoreHandle_t xSerialMutex = NULL;

SoftwareSerial HC12(txPin, rxPin); // HC-12 TX Pin, HC-12 RX Pin

void setup() {
  Serial.begin(9600);
  HC12.begin(9600);

  //Scale setup code from:
  //https://randomnerdtutorials.com/arduino-load-cell-hx711/#load-cell-hx711-arduino-wiring
  Serial.println("Initializing the scale");

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  Serial.println("Before setting up the scale:");
  Serial.print("read: \t\t");

  Serial.println(scale.read());                 // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));      // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));         // print the average of 5 readings from the ADC minus the tare weight (not set yet)

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);      // print the average of 5 readings from the ADC minus tare weight (not set) divided
                                              // by the SCALE parameter (not set yet)
            
  scale.set_scale(207);
  //scale.set_scale(-471.497);               // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();

  Serial.println("After setting up the scale:");

  Serial.print("read: \t\t");
  Serial.println(scale.read());             // print a raw reading from the ADC

  Serial.print("read average: \t\t");
  Serial.println(scale.read_average(20));   // print the average of 20 readings from the ADC

  Serial.print("get value: \t\t");
  Serial.println(scale.get_value(5));       // print the average of 5 readings from the ADC minus the tare weight, set with tare()

  Serial.print("get units: \t\t");
  Serial.println(scale.get_units(5), 1);    // print the average of 5 readings from the ADC minus tare weight, divided
                                            // by the SCALE parameter set with set_scale


  xSerialMutex = xSemaphoreCreateMutex();
  if (xSerialMutex == NULL) {
    Serial.println("Mutex creation failed");
  }

  xTaskCreate(
    TaskScale,
    "Scale",
    128,
    NULL,
    1,
    NULL );

  xTaskCreate(TaskTxRx, "Communication", 128, NULL, 1, NULL );

}

void loop() 
{
  //empty, because tasks
}


/*----------------- TASKS ------------------*/

void TaskScale(void *pvParameters) {
  const TickType_t xDelay = 3000 / portTICK_PERIOD_MS; //delay of 3s, report weight every 3s
  for (;;) {
    if (scale.is_ready()) {
      float prev = weight;
      weight = scale.get_units();
      Serial.println(weight);
    }
    if (xSemaphoreTake(xSerialMutex, (TickType_t) 10) == pdTRUE) {
      if (weight > 1.0) {
        bytePack = 'W';
        Serial.println("Weight detected...");
      } else {
        bytePack = 'E';
        Serial.println("Scale is empty...");
      }
    xSemaphoreGive(xSerialMutex); }
  vTaskDelay(xDelay); //where the delay happens
  }
}

void TaskTxRx(void *pvParameters) {
  for (;;) {
    if (xSemaphoreTake(xSerialMutex, (TickType_t) 10) == pdTRUE) {
      while (HC12.available()) { //RX, HC12 has data to receive
        Serial.write(HC12.read());
      }
      while (bytePack != 0) { //TX, bytePack has data to send
        HC12.write(bytePack);
        bytePack = 0;
      }
      xSemaphoreGive(xSerialMutex);
    }
  }
}
