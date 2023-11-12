// Interface to motor board.

/*
  Status: works
  Version: 6
  Last mod.: 2023-11-12
*/

#include "Motors.h"

#include <SoftwareSerial.h>

auto & HardwareSerial_ = Serial;
EspSoftwareSerial::UART SoftwareSerial_;

// ---

const uint16_t Motorboard_PrintHelpMaxTime_Ms = 4000;

// Delay to wait when motorboard is transferring. Will set in Setup..().
static uint8_t TimePerCharacter_Ms = 0;

bool SetupSoftwareSerial(uint32_t Baud, uint8_t Receive_Pin, uint8_t Transmit_Pin);
uint32_t GetTimePassed_Ms(uint32_t StartTime_Ms, uint32_t EndTime_Ms = 0);

// Setup communication channel and test connection.
bool SetupMotorboardCommunication(uint32_t Baud, uint8_t Receive_Pin, uint8_t Transmit_Pin)
{
  bool IsConnected = false;

  HardwareSerial_.print("Motorboard initialization... ");

  if (!SetupSoftwareSerial(Baud, Receive_Pin, Transmit_Pin))
  {
    // This never happened in my experience.
    HardwareSerial_.println("Software serial initialization failed.");
    return false;
  }

  TimePerCharacter_Ms = 1000 / (Baud / 10) + 1;

  IsConnected = TestConnection();

  if (!IsConnected)
  {
    uint32_t StartTime_Ms = millis();
    while (!IsConnected && (GetTimePassed_Ms(StartTime_Ms) < Motorboard_PrintHelpMaxTime_Ms * 10))
    {
      delay(10);
      IsConnected = TestConnection();
    }
  }

  if (IsConnected)
    HardwareSerial_.println("yep.");
  else
    HardwareSerial_.println("nah!");

  if (IsConnected)
  {
    HardwareSerial_.print("Measuring motorboard ping: ");
    uint16_t PingValue_Ms = DetectPing_Ms();
    HardwareSerial_.printf("%d ms\n", PingValue_Ms);
  }

  return IsConnected;
}

bool WaitForReadySignal(uint16_t Timeout_Ms);

/*
  Core function.

  Send command to motor board and wait for feedback.

    Response wait timeout.

    As we do not parse commands, we dont know how they will take to
    execute. We just sending them and waiting for feedback.

    But if connection to motorboard is dropped we will never receive
    feedback. In this case we are stopping listening and exiting by
    timeout.

    So here is tradeoff between maximum command execution time and
    time wasted when connection was dropped.

    Protocol sets high limit for duration phase 5s but we can have
    more commands in that string.

    Command is limited to chunk size. I expect chunk size is less than
    100 bytes. So theoretical limit is how many time may be spent on
    processing 100 byte string. "D5000" takes 5 second per 5 bytes.
    So 100 seconds.
*/
bool SendCommand(const char * Commands, uint16_t Timeout_Ms /* = ... */)
{
  if (SoftwareSerial_.available())
  {
    /*
      Motorboard is sending something to us. Thats not typical.

      Probably it's startup help text. Wait for ready signal.
      It should be after help text is printed.
    */

    if (!WaitForReadySignal(Motorboard_PrintHelpMaxTime_Ms))
      return false;
  }

  SoftwareSerial_.write(Commands);

  return WaitForReadySignal(Timeout_Ms);
}

// ---

bool SetupSoftwareSerial(uint32_t Baud, uint8_t Receive_Pin, uint8_t Transmit_Pin)
{
  EspSoftwareSerial::Config ByteEncoding = SWSERIAL_8N1;

  SoftwareSerial_.begin(Baud, ByteEncoding, Receive_Pin, Transmit_Pin);

  return (bool) SoftwareSerial_;
}

uint32_t GetTimePassed_Ms(uint32_t StartTime_Ms, uint32_t EndTime_Ms /* = 0 */)
{
  if (EndTime_Ms == 0)
    EndTime_Ms = millis();

  return EndTime_Ms - StartTime_Ms;
}

