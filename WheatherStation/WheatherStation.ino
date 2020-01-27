#include <DHT.h>
#include <WiFi.h>
#include <OneWire.h>
#include <IRremote.h>
#include <WebServer.h>
#include <DallasTemperature.h>
#include "freertos/semphr.h"

const int CHANEL_MINUS = 16753245;
const int CHANEL = 4294967295; // IR_FAILED
const int CHANEL_PLUS = 16769565;
const int PREV = 16720605;
const int NEXT = 16712445;
const int PLAY = 4294967295; // IR_FAILED
const int VOL_MINUS = 16769055;
const int VOL_PLUS = 16754775;
const int IR_0 = 16738455;
const int IR_1 = 16724175;
const int IR_2 = 16718055;
const int IR_3 = 16743045;
const int IR_4 = 16716015;
const int IR_5 = 16726215;
const int IR_6 = 16734885;
const int IR_7 = 16728765;
const int IR_8 = 16730805;
const int IR_9 = 4294967295; // IR_FAILED
const int IR_FAILED = 4294967295;

//dht pin
const int DHTPIN = 4;
const int DHTTYPE = DHT11;

//dallas pin
const int oneWireBus = 5;

float temperatures[6];

int RECV_PIN = 19;
IRrecv irrecv(RECV_PIN);
decode_results results;

const char * ssid = "NETWORK";
const char * password = "12345678";
WebServer server(80);

int sensor_A0 = 2;
const int ledPin = 23;
float soilMoisureRaw = 1;
float temperatureGlobal = -1;
float humidityGlobal = 1;
float minTemperatureGlobal = -200;
float maxTemperatureGlobal = 100;
float minHumidityGlobal = 0;
float maxHumidityGlobal = 100;
long remoteControlTab[100];
int remoteControlTabIndex = 0;
int resetLimits = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t TaskDHTHandler;
TaskHandle_t TaskDallasHandler;
TaskHandle_t TaskBuzzerHandler;
TaskHandle_t TaskSoilMoisureHandler;

SemaphoreHandle_t xBinarySemaphore;
SemaphoreHandle_t xBinarySemaphoreHumidity;
SemaphoreHandle_t xBinarySemaphoreTemperature;

void TaskDHTCode(void * pvParameters);
void TaskBuzzerCode(void * pvParameters);
void TaskDallasCode(void * pvParameters);

void TaskDHTCode(void * pvParameters) {
  DHT dht(DHTPIN, DHTTYPE);
  dht.begin();
  for (;;) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
      delay(2000);
      continue;
    }

    xSemaphoreTake(xBinarySemaphoreHumidity, (TickType_t) 1000);

    humidityGlobal = h;

    xSemaphoreGive(xBinarySemaphoreHumidity);

    xSemaphoreTake(xBinarySemaphore, (TickType_t) 1000);

    Serial.print("DHT - Humidity: ");
    Serial.print(h);
    Serial.print("%  Temperature: ");
    Serial.print(t);
    Serial.print("°C = ");
    Serial.print(f);
    Serial.println("°F");
    xSemaphoreGive(xBinarySemaphore);
    delay(2000);
  }
}

void TaskBuzzerCode(void * pvParameters) {
  digitalWrite(ledPin, LOW);
  for (;;) {
    while (true) {
      if (resetLimits == 1) {
        break;
      }

      xSemaphoreTake(xBinarySemaphoreHumidity, (TickType_t) 1000);
      if (humidityGlobal < minHumidityGlobal || humidityGlobal > maxHumidityGlobal) {
        Serial.println("Values outside limits -> IOIOIOOIOIOOI!");
        digitalWrite(ledPin, HIGH);

      }
      xSemaphoreGive(xBinarySemaphoreHumidity);

      xSemaphoreTake(xBinarySemaphoreTemperature, (TickType_t) 1000);
      if (temperatureGlobal < minTemperatureGlobal || temperatureGlobal > maxTemperatureGlobal) {
        Serial.println("Values outside limits -> IOIOIOOIOIOOI!");
        digitalWrite(ledPin, HIGH);
      }
      xSemaphoreGive(xBinarySemaphoreTemperature);


      delay(500);
    }
    digitalWrite(ledPin, LOW);
    resetLimits = 0;
    minTemperatureGlobal = -200;
    maxTemperatureGlobal = 100;
    minHumidityGlobal = 0;
    maxHumidityGlobal = 100;
    delay(3000);
    Serial.println("Limits were restarted");

  }
}

void TaskDallasCode(void * pvParameters) {
  OneWire oneWire(oneWireBus);
  DallasTemperature sensors( & oneWire);
  sensors.begin();

  for (;;) {
    sensors.requestTemperatures();
    float temperatureC = sensors.getTempCByIndex(0);
    float temperatureF = sensors.getTempFByIndex(0);
    xSemaphoreTake(xBinarySemaphoreTemperature, (TickType_t) 100000);

    temperatureGlobal = temperatureC;
    xSemaphoreGive(xBinarySemaphoreTemperature);

    xSemaphoreTake(xBinarySemaphore, (TickType_t) 100000);

    Serial.print("Dallas - Temperature: ");
    Serial.print(temperatureC);
    Serial.print("°C = ");
    Serial.print(temperatureF);
    Serial.println("°F");
    xSemaphoreGive(xBinarySemaphore);

    delay(1400);
  }
}

