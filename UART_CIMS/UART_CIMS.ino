#include <WiFiManager.h> // wifi manager
#include <MicroOcpp.h> //OCPP library
#include <SPI.h> //SPI communication
#include <MFRC522.h> // RC522 module

//-----------------------------------------------------------
#define RST       22  //RST pin in ESP32 module 30 pin
#define SS_PIN    21  //SS  pin 
//-----------------------------------------------------------
#define OCPP_BACKEND_URL "ws://42.118.3.87:34589/steve/websocket/CentralSystemService/" //URL to server OCPP
#define OCPP_CHARGE_BOX_ID "esp32-charger-new" //device ID in server, need declare in server before, unique
//-----------------------------------------------------------
/**
 * Frame structure
 * byte |   0   |   1   |   2   |   3   |   4   |   5   |   6   |   7   |
 * ------------------------------------------------------------------
 * mean |  HH   |   HL  |  ID   |  D0   |   D1  |   D2  |   D3  | checksum|
 * 
 * HH: HEADER_HIGH
 * HL: HEADER_LOW
 * ID: ID of frame (meaning of frame)
 * 
 */
#define HEADER_HIGH 0xAB //header byte of communication frame
#define HEADER_LOW 0xCD
//-----------------------------------------------------------
//Frame ID
#define INTERNET_STATUS 0x11
#define CIMS_CHARGE_STATUS 0x12
#define HMI_STATUS 0x13
#define PLC_STATUS 0x14
#define ID_TAG 0x15
#define SLAVE_STATUS 0x16
#define CONNECTOR_STATUS 0x17
#define HMI_CONTROL_TRANSACTION 0x18
#define BEGIN_TRANSACTION 0x19
#define END_TRANSACTION 0x1A
#define TRANSACTION_CONFIRMATION 0x1B
#define TRIGGER_MESSAGE 0x1C
#define SYNC_CLOCK_TIME 0x1D
#define SYNC_CLOCK_DATE 0x1E
#define CURRENT_VALUE 0x20
#define VOLT_VALUE 0x21
//-----------------------------------------------------------
//UART 2 comunicatino to CIMS
#define RXD2 16
#define TXD2 17
//-----------------------------------------------------------
MFRC522 mfrc522(SS_PIN, RST); //class RFID reader
MFRC522::MIFARE_Key key;
WiFiManager wm;
WiFiManagerParameter custom_ocpp_server("server", "ocpp server", "", 40);
//-----------------------------------------------------------
// Declare task
TaskHandle_t OCPP_Server;
TaskHandle_t CIMS;
//-----------------------------------------------------------
// Variable save temporary state of system
uint8_t internetStatus = 0x00;
uint8_t cimsChargeStatus = 0x00;
uint8_t hmiStatus = 0x00;
uint8_t plcStatus = 0x00;
uint8_t idTagState = 0x00;
uint8_t slaveStatus[5];
uint8_t connectorStatus[4];
String idTag;
float currentValue[4];
float voltValue[4];
//-----------------------------------------------------------

void setup()
{
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.print(F("[main] Wait for WiFi: "));

  //init RFID reader
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  SPI.begin();          // Init SPI bus
  mfrc522.PCD_Init();    // Init MFRC522 (PCD is terminology
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details

  //init and config wifi manager
  wm.addParameter(&custom_ocpp_server);
  wm.setConfigPortalBlocking(false);
  wm.setSaveParamsCallback(saveParamsCallback);
  //automatically connect using saved credentials if they exist
  //If connection fails it starts an access point with the specified name
  if (wm.autoConnect("EVSE", "12345678")) {
    Serial.println("Wifi connected");
  }
  else {
    Serial.println("Configportal running");
  }

  //init and config OCPP
  mocpp_initialize(OCPP_BACKEND_URL, OCPP_CHARGE_BOX_ID, "Wallnut Charging Station New", "EVSE-iPAC-New");

  //handle json object confirm from EVSE to server 
  setOnSendConf("RemoteStopTransaction", [] (JsonObject payload) -> void {
    int connectorID = payload["connectorId"];
    if (!strcmp(payload["status"], "Accepted")) { //send to CIMS
      uint8_t data[] = {END_TRANSACTION, 0x00, 0x00, 0x00, (uint8_t) connectorID};
      sendData(data);
    }
  });

  setOnSendConf("RemoteStartTransaction", [] (JsonObject payload) -> void {
    int connectorID = payload["connectorId"];
    if (!strcmp(payload["status"], "Accepted")) { //send to CIMS
      uint8_t data[] = {BEGIN_TRANSACTION, 0x00, 0x00, 0x00, (uint8_t) connectorID};
      sendData(data);
    }
  });

  xTaskCreatePinnedToCore(
    OCPP_Server_handle, /* Task function. */
    "OCPP_Server",      /* name of task. */
    10000,              /* Stack size of task */
    NULL,               /* parameter of the task */
    1,                  /* priority of the task */
    &OCPP_Server,       /* Task handle to keep track of created task */
    0);                 /* pin task to core 0 */
  delay(500);
  xTaskCreatePinnedToCore(
    CIMS_handle, /* Task function. */
    "CIMS",      /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &CIMS,       /* Task handle to keep track of created task */
    0);          /* pin task to core 1 */
  delay(500);
}

