// esp8266 client that uses vegetronix sensors:
//    VHT400 soil moisture sensor
//    TH200  soil temperature sensor
//    LT150  light lux sensor

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// debugging flags, use false for deployment
#define DEBUG false
#define LEDS_ON false

// sensor name
#define SENSOR_NAME "testing"

// wifi credentials, ssid and passkey
#define STASSID ""
#define STAPSK  ""

// server and port of opengrower-sensor-server
#define SERVER "192.168.1.100:8080"

// flags to control whether to read a sensor (set to false if absent)
#define READ_TEMPERATURE true
#define READ_MOISTURE true
#define READ_LIGHT true

// delays for loop and sensor stablization after power up
#define LOOP_DELAY 120e3
#define TEMP_DELAY 4e3
#define MOISTURE_DELAY 1e3
#define LIGHT_DELAY 2e3
#define DEBUG_DELAY 1e3

// count and delay to average each sensor reading
// note: not much observable difference in averaging readings back to back,
//       therefore set low if power draw is a concern
#define SAMPLE_COUNT 2

#define SAMPLE_DELAY 100

// digital pins for LEDs
#define LED_BUILTIN D0
#define LED_BUILTIN_2 D4

// digital pins to turn sensors on/off
#define TEMPERATURE_D D8
#define MOISTURE_D D7
#define LIGHT_D D6

// analog pins for reading sensor data
#define TEMPERATURE_A A0
#define MOISTURE_A A0
#define LIGHT_A A0

#define VCC 3.3

// class to facilitate managing sensor data
class Sensor {
  private:
    String id;
    float temperature; // degrees farenheit soil
    float moisture;    // % volumetric water content
    float light;       // lux

  public:
    Sensor(String i) {
      id = i;
      temperature = 0.0;
      moisture = 0.0;
      light = 0.0;
    }

    void set_temperature(float t) {
      temperature = t;
    }
    void set_moisture(float m) {
      moisture = m;
    }
    void set_light(float l) {
      light = l;
    }

    float get_temperature() {
      return temperature;
    }
    float get_moisture() {
      return moisture;
    }
    float get_light() {
      return light;
    }

    String to_json() {
      return "{\"sensor\":\"" + id + "\","
             + "\"temperature\":" + temperature + ","
             + "\"moisture\":" + moisture + ","
             + "\"light\":" + light
             + "}";
    }

    String to_string() {
      return "sensor" + id + ", "
             + "temperature: " + temperature + ", "
             + "moisture: " + moisture + ", "
             + "light: " + light;
    }
};

// class to populate Sensor object with data
class SensorReader {
  public:
    SensorReader(Sensor &sensor) {
      if (READ_TEMPERATURE) {
        digitalWrite(TEMPERATURE_D, HIGH);
        delay(TEMP_DELAY);
        float temperature = 0.0;
        for(int i=0; i<SAMPLE_COUNT; i++) {
           temperature += analogRead(TEMPERATURE_A) * (VCC / 1023.0);
           delay(SAMPLE_DELAY);
        }
        float temperature_avg = temperature / SAMPLE_COUNT;
        sensor.set_temperature(temperature_avg * 75.006 - 40);
        digitalWrite(TEMPERATURE_D, LOW);
      }

      if (READ_MOISTURE) {
        digitalWrite(MOISTURE_D, HIGH);
        delay(MOISTURE_DELAY);
        float moisture = 0.0;
        for(int i=0; i<SAMPLE_COUNT; i++) {
           moisture += analogRead(MOISTURE_A) * (VCC / 1023.0);
           delay(SAMPLE_DELAY);
        }
        float moisture_avg = moisture / SAMPLE_COUNT;
        if (moisture_avg >= 0.0 && moisture_avg < 1.1) {
          sensor.set_moisture(10 * moisture_avg - 1);
        } else if (moisture_avg >= 1.1 && moisture_avg < 1.3) {
          sensor.set_moisture(25 * moisture_avg - 17.5);
        } else if (moisture_avg >= 1.3 && moisture_avg < 1.82) {
          sensor.set_moisture(48.08 * moisture_avg - 47.5);
        } else if (moisture_avg >= 1.82 && moisture_avg < 2.2) {
          sensor.set_moisture(26.32 * moisture_avg - 7.89);
        } else if (moisture_avg >= 2.2 && moisture_avg <= 3.0) {
          sensor.set_moisture(62.5 * moisture_avg - 87.5);
        } else {
          sensor.set_moisture(0.0);
        }
        digitalWrite(MOISTURE_D, LOW);
      }

      if (READ_LIGHT) {
        digitalWrite(LIGHT_D, HIGH);
        delay(LIGHT_DELAY);
        float light = 0.0;
        for(int i=0; i<SAMPLE_COUNT; i++) {
           light += analogRead(LIGHT_A)  * (VCC / 1023.0);
           delay(SAMPLE_DELAY);
        }
        float light_avg = light / SAMPLE_COUNT;
        sensor.set_light(light_avg * 50000);
        digitalWrite(LIGHT_D, LOW);
      }

    }
};

Sensor sensor(SENSOR_NAME);

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);

  if (DEBUG) {
    Serial.begin(115200);
  }

  pinMode(LED_BUILTIN_2, OUTPUT);
  pinMode(TEMPERATURE_D, OUTPUT);
  pinMode(MOISTURE_D, OUTPUT);
  pinMode(LIGHT_D, OUTPUT);

  pinMode(TEMPERATURE_A, INPUT);
  pinMode(MOISTURE_A, INPUT);
  pinMode(LIGHT_A, INPUT);

  digitalWrite(LED_BUILTIN_2, LOW);
  delay(100);
  digitalWrite(TEMPERATURE_D, LOW);
  digitalWrite(MOISTURE_D, LOW);
  digitalWrite(LIGHT_D, LOW);

  //WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (DEBUG) {
    Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());  
  }
  
  digitalWrite(LED_BUILTIN_2, HIGH);
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(100);
}

void loop() {
  if (LEDS_ON) {
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  SensorReader sensorReader(sensor);
  String payload = sensor.to_json();
  
  if (LEDS_ON) {
        digitalWrite(LED_BUILTIN, HIGH);
  }

  if ((WiFi.status() == WL_CONNECTED)) {
    if (LEDS_ON) {
        digitalWrite(LED_BUILTIN_2, LOW);
    }
    
    HTTPClient http;
    http.begin("http://" SERVER "/measurement");
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);
    if (httpCode < 0) {
      if (DEBUG) {
        Serial.println("server error code: " + String(httpCode));
      }
    } else {
      if(DEBUG) {
         Serial.println(http.getString());
      }
    }
    http.end();
    
   if( LEDS_ON ) {
      digitalWrite(LED_BUILTIN_2, HIGH);
   }
  }

  if (DEBUG) {
    Serial.println(payload);
    delay(DEBUG_DELAY);
  } else {
    delay(LOOP_DELAY);
  }  
}
  
 
