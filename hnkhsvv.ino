#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>
#include <EEPROM.h>           // Thêm thư viện EEPROM
#include <map>                // hoặc #include <unordered_map>
std::map<int, int> idToSlot;  // ID nhập (ví dụ: 2212345) → slot (0-299)
std::map<int, int> slotToId;  // slot (0-299) → ID nhập
// LCD
#define I2C_ADDR 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

// Fingerprint
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 14, 27, 26, 25 };
byte colPins[COLS] = { 33, 32, 18, 19 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// WiFi
// const char* ssid = "iPhone";
// const char* password = "12072004";
const char* ssid = "TP-LINK_E000";
const char* password = "07869501";
// const char* ssid = "Vo tuyen 217";
// const char* password = "votuyen217@";

// Google Apps Script Web App URL
const char* serverName = "https://script.google.com/macros/s/AKfycbxro-yrVkDr3sAfmcvlx9axNKbIAsBGs-CEZkfBiV394hsfKntARxcxHeipXJ2vPmH6Fw/exec";

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 13, 12);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  Serial.print("System ready");

  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor initialized");
    lcd.clear();
    lcd.print("Sensor ready");
  } else {
    lcd.clear();
    lcd.print("Sensor failed");
    while (1)
      ;
  }

  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  lcd.clear();
  lcd.print("WiFi OK");

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // Khởi tạo EEPROM
  EEPROM.begin(512);  // 512 bytes cho EEPROM
  loadMappingsFromEEPROM();
}
void loadMappingsFromEEPROM() {
  int address = 0;
  int count = EEPROM.read(address++);
  for (int i = 0; i < count; i++) {
    int slot = EEPROM.read(address++);
    int id = 0;
    for (int j = 0; j < 4; j++) {
      id = (id << 8) + EEPROM.read(address++);
    }
    slotToId[slot] = id;
    idToSlot[id] = slot;
  }
}
void saveMappingsToEEPROM() {
  int address = 0;
  EEPROM.write(address++, idToSlot.size());
  for (auto pair : idToSlot) {
    int id = pair.first;
    int slot = pair.second;
    EEPROM.write(address++, slot);
    for (int i = 3; i >= 0; i--) {
      EEPROM.write(address++, (id >> (8 * i)) & 0xFF);
    }
  }
  EEPROM.commit();
}


void loop() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == 'A') {
    handleFingerprint("check-in");
  } else if (key == 'B') {
    handleFingerprint("check-out");
  } else if (key == 'C' || key == 'D') {
    if (!verifyAdminPassword()) {
      lcd.clear();
      lcd.print("Wrong password!");
      delay(2000);
      return;
    }

    if (key == 'C') {
      addFingerprint();  // Thêm vân tay
    }

    else if (key == 'D') {
      lcd.clear();
      lcd.print("Clearing DB...");

      if (finger.emptyDatabase() == FINGERPRINT_OK) {
        // Xóa ánh xạ ID ↔ slot trong RAM
        idToSlot.clear();
        slotToId.clear();

        // Xóa toàn bộ EEPROM
        for (int i = 0; i < 512; i++) {
          EEPROM.write(i, 0);
        }
        EEPROM.commit();

        lcd.clear();
        lcd.print("DB & ID Cleared!");
      } else {
        lcd.clear();
        lcd.print("Clear Failed!");
      }
      delay(2000);
    }
    delay(200);
  }

  delay(200);
}
int getStudentIDFromSlot(int slot) {
  if (slotToId.count(slot)) return slotToId[slot];
  return -1;  // hoặc 0
}


bool verifyAdminPassword() {
  lcd.clear();
  lcd.print("Enter password:");
  String password = "";

  while (true) {
    char key = keypad.getKey();
    if (key == '#') break;
    else if (key == '*') {
      if (password.length() > 0) password.remove(password.length() - 1);
    } else if (key && password.length() < 8) {
      password += key;
    }

    lcd.setCursor(0, 1);
    for (int i = 0; i < password.length(); i++) {
      lcd.print('*');
    }
    delay(100);
  }
  return password == "1234";  // Thay bằng mật khẩu giáo viên bạn muốn
}

void handleFingerprint(String action) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place finger");

  while (finger.getImage() != FINGERPRINT_OK) {
    lcd.setCursor(0, 1);
    lcd.print("Waiting...");
    delay(1000);
  }

  if (finger.image2Tz(1) != FINGERPRINT_OK || finger.fingerFastSearch() != FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Unauthorized");
    delay(2000);
    return;
  }

  int employeeID = finger.fingerID;
  String timestamp = getTime();
  int studentID = getStudentIDFromSlot(employeeID);
  sendDataToGoogleSheets(studentID, timestamp, action);


  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(action == "check-in" ? "Checked in" : "Checked out");
  delay(2000);
}

void addFingerprint() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter ID:");

  String id = "";
  while (true) {
    char key = keypad.getKey();
    if (key == '#') break;
    else if (key == '*') {
      if (id.length() > 0) id.remove(id.length() - 1);
    } else if (key && id.length() < 10 && isDigit(key)) {
      id += key;
    }

    lcd.setCursor(0, 1);
    lcd.print(id + "          ");
  }

  int userID = id.toInt();
  if (userID == 0 || id.length() == 0) {
    lcd.clear();
    lcd.print("Invalid ID");
    delay(2000);
    return;
  }

  // Nếu ID đã tồn tại
  if (idToSlot.count(userID)) {
    lcd.clear();
    lcd.print("ID Exists!");
    delay(2000);
    return;
  }

  // Tìm slot trống
  int slot = -1;
  for (int i = 0; i < 300; i++) {
    if (slotToId.count(i) == 0) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    lcd.clear();
    lcd.print("DB full!");
    delay(2000);
    return;
  }

  // Quét vân tay
  lcd.clear();
  lcd.print("Scan finger 1");
  while (finger.getImage() != FINGERPRINT_OK) delay(1000);
  if (finger.image2Tz(1) != FINGERPRINT_OK) return;

  lcd.clear();
  lcd.print("Remove finger");
  delay(2000);

  lcd.clear();
  lcd.print("Scan finger 2");
  while (finger.getImage() != FINGERPRINT_OK) delay(1000);
  if (finger.image2Tz(2) != FINGERPRINT_OK) return;

  if (finger.createModel() == FINGERPRINT_OK && finger.storeModel(slot) == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Saved!");
    idToSlot[userID] = slot;
    slotToId[slot] = userID;
    saveMappingsToEEPROM();

  } else {
    lcd.clear();
    lcd.print("Failed!");
  }
  delay(2000);
}
String getTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}
void sendDataToGoogleSheets(int employeeID, String timestamp, String action) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String postData = "{\"employeeID\":\"" + String(employeeID) + "\",\"timestamp\":\"" + timestamp + "\",\"action\":\"" + action + "\"}";

    int httpResponseCode = http.POST(postData);
    if (httpResponseCode > 0) {
      Serial.println(http.getString());
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}