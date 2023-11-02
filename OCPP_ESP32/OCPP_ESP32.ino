// matth-x/MicroOcpp
// Copyright Matthias Akstaller 2019 - 2023
// MIT License


#include <WiFi.h>

#include <MicroOcpp.h>

#define STASSID "504 -2.4G"
#define STAPSK "minhminh"

// #define OCPP_BACKEND_URL   "ws://echo.websocket.events"
// #define OCPP_CHARGE_BOX_ID ""

//
//  Settings which worked for my SteVe instance:
//
#define OCPP_BACKEND_URL "ws://192.168.1.116:8080/steve/websocket/CentralSystemService/"
#define OCPP_CHARGE_BOX_ID "esp32-charger"

const int sw1 = 13;
const int sw2 = 27;
const int sw3 = 26;
const int LED = 25;

boolean state_sw1 = false;
boolean state_sw2 = false;
boolean state_sw3 = false;

String idTag = "0123456789ABCD";  //e.g. idTag = RFID.readIdTag();
String idTag_not_init = "0123456789ABCE";

void setup() {

  /*
     * Initialize Serial and WiFi
     */

  Serial.begin(115200);

  pinMode(sw1, INPUT);
  pinMode(sw3, INPUT);
  pinMode(sw2, INPUT);
  pinMode(LED, OUTPUT);
  if (digitalRead(sw1) == LOW) {
    state_sw1 = false;
  } else state_sw1 = true;

  if (digitalRead(sw2) == LOW) {
    state_sw2 = false;
  } else state_sw2 = true;

  if (digitalRead(sw3) == LOW) {
    state_sw3 = false;
  } else state_sw3 = true;
  Serial.println(state_sw1);
  Serial.println(state_sw2);
  Serial.println(state_sw3);
  digitalWrite(LED, LOW);

  Serial.print(F("[main] Wait for WiFi: "));

  WiFi.begin(STASSID, STAPSK);
  while (!WiFi.isConnected()) {
    Serial.print('.');
    delay(1000);
  }


  Serial.println(F(" connected!"));

  /*
     * Initialize the OCPP library
     */
  mocpp_initialize(OCPP_BACKEND_URL, OCPP_CHARGE_BOX_ID, "Wallnut Charging Station", "EVSE-iPAC");

  /*
     * Integrate OCPP functionality. You can leave out the following part if your EVSE doesn't need it.
     */
  setEnergyMeterInput([]() {
    //take the energy register of the main electricity meter and return the value in watt-hours
    return 0.f;
  });

  setSmartChargingCurrentOutput([](float limit) {
    //set the SAE J1772 Control Pilot value here
    Serial.printf("[main] Smart Charging allows maximum charge rate: %.0f\n", limit);
  });

  setConnectorPluggedInput([]() {
    //return true if an EV is plugged to this EVSE
    return false;
  });
  //... see MicroOcpp.h for more settings
}

void loop() {

  if (state_sw1) {
    if (digitalRead(sw1) == LOW) {
      state_sw1 = false;
    }
  } else {
    if (digitalRead(sw1) == HIGH) {
      state_sw1 = true;
      Serial.println("You have been scan RFID card");
      authorize(idTag.c_str());
      authorize(idTag_not_init.c_str());
    }
  }
  if (state_sw2) {
    if (digitalRead(sw2) == LOW) {
      state_sw2 = false;
    }
  } else {
    if (digitalRead(sw2) == HIGH) {
      state_sw2 = true;
    }
  }

  /*
     * Do all OCPP stuff (process WebSocket input, send recorded meter values to Central System, etc.)
     */
  mocpp_loop();

  /*
     * Energize EV plug if OCPP transaction is up and running
     */
  if (ocppPermitsCharge()) {
    //OCPP set up and transaction running. Energize the EV plug here
  } else {
    //No transaction running at the moment. De-energize EV plug
  }

  /*
     * Use NFC reader to start and stop transactions
     */
  if (/* RFID chip detected? */ state_sw1) {
    if (state_sw2) {
      setConnectorPluggedInput([]() {return true;},1);
    } else setConnectorPluggedInput([]() {return false;},1);

    if (!getTransaction()) {
      digitalWrite(LED, LOW);
      if (digitalRead(sw3) == HIGH && !state_sw3) {
        //no transaction running or preparing. Begin a new transaction
        state_sw3 = true;
        Serial.printf("[main] Can begin Transaction with idTag %s\n", idTag.c_str());

        /*
             * Begin Transaction. The OCPP lib will prepare transaction by checking the Authorization
             * and listen to the ConnectorPlugged Input. When the Authorization succeeds and an EV
             * is plugged, the OCPP lib will send the StartTransaction
             */
        auto ret = beginTransaction(idTag.c_str());

        if (ret) {
          Serial.println(F("[main] Transaction initiated. OCPP lib will send a StartTransaction when"
                           "ConnectorPlugged Input becomes true and if the Authorization succeeds"));
          digitalWrite(LED, HIGH);
        } else {
          Serial.println(F("[main] No transaction initiated"));
        }
      } 

    }      
    if(digitalRead(sw3) == LOW && state_sw3) {
      state_sw3 = false;
      if(getTransaction()) {
        Serial.println(F("[main] End transaction by RFID card"));
        endTransaction(idTag.c_str());
      }
    }
  }

}
