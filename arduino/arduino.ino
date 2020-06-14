#define DEBUG

// uncomment line to enable sensors log
//#define DEBUG_S

// uncomment line to enable op-functions log
//#define DEBUG_O

//#define DEBUG_C
#ifndef DEBUG
#define DISABLE_LOGGING
#endif

#include <SoftwareSerial.h>
#include <Time.h>
#include <SimpleDHT.h>
#include <ArduinoLog.h>
#include <EEPROM.h>

#define BAUDR 2000000
#define BT_BAUDR 115200
#define BT_TIMEOUT 150

enum MoistureLevel {
  L_LOW,
  L_MEDIUM,
  L_MEDIUM_P,
  L_HIGH,
  L_MAX,
};

const String MoistureLevelToString(const MoistureLevel &lvl) {
  switch (lvl) {
    case L_LOW:
      return "low";
    case L_MEDIUM:
      return "med";
    case L_MEDIUM_P:
      return "med+";
    case L_HIGH:
      return "hi";
    default:
      return "???";
  }
}

struct State {
  byte temp {};
  byte humidity {};
  byte waterLevel {};
  MoistureLevel moistureLevel {};
  byte tempTreshold {};
  unsigned long secondsSinceLastMoisture {};
  int daysSinceLastMoisture { 0 };
};


void serialEvent_(String &cmd, SoftwareSerial &serial);
void handleJson(const String& cmd);

const static byte BT_RX = 5;
const static byte BT_TX = 6;


 void runAtCmd(SoftwareSerial &serial, const String &cmd, bool expectOk = true, const char *term = "\n\r") {
      serial.print(cmd.c_str());
      serial.print(term);
      delay(250);
      String response = serial.readString();
      Log.notice("hc 06 responds: %s", response.c_str());
}

static const int DHT_PIN { 8 };
static const byte COOLER { 2 };
static const byte HEATER { 11 };
static const byte PUMP { 12 };
static const byte MOISTURE_SENSOR_PWR { 7 };
static const int WATER_SENSOR {A0};
static const int MOISTURE_SENSOR {A1};


//static int TANK_CAPACITY = 400; // ml
static int WATER_LVL_SENSOR_H = 480; // mm;

static State current_state {};// (26, 80, 0, 0);
static State target_state {};//(28, 0, 0, 1);
const int target_state_addr = 0;

using time_t = unsigned long;
static time_t epoch {millis()};
static time_t one_day = 86400000;

using BT = SoftwareSerial;

static BT bt_(BT_RX, BT_TX);
static SimpleDHT11 dht_{ DHT_PIN };
static bool IS_MANUAL {false};


void writeStat(State &state);

void loadState(const String &json, State &state);

float convertToVoltage(int pinVal, int resolution = 10, int operatingVoltage = 5);

void updateTemp(byte &temp, byte &humidity);
void updateWaterLvl(byte &water_lvl);
void updateMoistureLvl(MoistureLevel &lvl);

void operateTemp(byte current_temp, byte target_temp, byte cooler_pin = COOLER, byte heater_pin = HEATER);
void operateWaterPump(const MoistureLevel &cur_lvl, const MoistureLevel &target_lvl = L_MEDIUM, byte pump_pin = PUMP);

