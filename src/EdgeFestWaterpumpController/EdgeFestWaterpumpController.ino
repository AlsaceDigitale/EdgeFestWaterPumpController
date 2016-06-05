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

/* Constants */
// Number of bytes we allow a serial command to be.
const uint8_t COMMAND_BUFFER_SIZE = 254;
// Delay in ms before we can suppose enough pressure has been created by the pump, to open valve.
const uint16_t PUMP_PRESSURIZING_DELAY = 1000;
// For how long shall we open the valve? (in ms)
const uint16_t WATER_OUTPUT_TIME = 500;

/* Serial Buffer and Flags */
// Holds incoming data.
char inputString[COMMAND_BUFFER_SIZE];
// Flag whenever the serial command is complete.
boolean stringComplete = false;  

/* PLAYER FLAGS */
boolean fWetPlayer1 = false;
boolean fWetPlayer2 = false;

/* PUMP FINITE STATE MACHINE */
typedef enum ePumpState {
  STANDBY = 0, PRESSURISING, READY, WETTING_P1, WETTING_P2
} ePumpState;
ePumpState sPumpState = STANDBY;
unsigned long ulPumpTurnedOn = 0;
unsigned long ulPumpWettingStart = 0;
boolean bPumpRequireOutputUpdate = false;

/**
 * Initialize the system
 */
void setup() {
  // 8 bit, Parity: None, Stop bits: 1
  Serial.begin(9600, SERIAL_8N1);
  //TODO map GPIO to outputs and set them to low by default
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
      Serial.print("Queuing for player ");
      if(acCommand[2] == '1')
      {
        fWetPlayer1 = true;
        Serial.println("1");
      }
      else
      {
        fWetPlayer2 = true;
        Serial.println("1");
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
    if(fWetPlayer1 || fWetPlayer2) {
      sPumpState = PRESSURISING;
    }
    break;
    
    case PRESSURISING:
    // Store starting time of the pressurization
    if(ulPumpTurnedOn == 0)
    {
       ulPumpTurnedOn = millis();
    }
    // Have we waited to fullfit pressurizing condition?
    if(millis() >= (ulPumpTurnedOn + PUMP_PRESSURIZING_DELAY))
    {
      ulPumpTurnedOn = 0;
      sPumpState = READY;
    }
    break;

    case READY:
    if(fWetPlayer1 == true)
    {
      sPumpState = WETTING_P1;
    }
    else if(fWetPlayer2 == true)
    {
      ulPumpWettingStart = millis();
      sPumpState = WETTING_P2;
    }
    else
    {
      sPumpState = STANDBY;
    }
    break;

    case WETTING_P1:
    case WETTING_P2:
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
      if(sPumpState == WETTING_P1)
      {
        fWetPlayer1 = false;
      }
      else if(sPumpState == WETTING_P2)
      {
        fWetPlayer2 = false;
      }

      // Is there another player to wet?
      if(fWetPlayer1 || fWetPlayer2)
      {
        sPumpState = PRESSURISING;
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
    //TODO map the output values to GPIO
    case STANDBY:
    // pump off
    // valve 1 off
    // valve 2 off
    break;
    case PRESSURISING:
    // pump on
    // valve 1 off
    // valve 2 off
    break;
    case READY:
    // pump on
    // valve 1 off
    // valve 2 off
    break;
    case WETTING_P1:
    // pump on
    // valve 1 on
    // valve 2 off
    break;
    case WETTING_P2:
    // pump on
    // valve 1 off
    // valve 2 on
    break;
  }
}

