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

char ssid[] = "OppoA94";
char pass[] = "53e5926286b6";

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
bool locked = false;

//Buzzer
int BUZZER_PIN = 26;

//Touch
int lastState = 30;
int currentState;
const int VALUE_THRESHOLD = 10;
int touchPin = 13;

//Button
int btnLastState = LOW;
int btnCurrentState;
int btnPin = 25;

BluetoothSerial SerialBT;

//Not using Delay
const unsigned long executiontime = 10000; // 10 segundos
unsigned long pasttime = 0;
unsigned long presenttime = 0;

//LEDS
const int RED_PIN = 33; 
const int GREEN_PIN = 32;
const int LIGHT_PIN = 21;
const int FOTORESIST_PIN = 36;

//Btn wifi
const int WIFI_PIN = 39;
bool wifi = false;

BLYNK_WRITE(V0)
{
  // Set incoming value from pin V0 to a variable
  int value = param.asInt();
  if(value == 1)
  {
    alarmStatus = true;
    lock();
  }
  else
  {
    alarmStatus = false;
    unlock();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(btnPin, INPUT_PULLUP);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(WIFI_PIN, INPUT_PULLUP);
  lock();
  if(digitalRead(WIFI_PIN) == 1){
    wifi = true;
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  }
  
  SerialBT.begin("ESP32_SECURITY_SYSTEM"); 
	Serial.println("Ligação disponível");
}

void loop() {
  presenttime = millis();
  // put your main code here, to run repeatedly:
  if(wifi){
    Blynk.run();
  }

  light();
  keyPad();
  alarme();
  btCode();
  verifyLock();
}

void verifyLock(){
  if(locked){
      if (presenttime - pasttime >= executiontime) 
      {
        sendMsg("Write the password");
        tentativas = 0;
        locked = !locked;
        pasttime = presenttime;
      }
  }
}

void unlock()
{
  alarmOff();
  desligarLedVermelho();
  Blynk.virtualWrite(V1, 0);
  Blynk.virtualWrite(V2, 0);
  tentativas = 3;
  alarmStatus = false;
}

void lock()
{
  ligarLedVermelho();
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
      else if(input_password == masterKeyCode && alarmStatus && tentativas <= 0 && !locked)
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
        if(tentativas <= 0)
        {
          tentativas--;
          Serial.println("Introduza a master key para desativar o alarme!");
          alarmOn();
        }
        if(tentativas <= -3)
        {
          locked = true;
          pasttime = presenttime;
          Serial.println("Aguarde 10 segundos até introduzir novamente a mesterkey!\n");
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
    sendMsg("Movement!");
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
    sendMsg("Window open");
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
      sendMsg("The password is correct!\nAlarm disabled...");
      unlock();
    }
    else if(compareString(bt_input, masterKeyCode) && alarmStatus && tentativas <= 0  && !locked)
    {
      sendMsg("The master password is correct!\nAlarm disabled...");
      unlock();
    }
    else if((compareString(bt_input, keyCode) || compareString(bt_input, masterKeyCode)) && !alarmStatus)
    {
      sendMsg("The password is correct!\nAlarm enabled...");
      lock();
    }
    else
    {
      sendMsg("Wrong password!");
      if (tentativas > 0)
      {
        tentativas--;
      }
      if(tentativas <= 0)
      {
        tentativas--;
        sendMsg("Introduza a mster key para desativar o alarme!");
        alarmOn();
      }
      if(tentativas <= -3)
      {
        locked = true;
        pasttime = presenttime;
        sendMsg("Aguarde 10 segundos até introduzir novamente a mesterkey!\n");
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

void sendMsg(String msg)
{
  Serial.println(msg);
  SerialBT.println(msg);
}

void ligarLedVermelho()
{
  analogWrite(GREEN_PIN, 0);
  analogWrite(RED_PIN, 255);
}

void desligarLedVermelho()
{
  analogWrite(GREEN_PIN, 255);
  analogWrite(RED_PIN, 0);
}

void light(){
  int luz = analogRead(FOTORESIST_PIN);
  if(luz < 40 )
  {
    Blynk.virtualWrite(V4, 1);
    digitalWrite(LIGHT_PIN, HIGH);
    Serial.println("Noite");
  }
  else
  {
    Blynk.virtualWrite(V4, 0);
    digitalWrite(LIGHT_PIN, LOW);
    Serial.println("Dia");
  }
  
}
