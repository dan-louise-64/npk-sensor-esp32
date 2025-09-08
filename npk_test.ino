#include <SoftwareSerial.h>
#include <Wire.h>

#define RE 18  //output pin
#define DE 19  //output pin

#define DISPLAYTX 26
#define DISPLATRX 27

//const byte code[]= {0x01, 0x03, 0x00, 0x1e, 0x00, 0x03, 0x34, 0x0D};
const byte nitro[] = {0x01, 0x03, 0x00, 0x1e, 0x00, 0x01, 0xe4, 0x0c};
const byte phos[] = {0x01, 0x03, 0x00, 0x1f, 0x00, 0x01, 0xb5, 0xcc};
const byte pota[] = {0x01, 0x03, 0x00, 0x20, 0x00, 0x01, 0x85, 0xc0};

// A variable used to store NPK values
byte values[11];

// Sets up a new SoftwareSerial object 
// name(rx, tx)
SoftwareSerial npkSerial(5, 17);

HardwareSerial displaySerial(2);
 
void setup() {
  // Set the baud rate for the Serial port
  Serial.begin(9600);

  // Set the baud rate for the SerialSoftware object
  npkSerial.begin(9600);
  displaySerial.begin(9600, SERIAL_8N1, DISPLATRX, DISPLAYTX);

  // Define pin modes for RE and DE
  pinMode(RE, OUTPUT);
  pinMode(DE, OUTPUT);
  
  delay(500);
}
 
void loop() {
  // Read values
  byte val1,val2,val3;
  
  val2 = phosphorous();
  delay(2500);
  val3 = potassium();
  delay(2500);
  val1 = nitrogen();
  delay(2500);

  // Print values to the serial monitor
  Serial.print("Nitrogen: ");
  Serial.print(val1);
  Serial.println(" mg/kg");
  Serial.print("Phosphorous: ");
  Serial.print(val2);
  Serial.println(" mg/kg");
  Serial.print("Potassium: ");
  Serial.print(val3);
  Serial.println(" mg/kg");

  String sendToDisplay = "N" + String(val1) + "P" + String(val2) + "K" + String(val3) + "";
  displaySerial.println(sendToDisplay);
  Serial.println("Sent: " + sendToDisplay);
  
  delay(2000);
}
 
byte nitrogen(){
  digitalWrite(DE,HIGH);
  digitalWrite(RE,HIGH);
  delay(10);
  if(npkSerial.write(nitro,sizeof(nitro))==8){
    digitalWrite(DE,LOW);
    digitalWrite(RE,LOW);
    for(byte i=0;i<7;i++){
    //Serial.print(npkSerial.read(),HEX);
    values[i] = npkSerial.read();
    Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  return values[4];
}
 
byte phosphorous(){
  digitalWrite(DE,HIGH);
  digitalWrite(RE,HIGH);
  delay(10);
  if(npkSerial.write(phos,sizeof(phos))==8){
    digitalWrite(DE,LOW);
    digitalWrite(RE,LOW);
    for(byte i=0;i<7;i++){
    //Serial.print(npkSerial.read(),HEX);
    values[i] = npkSerial.read();
    Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  return values[4];
}
 
byte potassium(){
  digitalWrite(DE,HIGH);
  digitalWrite(RE,HIGH);
  delay(10);
  if(npkSerial.write(pota,sizeof(pota))==8){
    digitalWrite(DE,LOW);
    digitalWrite(RE,LOW);
    for(byte i=0;i<7;i++){
    //Serial.print(npkSerial.read(),HEX);
    values[i] = npkSerial.read();
    Serial.print(values[i],HEX);
    }
    Serial.println();
  }
  return values[4];
}