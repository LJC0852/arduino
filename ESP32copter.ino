#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <EEPROM.h>

#include "RC.h"

#define WIFI_CHANNEL 4
#define PWMOUT  // normal esc, uncomment for serial esc
#define LED 2
#define CALSTEPS 256 // gyro and acc calibration steps

extern int16_t accZero[3];

///引用PID.ino的參數值////
extern float yawRate;
extern float rollPitchRate;
extern float P_PID;
extern float I_PID;
extern float D_PID;
extern float P_Level_PID;
extern float I_Level_PID;
extern float D_Level_PID;



volatile boolean recv;
//volatile int peernum = 0;
//esp_now_peer_info_t slave;

void recv_cb(const uint8_t *macaddr, const uint8_t *data, int len)//宣告一個函示 recv_cb
{
  recv = true;
  //Serial.print("recv_cb ");
  //Serial.println(len); 
  if (len == RCdataSize) //當len等於RCdateSize
  {
    for (int i=0;i<RCdataSize;i++) RCdata.data[i] = data[i];//每次判斷i是否小於RCdateSize，其中i等於0，i+1 然後回傳RCdate.data[i] 變成 date[i]
  }
  /*
  if (!esp_now_is_peer_exist(macaddr))
  {
    Serial.println("adding peer ");
    esp_now_add_peer(macaddr, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0);
    peernum++;
  }
  */
};

#define ACCRESO 4096
#define CYCLETIME 3
#define MINTHROTTLE 1090
#define MIDRUD 1495
#define THRCORR 19

enum ang { ROLL,PITCH,YAW };//列舉 ang{ROLL,PITCH,YAW}

static int16_t gyroADC[3]; //靜態保證16位元整數的變數  gyroADC[3]
static int16_t accADC[3]; //靜態保證16位元整數的變數  accADC[3]
static int16_t gyroData[3]; //靜態保證16位元整數的變數  gyroData[3]
static float angle[2]    = {0,0}; //靜態浮點數變數  angle[2] 值都為0
extern int calibratingA; //靜態整數變數cailbratingA

#ifdef flysky //定義假如是flysky 就定義 ROL 0 PIT 1 THR 2 RUD 3
  #define ROL 0
  #define PIT 1
  #define THR 2
  #define RUD 3
#else //orangerx //否則 定義ROL 1 PIT 2 THR 0 RUD 3
  #define ROL 1
  #define PIT 2
  #define THR 0
  #define RUD 3
#endif //結束假設

#define AU1 4
#define AU2 5
static int16_t rcCommand[] = {0,0,0};//靜態保證16位元整數的變數rcCommand[] 值都為0

#define GYRO     0
#define STABI    1
static int8_t flightmode;//靜態保證8位元整數的變數flightmode
static int8_t oldflightmode;//靜態保證8位元整數的變數oldflightmode

boolean armed = false; //布林值 armed = flase
uint8_t armct = 0; //unsigned 8位元 armct =0
int debugvalue = 0;

void setup() 
{
  Serial.begin(115200); Serial.println();//序列埠鲍率 115200 印出空白並換行

  delay(3000); // give it some time to stop shaking after battery plugin(延遲3秒)
  MPU6050_init();//引入MPU6050.ino 的MPU6050_init()函式
  MPU6050_readId(); // must be 0x68, 104dec 引入MPU6050.ino 的MPU6050_readId()函式
  
  EEPROM.begin(64); //EEPROM.begin(size)
  if (EEPROM.read(63) != 0x55) Serial.println("Need to do ACC calib");//假如EEPROM.read(size=63)不等於0x55這個位子 序列埠印出("Need to do ACC calib")
  else ACC_Read(); // eeprom is initialized 
  if (EEPROM.read(62) != 0xAA) Serial.println("Need to check and write PID");
  //假如EEPROM.read(size=62)不等於0xAA這個位子 序列埠印出("Need to check and write PID")
  else PID_Read(); // eeprom is initialized

  
  WiFi.mode(WIFI_STA); // Station mode for esp-now 
  #if defined webServer
    setupwebserver();
    delay(500); 
  #endif


  #if defined externRC
    init_RC();
  #else
    Serial.printf("This mac: %s, ", WiFi.macAddress().c_str());//序列埠印出"This mac:一個字串 獲取 WiFi shield 的 MAC 地址" 
    Serial.printf(", channel: %i\n", WIFI_CHANNEL); 
    if (esp_now_init() != 0) Serial.println("*** ESP_Now init failed");
    esp_now_register_recv_cb(recv_cb);
  #endif

  delay(500);//延遲0.5S
  pinMode(LED,OUTPUT);//設置LED腳位為OUTPUT
  digitalWrite(LED,LOW);//LED腳位狀態為低電位
  initServo();//
}

