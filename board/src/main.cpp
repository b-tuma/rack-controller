#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <EEPROM.h>
#include <Servo.h>

#define BOARD_LED_PIN 13
// Top to bottom
#define FAN_0_PIN 20
#define FAN_1_PIN 21
#define FAN_2_PIN 22
#define FAN_3_PIN 23
#define FAN_4_PIN 6 // Red
// Top to bottom
#define SERVO_0_PIN 9
#define SERVO_1_PIN 10
#define SERVO_2_PIN 5
// Left to right
#define SENSOR_0_PIN 2
#define SENSOR_1_PIN 1
#define SENSOR_2_PIN 0

//----- EEPROM
#define FAN_0_ADDR 0
#define FAN_1_ADDR 1
#define FAN_2_ADDR 2
#define FAN_3_ADDR 3
#define FAN_4_ADDR 4
#define SERVO_0_MIN_ADDR 5
#define SERVO_1_MIN_ADDR 6
#define SERVO_2_MIN_ADDR 7
#define SERVO_0_MAX_ADDR 8
#define SERVO_1_MAX_ADDR 9
#define SERVO_2_MAX_ADDR 10
#define SERVO_0_DEFAULT_ADDR 11
#define SERVO_1_DEFAULT_ADDR 12
#define SERVO_2_DEFAULT_ADDR 13
#define SENSOR_0_ENABLED_ADDR 17
#define SENSOR_1_ENABLED_ADDR 18
#define SENSOR_2_ENABLED_ADDR 19
#define FACTORY_ADDR 20

DHT_Unified dht[3] = {DHT_Unified(SENSOR_0_PIN, DHT22), DHT_Unified(SENSOR_1_PIN, DHT22), DHT_Unified(SENSOR_2_PIN, DHT22)};
const uint8_t SENSOR_ADDR[3] = {SENSOR_0_ENABLED_ADDR, SENSOR_1_ENABLED_ADDR, SENSOR_2_ENABLED_ADDR};
bool sensor_enabled[3];


const uint8_t FAN_PIN[5] = {FAN_0_PIN, FAN_1_PIN, FAN_2_PIN, FAN_3_PIN, FAN_4_PIN};
const uint8_t FAN_ADDR[5] = {FAN_0_ADDR, FAN_1_ADDR, FAN_2_ADDR, FAN_3_ADDR, FAN_4_ADDR};
uint8_t fan_pwm[5];

const uint8_t SERVO_PIN[3] = {SERVO_0_PIN, SERVO_1_PIN, SERVO_2_PIN};
const uint8_t SERVO_DEFAULT_ADDR[3] = {SERVO_0_DEFAULT_ADDR, SERVO_1_DEFAULT_ADDR, SERVO_2_DEFAULT_ADDR};
const uint8_t SERVO_MIN_ADDR[3] = {SERVO_0_MIN_ADDR, SERVO_1_MIN_ADDR, SERVO_2_MIN_ADDR};
const uint8_t SERVO_MAX_ADDR[3] = {SERVO_0_MAX_ADDR, SERVO_1_MAX_ADDR, SERVO_2_MAX_ADDR};
uint8_t servo_pwm[3];
uint8_t servo_max[3];
uint8_t servo_min[3];
Servo servo[3];

String inputString = "";
bool stringComplete = false;  // whether the string is complete
unsigned long start_time; 
unsigned long timed_event = 5000;
unsigned long current_time;
uint8_t i = 0;

// Factory Reset EEPROM settings
void factoryReset(){
  EEPROM.write(FAN_ADDR[0], 255);
  EEPROM.write(FAN_ADDR[1], 0);
  EEPROM.write(FAN_ADDR[2], 0);
  EEPROM.write(FAN_ADDR[3], 0);
  EEPROM.write(FAN_ADDR[4], 255);
  EEPROM.write(SERVO_MIN_ADDR[0], 95);
  EEPROM.write(SERVO_MIN_ADDR[1], 0);
  EEPROM.write(SERVO_MIN_ADDR[2], 0);
  EEPROM.write(SERVO_MAX_ADDR[0], 250);
  EEPROM.write(SERVO_MAX_ADDR[1], 255);
  EEPROM.write(SERVO_MAX_ADDR[2], 255);
  EEPROM.write(SERVO_DEFAULT_ADDR[0], 0);
  EEPROM.write(SERVO_DEFAULT_ADDR[1], 0);
  EEPROM.write(SERVO_DEFAULT_ADDR[2], 0);
  EEPROM.write(SENSOR_ADDR[0], true);
  EEPROM.write(SENSOR_ADDR[1], false);
  EEPROM.write(SENSOR_ADDR[2], false);
  EEPROM.write(FACTORY_ADDR, false);
  delay(500);
  _reboot_Teensyduino_();
}

