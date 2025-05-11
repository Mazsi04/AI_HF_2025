#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RadioLib.h>

// CC1101 has the following connections:
// CS pin:    10
// GDO0 pin:  2
// RST pin:   unused
// GDO2 pin:  3
CC1101 radio = new Module(5, 2, RADIOLIB_NC, 4);

volatile bool receivedFlag = false;

ICACHE_RAM_ATTR

void setFlag(void) {
  // we got a packet, set the flag
  receivedFlag = true;
}

//#define GDO0 2

const char* ssid = "fhazinet300";
const char* password = "Mazsola04";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Szenzor értékek
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;

// Frissítési időzítés
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1000;

byte buffer[61] = {0};

enum MotorCmd { CMD_NONE, CMD_OPEN, CMD_CLOSE, CMD_STOP };
volatile MotorCmd pendingCmd = CMD_NONE;

// HTML oldal visszaadása
void handleRoot(AsyncWebServerRequest *request) {
  String html = "<h1>ESP32 Webszerver</h1>";
  html += "<p>Valos ideju adatok:</p>";
  html += "<div id=\"temp\">Homerseklet: ...</div>";
  html += "<div id=\"humi\">Paratartalom: ...</div>";
  html += "<div id=\"pres\">Legnyomas: ...</div>";
  /*html += "<p><a href=\"/motor_on\"><button>Motor Bekapcsol</button></a></p>";
  html += "<p><a href=\"/motor_off\"><button>Motor Kikapcsol</button></a></p>";*/
  html += "<p>";
  html += "<a href=\"/motor_open\"><button>Nyitas</button></a> ";
  html += "<a href=\"/motor_close\"><button>Zaras</button></a> ";
  html += "<a href=\"/motor_stop\"><button>STOP</button></a>";
html += "</p>";
  html += "<script>\n";
  html += "  if (!!window.EventSource) {\n";
  html += "    var source = new EventSource('/events');\n";
  html += "    source.onmessage = function(e) {\n";
  html += "      var data = e.data.split(',');\n";
  html += "      document.getElementById('temp').innerHTML = 'Homerseklet: ' + data[0] + ' &#8451;';\n";
  html += "      document.getElementById('humi').innerHTML = 'Paratartalom: ' + data[1] + ' %';\n";
  html += "      document.getElementById('pres').innerHTML = 'Legnyomas: ' + data[2] + ' hPa';\n";
  html += "    }\n";
  html += "  }\n";
  html += "</script>\n";
  request->send(200, "text/html", html);
}

void handleMotorOpen(AsyncWebServerRequest *request) {
  //ELECHOUSE_cc1101.SendData((uint8_t*)"MOTOR_OPEN", strlen("MOTOR_OPEN"));
  pendingCmd = CMD_OPEN;
  //request->send(200, "text/html", "<p>Motor NYIT! <a href=\"/\">Vissza</a></p>");
  request->send(204);
}
void handleMotorClose(AsyncWebServerRequest *request) {
  //ELECHOUSE_cc1101.SendData((uint8_t*)"MOTOR_CLOSE", strlen("MOTOR_CLOSE"));
  pendingCmd = CMD_CLOSE;
  //request->send(200, "text/html", "<p>Motor ZAR! <a href=\"/\">Vissza</a></p>");
  request->send(204);
}
void handleMotorStop(AsyncWebServerRequest *request) {
  //ELECHOUSE_cc1101.SendData((uint8_t*)"MOTOR_STOP", strlen("MOTOR_STOP"));
  pendingCmd = CMD_STOP;
  //request->send(200, "text/html", "<p>Motor MEGALL! <a href=\"/\">Vissza</a></p>");
  request->send(204);
}


portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t cc1101ReciveTaskHandle = NULL;
TaskHandle_t cc1101SendTaskHandle = NULL;
TaskHandle_t WebserverTaskHandle = NULL;

