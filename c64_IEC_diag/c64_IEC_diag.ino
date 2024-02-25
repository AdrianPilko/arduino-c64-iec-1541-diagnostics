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
#define INIT_DEBUG_OUT_TIMER 10000
#define IEC_1541_DATA  2 
#define IEC_1541_CLOCK  3
#define IEC_1541_ATN  4
#define IEC_1541_RESET  5
// the other connections are just ground and SRQ which isn't used here
// from the computer female port they are numbered like this (note notch at top of the DIN 6 connector)
//      
//              | | 
//  DATA  (5)         (1)  SRQ (unused here)
//              (6) RESET      
//
//  CLK   (4)         (2)  GROUND
//              (3) ATN

void diagnoseComputer()
{
  byte loop = 0;
  // comms start
  pinMode(IEC_1541_DATA, OUTPUT);
  pinMode(IEC_1541_CLOCK, INPUT);
  pinMode(IEC_1541_ATN, INPUT);
  pinMode(IEC_1541_RESET, INPUT);

  while(loop++ < 10)
  {
    bool clockState = IEC_TRUE;  // set by digital read but initialising anyway
    bool dataState =  IEC_TRUE;
    short debugOutputTimer = INIT_DEBUG_OUT_TIMER;
    // initially the talk holds the clock to true
    //         , the listener holds data to true
    digitalWrite(IEC_1541_DATA, IEC_TRUE);
    clockState = digitalRead(IEC_1541_CLOCK);    
    Serial.print("read Clock=");
    Serial.println(clockState);
    Serial.println("wrote data IEC_TRUE");

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
    Serial.println("talker released clock line to false");  

    // the listener must respond by releasing the data line to FALSE
    digitalWrite(IEC_1541_DATA, IEC_FALSE);
    Serial.println("listener released clock line to false");  

    // if 200msec pass before the data line goes back to true (usually within 60msec)
    // then the listener must perform EOI
    Serial.println("waiting for data to go IEC_TRUE");
    pinMode(IEC_1541_DATA, INPUT);

    int after = 0;
    int before = millis();    
    while (dataState == IEC_FALSE)
    {
      dataState = digitalRead(IEC_1541_DATA);      
    }
    after = millis();
    int delayTimeMS= after - before ;
    if (delayTimeMS > 200) 
    {
      Serial.println("took longer than 200msec EOI exiting back to main loop");
      return;
    }
    else
    {
      Serial.println("Ready to read data");
      // set both clock and data to input
      pinMode(IEC_1541_DATA, INPUT);
      pinMode(IEC_1541_CLOCK, INPUT);

      loop = 0;
      // eventually need to control this properly  (ie end of tx + response from listener)
      while (loop++ < 10000)
      {
        clockState = digitalRead(IEC_1541_CLOCK);        
        // data pin only valid on clock transition low to high
        while (digitalRead(IEC_1541_CLOCK) == IEC_TRUE)
        {
          // waiting for clock to go HIGH, IEC_FALSE
        }        
        bool data = digitalRead(IEC_1541_DATA);
        Serial.print(data);      
        if (loop % 50 == 0) Serial.println();

        while (digitalRead(IEC_1541_CLOCK) == IEC_FALSE)
        {
          // waiting for clock to go low again
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