/*
 * Handle RFID card 
 * If EVSE is not logged in -> authorization to OCPP and response to user
 * Else -> notify logged in
*/
void readPICC() {
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  byte sector         = 1;
  byte blockAddr      = 4;
  byte trailerBlock   = 7;
  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);

  status = (MFRC522::StatusCode) mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  status = (MFRC522::StatusCode) mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }

  idTag = (char *)buffer;
  if (!idTagState) {
    authorize(idTag.c_str(), [] (JsonObject payload) -> void {
      JsonObject idTagInfo = payload["idTagInfo"];
      if (strcmp("Accepted", idTagInfo["status"] | "UNDEFINED")) {
        uint8_t data[] = {uint8_t(ID_TAG), 0x01, 0x00, 0x00, 0x00};
        idTagState = 0;
        Serial.println("authorize reject");
        sendData(data);
      }
      else {
        uint8_t data[] = {uint8_t(ID_TAG), 0x01, 0x00, 0x00, 0x00};
        idTagState = 1;
        Serial.println("authorize success");
        data[4] = 1;
        sendData(data);
      }
    }, nullptr, nullptr, nullptr);
  }
  else Serial.println("you are logged in");


  // Halt PICC
  mfrc522.PICC_HaltA();
  // Stop encryption on PCD prepare to next read
  mfrc522.PCD_StopCrypto1();
}

//Handle when new params set in config portal
void saveParamsCallback () {
  Serial.println("Get Params:");
  Serial.print(custom_ocpp_server.getID());
  Serial.print(" : ");
  Serial.println(custom_ocpp_server.getValue());
}

//send data current to server
// big endian
void currentHandle(uint8_t buffer[8]) {
  int data = 0;
  data = buffer[3] | (buffer[4] << 8) | (buffer[5] << 16);

  Serial.println(data);
  currentValue[buffer[6]] = float(data);
  /*
  * void addMeterValueInput(std::function<float ()> valueInput, 
  * const char *measurand = nullptr, 
  * const char *unit = nullptr, 
  * const char *location = nullptr, 
  * const char *phase = nullptr, 
  * unsigned int connectorId = 1)
  */
  switch (buffer[6])
  {
    case 0:
      addMeterValueInput([]() {
        return currentValue[0];
      }, nullptr, "A", nullptr, nullptr, 0);
      break;
    case 1:
      addMeterValueInput([]() {
        return currentValue[1];
      }, nullptr, "A", nullptr, nullptr, 1);
      break;
    case 2:
      addMeterValueInput([]() {
        return currentValue[2];
      }, nullptr, "A", nullptr, nullptr, 2);
      break;
    case 3:
      addMeterValueInput([]() {
        return currentValue[3];
      }, nullptr, "A", nullptr, nullptr, 3);
      break;
  }

}

//send data voltage to server
//big endian
void voltHandle(uint8_t buffer[8]) {
  int data = 0;
  data = buffer[3] | (buffer[4] << 8) | (buffer[5] << 16);
  Serial.println(data);
  voltValue[buffer[6]] = float(data);
  switch (buffer[6])
  {
    case 0:
      addMeterValueInput([]() {
        return voltValue[0];
      }, nullptr, "V", nullptr, nullptr, 0);
      break;
    case 1:
      addMeterValueInput([]() {
        return voltValue[1];
      }, nullptr, "V", nullptr, nullptr, 1);
      break;
    case 2:
      addMeterValueInput([]() {
        return voltValue[2];
      }, nullptr, "V", nullptr, nullptr, 2);
      break;
    case 3:
      addMeterValueInput([]() {
        return voltValue[3];
      }, nullptr, "V", nullptr, nullptr, 3);
      break;
  }
}