uint32_t rxt; // receive time, used for falisave
//非負值的32位元數 rxt 

void loop() 
{
  uint32_t now,mnow,diff; 
  //非負值的32位元數 now,mnow,diff
  now = millis(); // actual time
  //now = 毫秒
  if (debugvalue == 5) mnow = micros();
  //假如 debugvalue == 5 mnow = 毫秒()
  #if defined webServer
    loopwebserver();
  #endif

  if (recv)
  {
    recv = false;  
    #if !defined externRC  
      buf_to_rc();
    #endif

    if (debugvalue == 4) Serial.printf("%4d %4d %4d %4d \n", rcValue[0], rcValue[1], rcValue[2], rcValue[3]); 
    //假如debugvalue == 4 序列埠印出(四個長度為四的十進位整數 換行 陣列rcValue[0], rcValue[1], rcValue[2], rcValue[3]的值 )
    if      (rcValue[AU1] < 1300) flightmode = GYRO;
    //假如(rcValue[AU1] < 1300)， flightmode = GYRO;
    else                          flightmode = STABI;  
    //否則  flightmode = STABI;
    if (oldflightmode != flightmode)//假如oldflightmode 不等於 flightmode
    {
      zeroGyroAccI();//執行zeroGyroAccI()函式
      oldflightmode = flightmode;
    }

    if (armed) 
    {
      rcValue[THR]    -= THRCORR;//rcValue[THR]=rcValue[THR]-THRCORR
      rcCommand[ROLL]  = rcValue[ROL] - MIDRUD;
      rcCommand[PITCH] = rcValue[PIT] - MIDRUD;
      rcCommand[YAW]   = rcValue[RUD] - MIDRUD;
    }  
    else
    {  
      if (rcValue[THR] < MINTHROTTLE) armct++;
      if (armct >= 25) 
      { 
        digitalWrite(LED,HIGH); //改變LED腳位電壓為高電壓
        armed = true;//armed =布林值正
      }
    }

    if (debugvalue == 5) Serial.printf("RC input ms: %d\n",now - rxt);
    rxt = millis();
  }

  Gyro_getADC();//執行函式 Gyro_getADC()
  
  ACC_getADC();//執行函式 ACC_getADC()

  getEstimatedAttitude();//執行函式 getEstimatedAttitude()

  pid();//執行函式  pid()

  mix();//執行函式mix()

  writeServo();//執行函式 writeServo()
  
  // Failsave part
  if (now > rxt+90)
  {
    rcValue[THR] = MINTHROTTLE;
    if (debugvalue == 5) Serial.printf("RC Failsafe after %d \n",now-rxt);
    rxt = now;
  }

  // parser part
  if (Serial.available()) //判斷串列埠緩衝區有無資料
  {
    char ch = Serial.read();//字元 ch 等於序列埠讀到的值
    // Perform ACC calibration
    if (ch == 10) Serial.println();
    else if (ch == 'A')
    { 
      Serial.println("Doing ACC calib");
      calibratingA = CALSTEPS;
      while (calibratingA != 0)//當calibratingA 不等於 0
      {
        delay(CYCLETIME);
        ACC_getADC(); //執行ACC_getADC()函式
      }
      ACC_Store();//執行 ACC_Store()函式
      Serial.println("ACC calib Done");//序列埠印出ACC calib Done
    }
    else if (ch == 'R')//假使字元等於R
    {

      /*序列埠印出(Act Rate :yawRate的值   rollPitchRate的值
                  Act PID :
                  P_PID的值  I_PID的值  D_PID的值
                  P_Level_PID的值  I_Level_PID的值  D_Level_PID的值
                  )*/
      Serial.print("Act Rate :  ");
      Serial.print(yawRate); Serial.print("  ");
      Serial.print(rollPitchRate); Serial.println();
      Serial.println("Act PID :");
      Serial.print(P_PID); Serial.print("  ");
      Serial.print(I_PID); Serial.print("  ");
      Serial.print(D_PID); Serial.println();
      Serial.print(P_Level_PID); Serial.print("  ");
      Serial.print(I_Level_PID); Serial.print("  ");
      Serial.print(D_Level_PID); Serial.println();
    }
    else if (ch == 'D')/假使字元等於D
    {
      Serial.println("Loading default PID");//序列埠印出Loading default PID
      yawRate = 6.0;
      rollPitchRate = 5.0;
      P_PID = 0.15;    // P8
      I_PID = 0.00;    // I8
      D_PID = 0.08; 
      P_Level_PID = 0.35;   // P8
      I_Level_PID = 0.00;   // I8
      D_Level_PID = 0.10;
      PID_Store();
    }
    else if (ch == 'W')
    {
      char ch = Serial.read();//序列埠讀到的值
      int n = Serial.available();//序列埠緩衝區的值
      if (n == 3)
      {
        //根據序列埠讀到的值去改變值並印初期值為多少
        n = readsernum();        
        if      (ch == 'p') { P_PID       = float(n) * 0.01 + 0.004; Serial.print("pid P ");       Serial.print(P_PID); }
        else if (ch == 'i') { I_PID       = float(n) * 0.01 + 0.004; Serial.print("pid I ");       Serial.print(I_PID); }
        else if (ch == 'd') { D_PID       = float(n) * 0.01 + 0.004; Serial.print("pid D ");       Serial.print(D_PID); }
        else if (ch == 'P') { P_Level_PID = float(n) * 0.01 + 0.004; Serial.print("pid Level P "); Serial.print(P_Level_PID); }
        else if (ch == 'I') { I_Level_PID = float(n) * 0.01 + 0.004; Serial.print("pid Level I "); Serial.print(I_Level_PID); }
        else if (ch == 'D') { D_Level_PID = float(n) * 0.01 + 0.004; Serial.print("pid Level D "); Serial.print(D_Level_PID); }
        else Serial.println("unknown command");
      }
      else if (ch == 'S') { PID_Store(); Serial.print("stored in EEPROM"); }
      else 
      {
        Serial.println("Input format wrong");
        Serial.println("Wpxx, Wixx, Wdxx - write gyro PID, example: Wd13");
        Serial.println("WPxx, WIxx, WDxx - write level PID, example: WD21");
      }
    }
    else if (ch >= '0' && ch <='9') debugvalue = ch -'0';
    else
    {
      /*序列埠印出(A - acc calib
                 D - write default PID
                 R - read actual PID
                 Wpxx, Wixx, Wdxx - write gyro PID
                 WPxx, WIxx, WDxx - write level PID
                 WS - Store PID in EEPROM
                 Display data:
                 0 - off
                 1 - Gyro values
                 2 - Acc values
                 3 - Angle values
                 4 - RC values
                 5 - Cycletime*/
      Serial.println("A - acc calib");
      Serial.println("D - write default PID");
      Serial.println("R - read actual PID");
      Serial.println("Wpxx, Wixx, Wdxx - write gyro PID");
      Serial.println("WPxx, WIxx, WDxx - write level PID");
      Serial.println("WS - Store PID in EEPROM");
      Serial.println("Display data:");
      Serial.println("0 - off");
      Serial.println("1 - Gyro values");
      Serial.println("2 - Acc values");
      Serial.println("3 - Angle values");
      Serial.println("4 - RC values");
      Serial.println("5 - Cycletime");
    }
  }

  if      (debugvalue == 1) Serial.printf("%4d %4d %4d \n", gyroADC[0], gyroADC[1], gyroADC[2]);  
  else if (debugvalue == 2) Serial.printf("%5d %5d %5d \n", accADC[0], accADC[1], accADC[2]);
  else if (debugvalue == 3) Serial.printf("%3f %3f \n", angle[0], angle[1]); 
  
  delay(CYCLETIME-1);  

  if (debugvalue == 5) 
  {
    diff = micros() - mnow;
    Serial.println(diff); 
  }
}


//宣告一個函式readsernum
int readsernum()
{
  int num;
  char numStr[3];  
  numStr[0] = Serial.read();
  numStr[1] = Serial.read();
  return atol(numStr);
}
