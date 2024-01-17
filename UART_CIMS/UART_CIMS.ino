#include <WiFi.h>
#include <MicroOcpp.h>
//-----------------------------------------------------------
#define STASSID "wifi"
#define STAPSK "pass"
//-----------------------------------------------------------
#define OCPP_BACKEND_URL "ws://192.168.1.116:8080/steve/websocket/CentralSystemService/"
#define OCPP_CHARGE_BOX_ID "esp32-charger-new"
//-----------------------------------------------------------
#define HEADER_HIGH 0xAB
#define HEADER_LOW 0xCD
//-----------------------------------------------------------
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
#define RXD2 16
#define TXD2 17
//-----------------------------------------------------------
TaskHandle_t OCPP_Server;
TaskHandle_t CIMS;
//-----------------------------------------------------------

//-----------------------------------------------------------
uint8_t internetStatus = 0x00;
uint8_t cimsChargeStatus = 0x00;
uint8_t hmiStatus = 0x00;
uint8_t plcStatus = 0x00;
uint8_t idTagState = 0x00;
uint8_t slaveStatus[5];
uint8_t connectorStatus[4];

//-----------------------------------------------------------
String idTag = "0123456789ABCD";
float currentValue[4];
float voltValue[4];
//-----------------------------------------------------------
void setup()
{
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.print(F("[main] Wait for WiFi: "));

  WiFi.begin(STASSID, STAPSK);
  while (!WiFi.isConnected())
  {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(F("Connected!"));
  mocpp_initialize(OCPP_BACKEND_URL, OCPP_CHARGE_BOX_ID, "Wallnut Charging Station New", "EVSE-iPAC-New");

  setOnSendConf("RemoteStopTransaction", [] (JsonObject payload) -> void {
    int connectorID = payload["connectorId"];   
    if (!strcmp(payload["status"], "Accepted")) {
      uint8_t data[] = {END_TRANSACTION, 0x00, 0x00, 0x00, (uint8_t) connectorID};
      sendData(data);
    }  
  });


  setOnSendConf("RemoteStartTransaction", [] (JsonObject payload) -> void {
    int connectorID = payload["connectorId"];   
    if (!strcmp(payload["status"], "Accepted")) {
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

//send data current to server
void currentHandle(uint8_t buffer[8]) {
  int data = 0;
  data = buffer[3] | (buffer[4] << 8) | (buffer[5] << 16);

  Serial.println(data);
  currentValue[buffer[6]] = float(data);
  //void addMeterValueInput(std::function<float ()> valueInput, const char *measurand = nullptr, const char *unit = nullptr, const char *location = nullptr, const char *phase = nullptr, unsigned int connectorId = 1)
  //  measurand giá trị đc đo
  //  unit đơn vị
  //  location địa điểm
  //  phase pha
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
  uint8_t data[] = {uint8_t(TRANSACTION_CONFIRMATION), 0x01, 0x00, buffer[5], 0x01};;
  if (!buffer[6]) {
    Serial.printf("HMI cancel transaction connector %d", buffer[5]);
    if (getTransaction(buffer[5])) {
      endTransaction(idTag.c_str());
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
      auto ret = beginTransaction(idTag.c_str());

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
