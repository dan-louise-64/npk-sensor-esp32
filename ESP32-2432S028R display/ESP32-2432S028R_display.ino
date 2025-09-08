#include "FS.h"
#include <LittleFS.h>
#define FORMAT_LITTLEFS_IF_FAILED true

#include <HardwareSerial.h>
//tx and rx
#define TX2 22
#define RX2 35
HardwareSerial displaySerial(2);

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

#include <XPT2046_Touchscreen.h> // touchscreen

TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// Touchscreen pins
#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define BORDER 10
#define FONT_SIZE_S 2
#define FONT_SIZE_L 4

#include <WiFi.h>
#include <HTTPClient.h>

#include <CSV_Parser.h>

#define CONNECTION_TIMEOUT 10

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;

// touchscreen stuff
TFT_eSPI_Button keypad[12];
String keys[] = {"9", "8", "7", "AC",
                  "6", "5", "4", "CE",
                  "3", "2", "1", "0",};

//npk values
int NVar = 0;
int PVar = 0;
int KVar = 0;

const char* logFile = "/offlineLogs.csv";

// Replace with your network credentials
const char* ssid     = "REPLACEME"; // REPLACE THIS
const char* password = "REPLACEME"; // REPLACE THIS

// REPLACE with your Domain name and URL path or IP address with path
const char* serverName = "https://agritech.pts-app.com/api/insert_data.php";

String plotID = "";

String httpCode = "";
String payload = "";

bool isConnected = false;
bool isSending = false;

int displayMode = 1;

