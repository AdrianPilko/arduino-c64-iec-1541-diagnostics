// This is an attempt to write a fully function diagnostic for the 1541 disc drive and IEC bus
// Arduino was chosen because it is natively logic level 5V - like the c64 and the IEC bus
// The code makes heavy use and reference of the excellent Jim Butterfield guide 1541 Disected
//  https://www.commodoreserver.com/FileDownload.asp?FID=1A8DEC655F34480F846540B6215493EE
// or search "1541 disected"
// IEC Bus True = 0V (or pulled down)  = logical 0
//         False= 5V (or released)     = logical 1
// bytes sent low bit first
// data valid on rising edge of the clock

// Adrian Pilkington 2024


#define IEC_TRUE 0
#define IEC_FALSE 1
#define INIT_DEBUG_OUT_TIMER 60000
#define INIT_DEBUG_OUT_TIMER2 80000
#define IEC_1541_DATA  2
#define IEC_1541_CLOCK  6
#define IEC_1541_ATN  4
#define IEC_1541_RESET  5

// we're configuring this device as 8 - standard 1541 disk drive
#define BUS_IDENT 0x8  
// these next two will need to change with real data and real sequeces of commands
#define MAX_SEND_DATA 3
#define MAX_SEQUENCES 1

uint16_t microsBefore[8];
uint16_t microsAfter[8];

typedef enum _busMode {e_ATNCommands, e_Data} t_busMode;
typedef enum _busDeviceState {e_unlisten,e_untalk, e_listen, e_talk} t_busDeviceState ;
typedef enum _protocolState {e_waitingForBusCommand, e_ReadingData, e_WritingData} t_protocolState;

byte dataSequences[MAX_SEQUENCES][MAX_SEND_DATA] = {{0xaa,0xa1,0xa2}};
   

// the other connections are just ground and SRQ which isn't used here
// from the computer female port they are numbered like this (note notch at top of the DIN 6 connector)
//      
//              | | 
//  DATA  (5)         (1)  SRQ (unused here)
//              (6) RESET      
//
//  CLK   (4)         (2)  GROUND
//              (3) ATN

// from https://forum.arduino.cc/t/getpinmode/232960
int getPinMode(uint8_t pin)
{
  uint8_t bit = digitalPinToBitMask(pin);
  uint8_t port = digitalPinToPort(pin);
  volatile uint8_t *reg, *out;

  if (port == NOT_A_PIN) return -1;

  // JWS: can I let the optimizer do this?
  reg = portModeRegister(port);
  out = portOutputRegister(port);

  if ((~*reg & bit) == bit) // INPUT OR PULLUP
  {
    if ((~*out & bit)  == bit) return INPUT;
    else return INPUT_PULLUP;
  }
  return OUTPUT;
}

inline void outputClock(bool level)
{
  digitalWrite(IEC_1541_CLOCK, level);
}

inline void waitForDataIEC_TRUE()
{  
  while (digitalRead(IEC_1541_DATA) == IEC_FALSE)
  {
  }
}
inline void waitForDataIEC_FALSE()
{  
  while (digitalRead(IEC_1541_DATA) == IEC_TRUE)
  {
  }
}

inline void waitForClockIEC_TRUE()
{
  // sit here waiting for the clock line to go false (IEC_FALSE 5V)
  while (digitalRead(IEC_1541_CLOCK)  == IEC_FALSE)
  {

  }
}
inline void waitForClockIEC_FALSE()
{
  // sit here waiting for the clock line to go false (IEC_FALSE 5V)
  while (digitalRead(IEC_1541_CLOCK)  == IEC_TRUE)
  {

  }
}

inline void assertDataIEC_TRUE()
{
  digitalWrite(IEC_1541_DATA, IEC_TRUE);  
}
inline void assertDataIEC_FALSE()
{
  digitalWrite(IEC_1541_DATA, IEC_FALSE);  
}

void writeData(uint8_t data)
{
 /// todo
}

inline byte readData()
{ 
  /// these two control if EOI - end or identify sequence 200usec
  unsigned long beforeEOITiming = 0;
  unsigned long afterEOITiming = 0;

  pinMode(IEC_1541_CLOCK, INPUT);     
  pinMode(IEC_1541_DATA, OUTPUT);    

  waitForClockIEC_TRUE();    
  assertDataIEC_TRUE();     
  
  beforeEOITiming = micros();       
  waitForClockIEC_FALSE();    
  afterEOITiming = micros();
  // if it took >= 200usec for the talker to put clock line to false then EOI - C64 prog ref calls this End or identify
  // Jim Butterfields IEC disected calls this End of indicator - which is correct ??? (does it matter - no!)
  if (afterEOITiming - beforeEOITiming > 200) 
  {
    //Serial.print("EOI ");
    //Serial.println(afterEOITiming - beforeEOITiming);
    /// pull data line true for 60usec
    pinMode(IEC_1541_DATA, OUTPUT);        
    assertDataIEC_TRUE();
    beforeEOITiming = micros();
    do
    {
      afterEOITiming = micros();
    }while (afterEOITiming - beforeEOITiming <= 60);
    //holdDataIEC_FALSE();
    pinMode(IEC_1541_DATA, INPUT);       // setting pin mode releases to 5V
    return 0;
  }  
  pinMode(IEC_1541_DATA, INPUT);   // we're ready to listen, the release is done by setting pin to INPUT
  uint8_t data = 0x00;
  uint8_t i = 0;
  do
  {    
    waitForClockIEC_FALSE();     
    byte temp = digitalRead(IEC_1541_DATA);    
    data = temp | data;
    data = data << 1;  
    waitForClockIEC_TRUE();     
  }
  while (++i < 8);

  pinMode(IEC_1541_DATA, OUTPUT);
  assertDataIEC_TRUE(); // acknowledge    

  return data;
}

