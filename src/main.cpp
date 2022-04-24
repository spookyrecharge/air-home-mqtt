#include <Arduino.h>
#include <SoftwareSerial.h> 

///////////////////////////////////////////
bool debug = true;  
///////////////////////////////////////////

unsigned long previousMillis = 0;
const long interval = 10000; 
int co2;
int freq = 10000;
int counter = 1;

SoftwareSerial co2_serial(13, 15); // RX, TX

const byte s8_co2[8] = {0xfe, 0x04, 0x00, 0x03, 0x00, 0x01, 0xd5, 0xc5};
const byte s8_fwver[8] = {0xfe, 0x04, 0x00, 0x1c, 0x00, 0x01, 0xe4, 0x03};
const byte s8_id_hi[8] = {0xfe, 0x04, 0x00, 0x1d, 0x00, 0x01, 0xb5, 0xc3};
const byte s8_id_lo[8] = {0xfe, 0x04, 0x00, 0x1e, 0x00, 0x01, 0x45, 0xc3};
byte buf[10];

void myread(int n) {
  int i;
  memset(buf, 0, sizeof(buf));
  for(i = 0; i < n; ) {
    if(co2_serial.available() > 0) {
      buf[i] = co2_serial.read();
      i++;
    }
    yield();
    delay(10);
  }
}

// Compute the MODBUS RTU CRC
uint16_t ModRTU_CRC(byte* buf, int len){
  uint16_t crc = 0xFFFF;
  
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];            // XOR byte into least sig. byte of crc
  
    for (int i = 8; i != 0; i--) {        // Loop over each bit
      if ((crc & 0x0001) != 0) {          // If the LSB is set
        crc >>= 1;                        // Shift right and XOR 0xA001
        crc ^= 0xA001;
      }
      else                                // Else LSB is not set
        crc >>= 1;                        // Just shift right
    }
  }
  // Note, this number has low and high bytes swapped, so use it accordingly (or swap bytes)
  return crc;  
}


uint16_t readco2() {
  uint16_t crc, got, co2;
  co2_serial.write(s8_co2, 8);
  myread(7);
  co2 = (uint16_t)buf[3] * 256 + (uint16_t)buf[4];
  crc = ModRTU_CRC(buf, 5);
  got = (uint16_t)buf[5] + (uint16_t)buf[6] * 256;
  if(crc != got) {
    Serial.print("Invalid checksum.  Expect: ");
    Serial.print(crc, HEX);
    Serial.print("  Got: ");
    Serial.println(got, HEX);
  }
  return co2;
}


void get_readings()
{

  if (debug == true)
    {
    co2 = readco2();
    Serial.print("Iteration #"); Serial.println(counter);
    Serial.print("millis = "); Serial.println(millis());
    Serial.print("co2 = "); Serial.println(co2);
    Serial.println("");
    }

  counter++;
}


void setup()
{
  Serial.begin(9600);  
  co2_serial.begin(9600);
}


void loop() 
{
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    get_readings();
    if (counter >= 6666) // Still don't know how to handle RAM properly
    {
      Serial.print("RESTARTING !!!");
      ESP.restart();
    }
  }
}