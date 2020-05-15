//#include "Stream.h"
#include <SoftwareSerial.h>
#include <Time.h>
#include <SimpleDHT.h>
#include <ArduinoJson.h>

#define BAUDR 250000

#define DEBUG

#define DEBUG_S
// comment line to enable sensors log
#undef DEBUG_S

#define DEBUG_O
// comment line to enable op-functions log
#undef DEBUG_O

#ifndef DEBUG
#define DISABLE_LOGGING
#endif
#include <ArduinoLog.h>

#define JSON_CAPACITY 512

enum MoistureLevel {
  L_LOW,
  L_MEDIUM,
  L_HIGH,
  L_MAX,
};

const String MoistureLevelToString(const MoistureLevel &lvl) {
  switch (lvl) {
    case L_LOW:
      return "low";
    case L_MEDIUM:
      return "med";
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
};
//
//enum BTCommands {
//  GET_TEMP,
//  GET_HUMIDITY,
//  GET_WATER_LEVEL,
//  GET_MOISTURE_LEVEL,
//  GET_TEMP_THRESHOLD,
//  SET_TEMP,
//  SET_HUMIDITY,
//  SET_WATER_LEVEL,
//  SET_MOISTURE_LEVEL,
//  SET_TEMP_THRESHOLD,
//}

void serialEvent_(Stream &stream);
void handleJson(const String& cmd);

class BT {
  public:
    const static byte rx {
      5
    };
    const static byte tx {6};
    const long m_rate;
    SoftwareSerial m_serial{rx, tx};
    SoftwareSerial m_hardware_serial { 1, 0 };
    bool m_echo = false;
    String m_term {"\n\r"};
    BT(): BT(57600) {}
    BT(long rate): m_rate {rate} {
      m_serial.begin(57600);
      m_serial.setTimeout(1000);
      m_hardware_serial.begin(BAUDR);
      m_serial.listen();
    }
    void runAtCmd(const String &cmd, bool expectOk = true) {
      m_serial.print(cmd.c_str());
      m_serial.print(m_term);
      delay(1000);
      String response = m_serial.readString();
      Serial.print("hc 06 responds: ");
      Serial.print(response);
    }
    void doRoutine() {
      if (!m_serial.available()) {
        return;
      }
      Log.notice("handilng bt evt");
      serialEvent_(m_serial);
    }

    const char &termChar() {
      return m_term[m_term.length() - 1];
    }
};

static const int DHT_PIN { 8 };
static const byte COOLER { 10};
static const byte HEATER { 11 };
static const byte PUMP { 12 };


static const int WATER_SENSOR {A0};
static const int MOISTURE_SENSOR {A1};
;

//static int TANK_CAPACITY = 400; // ml
static int WATER_LVL_SENSOR_H = 480; // mm;

static State current_state {};// (26, 80, 0, 0);
static State target_state {};//(28, 0, 0, 1);

static BT bt_;
static SimpleDHT11 *dht_ {nullptr};
static bool IS_MANUAL {false};


void writeStat(State &state);
DynamicJsonDocument getStats(const State &state);
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
  dht_ = new SimpleDHT11(DHT_PIN);
  pinMode(WATER_SENSOR, INPUT);
#ifndef DISABLE_LOGGING
  Log.begin(LOG_LEVEL_VERBOSE, &Serial, true);
  Log.setSuffix([]() {
    Serial.println();
  });
#endif
  pinMode(COOLER, OUTPUT);
  pinMode(HEATER, OUTPUT);
  target_state.moistureLevel = L_HIGH;
  target_state.temp = 40;
}

void loop()
{
  updateTemp(current_state.temp, current_state.humidity);
  updateWaterLvl(current_state.waterLevel);
  updateMoistureLvl(current_state.moistureLevel);
  bt_.doRoutine();
  operateTemp(current_state.temp, target_state.temp);
  operateWaterPump(current_state.moistureLevel, target_state.moistureLevel);
#ifdef DEBUG
  delay(1000); // so we can actually read some values
#endif
}
enum NextEvt {
  UNSET,
  SET_TARGET_STATE,
  SET_TARGET_SHEET
};

enum Command {
  JSON_REQUEST,
  STATS,
};
void serialEvent() {
  serialEvent_(Serial);
}

