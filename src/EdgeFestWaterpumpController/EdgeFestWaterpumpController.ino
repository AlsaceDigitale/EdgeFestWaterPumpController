/**
 * Edgefest Waterpump Controller
 * Created on: 08/06/2016
 * Copyright (C) 2016 Manuel Rauscher, Alsace Digitale.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed WITHOUT ANY WARRANTY; 
 * without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* PIN MAPPING */
#define PUMP_PIN_P1_A 2
#define PUMP_PIN_P1_B 3
#define PUMP_PIN_P2_A 6
#define PUMP_PIN_P2_B 7

/* CONFIGRUATION */
// Number of bytes we allow a serial command to be.
const uint8_t COMMAND_BUFFER_SIZE = 254;
// Delay in ms before we can switch to the other player, to avoid shorting batteries because of relay commutaiton delay. (In ms)
const uint16_t PUMP_LATCHING_DELAY = 500;
// For how long shall we open the valve? (in ms)
const uint16_t WATER_OUTPUT_TIME = 5000;

/* Serial Buffer and Flags */
// Holds incoming data.
char inputString[COMMAND_BUFFER_SIZE];
// Flag whenever the serial command is complete.
boolean stringComplete = false;  

/* PLAYER FLAGS */
boolean fDampPlayer1 = false;
boolean fDampPlayer2 = false;

/* PUMP FINITE STATE MACHINE */
typedef enum ePumpState {
  STANDBY = 0, LATCHING_DELAY, READY, MOISTING_P1, MOISTING_P2
} ePumpState;
ePumpState sPumpState = STANDBY;
unsigned long ulLatchingDeleayOn = 0;
unsigned long ulPumpWettingStart = 0;
boolean bPumpRequireOutputUpdate = false;

/**
 * Initialize the system
 */
void setup() {
  // 8 bit, Parity: None, Stop bits: 1
  Serial.begin(9600, SERIAL_8N1);
  // Pin In/out configuration
  pinMode(PUMP_PIN_P1_A, OUTPUT);
  pinMode(PUMP_PIN_P1_B, OUTPUT);
  pinMode(PUMP_PIN_P2_A, OUTPUT);
  pinMode(PUMP_PIN_P2_B, OUTPUT);
  Serial.println("Ready");
}

/**
 * Main programm loop
 */
void loop() {
  // is the command ready to be parsed?
  if (stringComplete) {
    parseCommand(inputString, COMMAND_BUFFER_SIZE);
    stringComplete = false;
  }
   
  // States machines heartbeat
  pumpStateMachineHandler();
  if(bPumpRequireOutputUpdate)
  {
    pumpStateMachineOutput();
    bPumpRequireOutputUpdate = false;
  }
}

/**
 * Parses commands inside in the buffer
 */
void parseCommand(char* acCommand, int iBufferSize) {
    // Command parsing: 
    if(acCommand[0] == 'p' && acCommand[1] == ':' && (acCommand[2] == '1' || acCommand[2] == '2'))
    {
      if(acCommand[2] == '1')
      {
        if(fDampPlayer1) {
          Serial.println("Player 1 is already queued.");
        }
        else 
        {
           fDampPlayer1 = true;
           Serial.println("Queuing moisting for player 1.");
        }
      }
      else
      {
        if(fDampPlayer2) {
          Serial.println("Player 2 is already queued.");
        }
        else 
        {
           fDampPlayer2 = true;
           Serial.println("Queuing moisting for player 2.");
        }
      }
    }
    else
    {
      Serial.print("Invalid command: ");
      Serial.println(acCommand);
    }
    
     // Clear command parser buffer
     for(uint8_t i = 0; i < iBufferSize; i++)
     {
       acCommand[i] = ' ';
     }
}

/*
  SerialEvent is called whenever a new data comes in the
  hardware serial RX. This routine is run between each
  time loop() runs.
  */
