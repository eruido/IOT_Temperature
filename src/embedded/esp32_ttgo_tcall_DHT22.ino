/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-sim800l-publish-data-to-cloud/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

/*
  Antonio Velasquez
  Codigo para envio de datos de sensores via http a servidor via post
   usando una placa TTGO-TCALL con SIM800 (transmision via red celular)
   para usar modulo http ethernet vea ejemplo ethernet 
  //20200918 - Incluye Lectura A0 Back Pressure Sensor

*/

/*
//Esto es para termocupla. En este ejemplo no se usa. Solo usaremos DHT22 ver mas abajo. 
#include "max6675.h"
// Puertos en ESP 32
int ktcSO = 8;
int ktcCS = 9;
int ktcCLK = 10;

MAX6675 ktc(ktcCLK, ktcCS, ktcSO);

*/

// Your GPRS credentials (leave empty, if not needed)
// Defina segun su compañia de datos
const char apn[]      = "imovil.entelpcs.cl"; // APN (example: internet.vodafone.pt) use https://wiki.apnchanger.org
const char gprsUser[] = "entelpcs"; // GPRS User
const char gprsPass[] = "entelpcs"; // GPRS Password

// SIM card PIN (leave empty, if not defined)
const char simPIN[]   = "1234"; 


#define ONBOARD_LED  2
 
#define TEMPHUM



#ifdef TEMPHUM
  const char server[] = "yourserver.yourdomain.com"; // domain name: example.com, maker.ifttt.com, etc
  const char resource[] = "/post-data.php";         // resource path, for example: /post-data.php
  const int  port = 80;                             // server port number
#endif



// Keep this API Key value to be compatible with the PHP code provided in the project page. 
// If you change the apiKeyValue value, the PHP file /post-data.php also needs to have the same key 
String apiKeyValue = "<api key defined at ../php/popst-data.php>";

// TTGO T-Call pins
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define I2C_SDA              21
#define I2C_SCL              22
// BME280 pins
#define I2C_SDA_2            18
#define I2C_SCL_2            19

// Set serial for debug console (to Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to SIM800 module)
#define SerialAT Serial1

// Configure TinyGSM library
#define TINY_GSM_MODEM_SIM800      // Modem is SIM800
#define TINY_GSM_RX_BUFFER   1024  // Set RX buffer to 1Kb

// Define the serial console for debug prints, if needed
#define DUMP_AT_COMMANDS

#include <Wire.h>
#include <TinyGsmClient.h>

#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

//#include <Adafruit_Sensor.h>
//#include <Adafruit_BME280.h>

// I2C for SIM800 (to keep it running when powered from battery)
TwoWire I2CPower = TwoWire(0);

// I2C for BME280 sensor
//TwoWire I2CBME = TwoWire(1);
//Adafruit_BME280 bme; 

// TinyGSM Client for Internet connection
TinyGsmClient client(modem);

#define uS_TO_S_FACTOR 1000000     /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  3600        /* Time ESP32 will go to sleep (in seconds) 3600 seconds = 1 hour */

#define IP5306_ADDR          0x75
#define IP5306_REG_SYS_CTL0  0x00

bool setPowerBoostKeepOn(int en){
  I2CPower.beginTransmission(IP5306_ADDR);
  I2CPower.write(IP5306_REG_SYS_CTL0);
  if (en) {
    I2CPower.write(0x37); // Set bit1: 1 enable 0 disable boost keep on
  } else {
    I2CPower.write(0x35); // 0x37 is default reg value
  }
  return I2CPower.endTransmission() == 0;
}


//DHT22 Sensor
#define temphum

#ifdef temphum
  #include <dhtnew.h>
  DHTNEW mySensor(25);  //sensor de temperatura y humedad
#endif




