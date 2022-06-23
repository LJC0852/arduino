#include "I2Cdev.h"
#include <Wire.h>
#include <MPU6050.h>
#include <BleMouse.h>

#define RESTRICT_PITCH

BleMouse bleMouse;
MPU6050 imu; // MPU6050設定為imu
int16_t ax, ay, az, gx, gy, gz;
int buttonL = 0; // IO0 button
int button1 = 4; 
int buttonLstate = HIGH; 
int button1state = HIGH;
double compAngleX, compAngleY; 
uint32_t timer;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(buttonL, INPUT_PULLUP);//上拉電阻
  pinMode(button1, INPUT_PULLUP);
   
  Serial.print("MPU6050 initializing");
  imu.initialize();
  while (!imu.testConnection()) { Serial.print("."); }
  Serial.println();  
  Serial.println("BLE Mouse starting !");
  bleMouse.begin();
}

void loop() {
  imu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz); // 取得MPU6050的ax, ay, az, gx, gy, gz
  double dt = (double)(micros() - timer) / 1000000; // 計算 delta time
  timer = micros();

  #ifdef RESTRICT_PITCH //定義RESTRICT_PITCH
    double roll  = atan2(ay, az) * RAD_TO_DEG; // roll 從ay,az計算得出(轉換成度)
    double pitch = atan(-ax / sqrt(ay * ay + az * az)) * RAD_TO_DEG;//pitch也是
  #else 
    double roll  = atan(ay / sqrt(ax * ax + az * az)) * RAD_TO_DEG;
    double pitch = atan2(-ax, az) * RAD_TO_DEG;
  #endif
  
    compAngleX = roll;
    compAngleY = pitch; 

  double gyroXrate = gx / 131.0; // Convert to deg/s
  double gyroYrate = gy / 131.0; // Convert to deg/s


  #ifdef RESTRICT_PITCH
  if ((roll < -90 ) || (roll > 90 )) {
    compAngleX = roll;
  }
#else
  if ((pitch < -90 ) || (pitch > 90 )) {
    compAngleY = pitch;
  } 
#endif

  compAngleX = ((0.5 * (compAngleX + gyroXrate * dt) + 0.5 * roll)+0.5); // Calculate the angle using a Complimentary filter
  compAngleY = ((0.5 * (compAngleY + gyroYrate * dt) + 0.5 * pitch) - 9.45);
  
  Serial.print("X = ");    Serial.print(compAngleY);
  Serial.print(", Y = ");  Serial.println(compAngleX);

  
  button1state = digitalRead(button1); //讀出button1的狀態
  if(button1state == HIGH) //當按鈕放開
    bleMouse.move(compAngleY,compAngleX,0); // 鼠標移動
  else
    bleMouse.move(0,0,0.3*compAngleY); //頁面滾動
 
    
  buttonLstate = digitalRead(buttonL); //讀出buttonL的狀態
  if (buttonLstate == LOW) { // press button to Ground
    bleMouse.click(MOUSE_LEFT); // 滑鼠點擊(等於滑鼠左鍵功能)
    delay(100);
  } 
  delay(100);
}
