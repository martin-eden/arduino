// Measures soil dryness and pours if needed.

const char
  code_name[] = "Pour manager",
  code_descr[] = "Measures soil dryness and pours if needed.",
  version[] = "2.5.0";

/*
  Status: stable
  Last mod.: 2020-12-28
*/

/*
  Requirements:
    * 4-digit LED display
    * Capacitive soil moisture sensor
    * Relay switch
    * Water pump motor
*/

#include <me_switch.h>
#include <me_CapacitiveFilter.h>
#include <TM1637Display.h>

const uint8_t
  MEASURER_SIGNAL_PIN = A0,

  MOTOR_CONTROL_PIN = 2,

  DISPLAY_INPUT_PIN = 5,
  DISPLAY_CLOCK_PIN = 6;

const uint16_t
  MEASURER_RANGE_LOW = 315,
  MEASURER_RANGE_HIGH = 318,
  MEASURER_ABS_LOW = 240,
  MEASURER_ABS_HIGH = 650;

const bool
  MEASURER_HIGH_MEANS_DRY = true;

const uint32_t
  MAX_MOTOR_ON_TIME_S = 120,
  /* debug:
  GET_HUMIDITY_TICK_S = 1,
  IDLE_DURATION_S = 2,
  PRE_POUR_DURATION_S = 2,
  BASE_POUR_DURATION_S = 3,
  POST_POUR_DURATION_S = 2,
  DESIRED_POUR_CYCLE_S = 40;
  //*/
  //* work:
  GET_HUMIDITY_TICK_S = 10,
  IDLE_DURATION_S = 60,
  PRE_POUR_DURATION_S = 180,
  BASE_POUR_DURATION_S = 20,
  POST_POUR_DURATION_S = 900,
  DESIRED_POUR_CYCLE_S = 86400;
  //*/

c_switch motor = c_switch(MOTOR_CONTROL_PIN);
CapacitiveFilter capacitiveFilter = CapacitiveFilter(11);
TM1637Display display(DISPLAY_CLOCK_PIN, DISPLAY_INPUT_PIN);

void setup() {
  Serial.begin(9600);
  display.setBrightness(0x0F);
}

const char
  CMD_GET_STATE = 'G';

void print_usage() {
  String msg = "";
  msg =
    msg +
    "----------------------------------------------------\n" +
    "Code: \"" + code_name + "\"\n" +
    "Description: " + code_descr + "\n" +
    "Version: " + version + "\n" +
    "----------------------------------------------------\n" +
    "Wiring:\n"
    "  MOTOR_CONTROL_PIN: " + MOTOR_CONTROL_PIN + "\n" +
    "  MEASURER_SIGNAL_PIN: " + MEASURER_SIGNAL_PIN + "\n" +
    "  DISPLAY_INPUT_PIN: " + DISPLAY_INPUT_PIN + "\n" +
    "  DISPLAY_CLOCK_PIN: " + DISPLAY_CLOCK_PIN + "\n" +
    "\n" +
    "Settings:\n" +
    "  MEASURER_RANGE_LOW: " + MEASURER_RANGE_LOW + "\n" +
    "  MEASURER_RANGE_HIGH: " + MEASURER_RANGE_HIGH + "\n" +
    "  MEASURER_ABS_LOW: " + MEASURER_ABS_LOW + "\n" +
    "  MEASURER_ABS_HIGH: " + MEASURER_ABS_HIGH + "\n" +
    "\n";
  Serial.print(msg);

  Serial.print("  High values means dry?: ");
  Serial.println(MEASURER_HIGH_MEANS_DRY);

  msg = "";
  msg =
    msg +
    "Usage:" + "\n" +
    "  " + CMD_GET_STATE + " - Print current status.\n" +
    "\n";
  Serial.print(msg);
}

void handle_command() {
  char cmd = Serial.peek();
  uint8_t data_length = Serial.available();
  int8_t on_off;
  uint8_t value;
  switch (cmd) {
    case CMD_GET_STATE:
      Serial.read();
      print_status();
      break;
    default:
      Serial.read();
      print_usage();
      break;
  }
}

int16_t get_raw_value() {
  return analogRead(MEASURER_SIGNAL_PIN);
}

bool is_line_problem() {
  int16_t result = get_raw_value();
  return (result <= MEASURER_ABS_LOW) || (result >= MEASURER_ABS_HIGH);
}

