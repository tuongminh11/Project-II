#include <WiFi.h>
#include <MicroOcpp.h>
//-----------------------------------------------------------
#define STASSID "VTV3"
#define STAPSK "01692459368"
//-----------------------------------------------------------
#define OCPP_BACKEND_URL "ws://192.168.1.116:8080/steve/websocket/CentralSystemService/"
#define OCPP_CHARGE_BOX_ID "esp32-charger"
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
  mocpp_initialize(OCPP_BACKEND_URL, OCPP_CHARGE_BOX_ID, "Wallnut Charging Station", "EVSE-iPAC");

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

void currentHandle(uint8_t buffer[8]) {
  int currentValue = 0;
  currentValue = buffer[3] | (buffer[4] << 8) | (buffer[5] << 16) | (buffer[6] << 24);
  Serial.println(currentValue);
}

void voltHandle(uint8_t buffer[8]) {
  int voltValue = 0;
  voltValue = buffer[3] | (buffer[4] << 8) | (buffer[5] << 16) | (buffer[6] << 24);
  Serial.println(voltValue);
}

void hmiTranControl(uint8_t buffer[8]) {
  if(!buffer[6]){
    Serial.printf("HMI cancel transaction connector %d", buffer[5]);
    if(getTransaction(buffer[5])){
      endTransaction(idTag.c_str());
    }
  }
  else {
    Serial.printf("HMI start transaction connector %d", buffer[5]);
    if (!getTransaction(buffer[5])) {
      auto ret = beginTransaction(idTag.c_str());
  
      if (ret) {
        Serial.println(F("[main] Transaction initiated. OCPP lib will send a StartTransaction when"
                         "ConnectorPlugged Input becomes true and if the Authorization succeeds"));
      } else {
        Serial.println(F("[main] No transaction initiated"));
      } 
    }

  }
}

void connectorStHandle(uint8_t buffer[8]) {
  if(buffer[6]){
    setConnectorPluggedInput([]() {return true;},buffer[5]);
  }
  else {
    setConnectorPluggedInput([]() {return false;},buffer[5]);
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
    case TRANSACTION_CONFIRMATION:
      // chưa biết cái này sinh ra làm gì
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
