#include <M5StickCPlus.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include "time.h"

#define MODE_A 0 // 数値表示
#define MODE_B 1 // 姿勢表示
#define MODE_C 2 // 波形表示
#define MODE_D 3 // WiFi
#define MODE_E 4 // BT

uint8_t disp_mode = MODE_A;

#define BTN_A_PIN  37
#define BTN_ON  LOW
#define BTN_OFF HIGH
uint8_t prev_btn_a = BTN_OFF;
uint8_t btn_a      = BTN_OFF;

#define POSE_P_X 0
#define POSE_M_X 1
#define POSE_P_Y 2
#define POSE_M_Y 3
#define POSE_P_Z 4
#define POSE_M_Z 5
uint8_t pose = POSE_P_X;
uint8_t prev_pose = POSE_P_X;

#define SAMPLE_PERIOD 20    // サンプリング間隔(ms)
#define SAMPLE_SIZE 240     // サンプリング間隔(20) x 画面幅(240) = 4.8s
float ax, ay, az[SAMPLE_SIZE];


#define X0 5  // 横軸の描画開始座標

// 水平静止で重力加速度1000mGが常にかかることを考慮する
#define MINZ -1000  // 縦軸の最小値 mG
#define MAXZ 3000  // 縦軸の最大値 mG

// 加速度。センサで取得できる値の単位は[g]なので、通常の[m/s^2]単位で考えるなら9.8倍する
float accX_g = 0;
float accY_g = 0;
float accZ_g = 0;
float accX_mpss = 0;
float accY_mpss = 0;
float accZ_mpss = 0;

// 角速度。センサで取得できる値の単位は[dps, degree per second]
float gyroX_dps = 0;
float gyroY_dps = 0;
float gyroZ_dps = 0;

//通信関係
//WiFi
const char* ssid = "ZZZZZZ"; // ご自分のWi-FiルータのSSIDを記述します。 
const char* password = "ZZZZZZ"; // ご自分のWi-Fiルータのパスワードを記述します。
const char* ntpServer =  "aaa.bbb.jp";
const char* saddr = "XXX.XXX.XXX.XXX"; //taeget PC address
const int sport = 55998;
const int kport = 5556;
uint8_t mac3[6];
WiFiUDP Udp;

//Bluethooth
#include <BluetoothSerial.h>

boolean near_p_g(float value){
  if(8.0 < value && value < 12.0){
    return true;
  }else{
    return false;
  }
}

boolean near_m_g(float value){
  if(-12.0 < value && value < -8.0){
    return true;
  }else{
    return false;
  }
}

boolean near_zero(float value){
  if(-2.0 < value && value < 2.0){
    return true;
  }else{
    return false;
  }
}

///////////////////////////////////////////////////////////////

void setup() {
  // Initialize the M5StickC object
  M5.begin();
  pinMode(BTN_A_PIN,  INPUT_PULLUP);
  // 6軸センサ初期化
  M5.IMU.Init();
  // LCD display
  M5.Lcd.setRotation(1);  // ボタンBが上になる向き
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);
}

