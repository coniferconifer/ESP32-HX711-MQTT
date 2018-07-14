/* EDP32-HX711 WiFi/MQTT enhanced prompting weight scale using legacy weight scale
   Once you use this scale, the scale will wake up  TIME_TO_SLEEP sec later to prompt you to
   use it again. (ex. 86500 sec (1day ) , not tested yet )
   I'm running Node-RED on raspberry pi and node graph can out put weight values to
   slack for recording and prompting to use it regularly.

   Author: coniferconifer
   License: Apache License v2

   HX711 library for ESP32 from
   https://github.com/SensorsIot/Weight-Sensors/tree/master/HX711
   GPIO_NUM_26 for HX711 DOUT
   GPIO_NUM_25 for SCK
   GPIO_NUM_0 is connected to tactile switch to wake up ESP32
   GPIO_NUM_12 is connected to speaker to beep

*/
#include "HX711.h"
#include "soc/rtc.h"
#include <WiFi.h>
#include <PubSubClient.h>

RTC_DATA_ATTR int bootCount = 0;

#include "credentials.h"
char* ssidArray[] = { WIFI_SSID , WIFI_SSID1, WIFI_SSID2};
char* passwordArray[] = {WIFI_PASS, WIFI_PASS1, WIFI_PASS2};
char* tokenArray[] = { TOKEN , TOKEN1, TOKEN2};
char* serverArray[] = {SERVER, SERVER1, SERVER2};
#define MQTTRETRY 1
#define DEVICE_TYPE "ESP32" // 
String clientId = DEVICE_TYPE ; //uniq clientID will be generated from MAC
char topic[] = "v1/devices/me/telemetry"; //for Thingsboard
#define MQTTPORT 2883 //for MQTT server running on raspberry pi
#define LED GPIO_NUM_2
#define SPEAKER GPIO_NUM_12
HX711 scale;
//This value is obtained using the SparkFun_HX711_Calibration sketch or used #define CALIB in this sketch
//#define calibration_factor -1050.02 // for 1kg load cell
#define calibration_factor -31900 // for my legacy weight scale
/* https://github.com/bogde/HX711
   How to Calibrate Your Scale

    Call set_scale() with no parameter.
    Call tare() with no parameter.
    Place a known weight on the scale and call get_units(10).
    Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale.
    Adjust the parameter in step 4 until you get an accurate reading.

*/
WiFiClient wifiClient;
PubSubClient client(serverArray[0], MQTTPORT, wifiClient);
#define BOOTSOUND 1000 //Hz
#define SCALESOUND 1300 //Hz 
#define WIFISOUND 880 //Hz Ab
#define SHUTDOWNSOUND 587 //Hz D
#define FAILSOUND 440//Hz A
//https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/TimerWakeUp/TimerWakeUp.ino
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 600/* ESP32 sleeps after measurement (in seconds) */

/* https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/DeepSleep/ExternalWakeUp/ExternalWakeUp.ino
  Method to print the reason by which ESP32
  has been awaken from sleep
*/
void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason)
  {
    case 1  : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case 2  : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case 3  : Serial.println("Wakeup caused by timer"); break;
    case 4  : Serial.println("Wakeup caused by touchpad"); break;
    case 5  : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.println("Wakeup was not caused by deep sleep"); break;
  }
}

#define LEDC_CHANNEL_2     2
#define LEDC_TIMER_13_BIT  13
#define LEDC_BASE_FREQ     5000
void tone(int pin, int freq)
{
  ledcSetup(LEDC_CHANNEL_2, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT) ;
  ledcAttachPin(pin, LEDC_CHANNEL_2) ;
  ledcWriteTone(LEDC_CHANNEL_2, freq) ;
}
void noTone(int pin)
{
  ledcWriteTone(LEDC_CHANNEL_2, 0.0) ;
}

int AP;
void setup() {
  Serial.begin(115200);
  // Special thanks to https://github.com/SensorsIot/Weight-Sensors/blob/master/ESP32_Dishka/ESP32_Dishka.ino
  // ESP32 should be slow down to 80MHz to allow HX711 library works properly.
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);

  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  Serial.println("ESP32-HX711-MQTT v1.0.0 Jul 14,2018");

  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH); delay(1000); digitalWrite(LED, LOW);
  pinMode(SPEAKER, OUTPUT);
  tone(SPEAKER, BOOTSOUND); delay(100); noTone(SPEAKER); digitalWrite(SPEAKER, LOW);

  // generate uniq clientId
  uint64_t chipid;
  chipid = ESP.getEfuseMac(); clientId += "-";
  clientId += String((uint32_t)chipid, HEX);
  Serial.println("clientId :" + clientId);

  Serial.println("Initializing the scale");
  scale.begin(GPIO_NUM_26, GPIO_NUM_25); // DOUT,SCK
#ifdef CALIB
  scale. set_scale();
  scale.tare();
  Serial.println("Put known weight on ");
  delay(2500);
  Serial.print(scale.get_units(10));
  Serial.print(" Divide this value into the weight and use it as calibration_factor");

  while (1 == 1);