void CC1101SendTask(void *pvParameters) {
  // Retrieve the core number the task is running on
    

    String str[3] = {"MOTOR_OPEN","MOTOR_CLOSE","MOTOR_STOP"};
    for (;;) {
      if (pendingCmd != CMD_NONE) {
      Serial.print("TX cmd: ");
      Serial.println(pendingCmd);
      switch (pendingCmd) {
        case CMD_OPEN:  
          Serial.println(str[0]);
          //portENTER_CRITICAL(&mux);
          radio.transmit(str[0]);
          //portEXIT_CRITICAL(&mux);
          break;
        case CMD_CLOSE: 
          Serial.println(str[1]);
          //portENTER_CRITICAL(&mux);
          radio.transmit(str[1]);
          //portEXIT_CRITICAL(&mux);
          break;
        case CMD_STOP:  
          Serial.println(str[2]);
          //portENTER_CRITICAL(&mux);
          radio.transmit(str[2]);
          //portEXIT_CRITICAL(&mux);
          break;
        default: 
            Serial.println("default"); 
          break;
      }
      vTaskDelay(5 / portTICK_PERIOD_MS);                       // engedjük lezajlani a TX-et
      pendingCmd = CMD_NONE;
      radio.startReceive();
      Serial.println(pendingCmd);
      yield();  
    }
       vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void CC1101ReciveTask(void *pvParameters) {
  // Retrieve the core number the task is running on
  String str;
  int state = 0;
    for (;;) {
      if(receivedFlag) {
        // reset flag
        receivedFlag = false;
        // you can read received data as an Arduino String
        state = radio.readData(str);
        Serial.println(str);
        radio.startReceive();

        if (str.startsWith("T=")) {
          temperature = str.substring(2).toFloat();  // csak a szám, pl. "25.48"
        }
        if (str.startsWith("P=")) {
          pressure = str.substring(2).toFloat();  // csak a szám, pl. "25.48"
        }
        if (str.startsWith("H=")) {
          humidity = str.substring(2).toFloat();  // csak a szám, pl. "25.48"
        }
        //if (dataStr.startsWith("T:")) sscanf((char *)buffer, "T:%f", &temperature);
          //else if (dataStr.startsWith("H:")) sscanf((char *)buffer, "H:%f", &humidity);
          //else if (dataStr.startsWith("P:")) sscanf((char *)buffer, "P:%f", &pressure);
      }


        //portEXIT_CRITICAL(&mux);
       vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void WebserverTask(void *pvParameters) {
  // Retrieve the core number the task is running on
  server.on("/", HTTP_GET, handleRoot);
  server.on("/motor_open", HTTP_GET, handleMotorOpen);
  server.on("/motor_close", HTTP_GET, handleMotorClose);
  server.on("/motor_stop", HTTP_GET, handleMotorStop);
  server.addHandler(&events);
  server.begin();

    for (;;) {
    

    /* SSE frissítés */
    if (millis() - lastUpdateTime >= updateInterval) {
      char ssend[64];
      snprintf(ssend, sizeof(ssend), "%.2f,%.2f,%.2f", temperature, humidity, pressure);
      events.send(ssend, "message", millis());
      lastUpdateTime = millis();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);


    WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  Serial.print(F("[CC1101] Initializing ... "));
  int state = radio.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  radio.setPacketReceivedAction(setFlag);

  // start listening for packets
  Serial.print(F("[CC1101] Starting to listen ... "));
  state = radio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true) { delay(10); }
  }

  // counter to keep track of transmitted packets
//int count = 0;


  xTaskCreatePinnedToCore( CC1101ReciveTask, "CC1101_Recive_Task", 4096, NULL, 2, &cc1101ReciveTaskHandle, 0 );
  xTaskCreatePinnedToCore( CC1101SendTask, "CC1101_Send_Task", 4096, NULL, 1, &cc1101SendTaskHandle, 0 );
  xTaskCreatePinnedToCore( WebserverTask, "Webserver_Task", 4096, NULL, 1, &WebserverTaskHandle, 1 );

}

void loop() {
  // put your main code here, to run repeatedly:

}
