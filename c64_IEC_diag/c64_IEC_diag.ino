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
void waitForATN()
{
  Serial.println("waitForATN()");
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

void waitForClockHigh()
{
  Serial.println("waitForClockHigh()");
  bool clockState = IEC_TRUE;  // set by digital read but initialising anyway
  pinMode(IEC_1541_CLOCK, INPUT);
  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  
  // sit here waiting for the clock line to go false (IEC_FALSE 5V)
  while (digitalRead(IEC_1541_CLOCK) == IEC_TRUE)
  {
     if (debugOutputTimer <= 0)
     {
        Serial.print("C"); 
        debugOutputTimer = INIT_DEBUG_OUT_TIMER;
     }
     debugOutputTimer--;
  }
  Serial.print("read Clock=");
  Serial.println(clockState);
  Serial.println("wrote data IEC_TRUE");

}

void assertDataHigh()
{
  Serial.println("assertDataHigh()");
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, IEC_TRUE);  
}

void assertDataLow()
{
  Serial.println("assertDataLow()");
  pinMode(IEC_1541_DATA, OUTPUT);
  digitalWrite(IEC_1541_DATA, IEC_TRUE);  
}
void waitForData()
{
  Serial.println("waitForData()");
  bool dataState =  IEC_FALSE;
  Serial.println("waiting for data to go IEC_TRUE");
  pinMode(IEC_1541_DATA, INPUT);
  
  long debugOutputTimer = INIT_DEBUG_OUT_TIMER2;
  
  while (digitalRead(IEC_1541_DATA) == IEC_TRUE)
  {   
     if (debugOutputTimer <= 0)
     {
       Serial.println("D"); 
       debugOutputTimer = INIT_DEBUG_OUT_TIMER2;
     }
     debugOutputTimer--;    
  }
  Serial.println("Data line now false");
}

void readData()
{
  Serial.println("readData()");
  // set both clock and data to input
  long debugOutputTimer = INIT_DEBUG_OUT_TIMER2;
  pinMode(IEC_1541_DATA, INPUT);
  pinMode(IEC_1541_CLOCK, INPUT);

  while (digitalRead(IEC_1541_CLOCK) == IEC_FALSE)
  {
  }       
  
  int data = digitalRead(IEC_1541_DATA);
  Serial.print(data);      
  Serial.println();

  debugOutputTimer = INIT_DEBUG_OUT_TIMER2;
  while (digitalRead(IEC_1541_CLOCK) == IEC_TRUE)
  {
  }       
  Serial.println("CLOCK went true = 0V");
}

bool checkATN()
{
    pinMode(IEC_1541_ATN, INPUT);
    return digitalRead(IEC_1541_ATN);
}

void diagnoseComputer()
{
  byte loop = 0;  

  long debugOutputTimer = INIT_DEBUG_OUT_TIMER;
  t_busDeviceState state = e_listener;
  t_busMode mode = e_select;
  
  // comms start
  pinMode(IEC_1541_DATA, OUTPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  pinMode(IEC_1541_ATN, INPUT);
  pinMode(IEC_1541_RESET, INPUT);

  while (1)
  {
    switch (mode)
    {
      // initially the talk holds the clock to true
      //         , the listener holds data to true
      //         , only start when ATN goes low
      case e_select: 
                waitForATN();
                mode = e_data;
            break;
       case e_data:
                assertDataHigh();
                waitForClockHigh();              
                assertDataLow();
                waitForData();
                Serial.println("Ready to read data");
                readData();
                break;
       default: break;
     };
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
  while (Serial.available() <= 0) delay(20);
  commandMode = Serial.read();
  Serial.println(commandMode);

  switch (commandMode)
  {
    case '1' : diagnoseComputer(); break;
    case '2' : diagnoseDrive(); break;
    case '3' : listenToBus(); break;
    default : break;
  }
}