#endif

  scale.set_scale();
  scale.tare();                // reset the scale to 0
  scale.set_scale(calibration_factor);  // this value is obtained by calibrating the scale with known weights; see the README for details

  Serial.println("Readings:");
  //set wakeup key
  gpio_pullup_en(GPIO_NUM_0);    // use pullup on GPIO
  gpio_pulldown_dis(GPIO_NUM_0); // not use pulldown on GPIO

}
#define LOOPMAX 3
#define REPORTTHRESH 10
double w[LOOPMAX];


void loop() {
  digitalWrite(2, HIGH);
  int i;
  for (i = 0; i < LOOPMAX ; i++) {
    // beep sound
    tone(SPEAKER, SCALESOUND); delay(200); noTone(SPEAKER); digitalWrite(SPEAKER, LOW);
    scale.power_down();// put the ADC in sleep mode
    delay(2000);
    scale.power_up();
    w[i] = scale.get_units(10);
    Serial.println(w[i], 1);
  }
  scale.power_down();// put the ADC in sleep mode
  digitalWrite(2, LOW);
  int datacnt = 0;
  for (i = 0; i < LOOPMAX; i++) {
    if ( w[i] > REPORTTHRESH ) datacnt++;
  }
  if (datacnt > 0 ) {
    AP = initWiFi();
    if ( AP != -1) {  // found  WiFi AP
      client.setClient(wifiClient);
      client.setServer(serverArray[AP], MQTTPORT); // MQTT server for NodeRED or MQTT by Thingsboarxd
    }
    while (1) {
      Serial.print("Connecting to MQTT at ");
      Serial.print(serverArray[AP]); Serial.print(":"); Serial.print( MQTTPORT );
      if (client.connect(clientId.c_str(),  tokenArray[AP], NULL)) {
        Serial.println(" connected");
        break;
      } else {
        Serial.print("failed with state ");
        tone(SPEAKER, FAILSOUND); delay(200); noTone(SPEAKER); digitalWrite(SPEAKER, LOW);
        Serial.println(client.state());
        WiFi.mode(WIFI_OFF);
        ESP.restart();
      }
    }

    for (i = 0; i < LOOPMAX ; i++) {
      if ( w[i] > REPORTTHRESH ) {
        String payload = "{";
        payload += "\"weight\":"; payload += w[i];  payload += ",";
        payload += "\"rssi\":"; payload += WiFi.RSSI();
        payload += "}";
        Serial.print((char*) payload.c_str());
        if (client.publish(topic, (char*) payload.c_str())) {
          Serial.println(" published");
          delay(1000);
        }
      } else {
        Serial.println(" less than threshold , not published"); ;
      }
    }
  }
  WiFi.mode(WIFI_OFF);

  scale.power_down();// put the ADC in sleep mode
  /*
    First we configure the wake up source
    We set our ESP32 to wake up for an external trigger.
    There are two types for ESP32, ext0 and ext1 .
    ext0 uses RTC_IO to wakeup thus requires RTC peripherals
    to be on while ext1 uses RTC Controller so doesnt need
    peripherals to be powered on.
    Note that using internal pullups/pulldowns also requires
    RTC peripherals to be turned on.
  */

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); // wakeup when GPIO_NUM_0 is LOW
  /*
    First we configure the wake up source
    We set our ESP32 to wake up every TIME_TO_SLEEP seconds
  */
  if (datacnt > 0) {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to wakeup after " + String(TIME_TO_SLEEP) +
                   " seconds to promote re-scale again."); //Go to sleep now
  }
  Serial.println("Going to sleep now");
  tone(SPEAKER, SHUTDOWNSOUND); delay(500); noTone(SPEAKER); digitalWrite(SPEAKER, LOW);
  esp_deep_sleep_start();

}

#define MAX_TRY 15
int initWiFi() {
  int i ;
  int numaccesspt = (sizeof(ssidArray) / sizeof((ssidArray)[0]));
  Serial.print("Number of Access Point = "); Serial.println(numaccesspt);
  for (i = 0;  i < numaccesspt; i++) {
    Serial.print("WiFi connecting to "); Serial.println(ssidArray[i]);
    WiFi.mode(WIFI_OFF);
    WiFi.begin(ssidArray[i], passwordArray[i]);

    int j;
    for (j = 0; j < MAX_TRY; j++) {
      if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        Serial.printf("RSSI= %d\n", rssi);
        //        configTime(TIMEZONE * 3600L, 0,  NTP1, NTP2, NTP3);
        Serial.print("WiFi connected, IP address: "); Serial.println(WiFi.localIP());
        return (i);
      }
      delay(500); tone(SPEAKER, WIFISOUND); delay(100); noTone(SPEAKER); digitalWrite(SPEAKER, LOW);
      Serial.print(".");
    }
    Serial.println(" can not connect to WiFi AP");
  }
  return (-1);
}