void TaskSoilMoisureCode(void * pvParameters) {
  pinMode(2, INPUT);

  for (;;) {
    soilMoisureRaw = analogRead(sensor_A0);

    xSemaphoreTake(xBinarySemaphore, (TickType_t) 1000);

    Serial.print("Moisure level: ");
    Serial.print(soilMoisureRaw);
    if (soilMoisureRaw < 1) {
      Serial.println(" - please check if sensor is correctly connected");
    }

    if (soilMoisureRaw > 2500) {
      Serial.println(" - very low moisure level ");
    } else {
      if (soilMoisureRaw > 1900) {
        Serial.println(" - low moisure level ");
      } else {
        if (soilMoisureRaw > 1600) {
          Serial.println(" - medium moisure level ");
        } else {
          if (soilMoisureRaw > 1400) {
            Serial.println(" - high moisure level ");
          } else {
            Serial.println(" - very high moisure level ");
          }

        }
      }
    }
    xSemaphoreGive(xBinarySemaphore);

    delay(2500);
  }
}

long decodeSingleNumber(long value) {
  switch (value) {
  case IR_0:
    return 0;
  case IR_1:
    return 1;
  case IR_2:
    return 2;
  case IR_3:
    return 3;
  case IR_4:
    return 4;
  case IR_5:
    return 5;
  case IR_6:
    return 6;
  case IR_7:
    return 7;
  case IR_8:
    return 8;
  case IR_9:
  default:
    return 9;
  }
}

void remoteControlParser() {
  if (remoteControlTab[0] == CHANEL_MINUS) {
    Serial.println("Recognized channel_minus button");
    if (remoteControlTab[1] == VOL_PLUS || remoteControlTab[1] == VOL_MINUS) {
      Serial.println("Recognized limits button");
      double limit = -1000;
      if (remoteControlTab[2] != CHANEL_PLUS) limit = decodeSingleNumber(remoteControlTab[2]);
      if (remoteControlTab[3] != CHANEL_PLUS) limit = limit * 10 + decodeSingleNumber(remoteControlTab[3]);
      if (remoteControlTab[1] == VOL_PLUS) maxTemperatureGlobal = limit;
      else minTemperatureGlobal = limit;
      Serial.print("Temperautre limit was set: ");
      Serial.println(limit);
    }
  }
  if (remoteControlTab[0] == PREV) {
    Serial.println("Recognized prev button");
    if (remoteControlTab[1] == VOL_PLUS || remoteControlTab[1] == VOL_MINUS) {
      Serial.println("Recognized limits button");
      double limit = -1000;
      if (remoteControlTab[2] != CHANEL_PLUS) limit = decodeSingleNumber(remoteControlTab[2]);
      if (remoteControlTab[3] != CHANEL_PLUS) limit = limit * 10 + decodeSingleNumber(remoteControlTab[3]);
      if (remoteControlTab[1] == VOL_PLUS) maxHumidityGlobal = limit;
      else minHumidityGlobal = limit;
      Serial.print("Humidity limit was set: ");
      Serial.println(limit);
    }
  }
  for (int i = 0; i < 10; i++) remoteControlTab[i] = 0;
  remoteControlTabIndex = 0;
  Serial.println("Received values was restart");
}

