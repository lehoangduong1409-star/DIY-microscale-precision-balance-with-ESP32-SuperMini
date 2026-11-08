#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- CẤU HÌNH CHÂN CHO ESP32-C3 SUPERMINI ---
#define HX711_DOUT_PIN  2   
#define HX711_SCK_PIN   3   

#define OLED_SDA        8   
#define OLED_SCL        9   

#define BUZZER_PIN      4   

#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// --- HỆ SỐ CÂN VÀ NGƯỠNG NHẠY ---
#define CALIBRATION_FACTOR  420.0f 
#define MIN_WEIGHT_TRIGGER  0.1f   // Hạ xuống 0.1g để nhận diện ngay vật siêu nhẹ (< 15g)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

enum ScaleState {
  STATE_READY,       
  STATE_WAITING_3S,  
  STATE_DONE         
};

ScaleState currentState = STATE_READY;
unsigned long startTime = 0;       
double weightAccumulator = 0.0;    
int sampleCounter = 0;             
float finalWeight = 0.0;           
long zeroOffset = 8388608;         

// Hàm đọc dữ liệu thô HX711 trực tiếp
long readHX711() {
  unsigned long count = 0;
  unsigned long startTimeOut = millis();
  while (digitalRead(HX711_DOUT_PIN) == HIGH) {
    if (millis() - startTimeOut > 100) return 0; 
  }

  for (byte i = 0; i < 24; i++) {
    digitalWrite(HX711_SCK_PIN, HIGH);
    delayMicroseconds(1);
    count = count << 1;
    digitalWrite(HX711_SCK_PIN, LOW);
    delayMicroseconds(1);
    if (digitalRead(HX711_DOUT_PIN)) {
      count++;
    }
  }

  digitalWrite(HX711_SCK_PIN, HIGH);
  delayMicroseconds(1);
  digitalWrite(HX711_SCK_PIN, LOW);
  delayMicroseconds(1);

  count ^= 0x800000;
  return (long)count;
}

// Đọc trung bình 5 mẫu để vừa giữ độ nhạy vật nhẹ vừa không bị trễ
float getFilteredWeight() {
  double sumRaw = 0;
  int numReadings = 5; 
  
  for (int i = 0; i < numReadings; i++) {
    sumRaw += readHX711();
  }
  double avgRaw = sumRaw / numReadings;
  
  float weight = (float)(avgRaw - zeroOffset) / CALIBRATION_FACTOR;
  if (weight < 0.05f) weight = 0.0f; // Triệt tiêu nhiễu tuyệt đối khi bàn cân trống
  return weight;
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(HX711_DOUT_PIN, INPUT);
  pinMode(HX711_SCK_PIN, OUTPUT);
  digitalWrite(HX711_SCK_PIN, LOW);

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println(F("Dang khoi tao..."));
  display.display();
  delay(500);

  updateDisplay("San sang", 0.00);

  // Lấy điểm 0 (Tare) chính xác khi khởi động
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += readHX711();
    delay(10);
  }
  zeroOffset = sum / 20;
}

void loop() {
  float currentWeight = getFilteredWeight();

  switch (currentState) {
    case STATE_READY:
      if (currentWeight >= MIN_WEIGHT_TRIGGER) {
        startTime = millis();
        weightAccumulator = 0.0;
        sampleCounter = 0;
        currentState = STATE_WAITING_3S;
        updateDisplay("Dang can (3s)...", currentWeight);
      }
      break;

    case STATE_WAITING_3S:
      if (currentWeight < MIN_WEIGHT_TRIGGER) {
        currentState = STATE_READY;
        updateDisplay("San sang", 0.0);
      } 
      else {
        weightAccumulator += currentWeight;
        sampleCounter++;

        unsigned long elapsedTime = millis() - startTime;
        
        if (elapsedTime < 3000) {
          updateDisplay("Dang can (3s)...", currentWeight);
        } else {
          if (sampleCounter > 0) {
            finalWeight = (float)(weightAccumulator / sampleCounter);
          } else {
            finalWeight = currentWeight;
          }

          updateDisplay("KET QUA CAN:", finalWeight);

          digitalWrite(BUZZER_PIN, HIGH);
          delay(200);
          digitalWrite(BUZZER_PIN, LOW);

          currentState = STATE_DONE;
        }
      }
      break;

    case STATE_DONE:
      if (currentWeight < MIN_WEIGHT_TRIGGER) {
        currentState = STATE_READY;
        updateDisplay("San sang", 0.0);
      }
      break;
  }

  delay(20);
}

void updateDisplay(String status, float weight) {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 5);
  display.println(status);
  
  display.setTextSize(2);
  display.setCursor(10, 30);
  display.print(weight, 2); // Hiển thị 2 số thập phân để thấy rõ độ nhạy ở dải nhẹ
  display.print(" g");

  if (currentState == STATE_DONE) {
    display.setTextSize(1);
    display.setCursor(95, 50);
    display.print("[OK]");
  }

  display.display();
}