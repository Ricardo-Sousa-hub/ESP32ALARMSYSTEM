#include <Keypad.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED) 
#error Bluetooth is not enabled! Please run 'make menuconfig' to and enable it 
#endif

#define BLYNK_TEMPLATE_ID "TMPL5quwn_Id9"
#define BLYNK_TEMPLATE_NAME "SecuritySystem"
#define BLYNK_AUTH_TOKEN "75K3OnbfJZNOwzEhMcWKokC7nBKMLEtf"

#define BLYNK_PRINT Serial

#define ROW_NUM     4 // four rows
#define COLUMN_NUM  4 // four columns
const String keyCode = "1234";
const String masterKeyCode = "0000";

char ssid[] = "MEO-9A8E10";
char pass[] = "e7642fef10";

char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pin_rows[ROW_NUM]      = {19, 18, 5, 17}; // GIOP19, GIOP18, GIOP5, GIOP17 connect to the row pins
byte pin_column[COLUMN_NUM] = {16, 23, 22, 2};   // GIOP16, GIOP4, GIOP0, GIOP2 connect to the column pins

Keypad keypad = Keypad( makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM );

String input_password;

bool alarmStatus;
int tentativas;

int BUZZER_PIN = 26;

int lastState = 30;
int currentState;
const int VALUE_THRESHOLD = 30;
int touchPin = 13;

int btnLastState = LOW;
int btnCurrentState;
int btnPin = 25;

bool trigWindow;
//bool trigQuarto;
bool trigSala;

BluetoothSerial SerialBT;

BLYNK_WRITE(V0)
{
  // Set incoming value from pin V0 to a variable
  int value = param.asInt();
  if(value == 1){
    alarmStatus = true;
    lock();
  }
  else{
    alarmStatus = false;
    unlock();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(btnPin, INPUT_PULLUP);
  lock();
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  SerialBT.begin("ESP32_SECURITY_SYSTEM"); 
	Serial.println("Ligação disponível");
}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();
  keyPad();
  alarme();
  btCode();
  
}

void unlock()
{
  alarmOff();
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
  tentativas = 3;
  alarmStatus = false;
}

void lock()
{
  alarmOff();
  btnLastState = LOW;
  tentativas = 3;
  alarmStatus = true;
}

void keyPad(){
  char key = keypad.getKey();
  if (key)
  {
    if(key == '*')
    {
      input_password = "";
    }
    else if(key == '#')
    {
      if((input_password == keyCode || input_password == masterKeyCode) && alarmStatus && tentativas > 0)
      {
        Serial.println("The password is correct!\nAlarm disabled...");
        unlock();
      }
      else if(input_password == masterKeyCode && alarmStatus && tentativas == 0)
      {
        Serial.println("The master password is correct!\nAlarm disabled...");
        unlock();
      }
      else if((input_password == keyCode || input_password == masterKeyCode) && !alarmStatus)
      {
        Serial.println("The password is correct!\nAlarm enabled...");
        lock();
      }
      else
      {
        Serial.println("Wrong password!");
        if (tentativas > 0)
        {
          tentativas--;
        }
        if(tentativas == 0)
        {
          Serial.println("Introduza a mster key para desativar o alarme!");
          alarmOn();
        }
      }
      input_password = "";
    }
    else
    {
      input_password += key;
    }
  }
}

void alarmOn()
{
  Blynk.logEvent("intruso_detetado");
  digitalWrite(BUZZER_PIN, HIGH);
}

void alarmOff()
{
  digitalWrite(BUZZER_PIN, LOW);
}


void alarme()
{
  if(alarmStatus){
    chao();
    janela();
  } 
}

void chao()
{
  currentState = touchRead(touchPin);

  if(lastState >= VALUE_THRESHOLD  && currentState < VALUE_THRESHOLD)
  {
    Blynk.virtualWrite(V1, 1);
    Serial.println("Movement!");
    SerialBT.println("Movement");
    alarmOn();
  }
  lastState = currentState;
}

void janela()
{
  btnCurrentState = digitalRead(btnPin);

  if(btnLastState == LOW && btnCurrentState == HIGH)
  {
    Blynk.virtualWrite(V2, 1);
    Serial.println("Window open");
    SerialBT.println("Window open");
    alarmOn();
  }

  // save the last state
  btnLastState = btnCurrentState;
}

void btCode(){
  if (SerialBT.available())
	{
    String bt_input = SerialBT.readStringUntil('\n');
    if((compareString(bt_input, keyCode) || compareString(bt_input, masterKeyCode)) && alarmStatus && tentativas > 0)
    { 
      Serial.println("The password is correct!\nAlarm disabled...");
      SerialBT.println("The password is correct!\nAlarm disabled...");
      unlock();
    }
    else if(compareString(bt_input, masterKeyCode) && alarmStatus && tentativas == 0)
    {
      Serial.println("The master password is correct!\nAlarm disabled...");
      SerialBT.println("The master password is correct!\nAlarm disabled...");
      unlock();
    }
    else if((compareString(bt_input, keyCode) || compareString(bt_input, masterKeyCode)) && !alarmStatus)
    {
      Serial.println("The password is correct!\nAlarm enabled...");
      SerialBT.println("The password is correct!\nAlarm enabled...");
      lock();
    }
    else
    {
      Serial.println("Wrong password!");
      SerialBT.println("Wrong password!");
      if (tentativas > 0)
      {
        tentativas--;
      }
      if(tentativas == 0)
      {
        Serial.println("Introduza a mster key para desativar o alarme!");
        SerialBT.println("Introduza a mster key para desativar o alarme!");
        alarmOn();
      }
    }
  }
}

bool compareString(String input, String b)
{
  int index = 0;
  bool result = true;
  if(input.length()-1 != b.length())
  {
    result = false;  
  }
  else
  {
    for(int i = 0; i < input.length()-1; i++)
    {
      if(input[i] != b[index])
      {
        result = false;
      }
      index++;
    }
  }
  return result;
}