// Interface to motor board.

/*
  Status: works
  Version: 2
  Last mod.: 2023-11-12
*/

#pragma once

#include <Arduino.h>

// Setup motorboard communication channel and test connection.
bool SetupMotorboardCommunication(uint32_t Baud, uint8_t Receive_Pin, uint8_t Transmit_Pin);

// Test connection by sending no-op command to motor board.
bool TestConnection();

// Send given M-codes. Returns TRUE if got response.
bool SendCommand(const char * Commands, uint16_t Timeout_Ms = 5000);

// Actually spin motors for some time.
void HardwareMotorsTest();

// Exploration. Send neutral command to measure ping.
uint16_t DetectPing_Ms(uint8_t NumMeasurements = 5);