void serialEvent_(Stream &stream) {
  static NextEvt next_evt {UNSET};
  Log.notice("processing evt");
  //emulate bt cmnds
  if (!stream.available()) {
    Log.notice("no data available");
    return;
  }
  String cmd {stream.readString()};
  cmd.replace("\n", "");
  cmd.replace("\r", "");

  Log.notice("got cmd: %s", cmd.c_str());

  if (cmd.startsWith("AT")) {
    Serial.println("got at cmd, redirecting to bt module");
    bt_.runAtCmd(cmd);
  } else if (cmd.startsWith("+DISK")) {
    Log.notice("got some mgmt cmnd");
  }
  else {
  Log.notice("waiting evt: %d", next_evt);
  switch (cmd.toInt()) {
    //todo: dat is stupid actually, do it like F+{SMSHIT}, this should be faster than serialization/deserialization
    case JSON_REQUEST:
      handleJson(cmd);
      break;
    case STATS:
      writeStat(current_state);
      break;
    default:
      Log.error("unknown command");
  }
}
}

void handleJson(const String& cmd) {
  //i dont care 'bout rest
  loadState(cmd, target_state);
}

void updateTemp(byte &temp, byte &humidity) {
  dht_->read(&temp, &humidity, NULL);
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

void updateMoistureLvl(MoistureLevel &lvl) {
  int v {analogRead(MOISTURE_SENSOR)};
  /* doc http://wiki.amperka.ru/products:sensor-soil-moisture-resistive
     logic here is that, op voltage is 5v and resolution is 1024
     the out value of moisture sensor is within range 0-3.5V, so this should equal to {3.5 * 1024 / 5}
  */
  static const int sensMax { 1024 * 3 / 5 };
  lvl = MoistureLevel(map(v, 0, sensMax, L_LOW, L_HIGH));
#ifdef DEBUG_S
  Log.notice("moisture sensor value: %d, map it to %d", v, lvl);
#endif
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
    return;
  }
  byte diff { abs(current_temp - target_temp) };
  byte target_pin {};
  if (current_temp < target_temp) {
    target_pin = heater_pin;
#ifdef DEBUG_O
    Log.notice("turning heater on");
#endif
  } else {
    target_pin = cooler_pin;
#ifdef DEBUG_O
    Log.notice("turning cooler on");
#endif
  }
  // max prevents overflow
  analogWrite(target_pin, map(diff, 1, max(15, diff), 0, 255));
}

void operateWaterPump(const MoistureLevel &cur_lvl, const MoistureLevel &target_lvl, byte pump_pin) {
#ifdef DEBUG_O
  Log.notice("current moisture: %s, target_moisture: %s", MoistureLevelToString(cur_lvl).c_str(), MoistureLevelToString(target_lvl).c_str());
#endif
  if (cur_lvl < target_lvl) {
#ifdef DEBUG_O
    Log.notice("start pumping");
#endif
    digitalWrite(pump_pin, HIGH);
    delay(1000);
    digitalWrite(pump_pin, LOW);
  }
}

void loadState(const String &json, State &state) {
  Log.notice("loading json: %s", json.c_str());
  DynamicJsonDocument doc(JSON_CAPACITY);
  DeserializationError err { deserializeJson(doc, json.c_str()) };
  if (err) {
    Log.fatal("got error(code: %d), while parsing json", err);
    return;
  }
  #ifdef DEBUG
  serializeJsonPretty(doc, Serial);
  Serial.println();
  #endif
  state.temp = doc["temp"].as<byte>();
  state.moistureLevel = MoistureLevel(doc["moistureLevel"].as<int>());
};


DynamicJsonDocument getStats(const State &state) {
  DynamicJsonDocument doc(JSON_CAPACITY);
  doc["temp"] = state.temp;
  doc["humidity"] = state.humidity;
  doc["waterLevel"] = state.waterLevel;
  doc["moistureLevel"] = state.moistureLevel;
  doc.shrinkToFit();
  return doc;
}

void writeStat(State &state) {
  DynamicJsonDocument stat {getStats(state)};
#ifdef DEBUG
  serializeJsonPretty(stat, Serial);
  Serial.println();
#endif
  serializeJson(stat, bt_.m_serial);
};
