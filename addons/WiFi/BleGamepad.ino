

#ifdef bleGamepad_ON

//******************************************************************************
// HID notification callback handler.
//******************************************************************************
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify)
  {
  // we are getting the two trigger buttons in the first byte, joyX & joyY in 2nd & 3rd bytes
  // A four byte report is the joystick/trigger buttons.
  // A two byte report is either the A/B buttons or the C/D buttons
  // Low nibble equal to 0x50 indicates A/B buttons.
     if (4 == length)
    {
      // copy data to VrBoxData
      for (int i = VB_TRIGGERS; i < VB_BTNAB; i++)
        VrBoxData[i] = pData[i];
  
      // wake up the joystick/trigger buttons handler task
      if (HandleJS)
        vTaskResume(HandleJS);
        
      // restart the joystick timer
      joyTimer = millis() + JOYTIMEOUT;
    }
    else if (2 == length)
    {
      // show the received data
       if (0x50 == pData[1])
      {
        // A/B button report, wake the A/B button handler task
         VrBoxData[VB_BTNAB] = pData[0];
        if (HandleAB)
          vTaskResume(HandleAB);
      }
      else
      {
        // C/D button report, wake the C/D button handler task
    //      Serial.println("wake the C/D button handler task");
        VrBoxData[VB_BTNCD] = pData[0];
        if (HandleCD)
          vTaskResume(HandleCD);
      }
   }
}
//  End of notifyCallback

//******************************************************************************
// Connection state change event callback handler.
//******************************************************************************
class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient* pclient)
  {
    digitalWrite(bleLED, LEDOFF);     // indicate connected
  }

  void onDisconnect(BLEClient* pclient)
  {
     #ifdef DEBUG_ON
     Ser.println("onDisconnect event");
     #endif
    connected = false;
    digitalWrite(bleLED, LEDON);     // indicate disconnected
  }
};

//******************************************************************************
// Connect to a service, register for notifications from Report Characteristics.
//******************************************************************************

//const uint8_t v[]={0x1,0x0};
//pRemoteCharacteristic->registerForNotify(notifyCallback);
//pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)v,2,true);

bool setupCharacteristics(BLERemoteService* pRemoteService, NotifyCallback pNotifyCallback)
{
  // get all the characteristics of the service using the handle as the key
  pmapbh = pRemoteService->getCharacteristicsByHandle();
  
  // only interested in report characteristics that have the notify capability
  for (itrbh = pmapbh->begin(); itrbh != pmapbh->end(); itrbh++)
  {
    BLEUUID x = itrbh->second->getUUID();
    
    // the uuid must match the report uuid
    if (ReportCharUUID.equals(itrbh->second->getUUID()))
    {
      // found a report characteristic
      if (itrbh->second->canNotify())
      {
        // register for notifications from this characteristic
        itrbh->second->registerForNotify(pNotifyCallback);
      }
    }
  }
}
// End of setupCharacteristics
 
//******************************************************************************
// Validate the server has the correct name and services we are looking for.
// The server must have the HID service.
//******************************************************************************
bool connectToServer()
{
  BLEClient*  pClient  = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  #ifdef DEBUG_ON
    Ser.println(" - Connected to bleserver");
  #endif
  doConnect = false;

  // BLE servers may offer several services, each with unique characteristics
  // we can identify the type of service by using the service UUID
  // Obtain a reference to the service we are after in the remote BLE server.
  // this will return a pointer to the remote service if it has a matching service UUID
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr)
  {
    pClient->disconnect();
    return false;
  }
  setupCharacteristics(pRemoteService, notifyCallback);

/*  
pRemoteService = pClient->getService(BatteryServiceUUID);
//  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find battery service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
  }
  else
  {
    Serial.println(" - Found battery service");
    setupCharacteristics(pRemoteService, BatteryNotifyCallback);
  }
*/
  connected = true;
}
//  End of connectToServer

//******************************************************************************
// Scan for BLE servers and find the first one that advertises the service we are looking for.
//******************************************************************************
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
  // Called for each advertising BLE server.
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    // we have found a server, see if it has the name we are looking for
    if (advertisedDevice.haveName())
    {
      Server_BLE_Address = new BLEAddress(advertisedDevice.getAddress()); 
      Scaned_BLE_Address = Server_BLE_Address->toString().c_str();
      if (Scaned_BLE_Address == My_BLE_Address)
       {
        // we found a server with the correct name, see if it has the service we are
        // interested in (HID)
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID))
        {
          BLEDevice::getScan()->stop();
          myDevice = new BLEAdvertisedDevice(advertisedDevice);
          doConnect = true;
          doScan = true;
        } // Found our server
      }
    }
  }
};
// End of MyAdvertisedDeviceCallbacks