inline bool checkATN()
{
    return digitalRead(IEC_1541_ATN);
}

inline byte decodeData(byte data)
{
  /*
https://en.wikipedia.org/wiki/Commodore_bus
  High level protocol[10]
Command	Destination	Meaning
/28	Device	Listen, device number 8
/F0	Device	Open channel 0
Device	Send filename bytes
/3F	Devices	Unlisten all devices
/48	Device	Talk, Device number 8
/60	Device	Reopen channel 0
Device number 8 becomes the master of the bus
Host	Receive byte data
The host becomes the master of the bus (normal operation)
/5F	Devices	Untalk all devices
/28	Device	Listen, device number 8
/E0	Device	Close channel 0
/3F	Devices	Unlisten all devices
*/
    if (data == 0x28)   // for bus device = 8
    {
      Serial.print("28,");
    }
    else if (data == 0x3f) 
    {   
      Serial.print("3f,");
    }
    else if (data == 0x48)
    {
      Serial.print("48,");
    }                
    else if (data == 0x5f) 
    {
      Serial.print("5f,");
    }
    //these all assume channel 0 for now    (channe0 | 0x60) == 0x60
    else if (data == 0x60)  
    {
      Serial.print("60,");                          
    }
    else if (data == 0xe0)
    {
      Serial.print("e0,");
    }
    else if (data == 0xf0) 
    {
      Serial.print("f0,");                
    }
    else
    {
      Serial.println(data);
    }
    
}

void diagnoseComputer()
{
  int mainLoopCount = 0;

  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  t_busDeviceState busDeviceState = e_unlisten;  
  t_protocolState  protocolState = e_waitingForBusCommand;
  byte data = 0;
  byte nextDataToSend = 0;
  byte sendSequence = 0;

  pinMode(IEC_1541_DATA, INPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  pinMode(IEC_1541_ATN, INPUT);
  pinMode(IEC_1541_RESET, INPUT);

  while (1)
  {     
      do 
      {
           
      } while (checkATN() == IEC_FALSE);
      //protocolState = e_waitingForBusCommand;    
      data = readData(); // read 8 bits then ack
      //sendSequence = decodeData(data);       
      Serial.println(data,HEX);
  }
}

void diagnoseDrive()
{

}

void listenToBus()
{
  // comms start
  pinMode(IEC_1541_DATA, INPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  pinMode(IEC_1541_ATN, INPUT);
  pinMode(IEC_1541_RESET, INPUT);

  unsigned long beforeTiming = 0;
  unsigned long afterTiming = 0;
  
  bool readVal_Clock = digitalRead(IEC_1541_CLOCK);
  bool readVal_Data = digitalRead(IEC_1541_DATA);
  bool readVal_ATN = digitalRead(IEC_1541_ATN);
  bool readVal_Reset = digitalRead(IEC_1541_RESET);

  bool prevClock = !readVal_Clock;
  bool prevData = !readVal_Data;
  bool prevATN = !readVal_ATN;
  bool prevReset = !readVal_Reset;
  uint32_t updateCount = 0;

  while(1)
  {    
    bool hadPrinted = false;
    
    beforeTiming = micros();       
    
    
    if (prevClock != readVal_Clock)
    {
      Serial.print(readVal_Clock);
      Serial.print(":");
      Serial.print(readVal_Data);
      Serial.print(":");
      Serial.println(readVal_ATN);  
     // hadPrinted = true;
    }
    else if (prevData != readVal_Data)
    {
      Serial.print(readVal_Clock);
      Serial.print(",");
      Serial.print(readVal_Data);
      Serial.print(",");
      Serial.println(readVal_ATN);  
     // hadPrinted = true;
    }
    else if (prevATN != readVal_ATN)
    {
      Serial.print(readVal_Clock);
      Serial.print(",");
      Serial.print(readVal_Data);
      Serial.print(",");
      Serial.println(readVal_ATN);  
    //  hadPrinted = true;
    }
//    if (prevReset != readVal_Reset)
 //   {
  //    Serial.print("\tATN=");
   //   Serial.print(readVal_Reset);  
   //   hadPrinted = true;  
    //}
    //if (hadPrinted == true)
   // {
    //  Serial.print(",");
    //  Serial.print(updateCount++);      
    //}

    prevClock = readVal_Clock;
    prevData = readVal_Data;
    prevATN = readVal_ATN;
    prevReset = readVal_Reset;

    readVal_Clock = digitalRead(IEC_1541_CLOCK);
    readVal_Data = digitalRead(IEC_1541_DATA);
    readVal_ATN = digitalRead(IEC_1541_ATN);
    readVal_Reset = digitalRead(IEC_1541_RESET);    

    //afterTiming = micros();    
    //if (hadPrinted == true)
   // {
    //  Serial.print(",");
     // Serial.println(afterTiming - beforeTiming); 
   // }

  }
}



void setup() 
{
  Serial.begin(250000);
  // only set the pins in the other functions since it depends on if listener or talker and
  // the mode changes through the duration of the execution of the protocol
}

void loop() 
{
  char commandMode;
  Serial.println();
  Serial.write("Enter mode - 1==diagnose computer, 2== diagnose drive, 3==listen to bus");
  Serial.println();  
  while (Serial.available() <= 0) delay(20);
  commandMode = Serial.read();
  Serial.println(commandMode);
  
  switch (commandMode)
  {
    case '1' : Serial.println("Disagnosing computer"); diagnoseComputer(); break;
    case '2' : Serial.println("Disagnosing drive"); diagnoseDrive(); break;
    case '3' : Serial.println("listen to bus"); listenToBus(); break;
    default : break;
  }
}