// file
void writeFile(fs::FS &fs, const char * path, String messageSTR){
  const char * message = messageSTR.c_str();

  Serial.printf("Writing file: %s\r\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file){
      Serial.println("- failed to open file for writing");
      return;
  }
  if(file.print(message)){
      Serial.println("- file written");
  } else {
      Serial.println("- write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Appending to file: %s\r\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
      Serial.println("- failed to open file for appending");
      return;
  }
  if(file.print(message)){
      Serial.println("- message appended");
  } else {
      Serial.println("- append failed");
  }
  file.close();
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return;
  }

  Serial.println("- read from file:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
}

int fileSize(fs::FS &fs, const char * path){
  File file = fs.open(path);

  if(!file){
    Serial.println("Failed to open file for reading");
    return -1;
  }
  int sizeInInt = static_cast<int>(file.size());

  file.close();

  return sizeInInt;
}

// Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
void printTouchToSerial(int touchX, int touchY, int touchZ) {
  Serial.print("X = ");
  Serial.print(touchX);
  Serial.print(" | Y = ");
  Serial.print(touchY);
  Serial.print(" | Pressure = ");
  Serial.print(touchZ);
  Serial.print(" | Display = ");
  Serial.print(displayMode);
  Serial.println();
}

void printNPKoriginal(int NVar, int PVar, int KVar) {
  Serial.print("N = ");
  Serial.print(NVar);
  Serial.print(" | P = ");
  Serial.print(PVar);
  Serial.print(" | K = ");
  Serial.print(KVar);
  Serial.println();
}

float CallibrateN(int NVar){
  float NVarCallibrated;
  float coefficients[] = {-0.0000009, 0.000216, 0.0086019, -0.0142589};

  NVarCallibrated = coefficients[0] * pow(NVar, 3) + coefficients[1] * pow(NVar, 2) + coefficients[2] * NVar + coefficients[3];

  return NVarCallibrated;
}

float CallibrateP(int PVar){
  float PVarCallibrated;
  float coefficients[] = {-0.0000837, 0.0140462, -0.1976964, 0.9252799, };

  PVarCallibrated = coefficients[0] * pow(PVar, 3) + coefficients[1] * pow(PVar, 2) + coefficients[2] * PVar + coefficients[3];

  return PVarCallibrated;
}

float CallibrateK(int KVar){
  float KVarCallibrated;
  float coefficients[] = {-0.0000956, 0.0183598, 0.4552952, -2.4587196};

  KVarCallibrated = coefficients[0] * pow(KVar, 3) + coefficients[1] * pow(KVar, 2) + coefficients[2] * KVar + coefficients[3];

  return KVarCallibrated;
}

// GUI
void valueDisplayLoop(int NVar, int PVar, int KVar) {
  // button dimentions
  int sendButtonXYWH[4] = {SCREEN_WIDTH - 2 * (BORDER) - 100, SCREEN_HEIGHT - 2 * (BORDER) - 30, 100, 30};
  int sendButtonCenterX = (sendButtonXYWH[0] * 2 + sendButtonXYWH[2]) / 2;
  int sendButtonCenterY = (sendButtonXYWH[1] * 2 + sendButtonXYWH[3]) / 2;

  int plotIDButtonXYWH[4] = {SCREEN_WIDTH - 2 * (BORDER) - 100, 2 * (BORDER), 100, 30};
  int plotIDButtonCenterX = (plotIDButtonXYWH[0] * 2 + plotIDButtonXYWH[2]) / 2;
  int plotIDButtonCenterY = (plotIDButtonXYWH[1] * 2 + plotIDButtonXYWH[3]) / 2;

  float NVarCallibrated = CallibrateN(NVar);
  float PVarCallibrated = CallibrateP(PVar);
  float KVarCallibrated = CallibrateK(KVar);
  // Clear TFT screen
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_YELLOW);
  //create rectangle
  tft.fillRect(BORDER, BORDER, SCREEN_WIDTH - 2 * (BORDER), SCREEN_HEIGHT - 2 * (BORDER), TFT_BLACK);
  int centerX = SCREEN_WIDTH / 2;
  int textY = 70;

  String plotIDText = "Plot ID: ";

  //warning stuff
  String warning = "";

  //plot id stuff
  if (plotID == ""){
    plotIDText += "None";
  } else {
    plotIDText += plotID;
  }
  tft.drawString(plotIDText, BORDER * 2, 20, FONT_SIZE_L);

  String tempText = "N = " + String(NVarCallibrated) + " mg/kg";
  tft.drawString(tempText, BORDER * 2, textY, FONT_SIZE_L);

  textY += 30;
  tempText = "P = " + String(PVarCallibrated) + " mg/kg";
  tft.drawString(tempText, BORDER * 2, textY, FONT_SIZE_L);

  textY += 30;
  tempText = "K = " + String(KVarCallibrated) + " mg/kg";
  tft.drawString(tempText, BORDER * 2, textY, FONT_SIZE_L);

  tft.drawCentreString("Touch the screen to refresh.", centerX, 160, FONT_SIZE_S);

  // draw button of changing plotID
  tft.fillRect(plotIDButtonXYWH[0], plotIDButtonXYWH[1], plotIDButtonXYWH[2], plotIDButtonXYWH[3], TFT_BLUE);
  tft.drawCentreString("Change ID", plotIDButtonCenterX, plotIDButtonCenterY - 10, FONT_SIZE_S);

  // draw button for sending data
  tft.fillRect(sendButtonXYWH[0], sendButtonXYWH[1], sendButtonXYWH[2], sendButtonXYWH[3], TFT_BLUE);
  tft.drawCentreString("Send", sendButtonCenterX, sendButtonCenterY - 10, FONT_SIZE_L);
  
  //check if offline mode
  if (isConnected) {
    tft.drawString("Online Mode", BORDER * 2, SCREEN_HEIGHT - BORDER * 3, FONT_SIZE_S);
  } else {
    tft.drawString("Offline Mode", BORDER * 2, SCREEN_HEIGHT - BORDER * 3, FONT_SIZE_S);
  }
  

  //touchscreen functionality
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    printTouchToSerial(x, y, z);

    if ((x > sendButtonXYWH[0] && x < sendButtonXYWH[0] + sendButtonXYWH[2]) && (y < (SCREEN_HEIGHT - sendButtonXYWH[1]) && y > SCREEN_HEIGHT - (sendButtonXYWH[1] + sendButtonXYWH[3]))){
      Serial.println("Send button has touched");

      if (!isSending){
        if(plotID == "" || plotID.toInt() == 0){
          warning = "No plot ID inserted.";
        } else {
          if (isConnected) {
            sendToDatabase(String(plotID), String(NVarCallibrated), String(PVarCallibrated), String(KVarCallibrated));
            if (payload == "1"){
              warning = "Upload Successful";
            } else {
              warning = "Upload Failed";
            }
          } else {
            if (fileSize(LittleFS, logFile) == -1 || fileSize(LittleFS, logFile) > 1024){ // size limit is 1 KB
              warning = "File too large/inaccessible";
            } else {
              String appendToCSV = String(plotID) + "," + String(NVarCallibrated) + "," + String(PVarCallibrated) + "," + String(KVarCallibrated) + "\n";

              readFile(LittleFS, logFile);
              appendFile(LittleFS, logFile, appendToCSV.c_str());
              readFile(LittleFS, logFile);
              warning = "Saved to internal storage";
              delay(1000);
            }
          }
        }
      }
    }
    // print problem here
    tft.drawString(warning, BORDER * 2, SCREEN_HEIGHT - BORDER * 5, FONT_SIZE_S);

    if ((x > plotIDButtonXYWH[0] && x < plotIDButtonXYWH[0] + plotIDButtonXYWH[2]) && (y < (SCREEN_HEIGHT - plotIDButtonXYWH[1]) && y > SCREEN_HEIGHT - (plotIDButtonXYWH[1] + plotIDButtonXYWH[3]))){
      Serial.println("Change ID button has touched");
      displayMode = 2;
      IDDisplayLoop();
      delay(1000);
    }

    delay(100);
  }
}