int16_t get_humidity() {
  float result = get_raw_value();
  int16_t raw_result = result;

  capacitiveFilter.addValue(raw_result);
  result = capacitiveFilter.getValue();

  uint16_t display_result = (int16_t)(result * 10);
  if (display_result >= 10000)
    display_result = 9999;

  display.showNumberDec(display_result);

  /*
  Serial.print("get_humidity: ");
  Serial.print(result, 3);
  Serial.print(" ");
  Serial.print(raw_result);
  Serial.println("");
  //*/

  if (is_line_problem()) {
    result = -1;
    delay(500);
    display.showNumberDec(result, false);
    delay(500);
  }

  return result;
}

bool is_low_rh(int16_t cur_hum) {
  return
    (cur_hum != -1) &&
    (
      (MEASURER_HIGH_MEANS_DRY && (cur_hum > MEASURER_RANGE_HIGH)) ||
      (!MEASURER_HIGH_MEANS_DRY && (cur_hum < MEASURER_RANGE_LOW))
    );
}

void motor_on() {
  if (motor.is_off()) {
    motor.switch_on();
    Serial.println("Motor ON.");
  }
}

void motor_off() {
  if (motor.is_on()) {
    motor.switch_off();
    Serial.println("Motor OFF.");
  }
}

bool ui32_gte(uint32_t a, uint32_t b) {
  return
    (a >= b) ||
    (
      (a < b) &&
      (b - a >= 0x40000000)
    );
}

const uint8_t
  STATE_IDLE = 0,
  STATE_PRE_POUR_STAGING = 1,
  STATE_POURING = 2;

uint8_t state = STATE_IDLE;
uint32_t next_state_update_time = millis();

void print_internal_status(uint32_t cur_time) {
  Serial.print(state);
  Serial.print(" ");
  Serial.print(cur_time);
  Serial.print(" ");
  Serial.print(next_state_update_time);
  Serial.println();
}

void serialEvent() {
  handle_command();
}

uint32_t last_motor_off_moment = 0;
uint32_t pour_duration_ms = BASE_POUR_DURATION_S * 1000;

void print_status() {
  String msg = "";

  msg = "";
  msg =
    msg +
    "motor.is_on(): " + motor.is_on() + "\n" +
    "cur_time: " + millis() + "\n" +
    "last_motor_off_moment: " + last_motor_off_moment + "\n" +
    "pour_duration_ms: " + pour_duration_ms + "\n" +
    "\n";
  Serial.print(msg);
}

void loop() {
  uint32_t cur_time = millis();
  int16_t cur_hum = get_humidity();

  // print_internal_status(cur_time);

  bool is_time_to_update = ui32_gte(cur_time, next_state_update_time);
  if (is_time_to_update) {
    if (state == STATE_IDLE) {
      if (is_low_rh(cur_hum)) {
        state = STATE_PRE_POUR_STAGING;
        next_state_update_time = cur_time + PRE_POUR_DURATION_S * 1000;
      } else {
        next_state_update_time = cur_time + IDLE_DURATION_S * 1000;
      }
    } else if (state == STATE_PRE_POUR_STAGING) {
      if (is_low_rh(cur_hum)) {
        state = STATE_POURING;
        motor_on();
        if (last_motor_off_moment != 0) {
          pour_duration_ms = pour_duration_ms * ((float)(DESIRED_POUR_CYCLE_S * 1000) / (cur_time - last_motor_off_moment));
          pour_duration_ms = min(pour_duration_ms, MAX_MOTOR_ON_TIME_S * 1000);
        }

        next_state_update_time = cur_time + pour_duration_ms;
      } else {
        state = STATE_IDLE;
        next_state_update_time = cur_time + IDLE_DURATION_S * 1000;
      }
    } else if (state == STATE_POURING) {
      motor_off();
      last_motor_off_moment = cur_time;
      state = STATE_IDLE;
      next_state_update_time = cur_time + POST_POUR_DURATION_S * 1000;
    }
  }

  delay(GET_HUMIDITY_TICK_S * 1000);
}

/*
  2016-03-16
  [...]
  2020-03-14
    Reduced mostly experimental features of "Flower frined" to minimum.
  2020-11-14
    Reduced code even more.
  2020-12-28
    [+] CapacitiveFilter. Reduced code.
  2020-12-31
    [+] Dynamic calculation of pour duration.
*/
