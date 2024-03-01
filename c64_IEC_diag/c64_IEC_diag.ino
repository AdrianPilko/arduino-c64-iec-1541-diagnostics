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

void waitForATN()
{
  pinMode(IEC_1541_ATN, INPUT);
  while (digitalRead(IEC_1541_ATN) == IEC_FALSE)
  {
  }
}

inline void waitForClock(bool level)
{
  #ifdef DEBUG_ENABLE
  Serial.println("waitForClock()");
  #endif  
  byte modeBefore = getPinMode(IEC_1541_CLOCK);
  pinMode(IEC_1541_CLOCK, INPUT);
  
  bool clockState = digitalRead(IEC_1541_CLOCK);
  
  // sit here waiting for the clock line to go false (IEC_FALSE 5V)
  while (clockState != level)
  {
     clockState = digitalRead(IEC_1541_CLOCK);
  }
  #ifdef DEBUG_ENABLE
  Serial.print("read Clock=");
  Serial.println(clockState);
  #endif
  pinMode(IEC_1541_CLOCK, modeBefore);
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
  while (digitalRead(IEC_1541_CLOCK)  != IEC_FALSE)
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

inline void holdDataIEC_TRUE()
{
  digitalWrite(IEC_1541_DATA, IEC_TRUE);  
}
inline void holdDataIEC_FALSE()
{
  digitalWrite(IEC_1541_DATA, IEC_FALSE);  
}

void writeData(uint8_t data)
{
  // write 8 bits to serial
  // set both clock and data to input  
                
  outputClock(IEC_FALSE);
  waitForClock(IEC_TRUE);                
  holdDataIEC_TRUE();                
  outputClock(IEC_TRUE);    

  pinMode(IEC_1541_DATA, OUTPUT);
  pinMode(IEC_1541_CLOCK, OUTPUT);

  uint8_t i = 0;
  uint8_t mask = 0x01;
  do
  {
    outputClock(IEC_FALSE); 
    digitalWrite(IEC_1541_DATA, data & mask);
    outputClock(IEC_TRUE); 
    delay(1);
    mask = mask << 1;
  }  
  while (++i < 8);
}

inline byte readData()
{ 
  /// these two control if EOI - end or identify sequence 200usec
  unsigned long beforeEOITiming = 0;
  unsigned long afterEOITiming = 0;

  pinMode(IEC_1541_CLOCK, INPUT);     
  pinMode(IEC_1541_DATA, OUTPUT);    

  waitForClockIEC_TRUE();    
  holdDataIEC_TRUE();     
  
  beforeEOITiming = micros();       
  waitForClockIEC_FALSE();    
  afterEOITiming = micros();
  // if it took >= 200usec for the talker to put clock line to false then EOI - C64 prog ref calls this End or identify
  // Jim Butterfields IEC disected calls this End of indicator - which is correct ??? (does it matter - no!)
  if (afterEOITiming - beforeEOITiming > 200) 
  {
    Serial.print("EOI ");
    Serial.println(afterEOITiming - beforeEOITiming);
    /// pull data line true for 60usec
    pinMode(IEC_1541_DATA, OUTPUT);        
    holdDataIEC_TRUE();
    beforeEOITiming = micros();
    do
    {
      afterEOITiming = micros();
    }while (afterEOITiming - beforeEOITiming <= 60);
    holdDataIEC_FALSE();
    pinMode(IEC_1541_DATA, INPUT);       
    return 0;
  }  
  //holdDataIEC_FALSE();    // we're ready to listen, the release is done by setting pin to INPUT
//  pinMode(IEC_1541_CLOCK, INPUT);   
  pinMode(IEC_1541_DATA, INPUT);   
  uint8_t data = 0x00;
  uint8_t i = 0;
  do
  {    
    waitForClockIEC_FALSE(); 
    data |= digitalRead(IEC_1541_DATA) << i;
    waitForClockIEC_TRUE();     
  }
  while (++i < 8);

  pinMode(IEC_1541_DATA, OUTPUT);
  holdDataIEC_TRUE(); // acknowledge    

#ifdef PRINT_TIMINGS  
  Serial.println("rd");
  for (byte i = 0; i < 8; i++)
    Serial.println(microsAfter[i] - microsBefore[i]);
#endif

  return data;
}

bool checkATN()
{
    return digitalRead(IEC_1541_ATN);
}

byte decodeData(t_busDeviceState *busDeviceState,  t_protocolState  *protocolState, byte data)
{
  // check if we need to decode a command sequence
  switch (*protocolState)
  {
     case e_waitingForBusCommand: 
            // commands for devices
            if (data == BUS_IDENT | 0x20)
            {
                Serial.print("0x28,");
                *busDeviceState = e_listen;                  
            }
            if (data == 0x3f) 
            {   
              Serial.print("0x3f,");
              *busDeviceState = e_unlisten; 
            }
            if (data == BUS_IDENT | 0x40)
            {
                Serial.print("0x48,");
                *busDeviceState = e_talk;
            }                
            if (data == 0x5f) 
            {
                Serial.print("0x5f,");
                *busDeviceState = e_untalk;
            }

            if (*busDeviceState == e_listen)
            {
              //these all assume channel 0 for now    (channe0 | 0x60) == 0x60
              if (data == 0x60)  
              {
                Serial.print("0x60");                          
              }
              if (data == 0xe0)
              {
                Serial.println("close channel");
              }
              if (data == 0xf0) 
              {
                Serial.println("open channel");                
              }
            }

      break;
    case e_ReadingData: Serial.print("Data = ");
                        Serial.println(data);
                        
      break;
    case e_WritingData:Serial.println("in wrong bus state under ATN"); break;  // should never happen

    default: break;
  }
}

void diagnoseComputer()
{
  int mainLoopCount = 0;

  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  t_busDeviceState busDeviceState = e_unlisten;  
  t_busMode        busMode  = e_ATNCommands;
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

    if (checkATN() == IEC_TRUE)  // if this is true it means drop everything and listen
    {
      busMode = e_ATNCommands;
      protocolState = e_waitingForBusCommand;       
      data = readData(); // read 8 bits then ack                                     
      sendSequence = decodeData(&busDeviceState, &protocolState, data);            
      data = readData(); // read 8 bits then ack                                     
      sendSequence = decodeData(&busDeviceState, &protocolState, data);            
    }
    else if (protocolState == e_ReadingData)
    {
      if (busDeviceState == e_listen)
      {
        data = readData(); // read 8 bits then ack                               
        sendSequence = decodeData(&busDeviceState, &protocolState, data);
      }
      else if (busDeviceState == e_talk)
      {
        // probably just start by sending a standard sequence like after LOAD"file",8
        writeData(dataSequences[sendSequence][nextDataToSend]);      
        if (nextDataToSend++ >= MAX_SEND_DATA)
        {
          nextDataToSend=0;
          // put mode back to listen
          /// todo
        }
      }
    }
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
  
  while(1)
  {
    Serial.print("CLOCK=");
    Serial.print(digitalRead(IEC_1541_CLOCK));
    Serial.print("\tDATA="); 
    Serial.print(digitalRead(IEC_1541_DATA));
    Serial.print("\tATN=");
    Serial.print(digitalRead(IEC_1541_ATN));    
    Serial.print("\tRESET=");
    Serial.println(digitalRead(IEC_1541_RESET));
  }
}



void setup() 
{
  Serial.begin(9600);
  // only set the pins in the other functions since it depends on if listener or talker and
  // the mode changes through the duration of the execution of the protocol
}

void loop() 
{
  char commandMode;
  Serial.println();
  Serial.write("Enter mode - 1==diagnose computer, 2== diagnose drive, 3==listen to bus");
  Serial.println();  
  //while (Serial.available() <= 0) delay(20);
  //commandMode = Serial.read();
  //Serial.println(commandMode);
  
  /////TESTING DEFAULT TO 1 diagnose computer
  commandMode = '1';

  switch (commandMode)
  {
    case '1' : Serial.println("Disagnosing computer"); diagnoseComputer(); break;
    case '2' : Serial.println("Disagnosing drive"); diagnoseDrive(); break;
    case '3' : Serial.println("listen to bus"); listenToBus(); break;
    default : break;
  }
}