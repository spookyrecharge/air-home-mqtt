#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "WiFiUdp.h"
// #include "ArduinoOTA.h"  // Board is hanging sometimes when enabled
#include "PMS.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <SoftwareSerial.h>
#include <NTPClient.h> 
#include "user_interface.h"
#define SEALEVELPRESSURE_HPA (1013.25)

///////////////////////////////////////////
bool serial_print = true;
///////////////////////////////////////////

const int period = 43200000; // 12 hours NTP sync period
const uint16_t utcOffsetInSeconds = 10800; // UTC +3 Kiev
unsigned long last_time = 0;
uint8_t previous_send = 111;

bool data_ready = false;
bool dust_readed = false;

float Temperature;
float Humidity;
float Pressure;
uint8_t Dust_1;
uint8_t Dust_2;
uint8_t Dust_10;
uint16_t co2;
uint32_t free_heap;
uint32_t counter = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ua.pool.ntp.org", utcOffsetInSeconds); // Change to your closest one


// ----------------- HOME --------------
const char* ssid = "<wifi-ssid>";
const char* password = "<wifi-pass>";
const char* mqttServer = "<mqtt-ip-addr>";
// -------------------------------------

WiFiClient espClient;
PubSubClient client(espClient);
Adafruit_BME280 bme;
SoftwareSerial pms_serial(14, 12); // RX, TX // D5
SoftwareSerial co2_serial(13, 15); // RX, TX
PMS pms(pms_serial); 
PMS::DATA data;

const byte s8_co2[8] = {0xfe, 0x04, 0x00, 0x03, 0x00, 0x01, 0xd5, 0xc5};
const byte s8_fwver[8] = {0xfe, 0x04, 0x00, 0x1c, 0x00, 0x01, 0xe4, 0x03};
const byte s8_id_hi[8] = {0xfe, 0x04, 0x00, 0x1d, 0x00, 0x01, 0xb5, 0xc3};
const byte s8_id_lo[8] = {0xfe, 0x04, 0x00, 0x1e, 0x00, 0x01, 0x45, 0xc3};
byte buf[10];


void myread(int n) 
{
  int i;
  memset(buf, 0, sizeof(buf));
  for(i = 0; i < n; ) {
    if(co2_serial.available() > 0) 
    {
      buf[i] = co2_serial.read();
      i++;
    }
    yield();
    delay(10);
  }
}