const char content [] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta charset = "utf-8" name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ESP32 Weather Station</title>
  <style>
    html 
    { 
      font-family: Helvetica; 
      display: inline-block; 
      margin: 0px auto; 
      text-align: center; 
    }
    body 
    {
      margin-top: 50px; 
      background-color: #DDDDDD; 
    }
    h1 
    {
      color: black; 
      margin: 50px auto 30px;
    }
    p 
    {
      font-size: 24px;
      margin-bottom: 10px;
    }
    .text { color: darkblue; }
  </style>
  <script type="application/javascript">
    var temperatures = [0, 0, 0, 0, 0, 0];
    setInterval (() => getSensorData (), 1000);
    function getSensorData () 
    {
      var xhttp = new XMLHttpRequest ();
      xhttp.onreadystatechange = function () 
      { 
        if (this.readyState == 4 && this.status == 200)
        {
          for (i = 5; i > 0; i--) temperatures [i] = temperatures [i - 1];
          temperatures [0] = parseInt (this.responseText);
          document.getElementById ("TEMPvalue").innerHTML = this.responseText;
          clear ();
          drawAxis ();
          draw ();
        }
      }
      xhttp.open ("GET", "TEMPread", true);
      xhttp.send ();
      //
      var xhttp = new XMLHttpRequest ();
      xhttp.onreadystatechange = function () { if (this.readyState == 4 && this.status == 200) document.getElementById ("HUMIvalue").innerHTML = this.responseText; }
      xhttp.open ("GET", "HUMIread", true);
      xhttp.send ();
    }
    function clear ()
    {
      var canvas = document.getElementById ("plot");
      if (canvas.getContext)
      {
        var ctx = canvas.getContext ("2d");
        ctx.fillStyle = "#DDDDDD";
        ctx.fillRect (0, 0, 400, 500);
      }
    }
    function drawAxis ()
    {
      var canvas = document.getElementById ("plot");
      if (canvas.getContext)
      {
        var ctx = canvas.getContext ("2d");
        ctx.fillStyle = "#000000";
        ctx.beginPath ();
        ctx.moveTo (5, 495);
        ctx.lineTo (5, 5);
        ctx.stroke ();
        ctx.beginPath ();
        ctx.moveTo (5, 0);
        ctx.lineTo (10, 10);
        ctx.lineTo (0, 10);
        ctx.lineTo (5, 0);
        ctx.fill ();
        ctx.beginPath ();
        ctx.moveTo (5, 495);
        ctx.lineTo (400, 495);
        ctx.stroke ();
        ctx.beginPath ();
        ctx.moveTo (400, 495);
        ctx.lineTo (390, 490);
        ctx.lineTo (390, 500);
        ctx.lineTo (400, 495);
        ctx.fill ();
        ctx.stroke ();
      }
    }
       
    function draw ()
    {
      var canvas = document.getElementById ("plot");
      if (canvas.getContext)
      {
        var ctx = canvas.getContext ("2d");
        ctx.fillStyle = "#000000";
        for (j = 0; j < 6; j++) if (temperatures [j] !== 0)
        {
          ctx.beginPath ();
          ctx.arc (80 * j + (j === 0 ? 5 : (j === 5 ? -5 : 0)), 250 - 100 * (temperatures [j] - 20) + (j === 0 ? 5 : 0), 5, 0, Math.PI * 2, true);
          ctx.fill ();
        }
      }
    }
  </script>
</head>
<body>
  <h1>ESP32 Weather Station</h1>
  <p class = "text">Temperature: <span id = "TEMPvalue">0</span>&degC</p>
  <p class = "text">Humidity: <span id = "HUMIvalue">0</span>%</p>
  <br>
  <canvas id="plot" width="400" height="500"></canvas>
</body>
</html>
)=====";

void handleRoot () 
{
  server.send (200, "text/html", content);
}
void handleTEMP () 
{ 
  server.send (200, "text/plain", String (temperatureGlobal));
}

void handleHUMI () 
{
  server.send (200, "text/plain", String (humidityGlobal));
}


void IRAM_ATTR detectsMovement() {
  if (irrecv.decode( & results)) {
    Serial.print("Received signal: ");
    Serial.println(results.value);
    irrecv.resume(); // Receive the next value
    remoteControlTab[remoteControlTabIndex] = results.value;
    remoteControlTabIndex++;
    if (results.value == CHANEL_PLUS || results.value == IR_FAILED) remoteControlParser();
  }
}

void gotTouch() {

  Serial.println("Touch interrupt\n");
  resetLimits = 1;
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 6; i++) temperatures[i] = .0;
  for (int i = 0; i < 10; i++) remoteControlTab[i] = 0;

  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);
  Serial.println("Welcome to ESP32 Meteo Station!");
  xBinarySemaphore = xSemaphoreCreateBinary();
  xBinarySemaphoreHumidity = xSemaphoreCreateBinary();
  xBinarySemaphoreTemperature = xSemaphoreCreateBinary();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED);
  Serial.print("\nWiFi connected! Got IP: ");
  Serial.println(WiFi.localIP());

  xTaskCreatePinnedToCore(
    TaskDHTCode, /* Task function. */
    "TaskDHT", /* name of task. */
    10000, /* Stack size of task */
    NULL, /* parameter of the task */
    1, /* priority of the task */
    &TaskDHTHandler, /* Task handle to keep track of created task */
    0); /* pin task to core 0 */


  xTaskCreatePinnedToCore(
    TaskDallasCode,
    "TaskDallas",
    10000,
    NULL,
    1,
    &TaskDallasHandler,
    0);


  xTaskCreatePinnedToCore(
    TaskBuzzerCode,
    "TaskBuzzer",
    10000,
    NULL,
    1,
    &TaskBuzzerHandler,
    1);


  xTaskCreatePinnedToCore(
    TaskSoilMoisureCode,
    "TaskSoilMoisure",
    10000,
    NULL,
    1,
    &TaskSoilMoisureHandler,
    1);


  server.on("/", handleRoot);
  server.on("/TEMPread", handleTEMP);
  server.on("/HUMIread", handleHUMI);
  server.begin();
  Serial.println("HTTP server started");

  irrecv.enableIRIn();
  attachInterrupt(digitalPinToInterrupt(RECV_PIN), detectsMovement, RISING);
  touchAttachInterrupt(T4, gotTouch, 20);
}

void loop() {
  delay(1);
  server.handleClient();
}
