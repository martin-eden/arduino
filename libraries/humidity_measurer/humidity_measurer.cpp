#include <Arduino.h>
#include "humidity_measurer.h"

humidity_measurer::humidity_measurer() {
  sensor_pin = A0;
  power_pin = 2;
  min_value = 0;
  max_value = 680;
  power_off_between_measures = false;
  high_means_dry = false;
  is_line_problem = false;
}

int humidity_measurer::get_raw_value() {
  const float measure_period = 3.8;
  const uint8_t num_measures = 20;
  const uint16_t measure_delay = measure_period * 1000 / num_measures;
  const uint16_t line_problem_threshold = 10;
  const uint16_t pre_measures_delay = 50;

  if (power_off_between_measures) {
    digitalWrite(power_pin, HIGH);
    delay(pre_measures_delay);
  }

  uint16_t raw_value;
  uint32_t sum = 0;
  for (uint8_t i = 1; i <= num_measures; i++) {
    raw_value = analogRead(sensor_pin);
    delay(measure_delay);
    sum += raw_value;
  }
  raw_value = sum / num_measures;

  if (power_off_between_measures)
    digitalWrite(power_pin, LOW);

  is_line_problem = (raw_value <= line_problem_threshold);

  return raw_value;
}

int humidity_measurer::get_value() {
  const int aligned_low = 0;
  const int aligned_high = 100;

  int result;
  result = get_raw_value();
  result = constrain(result, min_value, max_value);
  if (high_means_dry)
    result = map(result, min_value, max_value, aligned_high, aligned_low);
  else
    result = map(result, min_value, max_value, aligned_low, aligned_high);
  return result;
}
