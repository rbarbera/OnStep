// -----------------------------------------------------------------------------------
// Bluetooth Gamepad variables

// These must match your device exactly otherwise it will not connect!
String My_BLE_Address = "ff:ff:de:09:78:a0"; //Hardware Bluetooth MAC

// This is the blue LED on the Wemos ESP32 module
#define bleLED    2      // GPIO 2 -> Blue LED

// these values are for GPIO driven LEDs. 
#define LEDOFF    LOW
#define LEDON     HIGH

#define TaskStackSize   5120

//------ VR Box Definitions -----
enum
{
  VB_TRIGGERS = 0,
  VB_JOYX,
  VB_JOYY,
  VB_BTNAB,
  VB_BTNCD,
  VB_NUMBYTES
};

// ===== VR Box Button Masks =====

#define VB_BUTTON_UP      0x00
#define VB_LOW_TRIGGER    0x01
#define VB_UPR_TRIGGER    0x02
#define VB_BUTTON_A       0x01
#define VB_BUTTON_B       0x02
#define VB_BUTTON_C       0x01
#define VB_BUTTON_D       0x02
#define FRESHFLAG         0x80

#define  JOYTIMEOUT        120      // joystick no activity timeout in mS

#define JoyStickDeadZone  0

// we will connect to server by MAC address not name
static BLEAddress *Server_BLE_Address; 
String Scaned_BLE_Address; 

// All four modes send data. However each mode uses different byte positions and
// values for each of the switches. The joystick acts like a 'D' pad when not in
// Mouse Mode (no analog value).
 
typedef void (*NotifyCallback)(BLERemoteCharacteristic*, uint8_t*, size_t, bool);

// this is the service UUID of the VR Control handheld mouse/joystick device (HID)
static BLEUUID serviceUUID("00001812-0000-1000-8000-00805f9b34fb");

// Battery Service UUID - not used for now
//static BLEUUID BatteryServiceUUID("0000180F-0000-1000-8000-00805f9b34fb");

// this characteristic UUID works for joystick & triggers (report)
static BLEUUID ReportCharUUID("00002A4D-0000-1000-8000-00805f9b34fb"); // report

static boolean movingNorth = false;
static boolean movingSouth = false;
static boolean movingWest = false;
static boolean movingEast = false;
static boolean triggerPress = false;
static boolean timerReturn = false;
static boolean pressedOnce = false;

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

static BLERemoteCharacteristic* pBatRemoteCharacteristic;

// pointer to a list of characteristics of the active service,
// sorted by characteristic UUID
std::map<std::string, BLERemoteCharacteristic*> *pmap;
std::map<std::string, BLERemoteCharacteristic*> :: iterator itr;

// pointer to a list of characteristics of the active service,
// sorted by characteristic handle
std::map<std::uint16_t, BLERemoteCharacteristic*> *pmapbh;
std::map<std::uint16_t, BLERemoteCharacteristic*> :: iterator itrbh;

// storage for pointers to characteristics we want to work with
// to do: change to linked list ?
BLERemoteCharacteristic *bleRcs[4];

// This is where we store the data from the buttons and joystick
volatile byte   VrBoxData[VB_NUMBYTES];
volatile bool   flag = false;         // indicates new data to process

// joyTimer is a JOYTIMEOUT millisecond re-triggerable timer that sets the joystick 
// back to center if no activity on the joystick or trigger buttons. 
volatile uint32_t joyTimer = millis();

// task handles  
TaskHandle_t HandleJS = NULL;   // handle of the joystick task
TaskHandle_t HandleAB = NULL;   // handle of the A/B button task
TaskHandle_t HandleCD = NULL;   // handle of the C/D button task

char bfr[80];
