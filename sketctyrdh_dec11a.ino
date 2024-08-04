#include <ModbusMaster.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

const int RO_PIN = 10;
const int DI_PIN = 11;
const int RE_PIN = 8;
const int DE_PIN = 7;

// GSM Module configuration
const int GSM_RX_PIN = 18; // Use any available RX pin on Mega
const int GSM_TX_PIN = 19; // Use any available TX pin on Mega
SoftwareSerial gsmSerial(GSM_RX_PIN, GSM_TX_PIN); // RX, TX

//////////
#define sensor A1            // soil moisture sensor analog pin
#define SensorPin 0          // Ph sensor analog pin 
unsigned long int avgValue;  // Store the average value of the sensor feedback
float b,phValue;
int buf[10], temp,value,percent;

SoftwareSerial swSerial(RO_PIN, DI_PIN); // Receive (data in) pin, Transmit (data out) pin
ModbusMaster node;

// OLED display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1 // Reset pin not used with this display
#define OLED_ADDRESS 0x3C // Replace with your OLED display's I2C address

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Put the MAX485 into transmit mode
void preTransmission() {
  digitalWrite(RE_PIN, 1);
  digitalWrite(DE_PIN, 1);
}

// Put the MAX485 into receive mode
void postTransmission() {
  digitalWrite(RE_PIN, 0);
  digitalWrite(DE_PIN, 0);
}

void setup() {
  Serial.begin(9600);

  // configure the MAX485 RE & DE control signals and enable receive mode
  pinMode(RE_PIN, OUTPUT);
  pinMode(DE_PIN, OUTPUT);
  digitalWrite(DE_PIN, 0);
  digitalWrite(RE_PIN, 0);

  // Modbus communication runs at 9600 baud
  swSerial.begin(9600); // Use SoftwareSerial for NPK sensor

  // Modbus slave ID of NPK sensor is 1
  node.begin(1, swSerial);

  // Callbacks to allow us to set the RS485 Tx/Rx direction
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // Initialize the GSM module
  gsmSerial.begin(9600);

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
 
uint8_t result;

  // remove any characters from the receive buffer
  // ask for 3x 16-bit words starting at register address 0x0000
  result = node.readHoldingRegisters(0x0000, 3);

  if (result == node.ku8MBSuccess) {
    // Clear display
    display.clearDisplay();

    // Print sensor readings on the OLED display
    display.setCursor(0, 0);
    display.print("N: ");
    display.print(node.getResponseBuffer(0) * 10);
    display.println(" mg/kg");

    display.setCursor(0, 10);
    display.print("P: ");
    display.print(node.getResponseBuffer(1) * 10);
    display.println(" mg/kg");

    display.setCursor(0, 20);
    display.print("K: ");
    display.print(node.getResponseBuffer(2) * 10);
    display.println(" mg/kg");

    // Display the updated content
    display.display();
  } else {
    printModbusError(result);
  }

  delay(4000);

  //////////   data for ph sensor

  for (int i = 0; i < 10; i++) {
    buf[i] = analogRead(SensorPin);
    delay(10);
  }
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buf[i] > buf[j]) {
        temp = buf[i];
        buf[i] = buf[j];
        buf[j] = temp;
      }
    }
  }
  avgValue = 0;
  for (int i = 2; i < 8; i++)
    avgValue += buf[i];
  phValue = (float)avgValue * 5.0 / 1024 / 6;
  phValue = 3.5 * phValue;

  // Display pH data on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Ph Value");

  display.setTextSize(1);
  display.setCursor(70, 20);
  display.print(phValue);

  value = analogRead(sensor);   
   percent = map(value, 1024, 0, 0, 100);  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("MOISTURE");
  display.setCursor(70, 5);
  display.print(percent);
  display.print("%");
  display.display();

  delay(4000);

  // Read GSM module data and send to a mobile number
  readAndSendGSMData();
}

void readAndSendGSMData() {
  // Assuming you have functions to read data from sensors and format it for GSM
  // Modify the following lines according to your actual requirements

  // // For example, reading data from sensors
  // float temperature = 25.5;
  // int humidity = 50;

  // Format the message
  String message = "N: " + String(node.getResponseBuffer(0) * 10) + " mg/kg\n";
  message += "P: " + String(node.getResponseBuffer(1) * 10) + " mg/kg\n";
  message += "K: " + String(node.getResponseBuffer(2) * 10) + " mg/kg\n";
  message += "Ph: " + String(phValue) + "\n";
message += "Moisture: " + String(percent) + "%\n";

  // Send the message
  sendSMS(message);
}

void sendSMS(String message) {
  // Replace with the actual phone number you want to send the SMS to
  String phoneNumber = "+919818856542";

  // Clear any previous incoming data
  while (gsmSerial.available() > 0) {
    gsmSerial.read();
  }

  // Send AT commands to set up SMS mode
  gsmSerial.println("AT");
  delay(1000);
  gsmSerial.println("AT+CMGF=1"); // Set SMS mode to text
  delay(1000);
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(phoneNumber);
  gsmSerial.println("\"");
  delay(1000);
  gsmSerial.print(message);
  Serial.println("send");
  delay(100);
  gsmSerial.write(26); // Ctrl+Z to indicate the end of the message
  delay(1000);
}

//print out the error received from the Modbus library
void printModbusError( uint8_t errNum )
{
  switch ( errNum ) {
    case node.ku8MBSuccess:
      Serial.println(F("Success"));
      break;
    case node.ku8MBIllegalFunction:
      Serial.println(F("Illegal Function Exception"));
      break;
    case node.ku8MBIllegalDataAddress:
      Serial.println(F("Illegal Data Address Exception"));
      break;
    case node.ku8MBIllegalDataValue:
      Serial.println(F("Illegal Data Value Exception"));
      break;
    case node.ku8MBSlaveDeviceFailure:
      Serial.println(F("Slave Device Failure"));
      break;
    case node.ku8MBInvalidSlaveID:
      Serial.println(F("Invalid Slave ID"));
      break;
    case node.ku8MBInvalidFunction:
      Serial.println(F("Invalid Function"));
      break;
    case node.ku8MBResponseTimedOut:
      Serial.println(F("Response Timed Out"));
      break;
    case node.ku8MBInvalidCRC:
      Serial.println(F("Invalid CRC"));
      break;
    default:
      Serial.println(F("Unknown Error"));
      break;
  }
} 