bool WaitForReadySignal(uint16_t Timeout_Ms)
{
  /*
    Waiting for response.

    We ignoring all side output from board and waiting for "\nG\n"
    and empty stream as a signal that board is ready for further
    commands.
  */
  char Chars[3] = "";

  uint32_t StartTime_Ms = millis();

  while (GetTimePassed_Ms(StartTime_Ms) < Timeout_Ms)
  {
    if (SoftwareSerial_.available())
    {
      Chars[2] = Chars[1];
      Chars[1] = Chars[0];
      Chars[0] = SoftwareSerial_.read();

      // Correct response is "\nG\n":
      if ((Chars[2] == '\n') && (Chars[1] == 'G') && (Chars[0] == '\n'))
      {
        delay(TimePerCharacter_Ms);

        if (!SoftwareSerial_.available())
          return true;
      }
    }

    delay(TimePerCharacter_Ms);
  }

  return false;
}

// Send command and measure time.
bool SendCommand_Time_Ms(const char * Command, uint32_t * TimeTaken_Ms)
{
  uint32_t StartTime_Ms = millis();
  bool SendCommandResult = SendCommand(Command);
  *TimeTaken_Ms = GetTimePassed_Ms(StartTime_Ms);

  return SendCommandResult;
}

// SendCommand with time tracing and debug output.
bool SendCommand_Trace(const char * Command)
{
  HardwareSerial_.printf("SendCommand_Time_Ms(\"%s\"): ", Command);

  uint32_t TimeTaken_Ms = 0;

  bool SendCommandResult = SendCommand_Time_Ms(Command, &TimeTaken_Ms);

  HardwareSerial_.printf("%u\n", TimeTaken_Ms);

  return SendCommandResult;
}

// Send dummy command to get feedback.
bool TestConnection()
{
  uint16_t TestCommandTimeout_Ms = 3 * TimePerCharacter_Ms + 10;
  bool Result = SendCommand(" ", TestCommandTimeout_Ms);
  // HardwareSerial_.printf("\nTestConnection(%u): %u\n", TestCommandTimeout_Ms, Result);
  return Result;
}

String GenerateCommand(int8_t LeftMotor_Pc, int8_t RightMotor_Pc, uint16_t Duration_Ms)
{
  uint8_t MaxCommandSize = 32;
  char Command[MaxCommandSize];

  snprintf(
    Command,
    MaxCommandSize,
    "L %d R %d D %d ",
    LeftMotor_Pc,
    RightMotor_Pc,
    Duration_Ms
  );

  return String(Command);
}

// Exploration. Sending commands to measure ping.
uint16_t DetectPing_Ms(uint8_t NumMeasurements)
{
  char Command[] = " ";

  uint32_t TimePassed_Ms = 0;
  uint8_t NumMeasurementsDone = 0;

  for (NumMeasurementsDone = 0; NumMeasurementsDone < NumMeasurements; ++NumMeasurementsDone)
  {
    uint32_t SendCommand_Duration_Ms = 0;

    bool IsSent = SendCommand_Time_Ms(Command, &SendCommand_Duration_Ms);

    if (!IsSent)
      break;

    TimePassed_Ms += SendCommand_Duration_Ms;
  }

  return TimePassed_Ms / NumMeasurementsDone;
}

/*
  Send commands to motor board to briefly move motors.

  Originally it was linear progression [0, 100, 0, -100, 0].
  But it became too boring when I tested and debugged code.
  So now its sine sweep. Motor power is sin([0, 360]).

  Nonlinear acceleration.
*/
void HardwareMotorsTest()
{
  HardwareSerial_.print("Motors test.. ");

  /*
    Ideal test duration.

    Actual test time will be longer as sending commands will take
    additional time (~8 ms per command for 57600 baud).
  */
  const uint16_t TestDuration_Ms = 800;
  const uint16_t NumCommands = 12;

  const uint16_t CommandDuration_Ms = TestDuration_Ms / NumCommands;

  const uint16_t NumAnglesInCircle = 360;
  const uint16_t AngleIncerement = NumAnglesInCircle / NumCommands;
  const uint8_t Amplitude = 100;

  uint16_t Angle = 0;
  while (1)
  {
    if (Angle > NumAnglesInCircle)
      Angle = NumAnglesInCircle;

    float Angle_Rad = (float) Angle / NumAnglesInCircle * (2 * M_PI);
    int8_t MotorPower_Pc = Amplitude * sin(Angle_Rad);

    SendCommand(
      GenerateCommand(MotorPower_Pc, MotorPower_Pc, CommandDuration_Ms).c_str()
    );

    if (Angle == NumAnglesInCircle)
      break;

    Angle += AngleIncerement;
  }

  HardwareSerial_.println("done.");
}

// ---

/*
  2023-11-07
  2023-11-09
  2023-11-11
  2023-11-12
*/