//handle request start/end transaction from hmi 0x18
void hmiTranControl(uint8_t buffer[8]) {
  uint8_t data[] = {uint8_t(TRANSACTION_CONFIRMATION), 0x01, 0x00, buffer[5], 0x01};
  if (!buffer[6]) {
    Serial.printf("HMI cancel transaction connector %d", buffer[5]);
    if (getTransaction(buffer[5])) {
      endTransaction(idTag.c_str(), nullptr, buffer[5]);
      sendData(data); //ack
    }
    else {
      data[2] = 0xFF;
      data[4] = 0x00;
      sendData(data); //ack
    }
  }
  else {
    Serial.printf("HMI start transaction connector %d", buffer[5]);
    if (!getTransaction(buffer[5])) {
      auto ret = beginTransaction(idTag.c_str(), buffer[5]);

      if (ret) {
        Serial.println(F("[main] Transaction initiated. OCPP lib will send a StartTransaction when"
                         "ConnectorPlugged Input becomes true and if the Authorization succeeds"));
        data[2] = 0x00;
        data[4] = 0x01;
        sendData(data); //ack
      } else {
        Serial.println(F("[main] No transaction initiated"));
        data[2] = 0xFF;
        data[4] = 0x00;
        sendData(data); //ack
      }
    }
  }
}

//handle connector status change
void connectorStHandle(uint8_t buffer[8]) {
  if (buffer[6]) {
    setConnectorPluggedInput([]() {
      return true;
    }, buffer[5]);
  }
  else {
    setConnectorPluggedInput([]() {
      return false;
    }, buffer[5]);
  }
}

void process(uint8_t buffer[8])
{
  switch (buffer[2])
  {
    case CIMS_CHARGE_STATUS:
      cimsChargeStatus = buffer[6];
      Serial.printf("CIMS: %d\n", cimsChargeStatus);
      break;
    case HMI_STATUS:
      hmiStatus = buffer[6];
      Serial.printf("HMI: %d\n", hmiStatus);
      break;
    case PLC_STATUS:
      plcStatus = buffer[6];
      Serial.printf("PLC: %d\n", plcStatus);
      break;
    case SLAVE_STATUS:
      slaveStatus[buffer[5] - 2] = buffer[6];
      Serial.printf("Slave %d : %d\n", buffer[5] - 2, buffer[6]);
      break;
    case CONNECTOR_STATUS:
      connectorStatus[buffer[5]] = buffer[6];
      Serial.printf("Connector %d : %d\n", buffer[5], buffer[6]);
      connectorStHandle(buffer);
      break;
    case HMI_CONTROL_TRANSACTION:
      // code relate OCPP library
      hmiTranControl(buffer);
      break;
    case CURRENT_VALUE:
      // code relate OCPP library
      currentHandle(buffer);
      break;
    case VOLT_VALUE:
      // code relate OCPP library
      voltHandle(buffer);
      break;
    default:
      break;
  }
}

//package data and send to CIMS
void sendData(uint8_t data[5])
{
  uint8_t buffer[8] = {};
  buffer[0] = HEADER_HIGH;
  buffer[1] = HEADER_LOW;

  for (uint8_t i = 0; i < 5; i++)
  {
    buffer[i + 2] = data[i];
  }
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < 7; i++)
  {
    checksum += buffer[i];
  }
  buffer[7] = checksum;
  Serial2.write(buffer, 8);
}


void OCPP_Server_handle(void *pvParameters)
{
  for (;;)
  {
    wm.process();
    readPICC();
    mocpp_loop();
    vTaskDelay(1);
  }
}


void CIMS_handle(void *pvParameters)
{
  uint8_t uartData = 0;
  uint8_t buffer[8] = {};
  uint8_t count = 0;
  for (;;)
  {
    while (Serial2.available())
    {
      uartData = Serial2.read();
      // check header
      if (count == 0)
      {
        if (uartData == HEADER_HIGH)
        {
          buffer[count] = uartData;
          count++;
          continue;
        }
        else
        {
          count = 0;
        }
      }
      // check header
      if (count == 1)
      {
        if (uartData == HEADER_LOW)
        {
          buffer[count] = uartData;
          count++;
          continue;
        }
        else
        {
          count = 0;
        }
      }
      // save data
      if (count > 1 && count < 8)
      {
        buffer[count] = uartData;
        count++;
      }
      // check checksum & process
      if (count == 8)
      {
        count = 0;
        uint8_t checksum = 0;
        for (uint8_t i = 0; i < 7; i++)
        {
          checksum += buffer[i];
        }
        if (buffer[7] != checksum)
        {
          Serial2.println("error data"); // DEBUG
          continue;
        }
        process(buffer);
      }
    }
    vTaskDelay(1);
  }
}


void loop()
{
}