// All of these tasks are designed to run forever. The tasks are resumed when
// a notification message is received with new data.
//******************************************************************************
// Joystick handler Task.
// Moving the joystick off center causes this task to be resumed about every
// 15ms. Press of either trigger button will also resume this task.
//******************************************************************************
void taskJoyStick(void *parameter)
{
  int8_t  x;
  int8_t  y;
  uint8_t triggers;

  // forever loop
  while(true)
  {
    // give up the CPU, wait for new data
    vTaskSuspend(NULL);

    // we just woke up, new data is available, convert joystick data to
    // signed 8 bit integers
    x = (int8_t)VrBoxData[VB_JOYX];
    y = (int8_t)VrBoxData[VB_JOYY];
    triggers = VrBoxData[VB_TRIGGERS];

      if (pressedOnce)
      {
         pressedOnce = false;
         continue;
      }
        
      if (triggers & VB_LOW_TRIGGER)
      {
        // the lower trigger button is pressed
        Serial.print(":R9#");
        triggerPress = true;
        pressedOnce = true;
        continue;
      }
        
      if (triggers & VB_UPR_TRIGGER)
      {
        // the upper trigger button is pressed
        Serial.print(":Mp#");
        triggerPress = true;
        pressedOnce = true;      
        continue;
      }
         
    if (y < -JoyStickDeadZone)
    {
      // move North
     if (!movingNorth) 
     {
       triggerPress = false;
       movingNorth = true;
       Serial.print(":Mn#");
     }
    }
    else if (y > JoyStickDeadZone)
    {
      // move South
     if (!movingSouth) 
     {
       triggerPress = false;
       movingSouth = true;
       Serial.print(":Ms#");
     }
    }
        
    if (x > JoyStickDeadZone)
    {
      // move East
     if (!movingEast) 
     {
       triggerPress = false;
       movingEast = true;
       Serial.print(":Me#");
     }
    }
    else if (x < -JoyStickDeadZone)
    {
      // move West
     if (!movingWest) 
     {
       triggerPress = false;
       movingWest = true;
       Serial.print(":Mw#");
     }
    }
      if (timerReturn) 
      // joystick has been centered for JOYTIMEOUT ms
      {
        if (triggerPress) continue; 
  
        else if (JoyStickDeadZone == 0)
        {
          movingNorth = false; 
          movingSouth = false; 
          movingEast = false; 
          movingWest = false;                                  
          Serial.print(":Q#");
          triggerPress = false;   
          timerReturn = false;
         }
      }
  }
} 
// End of taskJoyStick

//******************************************************************************
// A & B Buttons handler Task.
// Press of either the A or B button will resume this task. 
//******************************************************************************
void taskButtonAB(void *parameter)
{
  uint8_t buttons;
 
  while(true)
  {
    // give up the CPU, wait for new data
    vTaskSuspend(NULL);
    
    // we just woke up, new data is available
    buttons = VrBoxData[VB_BTNAB];

    if (buttons & VB_BUTTON_A)
    {
      // button A pressed or is being held down
      Serial.print(":R2#");
    }

    if (buttons & VB_BUTTON_B)
    {
      // button B pressed or is being held down
      Serial.print(":R5#");
    }
  }
} 
//  End of taskButtonAB

//******************************************************************************
// C & D Buttons handler Task. 
// Press of either the C or D button will resume this task. 
//******************************************************************************
void taskButtonCD(void *parameter)
{
  uint8_t buttons;
  
  while(true)
  {
    // give up the CPU
    vTaskSuspend(NULL);

    // we just woke up, new data is available
    buttons = VrBoxData[VB_BTNCD];
    
    if (buttons & VB_BUTTON_C)
    {
      // button C pressed
      Serial.print(":R6#");
     }

    if (buttons & VB_BUTTON_D)
    {
      // button D pressed
      Serial.print(":R8#");
    }
  } 
}
//  End of taskButtonCD

//******************************************************************************
//******************************************************************************
void bleSetup()
{
  BaseType_t xReturned;

  #ifdef DEBUG_ON
    Ser.println("Start bleSetup");
  #endif

  // free memory used by BT Classic to prevent out of memory crash
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // create tasks to handle the joystick and buttons
  xReturned = xTaskCreate(taskJoyStick,             // task to handle activity on the joystick.
                          "Joystick",               // String with name of task.
                          TaskStackSize,            // Stack size in 32 bit words.
                          NULL,                     // Parameter passed as input of the task
                          1,                        // Priority of the task.
                          &HandleJS);               // Task handle.
 
  xReturned = xTaskCreate(taskButtonAB,             // task to handle activity on the A & B buttons.
                          "ButtonsAB",              // String with name of task.
                          TaskStackSize,            // Stack size in 32 bit words.
                          NULL,                     // Parameter passed as input of the task
                          1,                        // Priority of the task.
                          &HandleAB);               // Task handle.

  xReturned = xTaskCreate(taskButtonCD,             // task to handle activity on the C & D buttons.
                          "ButtonsCD",              // String with name of task.
                          TaskStackSize,            // Stack size in 32 bit words.
                          NULL,                     // Parameter passed as input of the task
                          1,                        // Priority of the task.
                          &HandleCD);               // Task handle.

  BLEDevice::init("");

  // Retrieve a GATT Scanner and set the callback we want to use to be informed 
  // when we have detected a new device.  Specify that we want active scanning
  // and start the scan to run for 5 seconds.

  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(3, false);      // scan for 3 seconds

  if (!connected) (doScan = true);
  doConnect = false;
}
// End of bleSetup.

//******************************************************************************
// This is the first and after disconnect server connection
//******************************************************************************
void bleConnTest() 
{
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) 
  {
    if (connectToServer())
    {
      #ifdef DEBUG_ON
      Ser.println("We are now connected to the BLE Server.");
      #endif
    } 
    else
    {
      #ifdef DEBUG_ON
      Ser.println("BLE Server connection failure - fatal");
      #endif 
      doConnect = false;
    }
  }
 
  else if ((!connected) && (doScan))
  {
      #ifdef DEBUG_ON
      Ser.println("Start scanning after disconnect"); 
      #endif    
      BLEDevice::getScan()->start(0);
  }
}
// End of bleConnTest

void bleCenter() 
{
  // joystick no activity detector
    
  if (connected)
  {
    if (joyTimer && (joyTimer < millis()))
    {
      // no joystick notification for JOYTIMEOUT mS, center the joystick
      VrBoxData[VB_JOYX] = VrBoxData[VB_JOYY] = 0;
      // wake up the joystick task
      timerReturn = true;
      vTaskResume(HandleJS);
      joyTimer = 0;
    }
  }
}
// end of bleCenter

#endif