void loadSettings()
{
  for(i=0; i<sizeof(sensor_enabled); i++)
    sensor_enabled[i] = EEPROM.read(SENSOR_ADDR[i]);
  for(i=0; i<sizeof(FAN_PIN); i++)
    fan_pwm[i] = EEPROM.read(FAN_ADDR[i]);

  for(i=0; i<sizeof(SERVO_PIN); i++)
  {
    servo_min[i] = EEPROM.read(SERVO_MIN_ADDR[i]);
    servo_max[i] = EEPROM.read(SERVO_MAX_ADDR[i]);
    servo_pwm[i] = EEPROM.read(SERVO_DEFAULT_ADDR[i]);
  } 
}

// Returns checksum for a string
String calculateChecksum(String sentence){
  uint8_t crc = 0;
  for (uint8_t i = 0; i < sentence.length(); i++)
  {
    crc ^= sentence[i];
  }
  String crcOutput = String(crc, HEX).toUpperCase();
  if (crcOutput.length() == 1)
    return '0' + crcOutput;
  else
    return crcOutput;
}

// Check if valid NMEA message (returns content if valid)
String sentenceValidator(String input){
  uint8_t length = input.length();
  int8_t startChar = input.indexOf('$');
  int8_t endChar = input.indexOf('*');
  if(length >= 6)
  {
    if(startChar == 0 && endChar == length-4)
    {
      String sentence = input.substring(startChar+1, endChar);
      String checksum = input.substring(endChar+1, endChar+3).toUpperCase();
      String calculatedChecksum = calculateChecksum(sentence);
      if(checksum == calculatedChecksum)
      {
        return sentence;
      }
    }
  }
  return "";
}

bool set_fan(uint8_t id, uint8_t pwm){
  if(id >= sizeof(FAN_PIN))
    return false;
  
  fan_pwm[id] = pwm;
  analogWrite(FAN_PIN[id], pwm);
  return true;
}

bool config_fan(uint8_t id, uint8_t pwm){
  if(id >= sizeof(FAN_PIN))
    return false;

  EEPROM.write(FAN_ADDR[id], pwm);
  return true;
}

bool config_sensor(uint8_t id, bool enabled){
  if(id >= sizeof(sensor_enabled))
    return false;

  EEPROM.write(SENSOR_ADDR[id], enabled);
  return true;
}

bool config_servo(uint8_t id, uint8_t min, uint8_t max, uint8_t pwm){
  if(id >= sizeof(SERVO_PIN))
    return false;

  EEPROM.write(SERVO_MIN_ADDR[id], min);
  EEPROM.write(SERVO_MAX_ADDR[id], max);
  EEPROM.write(SERVO_DEFAULT_ADDR[id], pwm);
  return true;
}

bool set_servo(uint8_t id, uint8_t pwm){
  if(id >= sizeof(SERVO_PIN))
    return false;

  servo[id].attach(SERVO_PIN[id]);
  servo_pwm[id] = pwm;
  servo[id].write(map(pwm, 0, 255, servo_min[id], servo_max[id]));
  delay(1000);
  servo[id].detach();
  return true;
}

void setup() {
  Serial.println("Initializing RackController");
  pinMode(BOARD_LED_PIN, OUTPUT);
  for(i=0; i<sizeof(FAN_PIN); i++)
    pinMode(FAN_PIN[i], OUTPUT);
  inputString.reserve(200);

  Serial.println("Loading saved settings...");
  bool doFactoryReset = EEPROM.read(FACTORY_ADDR);
  if(doFactoryReset){
    factoryReset();
    return;
  }

  // Load values
  loadSettings();

  // Initialize fans
  for(i=0; i<sizeof(FAN_PIN); i++)
    set_fan(i, fan_pwm[i]);

  // Initialize sensors
  for(i=0; i<sizeof(sensor_enabled); i++){
    if(sensor_enabled[i])
      dht[i].begin();
  }
    
  // Initialize servos
  for(i=0; i<sizeof(SERVO_PIN); i++){
    set_servo(i, servo_pwm[i]);
  }

  Serial.println("Initialized.");
  current_time = millis();
  start_time = current_time;
}