void setup()
{
  Serial.begin(BAUDR);
  bt_.begin(BT_BAUDR);
  bt_.setTimeout(BT_TIMEOUT);
  pinMode(WATER_SENSOR, INPUT);
#ifndef DISABLE_LOGGING
  Log.begin(LOG_LEVEL_VERBOSE, &Serial, true);
  Log.setSuffix([]() {
    Serial.println();
  });
#endif

  pinMode(COOLER, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(HEATER, OUTPUT);
  pinMode(PUMP, OUTPUT);
  pinMode(MOISTURE_SENSOR_PWR, OUTPUT);
  target_state.moistureLevel = L_HIGH;
  target_state.temp = 40;

  
  EEPROM.get(target_state_addr, target_state);
  if (!(target_state.temp > 0 && target_state.temp < 40 && target_state.moistureLevel >= L_LOW && target_state.moistureLevel < L_MAX)) {
    target_state.temp = 21;
    target_state.moistureLevel = L_MEDIUM;
  }
  Log.notice("target: %d C, moisture: %s", target_state.temp, MoistureLevelToString(target_state.moistureLevel).c_str());
}

void loop()
{
  updateTemp(current_state.temp, current_state.humidity);
  updateWaterLvl(current_state.waterLevel);
  updateMoistureLvl(current_state.moistureLevel);
//  operateTemp(current_state.temp, target_state.temp);
  operateWaterPump(current_state.moistureLevel, target_state.moistureLevel, PUMP);
  
  if (!bt_.available()) {
    #ifdef DEBUG
    if (!Serial.available())
    #endif
    return;
    
  }
  String cmd {
  #ifdef DEBUG
  !bt_.available() ? Serial.readString():
  #endif
    bt_.readString()
  };

  if (millis() % (1000 * 60 * 10) == 0 && millis())
  {
     EEPROM.put(target_state_addr, target_state);
  }

  serialEvent_(cmd, bt_);
}

const char *CMD_PREFIX = "C+";
// should start with CMD prefix
enum BTCommands {
  GET_TEMP, //0
  GET_HUMIDITY, //1
  GET_WATER_LEVEL, // 2
  GET_MOISTURE_LEVEL, // 3
  GET_TEMP_THRESHOLD, // 4
  

  GET_TARGET_TEMP, //5 
  GET_TARGET_HUMIDITY, //6
  GET_TARGET_WATER_LVL, //7
  GET_TARGET_MOISTURE_LEVEL, //8
  
  SET_TEMP, //9
  SET_HUMIDITY, //10
  SET_WATER_LEVEL, //11
  SET_MOISTURE_LEVEL, //12
  SET_TEMP_THRESHOLD, //13
  
  GET_SECONDS_SINCE_LAST_MOISTURE, //14
  GET_DAYS_SINCE_LAST_MOISTURE, //15
};

void serialEvent_(String &cmd, SoftwareSerial &serial) {
  //emulate bt cmnds
  Log.notice("got cmd: %s", cmd.c_str());
  cmd.replace("\n", "");
  cmd.replace("\r", "");

  if (cmd.startsWith("AT")) {
    Log.notice("got at cmd, redirecting to bt module");
    runAtCmd(bt_, cmd);
  } else if (cmd.startsWith("+DISK")) {
    Log.notice("got some mgmt cmnd");
  } else if (cmd.startsWith(CMD_PREFIX)) {
    cmd.replace(CMD_PREFIX, "");
    int idx_of_val_splitter = cmd.indexOf("=");
    int int_cmd {};
    String cmd_val {};
    if (idx_of_val_splitter >= 0) {
      int_cmd = cmd.substring(0, idx_of_val_splitter).toInt();
      cmd_val = cmd.substring(idx_of_val_splitter + 1);
    } else {
      int_cmd = cmd.toInt();
    }
    #ifdef DEBUG_C
    #define serial Serial
    #endif
    switch (int_cmd) {
      case GET_TEMP:
        serial.print(current_state.temp);
        break;
      case GET_HUMIDITY:
        serial.print(current_state.humidity);
        break;
      case GET_WATER_LEVEL:
        serial.print(current_state.waterLevel);
        break;
      case GET_MOISTURE_LEVEL:
        serial.print(current_state.moistureLevel);
        break;
      case GET_TEMP_THRESHOLD:
        serial.print(current_state.tempTreshold);
        break;
      case GET_TARGET_TEMP:
        serial.print(target_state.temp);
        break;
      case GET_TARGET_HUMIDITY:
        serial.print(target_state.humidity);
        break;
      case GET_TARGET_MOISTURE_LEVEL:
        serial.print(target_state.moistureLevel);
        break;
      case SET_TEMP:
        target_state.temp = min(max(cmd_val.toInt(), 0), 255);
        break;
      case SET_HUMIDITY:
        target_state.humidity = min(max(cmd_val.toInt(), 0), 255);
        break;
      case SET_MOISTURE_LEVEL:
        target_state.moistureLevel = max(min(cmd_val.toInt(), L_MAX), 0);
        break;
      case SET_TEMP_THRESHOLD:
        current_state.tempTreshold = cmd_val.toInt();
        break;
      case GET_SECONDS_SINCE_LAST_MOISTURE:
        serial.print((millis() % one_day - current_state.secondsSinceLastMoisture ) / 1000);
        break;
      case GET_DAYS_SINCE_LAST_MOISTURE:
        serial.print(current_state.daysSinceLastMoisture);
        break;
      default:
        Log.error("bad command %d", int_cmd);
        serial.print(-1);
      #ifdef DEBUG_C
      #undef serial
      #endif
    };
    if (int_cmd >= SET_TEMP && int_cmd <= SET_TEMP_THRESHOLD) {
        EEPROM.put(target_state_addr, target_state);
    }
  } else {
    Log.error("unknown command");
  }
}

void handleJson(const String& cmd) {
  //i dont care 'bout rest
  loadState(cmd, target_state);
}

void updateTemp(byte &temp, byte &humidity) {
  dht_.read(&temp, &humidity, NULL);
}


float convertToVoltage(int pinVal, int resolution, int operatingVoltage) {
  return pinVal / (pow(2, resolution) - 1) * operatingVoltage;
}

void updateWaterLvl(byte &water_lvl) {
  /*
     for some reason, this shit aint working ;(
     datasheet: https://static.chipdip.ru/lib/184/DOC001184399.pdf
     schema: https://static.chipdip.ru/lib/184/DOC001184401.pdf
     main point here is max out is 1.8, which maps to 4.8cm
  */

  // using static here, to calculate it only once,
  // i dont care bout loss when converting since value should be high enough
  static const int sensMax {1024 * 3 / 5 };


  float v {convertToVoltage(analogRead(WATER_SENSOR))};

  int v100 = int(v * 100);
  int h = (v <= 1.3) ? map(v100, 0, 130, 0, 5) :
          (v <= 1.53) ? map(v100, 130, 153, 5, 10) :
          (v <= 1.62) ? map(v100, 153, 162, 10, 15) :
          (v <= 1.69) ? map(v100, 162, 169, 15, 20) :
          (v <= 1.74) ? map(v100, 169, 174, 20, 25) :
          (v <= 1.77) ? map(v100, 174, 177, 25, 30) :
          (v <= 1.81) ? map(v100, 177, 181, 30, 35) :
          (v <= 1.84) ? map(v100, 181, 184, 35, 40) :
          (v <= 1.86) ? map(v100, 184, 186, 40, 45) :
          (v <= 1.88) ? map(v100, 186, 188, 45, 48) : 48;
  water_lvl = map(h, 0, 48, 0, 100);
#ifdef DEBUG_S
  Log.notice("water sensor returned %F V (%d), mapped it to %d %%", v, v100, water_lvl);
#endif
}

static time_t last_time_moisture_checked {};
static time_t heating_start {};

void updateMoistureLvl(MoistureLevel &lvl) {
  int v {analogRead(MOISTURE_SENSOR)};
  /* doc http://wiki.amperka.ru/products:sensor-soil-moisture-resistive
     logic here is that, op voltage is 5v and resolution is 1024
     the out value of moisture sensor is within range 0-3.5V, so this should equal to {3.5 * 1024 / 5}
  */
  
  if (last_time_moisture_checked && millis() - last_time_moisture_checked <= 1000 * 60 * 45) {
    return;
  }
  if (!heating_start) {
    heating_start = millis();
    digitalWrite(MOISTURE_SENSOR_PWR, HIGH);
  }
  
  if (millis() - heating_start < 1000 * 10) 
    return;
  heating_start = 0;
  static const int sensMax { 1024 * 3.5 / 5 };
  lvl = MoistureLevel(map(v, 0, sensMax, L_LOW, L_HIGH));
//#ifdef DEBUG_S
  Log.notice("moisture sensor value: %d, map it to %d, %d", v, lvl, HIGH);
//#endif
  last_time_moisture_checked = millis();
  digitalWrite(MOISTURE_SENSOR_PWR, LOW);
}

void operateTemp(byte current_temp, byte target_temp, byte cooler_pin, byte heater_pin) {
#ifdef DEBUG_O
  Log.notice("current temp: %d C, target temp: %d C", current_temp, target_temp);
#endif
  if (abs(current_temp - target_temp) < 2) {
#ifdef DEBUG_O
    Log.notice("temp within 1 delta, turning off cooler/heater");
#endif
    analogWrite(cooler_pin, 0);
    analogWrite(heater_pin, 0);
    digitalWrite(3, 0);
    return;
  }
  
  byte target_pin { current_temp < target_temp ? heater_pin: cooler_pin};
  byte disable_pin { current_temp < target_temp ? cooler_pin: heater_pin};
  digitalWrite(target_pin, HIGH);
  digitalWrite(disable_pin, LOW);
  digitalWrite(3, target_pin == COOLER ? HIGH: LOW);
}

void operateWaterPump(const MoistureLevel &cur_lvl, const MoistureLevel &target_lvl, byte pump_pin) {

  if (!current_state.secondsSinceLastMoisture) {
    current_state.secondsSinceLastMoisture = epoch;
  }
  time_t cur_millis = millis();
  time_t delta {(cur_millis % one_day - current_state.secondsSinceLastMoisture ) / 1000 };
  if (cur_lvl < target_lvl && delta > 60 * 60 * 2) {
#ifdef DEBUG_O
    Log.notice("start pumping");
    Log.notice("resetting time since last moisture to %l", current_state.secondsSinceLastMoisture / 1000);
#endif
    digitalWrite(pump_pin, HIGH);
    current_state.secondsSinceLastMoisture = cur_millis % one_day;
  } else if (delta > 5) {
#ifdef DEBUG_O
    Log.notice("stop pumping, 5s elapsed");
#endif
    digitalWrite(pump_pin, LOW);
  }
}