void loop() {
  btn_a = digitalRead(BTN_A_PIN);

  //ボタンを押された場合の処理
  if(prev_btn_a == BTN_OFF && btn_a == BTN_ON){
    M5.Lcd.fillScreen(BLACK);
    if(disp_mode == MODE_A){
      disp_mode = MODE_B;
      M5.Lcd.setTextSize(2);
    }else if(disp_mode == MODE_B){
      disp_mode = MODE_C;
      //M5.Lcd.setTextSize(2);
      //M5.Lcd.setRotation(1);  // ボタンBが上になる向き
    }else if(disp_mode == MODE_C){
      M5.Lcd.setTextSize(1);
      disp_mode = MODE_D;
      
      // シリアルコンソールの開始　Start serial console.
      Serial.begin(115200); 

      // Wi-Fi接続 We start by connecting to a WiFi network 
      Serial.println(); // シリアルポート経由でPCのシリアルモニタに出力 
      Serial.println(); 
      Serial.print("Connecting to "); 
      Serial.println(ssid); 

      WiFi.begin(ssid, password); // Wi-Fi接続開始
      // Wi-Fi接続の状況を監視（WiFi.statusがWL_CONNECTEDになるまで繰り返し
      while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print(".");
  
        // Wi-Fi接続結果をシリアルモニタへ出力 
        Serial.println(""); 
        Serial.println("WiFi connected"); 
        Serial.println("IP address: "); 
        Serial.println(WiFi.localIP()); 

        M5.Lcd.print(".");
      }
      M5.Lcd.println("CONNECTED");
      delay(1000);

      // Set ntp time to local
      configTime(9 * 3600, 0, ntpServer);
      // Get local time
      struct tm timeInfo;
      if (getLocalTime(&timeInfo)) {
        // Set RTC time
        RTC_TimeTypeDef TimeStruct;
        TimeStruct.Hours   = timeInfo.tm_hour;
        TimeStruct.Minutes = timeInfo.tm_min;
        TimeStruct.Seconds = timeInfo.tm_sec;
        M5.Rtc.SetTime(&TimeStruct);

        RTC_DateTypeDef DateStruct;
        DateStruct.WeekDay = timeInfo.tm_wday;
        DateStruct.Month = timeInfo.tm_mon + 1;
        DateStruct.Date = timeInfo.tm_mday;
        DateStruct.Year = timeInfo.tm_year + 1900;
        M5.Rtc.SetData(&DateStruct);
      }
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(0, 1, 2);
      M5.Lcd.print("Connected \nto ");
      M5.Lcd.println(WiFi.localIP());

      esp_read_mac(mac3, ESP_MAC_WIFI_STA);
      M5.Lcd.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac3[0], mac3[1], mac3[2], mac3[3], mac3[4], mac3[5]);
      Udp.begin(kport); 

      M5.Lcd.setTextSize(2);
    }else if(disp_mode == MODE_D){
      if (WiFi.status() == WL_CONNECTED) {
        //Wi-Fiからの切断
        Serial.println("Disconnect from WiFi"); 
        WiFi.disconnect();

        // Wi-Fi接続の状況を監視（WiFi.statusがWL_DISCONNECTEDになるまで繰り返し
        while (WiFi.status() !=WL_DISCONNECTED) {
          delay(500);
          Serial.print(".");
          M5.Lcd.print(".");
        }

        // Wi-Fi切断結果をシリアルモニタへ出力
        Serial.println("");
        Serial.println("WiFi disconnected");
        M5.Lcd.print("WiFi disconnected");
      }
      M5.Lcd.setTextSize(2);
      disp_mode = MODE_E;
    }else{
      disp_mode = MODE_A;
      M5.Lcd.setTextSize(2);
      M5.Lcd.setRotation(1);  // ボタンBが上になる向き
    }
  }

  prev_btn_a = btn_a;

  // 加速度取得
  M5.IMU.getAccelData(&accX_g,&accY_g,&accZ_g);
  accX_mpss = accX_g * 9.8;
  accY_mpss = accY_g * 9.8;
  accZ_mpss = accZ_g * 9.8;
  // 角速度取得
  M5.IMU.getGyroData(&gyroX_dps,&gyroY_dps,&gyroZ_dps);

  if(disp_mode == MODE_A){
    // 取得した値を表示する
    M5.Lcd.setCursor(0, 30);
    M5.Lcd.printf("Acc :\n %.2f  %.2f  %.2f   ", accX_mpss, accY_mpss, accZ_mpss);
    M5.Lcd.setCursor(0, 60);
    M5.Lcd.printf("Gyro: \n%.2f  %.2f  %.2f    ", gyroX_dps, gyroY_dps, gyroZ_dps);
  }else if(disp_mode == MODE_B){
    // 現在の姿勢を検出する
    if(near_zero(accX_mpss) && near_p_g(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_P_Y;
    }else if(near_p_g(accX_mpss) && near_zero(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_P_X;
    }else if(near_zero(accX_mpss) && near_zero(accY_mpss) && near_p_g(accZ_mpss)){
      pose = POSE_P_Z;
    }else if(near_zero(accX_mpss) && near_m_g(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_M_Y;
    }else if(near_m_g(accX_mpss) && near_zero(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_M_X;
    }else if(near_zero(accX_mpss) && near_zero(accY_mpss) && near_m_g(accZ_mpss)){
      pose = POSE_M_Z;
    }

    // 姿勢に変化があった場合にのみ描画する
    if(prev_pose != pose){
      M5.Lcd.fillScreen(BLACK);
      switch(pose){
      case POSE_P_X:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("+X");
        break;
      case POSE_M_X:
        M5.Lcd.setRotation(3);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("-X");
        break;
      case POSE_P_Y:
        M5.Lcd.setRotation(0);
        M5.Lcd.setCursor(16, 56);
        M5.Lcd.print("+Y");
        break;
      case POSE_M_Y:
        M5.Lcd.setRotation(2);
        M5.Lcd.setCursor(16,56);
        M5.Lcd.print("-Y");
        break;
      case POSE_P_Z:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("+Z");
        break;
      case POSE_M_Z:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("-Z");
        break;
      default:
        ;
      }
    }

    prev_pose = pose;
  }else if(disp_mode == MODE_C){
    static int i = 0;

    if(i > SAMPLE_SIZE)
    {
      i = 0;
      M5.Lcd.fillScreen(BLACK);
    }
    M5.IMU.getAccelData(&ax,&ay,&az[i]);  // IMUから加速度を取得
    az[i] *= 1000; // mGに変換

    int y0 = map((int)(az[i - 1]), MINZ, MAXZ, M5.Lcd.height(), 0);
    int y1 = map((int)(az[i]), MINZ, MAXZ, M5.Lcd.height(), 0);
    M5.Lcd.drawLine(i - 1 + X0, y0, i + X0, y1, YELLOW);
    delay(SAMPLE_PERIOD);

    i++;
  }else if(disp_mode == MODE_D){
    // 現在の姿勢を検出する
    if(near_zero(accX_mpss) && near_p_g(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_P_Y;
    }else if(near_p_g(accX_mpss) && near_zero(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_P_X;
    }else if(near_zero(accX_mpss) && near_zero(accY_mpss) && near_p_g(accZ_mpss)){
      pose = POSE_P_Z;
    }else if(near_zero(accX_mpss) && near_m_g(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_M_Y;
    }else if(near_m_g(accX_mpss) && near_zero(accY_mpss) && near_zero(accZ_mpss)){
      pose = POSE_M_X;
    }else if(near_zero(accX_mpss) && near_zero(accY_mpss) && near_m_g(accZ_mpss)){
      pose = POSE_M_Z;
    }

    // 姿勢に変化があった場合にのみ描画する
    if(prev_pose != pose){
      M5.Lcd.fillScreen(BLACK);
      switch(pose){
      case POSE_P_X:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("+X");
        break;
      case POSE_M_X:
        M5.Lcd.setRotation(3);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("-X");
        break;
      case POSE_P_Y:
        M5.Lcd.setRotation(0);
        M5.Lcd.setCursor(16, 56);
        M5.Lcd.print("+Y");
        break;
      case POSE_M_Y:
        M5.Lcd.setRotation(2);
        M5.Lcd.setCursor(16,56);
        M5.Lcd.print("-Y");
        break;
      case POSE_P_Z:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("+Z");
        break;
      case POSE_M_Z:
        M5.Lcd.setRotation(1);
        M5.Lcd.setCursor(56, 16);
        M5.Lcd.print("-Z");
        break;
      default:
        ;
      }
      //UDP
      Udp.beginPacket(saddr, sport);
      Udp.printf("%d", pose);
      Udp.endPacket();  
      delay(100);
    }
  }else if(disp_mode == MODE_E){
    ;
  }

  delay(100);
}