void serialEvent(){
  static uint8_t cnt = 0;
  while (Serial.available()) {
    
     // Read byte from serial FIFO buffer and convert to char.
     char in = (char)Serial.read();
     // Do we have an overflow of our array or do we have an EoL?
     if(cnt >= COMMAND_BUFFER_SIZE || in == '\n')
     {
       cnt = 0; // Reset index counter
       stringComplete = true; // Set flag ready to parse
       return;
    }
    inputString[cnt++] = in;
  }
}


/**
 * Handles transistions between pump states
 */
void pumpStateMachineHandler()
{
  // Store old value
  ePumpState sOldPumpState = sPumpState;
  // Pump State Machine logic handler
  switch(sPumpState)
  {
    case STANDBY:
    // Turn on the pump when we need to wet a player
    if(fDampPlayer1 || fDampPlayer2) {
      sPumpState = LATCHING_DELAY;
    }
    break;
    
    case LATCHING_DELAY:
    // Store starting time of the latching delay
    if(ulLatchingDeleayOn == 0)
    {
       ulLatchingDeleayOn = millis();
    }
    // Have we waited to fullfit pressurizing condition?
    if(millis() >= (ulLatchingDeleayOn + PUMP_LATCHING_DELAY))
    {
      ulLatchingDeleayOn = 0;
      sPumpState = READY;
    }
    break;

    case READY:
    if(fDampPlayer1 == true)
    {
      sPumpState = MOISTING_P1;
    }
    else if(fDampPlayer2 == true)
    {
      ulPumpWettingStart = millis();
      sPumpState = MOISTING_P2;
    }
    else
    {
      sPumpState = STANDBY;
    }
    break;

    case MOISTING_P1:
    case MOISTING_P2:
    if(ulPumpWettingStart == 0)
    {
       ulPumpWettingStart = millis();
    }
    // Have we waited to fullfit the least ammount of time to open the valve?
    if(millis() >= (ulPumpWettingStart + WATER_OUTPUT_TIME))
    {
      // Reset timer
      ulPumpWettingStart = 0;
      // Reset flags
      if(sPumpState == MOISTING_P1)
      {
        fDampPlayer1 = false;
      }
      else if(sPumpState == MOISTING_P2)
      {
        fDampPlayer2 = false;
      }

      // Is there another player to wet?
      if(fDampPlayer1 || fDampPlayer2)
      {
        sPumpState = LATCHING_DELAY;
      }
      else
      {
        sPumpState = STANDBY;
      }
    }
    break;
  }
  // Has there been a change in state?
  if(sOldPumpState != sPumpState)
  {
    // Yes, require updating outputs
    bPumpRequireOutputUpdate = true;
  }
}

/**
 * Handles outputs of pump state machine
 */
void pumpStateMachineOutput()
{
  switch(sPumpState)
  {
    case LATCHING_DELAY:
    Serial.println("Latching...");
    case STANDBY:
    case READY:
    digitalWrite(PUMP_PIN_P1_A, LOW);
    digitalWrite(PUMP_PIN_P1_B, LOW);
    digitalWrite(PUMP_PIN_P2_A, LOW);
    digitalWrite(PUMP_PIN_P2_B, LOW);
    break;
    
    case MOISTING_P1:
    // Player 1
    digitalWrite(PUMP_PIN_P1_A, HIGH);
    digitalWrite(PUMP_PIN_P1_B, HIGH);
    digitalWrite(PUMP_PIN_P2_A, LOW);
    digitalWrite(PUMP_PIN_P2_B, LOW);
    Serial.println("Moisting P1...");
    break;
    
    case MOISTING_P2:
    // Player 2
    digitalWrite(PUMP_PIN_P1_A, LOW);
    digitalWrite(PUMP_PIN_P1_B, LOW);
    digitalWrite(PUMP_PIN_P2_A, HIGH);
    digitalWrite(PUMP_PIN_P2_B, HIGH);
    Serial.println("Moisting P2...");
    break;
  }
}