// the loop function runs over and over again forever
void loop() {
  current_time = millis();
  if(current_time - start_time >= timed_event)
  {
    start_time = current_time;
    float temperature = 0;
    float humidity = 0;
    String crc = "";
    String sentence = "";
    bool healthy = true;

    for(i=0; i<sizeof(sensor_enabled); i++)
    {
      sensors_event_t event;
      dht[i].temperature().getEvent(&event);
      if (!isnan(event.temperature)) {
        temperature = event.temperature;
      } else healthy = false;
      dht[i].humidity().getEvent(&event);
      if (!isnan(event.relative_humidity)) {
        humidity = event.relative_humidity;
      } else healthy = false;
      if(healthy)
      {
        sentence = "SENSOR,"+String(i)+","+String(temperature)+","+String(humidity);
        crc = calculateChecksum(sentence);
        Serial.println("$"+sentence+"*"+crc);
      }
    }
  }
  

  if (stringComplete) {
    Serial.println(inputString);
    // clear the string:
    String sentence = sentenceValidator(inputString);
    if(sentence != "")
    {
      bool success = false;
      uint8_t index = 0;
      char* object = ""; // FAN, SENSOR, SERVO...
      char* command = ""; // SET, CONFIG...
      uint8_t id = 0; // 1, 2, 3...
      uint8_t value1 = 0;
      uint8_t value2 = 0;
      uint8_t value3 = 0;

      // Parse sentence
      char* token;
      token = strtok((char*)sentence.c_str(),",");
      while (token != NULL)
      {
        switch (index)
        {
        case 0:
          object = token;
          break;
        case 1:
          command = token;
          break;
        case 2:
          id = atoi(token);
          break;
        case 3:
          value1 = atoi(token);
          break;
        case 4:
          value2 = atoi(token);
          break;
        case 5:
          value3 = atoi(token);
          break;
        default:
          break;
        }
        index++;
        token = strtok (NULL, ",");
      }

      // Fan commands
      if(strcmp(object, "FAN") == 0)
      {
        if(strcmp(command, "SET") == 0)
        {
          if(index == 4)
            success = set_fan(id, value1);
        }
        else if(strcmp(command, "CONFIG") == 0)
        {
          if(index == 4)
            success = config_fan(id, value1);
        }
      } 
      // Sensor commands
      else if(strcmp(object, "SENSOR") == 0)
      {
        if(strcmp(command, "CONFIG") == 0)
        {
          if(index == 4)
            success = config_sensor(id, value1);
        } 
      }
      // Servo commands
      else if(strcmp(object, "SERVO") == 0)
      {
        if(strcmp(command, "SET") == 0)
        {
          if(index == 4)
            success = set_servo(id, value1);
        }
        else if(strcmp(command, "CONFIG") == 0)
        {
          if(index == 6)
            success = config_servo(id, value1, value2, value3);
        }
      }
      // Controller commands
      else if(strcmp(object, "CONTROLLER") == 0)
      {
        if(strcmp(command, "RESTART") == 0)
        {
          _reboot_Teensyduino_();
          return;
        }
        else if(strcmp(command, "FACTORY") == 0)
        {
          EEPROM.write(FACTORY_ADDR, true);
          delay(200);
          _reboot_Teensyduino_();
          return;
        }
      }

      if(success)
      {
        digitalWrite(BOARD_LED_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(100);                       // wait for a second
        digitalWrite(BOARD_LED_PIN, LOW);
      }
      else{
        String sentence = "FAILURE,"+String(object);
        String crc = calculateChecksum(sentence);
        Serial.println("$"+sentence+"*"+crc);
      }
    }
    
    inputString = "";
    stringComplete = false;
  }
  delay(50);
}

// Completes string on new line.
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    if (inChar == '\n') {
      stringComplete = true;
    }
    else
    {
      inputString += inChar;
    }
  }
}