// Compute the MODBUS RTU CRC
uint16_t ModRTU_CRC(byte* buf, int len)
{
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) 
  {
    crc ^= (uint16_t)buf[pos];          // XOR byte into least sig. byte of crc
    for (int i = 8; i != 0; i--) 
    {    // Loop over each bit
      if ((crc & 0x0001) != 0) 
      {      // If the LSB is set
        crc >>= 1;                    // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                            // Else LSB is not set
        crc >>= 1;                    // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;  
}


void initWifi() 
{
  //Serial.print("Connecting to: "); 
  //Serial.print(ssid);
  //IPAddress device_ip (192, 168, 0, 183);
  //IPAddress dns_ip ( 1, 1, 1, 1);
  //IPAddress gateway_ip (192, 168, 0, 1);
  //IPAddress subnet_mask(255, 255, 255, 0);
  //WiFi.config(device_ip, dns_ip, gateway_ip, subnet_mask);
  WiFi.begin(ssid, password);  
  uint16_t timeout = 10000; // 10 seconds
  while(WiFi.status() != WL_CONNECTED  && (timeout-- > 0)) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  if(WiFi.status() != WL_CONNECTED) 
  {
     Serial.println("Failed to connect, going back to sleep");
  }
  Serial.print("WiFi connected in: "); 
  Serial.print(millis());
  Serial.print(", IP address: "); 
  Serial.println(WiFi.localIP());
}


uint16_t readco2() 
{
  uint16_t crc, got, co2;
  co2_serial.write(s8_co2, 8);
  myread(7);
  co2 = (uint16_t)buf[3] * 256 + (uint16_t)buf[4];
  crc = ModRTU_CRC(buf, 5);
  got = (uint16_t)buf[5] + (uint16_t)buf[6] * 256;
  if(crc != got) 
  {
    Serial.print("Invalid checksum.  Expect: ");
    Serial.print(crc, HEX);
    Serial.print("  Got: ");
    Serial.println(got, HEX);
  }
  return co2;
}


void sendmqtt()
{
  String payload;
  
  if (Temperature < -20 || isnan(Temperature))
  {
    Serial.println("wrong temperature value");
  }
  else
  {
  payload = "air,location=home temperature=";
  payload += Temperature;
  client.publish("sensors", (char*) payload.c_str());
  }
  //###########################################################
  if (Humidity == 100 || isnan(Humidity) || Humidity == 0) 
  {
    Serial.println("wrong humidity value");
  }
  else
  {
  payload = "air,location=home humidity=";
  payload += Humidity;
  client.publish("sensors", (char*) payload.c_str());
  }
  //###########################################################
  payload = "air,location=home pressure=";
  payload += Pressure;
  client.publish("sensors", (char*) payload.c_str());
  //###########################################################

  if (co2 < 200 || isnan(co2) || co2 > 4000) 
  {
    Serial.println("wrong co2 value");
  }
  else
  {
  payload = "air,location=home co2=";
  payload += co2;
  client.publish("sensors", (char*) payload.c_str());
  //###########################################################
  }
  
  if (isnan(Dust_1)) 
  {
    Serial.println("wrong dust value");
  }
  else
  {
  payload = "air,location=home dust_1.0=";
  payload += Dust_1;
  client.publish("sensors", (char*) payload.c_str());
  payload = "air,location=home dust_2.5=";
  payload += Dust_2;
  client.publish("sensors", (char*) payload.c_str());
  payload = "air,location=home dust_10.0=";
  payload += Dust_10;
  client.publish("sensors", (char*) payload.c_str());
  }
  //###########################################################
  payload = "sys,location=home free_heap=";
  payload += free_heap;
  client.publish("sensors", (char*) payload.c_str());
  
  if (serial_print == true)
    {
    Serial.println(timeClient.getFormattedTime());
    Serial.print("data sent to mqtt, packet #"); Serial.println(counter);
    Serial.print("millis = "); Serial.println(millis());
    Serial.print("temperature = "); Serial.println(Temperature);
    Serial.print("humidity = "); Serial.println(Humidity);
    Serial.print("pressure = "); Serial.println(Pressure);
    Serial.print("co2 = "); Serial.println(co2);
    Serial.print("dust_1 = "); Serial.println(Dust_1);
    Serial.print("dust_2 = "); Serial.println(Dust_2);
    Serial.print("dust_10 = "); Serial.println(Dust_10);  
    Serial.print("free_heap = "); Serial.println(free_heap);
    Serial.println("");
    }

  counter++;
  data_ready = false;
}


void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-Home";
    // clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str(), "mqtt", "")) 
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("inTopic");
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void synchronize_time()
{
  Serial.println("NTP precise sync start");
  timeClient.update();
  int last_seconds = timeClient.getSeconds();
  while (last_seconds == timeClient.getSeconds())
  {
    timeClient.update();
    delay(100);
  }
  Serial.println("NTP precise sync done");
}


void setup()
{
  pms_serial.begin(9600);
  Serial.begin(9600);  
  co2_serial.begin(9600, SWSERIAL_8N1, 13, 15, false, 256); // baud, bits, RX, TX, invert, buffer
  wifi_set_sleep_type(NONE_SLEEP_T);
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  } 
  delay(10);
  WiFi.mode(WIFI_STA);
  initWifi();
  delay(10);
  client.setServer(mqttServer, 1883);
 
  // ArduinoOTA.onStart([]() {
  //   Serial.println("Start");
  // });
  // ArduinoOTA.onEnd([]() {
  //   Serial.println("\nEnd");
  // });
  // ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  //   Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  // });
  // ArduinoOTA.onError([](ota_error_t error) {
  //   Serial.printf("Error[%u]: ", error);
  //   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
  //   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
  //   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
  //   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
  //   else if (error == OTA_END_ERROR) Serial.println("End Failed");
  // });
  // ArduinoOTA.begin();
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.begin();
  delay(10);
  timeClient.update();
  delay(10);
  synchronize_time();
}


void loop() 
{
  // ArduinoOTA.handle();

  if (pms.read(data) && data_ready == false) 
  {
    Dust_1 = data.PM_AE_UG_1_0;
    Dust_2 = data.PM_AE_UG_2_5;
    Dust_10 = data.PM_AE_UG_10_0;
    co2 = readco2();
    free_heap = system_get_free_heap_size();
    Temperature = bme.readTemperature();
    Temperature = Temperature - 1.2;
    Humidity = bme.readHumidity();
    Pressure = bme.readPressure()/133.32;   
    if (!client.connected()) // Check if MQTT connection persist
    {
      reconnect(); // Restore MQTT connection if not present
    }
    client.loop(); // Keep MQTT connection before sending
    data_ready = true;
  }
  
  if (data_ready == true)
  {
    //timeClient.update();
    uint8_t seconds = timeClient.getSeconds();
    if (seconds % 10 == 0 && previous_send != seconds) // send data every at round time (:00, :10, :20, ...)
    {
      sendmqtt();
      previous_send = seconds;
    }
  }

  if ((millis() - last_time) > period) // NTP sync every 12h
  {
    last_time = millis();
    synchronize_time();
  }

}