void IDDisplayLoop(){
  // interactibles
  handleKeypadTouch();

  int returnButtonXYWH[4] = {SCREEN_WIDTH - 2 * (BORDER) - 100, 2 * (BORDER), 100, 30};
  int returnButtonCenterX = (returnButtonXYWH[0] * 2 + returnButtonXYWH[2]) / 2;
  int returnButtonCenterY = (returnButtonXYWH[1] * 2 + returnButtonXYWH[3]) / 2;
  // Clear TFT screen
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_YELLOW);
  //create rectangle
  tft.fillRect(BORDER, BORDER, SCREEN_WIDTH - 2 * (BORDER), SCREEN_HEIGHT - 2 * (BORDER), TFT_BLACK);

  tft.drawString("Plot ID: " + plotID, BORDER * 2, BORDER * 2 + 3, FONT_SIZE_L);

  drawKeypad();

  //return to screen
  tft.fillRect(returnButtonXYWH[0], returnButtonXYWH[1], returnButtonXYWH[2], returnButtonXYWH[3], TFT_BLUE);
  tft.drawCentreString("Return", returnButtonCenterX, returnButtonCenterY - 10, FONT_SIZE_S);

  //touchscreen functionality
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    printTouchToSerial(x, y, z);

    if ((x > returnButtonXYWH[0] && x < returnButtonXYWH[0] + returnButtonXYWH[2]) && (y < (SCREEN_HEIGHT - returnButtonXYWH[1]) && y > SCREEN_HEIGHT - (returnButtonXYWH[1] + returnButtonXYWH[3]))){
      Serial.println("Return button has touched");
      displayMode = 1;
      valueDisplayLoop(NVar, PVar, KVar);
    }

    delay(100);
  }
}

void handleKeypadTouch(){
  TS_Point p = touchscreen.getPoint();
  x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
  y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);

  int bWidth = 70;
  int bHeight = 50;

  for (int b = 0; b < 12; b++) {
    int buttonX = 10 + bWidth * (b%4) + bWidth/3;
    int buttonY = 60 + bHeight * (b/4) + bHeight/3;

    if ((x > buttonX && x < buttonX + bWidth) && (y < (SCREEN_HEIGHT - buttonY) && y > SCREEN_HEIGHT - (buttonY + bHeight))){
      Serial.println(keys[b] + " button has touched");
      if (keys[b] == "AC"){
        plotID = "";
      } else if (keys[b] == "CE"){
        if (plotID.length() > 0) {
          plotID = plotID.substring(0, plotID.length() - 1);
        }
      } else {
        if (plotID.length() < 6) {
          plotID += keys[b];
        }
      }
    }
  }
}

void initKeypad(){
  uint16_t bWidth = 70;
  uint16_t bHeight = 50;

  // Generate buttons with different size X deltas
  for (int i = 0; i < 12; i++) {
    keypad[i].initButton(&tft,
                      30 + bWidth * (i%4) + bWidth/3,
                      80 + bHeight * (i/4) + bHeight/3,
                      bWidth,
                      bHeight,
                      TFT_BLACK, // Outline
                      TFT_BLUE, // Fill
                      TFT_YELLOW, // Text
                      "",
                      1);
  }
}

void drawKeypad(){
  for (int i = 0; i < 12; i++) {
    keypad[i].drawButton(false, keys[i]);
  }
}