void setup() {

   pinMode(ONBOARD_LED,OUTPUT);

  // Set serial monitor debugging window baud rate to 115200
  SerialMon.begin(115200);

#ifdef temphum
    Serial.println("\n4. LastRead test");
    mySensor.read();
    for (int i = 0; i < 1; i++)
    {
            if (millis() - mySensor.lastRead() > 1000)
            {
              mySensor.read();
              Serial.println("Prueba de lectura TempHum");
            }
            Serial.print(mySensor.getHumidity(), 1);
            Serial.print(",\t");
            Serial.println(mySensor.getTemperature(), 1);
            delay(250);
        
            Serial.println("\nDone temphum test...");
    }
#endif


//START MODEM SETUP
  

  // Start I2C communication
  I2CPower.begin(I2C_SDA, I2C_SCL, 400000);
  //I2CBME.begin(I2C_SDA_2, I2C_SCL_2, 400000);

  // Keep power when running from battery
  bool isOk = setPowerBoostKeepOn(1);
  SerialMon.println(String("IP5306 KeepOn ") + (isOk ? "OK" : "FAIL"));

  // Set modem reset, enable, power pins
  pinMode(MODEM_PWKEY, OUTPUT);
  pinMode(MODEM_RST, OUTPUT);
  pinMode(MODEM_POWER_ON, OUTPUT);
  digitalWrite(MODEM_PWKEY, LOW);
  digitalWrite(MODEM_RST, HIGH);
  digitalWrite(MODEM_POWER_ON, HIGH);

  // Set GSM module baud rate and UART pins
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  // Restart SIM800 module, it takes quite some time
  // To skip it, call init() instead of restart()
  SerialMon.println("Initializing modem...");
  modem.restart();
  // use modem.init() if you don't need the complete restart

  // Unlock your SIM card with a PIN if needed
  if (strlen(simPIN) && modem.getSimStatus() != 3 ) {
    modem.simUnlock(simPIN);
  }else{
    // Serial.println("No hay SIM!");
  }
  
  // You might need to change the BME280 I2C address, in our case it's 0x76
 /*
  if (!bme.begin(0x76, &I2CBME)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    //while (1);  //avp vit
  }*/

  // Configure the wake up source as timer wake up  
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void loop() {
  SerialMon.print("Connecting to APN: ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("Fallo de conexion a APN");
  }
  else {
    SerialMon.println("Conexion al APN OK");
    
    SerialMon.print("Conectando al servidor:  ");
    SerialMon.print(server);
    if (!client.connect(server, port)) {
      SerialMon.println("Falló la conexión al servidor");
    }
    else {
      SerialMon.println("--------------------------");
    
      // Making an HTTP POST request
      SerialMon.println("Haciendo el HTTP POST ...");
      // Prepare your HTTP POST request data (Temperature in Celsius degrees)
      global_leq=(global_leq)*0.9+random(70)*0.1;
      float nps=global_leq;
      float temp=55.77+random(17);
      float humidity=55.77+random(40);
      float pressure=1;
      
 String httpRequestData;



#ifdef temphum
    humidity=mySensor.getHumidity();
    temp = mySensor.getTemperature();
    Serial.print(mySensor.getHumidity(), 1);
    Serial.print(",\t");
    Serial.println(mySensor.getTemperature(), 1);

    httpRequestData = "api_key=" + apiKeyValue + "&value1=" + String(temp)
                             + "&value2=" + String(humidity) + "&value3=" + String(nps) + "";
#endif

     
      // Prepare your HTTP POST request data (Temperature in Fahrenheit degrees)
      //String httpRequestData = "api_key=" + apiKeyValue + "&value1=" + String(1.8 * bme.readTemperature() + 32)
      //                       + "&value2=" + String(bme.readHumidity()) + "&value3=" + String(bme.readPressure()/100.0F) + "";
          
      // You can comment the httpRequestData variable above
      // then, use the httpRequestData variable below (for testing purposes without the BME280 sensor)
      //String httpRequestData = "api_key=tPmAT5Ab3j7F9&value1=24.75&value2=49.54&value3=1005.14";
    
      client.print(String("POST ") + resource + " HTTP/1.1\r\n");
      client.print(String("Host: ") + server + "\r\n");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(httpRequestData.length());
      client.println();
      client.println(httpRequestData);

      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 10000L) {
        // Print available data (HTTP response from server)
        while (client.available()) {
          char c = client.read();
          SerialMon.print(c);
          timeout = millis();
        }
      }
      SerialMon.println();
    
      // Close client and disconnect
      client.stop();
      SerialMon.println(F("Servidor desconectado"));
      modem.gprsDisconnect();
      SerialMon.println(F("GPRS desconectado"));
    }
  }
  // Put ESP32 into deep sleep mode (with timer wake up)
  esp_deep_sleep_start();
}
