const int sw1 = 13;
const int sw2 = 27;
const int sw3 = 26;
const int LED = 25;

boolean state_sw1 = false;
boolean state_sw2 = false;
boolean state_sw3 = false;

void setup() {
  // put your setup code here, to run once:
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
  digitalWrite(LED, HIGH);
}

void loop() {
  // put your main code here, to run repeatedly:
  if (state_sw1) {
    digitalWrite(LED, HIGH);
    if (digitalRead(sw1) == LOW) {
      state_sw1 = false;
      Serial.print("sw1: ");
      Serial.println(state_sw1);
    }
  } else {
    digitalWrite(LED, LOW);
    if (digitalRead(sw1) == HIGH) {
      state_sw1 = true;
      Serial.print("sw1: ");
      Serial.println(state_sw1);
    }
  }
  if (state_sw2) {
    if (digitalRead(sw2) == LOW) {
      state_sw2 = false;
      Serial.print("sw2: ");
      Serial.println(state_sw2);
    }
  } else {
    if (digitalRead(sw2) == HIGH) {
      state_sw2 = true;
      Serial.print("sw2: ");
      Serial.println(state_sw2);
    }
  }
  if (state_sw3) {
    if (digitalRead(sw3) == LOW) {
      state_sw3 = false;
      Serial.print("sw3: ");
      Serial.println(state_sw3);
    }
  } else {
    if (digitalRead(sw3) == HIGH) {
      state_sw3 = true;
      Serial.print("sw3: ");
      Serial.println(state_sw3);
    }
  }
}