void sendToDatabase(String plotID, String NVar, String PVar, String KVar){
  HTTPClient http;
  isSending = true;
  String serverURL = String(serverName) + "?plot_id=" + plotID + "&nitrogen=" + NVar + "&phosphorus=" + PVar + "&potassium=" + KVar;

  if(isConnected){
    http.begin(serverURL.c_str());

    int httpCode = http.GET();

    if (httpCode > 0){
      if (httpCode == HTTP_CODE_OK) {
        payload = http.getString();
        Serial.println("Response: ");
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP] GET request to %s failed, error: %s\n", serverURL.c_str(), http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
  isSending = false;
}

void initInternet(){
  WiFi.begin(ssid, password);
  Serial.println("\nConnecting");
  int timeout_counter = 0;

  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
    timeout_counter++;
    if(timeout_counter >= CONNECTION_TIMEOUT*10){
      Serial.println("\nCan't establish WiFi connexion");
      isConnected = false;
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    isConnected = true;
  }
}

File fileLOGS;

char feedRowParser() {
  return fileLOGS.read();
}

bool rowParserFinished() {
  return ((fileLOGS.available()>0)?false:true);
}

void sendCSVToNet(){
  if (!LittleFS.exists(logFile)) {
    Serial.println("ERROR: File \"" + String(logFile) + "\" does not exist.");
  }

  fileLOGS = LittleFS.open(logFile, FILE_READ);
  if (!fileLOGS) {
    Serial.println("ERROR: File open failed");
  }
  CSV_Parser cp(/*format*/ "ssss");
  int row_index = 0;

  char **plotIDs = (char**)cp[0];
  char **NVars = (char**)cp[1];
  char **PVars = (char**)cp[2];
  char **KVars = (char**)cp[3];

  while (cp.parseRow()) {
    char *plotID = plotIDs[0];
    char *NVar = NVars[0];
    char *PVar = PVars[0];
    char *KVar = KVars[0];

    Serial.print(String(plotID) + "," + String(NVar) + "," + String(PVar) + "," + String(KVar) + "\n");
    sendToDatabase(String(plotID), String(NVar), String(PVar), String(KVar));

    row_index++;
  }
}

void setup(void) {
  Serial.begin(9600);

  displaySerial.begin(9600, SERIAL_8N1, RX2, TX2);

  if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
    Serial.println("LittleFS Mount Failed");
    return;
  }
  else{
    Serial.println("Little FS Mounted Successfully");
  }

  // Check if the file already exists to prevent overwritting existing data
  bool fileexists = LittleFS.exists(logFile);
  Serial.print(fileexists);
  if(!fileexists) {
    Serial.println("File doesn’t exist");  
    Serial.println("Creating file...");
    // Create File and add header
    writeFile(LittleFS, logFile, "plotID,NVar,PVar,KVar\r\n");
  }
  else {
    Serial.println("File already exists");
  }

  readFile(LittleFS, logFile);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0); 

  tft.init();

  tft.setRotation(3);
  tft.invertDisplay(1);

  // initialize display
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_YELLOW);
  tft.fillRect(BORDER, BORDER, SCREEN_WIDTH - 2 * (BORDER), SCREEN_HEIGHT - 2 * (BORDER), TFT_BLACK);

  tft.drawCentreString("Welcome to the Soil NPK Analyzer with", SCREEN_WIDTH / 2, 60, FONT_SIZE_S);
  tft.drawCentreString("Decision Support System Recoding Device.", SCREEN_WIDTH / 2, 75, FONT_SIZE_S);

  tft.drawCentreString("Connecting to internet...", SCREEN_WIDTH / 2, 90, FONT_SIZE_S);

  initKeypad();
  initInternet();

  if (isConnected){
    tft.drawCentreString("Connection Successful.", SCREEN_WIDTH / 2, 105, FONT_SIZE_S);
    tft.drawCentreString("Sending offline data to server...", SCREEN_WIDTH / 2, 120, FONT_SIZE_S);
    sendCSVToNet();

    //reset saved data
    writeFile(LittleFS, logFile, "plotID,NVar,PVar,KVar\r\n");
  } else {
    tft.drawCentreString("Connection failed. Switching to offline.", SCREEN_WIDTH / 2, 105, FONT_SIZE_S);
  }

  tft.drawCentreString("Touch the screen to start recording.", SCREEN_WIDTH / 2, 160, FONT_SIZE_S);
}

void loop() {
  int hasTouched = 0;

  // if (displaySerial.available() > 0) {
  //   // Read data and display it
  //   if (displaySerial.read() == 'N') {
  //     NVar = displaySerial.parseInt();  // read N variable
  //     if (displaySerial.read() == 'P'){
  //       PVar = displaySerial.parseInt(); // read P variable
  //       if (displaySerial.read() == 'K'){
  //         KVar = displaySerial.parseInt(); // read K variable
  //       }
  //     }
  //   }
  // }

  NVar = random(0, 255);
  PVar = random(0, 255);
  KVar = random(0, 255);

  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    // Calibrate Touchscreen points with map function to the correct width and height
    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    hasTouched = 1;
  }

  if (hasTouched == 1){
    if (displayMode == 1){
      valueDisplayLoop(NVar, PVar, KVar);
      printNPKoriginal(NVar, PVar, KVar);
    } else if (displayMode == 2){
      IDDisplayLoop();
    }
  }
}
