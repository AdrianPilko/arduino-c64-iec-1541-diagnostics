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

typedef enum _busMode {e_select, e_data} t_busMode;
typedef enum _busDeviceState {e_listener, e_talker} t_busDeviceState ;

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
  #ifdef DEBUG_ENABLE
  Serial.println("waitForATN()");
  #endif
  pinMode(IEC_1541_ATN, INPUT);
  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  
  while (digitalRead(IEC_1541_ATN) != IEC_FALSE)
  {
     if (debugOutputTimer <= 0)
     {
        Serial.println("A"); 
        debugOutputTimer = INIT_DEBUG_OUT_TIMER2;
     }
     debugOutputTimer--;
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

inline void waitForClockIEC_FALSE()
{
  bool clockState = digitalRead(IEC_1541_CLOCK);
  
  // sit here waiting for the clock line to go false (IEC_FALSE 5V)
  while (clockState != IEC_FALSE)
  {
     clockState = digitalRead(IEC_1541_CLOCK);
  }
}


void assertData(bool level, int msec)
{
  #ifdef DEBUG_ENABLE
  Serial.print("assertData() ");
  Serial.println(level);
  #endif
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, IEC_FALSE);  
  delay(msec);
  pinMode(IEC_1541_DATA, INPUT);
}


inline void holdData(bool level)
{
  #ifdef DEBUG_ENABLE
  Serial.print("assertData() ");
  Serial.println(level);
  #endif
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, level);  
}

inline void holdDataIEC_TRUE()
{
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, IEC_TRUE);  
}

inline void holdDataIEC_FALSE()
{
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, IEC_FALSE);  
}

void readData(byte numRead)
{
  #ifdef DEBUG_ENABLE
  Serial.print("readData() ");
  Serial.println(numRead);
  #endif
  
  // set both clock and data to input  
  pinMode(IEC_1541_DATA, INPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  
  uint8_t data = 0x00;
  uint8_t i = 0;
  do
  {
    waitForClockIEC_FALSE(); 
    uint8_t theBit = digitalRead(IEC_1541_DATA);
    data |= (theBit & 1) << i;
  }
  while (++i < numRead);

  holdDataIEC_TRUE(); // acknowledge    
  Serial.print("******* data = ");           
  Serial.println(data);
  Serial.println();  
}

bool checkATN()
{
    pinMode(IEC_1541_ATN, INPUT);
    return digitalRead(IEC_1541_ATN);
}

void diagnoseComputer()
{
  int mainLoopCount = 0;

  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  t_busDeviceState state = e_listener;
  t_busMode mode = e_select;
  
  // comms start
  pinMode(IEC_1541_DATA, OUTPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  pinMode(IEC_1541_ATN, INPUT);
  pinMode(IEC_1541_RESET, OUTPUT);

  digitalWrite(IEC_1541_RESET, IEC_FALSE);

  while (1)
  {
    //switch (mode)
    //{
      // initially the talk holds the clock to true
      //         , the listener holds data to true
      //         , only start when ATN goes low
      //case e_select: 
       //         waitForATN();
       //         mode = e_data;
       //     break;
       //case e_data:                
                holdDataIEC_TRUE();                
                waitForClock(IEC_TRUE);
                waitForClock(IEC_FALSE);                
                holdDataIEC_FALSE();                
                waitForClock(IEC_TRUE);                
                readData(8); // read 8 bits then ack                
                pinMode(IEC_1541_DATA, INPUT);
        //        break;
       //default: break;
     //};
     //Serial.println(mainLoopCount++);
  }
}

void diagnoseDrive()
{

}

void listenToBus()
{

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
  commandMode = '1';

  switch (commandMode)
  {
    case '1' : diagnoseComputer(); break;
    case '2' : diagnoseDrive(); break;
    case '3' : listenToBus(); break;
    default : break;
  }
}
