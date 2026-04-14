#include <Arduino.h>
#include <SoftwareSerial.h>
#include <HardwareSerial.h>
#include <STM32FreeRTOS.h>
#include <JY901.h>

#define Elec_Pin PB0 // 电量读取引脚 二分法
#define LKST_Pin PB7
#define Led_Pin PB3
#define Connect true
#define Disconnect false
#define FRAME_HEADER_1 0xFF
#define FRAME_HEADER_2 0xFE
#define FRAME_FOOTER   0xFD

bool BEHACD_LEFT = false;
bool BEHACD_RUGHT = false;
bool GUN_SHOOT = true;
bool MOTOR_LEFT = true;
bool MOTOR_RIGHT = true;
bool GUN_STOP = true;
bool MOTOR_STOP = true;
bool SEND_DATA1 = false;  // 用于控制持续发送数据的标志
bool SEND_DATA2 = false;  // 用于控制持续发送数据的标志
float initial_Z;          // 存储陀螺仪Z轴角度
bool recording = false;    // 记录状态标志


HardwareSerial BT_Serial(PA3, PA2);   // 硬串口2重定向 RX TX
HardwareSerial BR_Serial(PB11, PB10); // 硬串口3重定向
SoftwareSerial AS_Serial(PA6, PA7);   // 软串口波特率不能太高 RX TX

String comdata = ""; // AT模式的消息
String inputData = "";

uint8_t frontdata[] = {0x55, 0xA1, 0x01, 0xFF, 0xFF};
uint8_t backdata[] = {0x55, 0xB1, 0x01, 0xFF, 0xFF};
uint8_t leftdata[] = {0x55, 0xC1, 0x01, 0xFF, 0xFF};
uint8_t rightdata[] = {0x55, 0xD1, 0x01, 0xFF, 0xFF};
uint8_t stopdata[] = {0x55, 0xE1, 0x01, 0xFF, 0xFF};
uint8_t aimdata[] = {0x55, 0xF1, 0x99, 0xFF, 0xFF};
uint8_t resdata[] = {0x55, 0xF2, 0x70, 0xFF, 0xFF};
uint8_t shootdata[] = {0x55, 0xE2, 0x01, 0xFF, 0xFF};
uint8_t shitdata[] = {0x55, 0xE3, 0x01, 0xFF, 0xFF};
uint8_t smalldata1[] = {0x55, 0xC2, 0x01, 0xFF, 0xFF};
uint8_t smalldata2[] = {0x55, 0xD2, 0x01, 0xFF, 0xFF};

struct BrainPacket { // 定义脑电数据结构
   int generatedChecksum;
   byte checksum;
   // 接收数据长度和数据数组
    byte payloadLength;
    byte payloadData[32]; // 总共接收32个自己的数据
    byte signalquality; // 信号质量
    byte attention;     // 注意力值
    byte meditation;    // 放松度值
} BrainData_t = {0, 0, 0, {0}, 0, 0, 0};

struct AnglePacket { // 定义陀螺仪数据结构
    // 三轴角度
    float X_Angle; 
    float Y_Angle; 
    float Z_Angle;
    float X_ACC; 
    float Y_ACC; 
    float Z_ACC;


} AngleData_t = {0.0f, 0.0f, 0.0f,0.0f, 0.0f, 0.0f,};

float Battry_Voltage; // 电池电压

byte ReadOneByte() // 从串口读取一个字节数据
{
  int ByteRead;
  while (!BR_Serial.available());
  ByteRead = BR_Serial.read();
  return ByteRead; // 返回读到的字节
}

void AT_Mode(){ //调试模式
  
  if (Serial.available() > 0) {
    String date = Serial.readString() + "\n" + "\r";  // 加回车换行(串口监视器需要选择“回车”)
    AS_Serial.print(date);                             // 发送AT指令
    // Serial.print(date);                            // 发送AT指令
    while (Serial.read() > 0) {}
  }
  while (AS_Serial.available() > 0)  // 获取串口数据
  {
    comdata += char(AS_Serial.read());  // 保存
    delay(2);
  }
  if (comdata.length() > 0)  // 查看串口是否获得数据
  {
    Serial.print("ACK= ");
    Serial.println(comdata);  // 打印回复
    comdata = "";             // 清空
  }
  }

bool Get_LKST() { //蓝牙连接状态
  bool state = Disconnect;
  if (digitalRead(LKST_Pin) == HIGH) {
    state = Connect;
  } else state = Disconnect;
  return state;
}

void elecdata(){
  if (BT_Serial.available() > 0) {
    inputData = BT_Serial.readString();  // 读取串口数据
    inputData.trim();                     // 清除多余的空格和换行符
    Serial.println(inputData);

    if (inputData == "K") {              
      BT_Serial.print( Battry_Voltage);
      delay(10);
    }
}
}

void Debug_print() { // 调试打印
  // 处理脑电数据
  Serial.print("  SignalQuality: "); Serial.println(BrainData_t.signalquality, DEC); // 信号质量
  Serial.print("  Attention: ");     Serial.println(BrainData_t.attention, DEC); // 打印注意力值
  Serial.print("  Meditation: ");    Serial.println(BrainData_t.meditation, DEC); // 打印放松度值
    
  // 处理角度数据
  Serial.print("  Angle-X= "); Serial.println(AngleData_t.X_Angle);
  Serial.print("  Angle-Y= "); Serial.println(AngleData_t.Y_Angle);
  Serial.print("  Angle-Z= "); Serial.println(AngleData_t.Z_Angle);
}

void vReadBrainData(void *pvParameters);

void vReadOtherData(void *pvParameters);

void vInteraction(void *pvParameters);

void setup() // 初始化
{
  Serial.begin(115200);   // 调试串口1 PA10 PA9  RX TX
  BR_Serial.begin(57600); // 脑电串口  PB11 PB10 RX TX
  BT_Serial.begin(9600);  // 蓝牙串口  PA3  PA2  RX TX 地址码 8872D91003B
  AS_Serial.begin(9600);  // 天问串口 PA6  PA7  RX TX

  pinMode(Led_Pin, OUTPUT);
  pinMode(Elec_Pin, INPUT_ANALOG); // 电量采集
  pinMode(LKST_Pin, INPUT);        // 连接状态
  digitalWrite(Led_Pin, HIGH);
  

  Wire.setSCL(PB8); Wire.setSDA(PB9); // 重定向使用IIC1
  JY901.StartIIC();

  xTaskCreate(vInteraction, (const portCHAR *)"vInteraction", 1024, NULL, 2, NULL);
  xTaskCreate(vReadBrainData, (const portCHAR *)"vReadBrainData", 1024, NULL, 1, NULL);
  xTaskCreate(vReadOtherData, (const portCHAR *)"vReadOtherData", 1024, NULL, 1, NULL);

  vTaskStartScheduler(); // 启动任务调度器

  BT_Serial.println(" Init OK! ");
}

void loop() {
  // AT_Mode(); 
}


void vReadBrainData(void *pvParameters) { // 脑电数据读取任务
  (void)pvParameters;
  for (;;) {
    // 寻找数据包起始同步字节，2个
    if (ReadOneByte() == 0xAA) // 先读一个
    {
      if (ReadOneByte() == 0xAA) // 读第二个
      {
        BrainData_t.payloadLength = ReadOneByte(); // 读取第三个，数据包字节的长度
        if (BrainData_t.payloadLength == 0x20)     // 如果接收到的是大包数据才继续读取，小包数据则舍弃不读取
        {
          BrainData_t.generatedChecksum = 0;                  // 校验变量清0
          for (int i = 0; i < BrainData_t.payloadLength; i++) // 连续读取32个字节
          {
            BrainData_t.payloadData[i] = ReadOneByte();                  // 读取指定长度数据包中的数据
            BrainData_t.generatedChecksum += BrainData_t.payloadData[i]; // 计算数据累加和
          }
          BrainData_t.checksum = ReadOneByte(); // 读取校验字节
          // 校验
          BrainData_t.generatedChecksum = (~BrainData_t.generatedChecksum) & 0xff;
          // 比较校验字节
          if (BrainData_t.checksum == BrainData_t.generatedChecksum) // 数据接收正确，继续处理
          {
                   
            BrainData_t.signalquality = 0; // 信号质量变量
            BrainData_t.attention = 0;     // 注意力值变量
            BrainData_t.meditation =0;    // 放松度值变量
             
            // 赋值数据
            BrainData_t.signalquality = BrainData_t.payloadData[1]; // 信号值
            BrainData_t.attention = BrainData_t.payloadData[29];    // 注意力值
            BrainData_t.meditation = BrainData_t.payloadData[31];   // 放松度值
         
          }
        }
      }
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void vReadOtherData(void *pvParameters) { // 其他传感器数据读取任务
  (void)pvParameters;
  for (;;) {
    JY901.GetAngle();                                                   // 读取陀螺仪数据
    AngleData_t.X_Angle = (float)JY901.stcAngle.Angle[0] / 32768 * 180; // 读取X轴角度
    AngleData_t.Y_Angle = (float)JY901.stcAngle.Angle[1] / 32768 * 180;
    AngleData_t.Z_Angle = (float)JY901.stcAngle.Angle[2] / 32768 * 180;
   
    Battry_Voltage = analogRead(Elec_Pin) * (3.3 / 1024) * 2; // 计算电池电压
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void vInteraction(void *pvParameters) { // 设备数据交互任务
(void)pvParameters;
  for (;;) {
    elecdata();
    // Debug_print();
    if (AS_Serial.available() > 0) {
      char data1 = AS_Serial.read();
      switch (data1)
      {
      case 'W':
      BT_Serial.write(frontdata, sizeof(frontdata));
        break;
      case 'S':
      BT_Serial.write(backdata, sizeof(backdata));
        break;
      case 'A':
      BT_Serial.write(leftdata, sizeof(leftdata));
        break;
      case 'F':
      BT_Serial.write(rightdata, sizeof(rightdata));
        break;
      case 'B':
      BT_Serial.write(shootdata, sizeof(shootdata));
        break;
      case 'R':
      BT_Serial.write(aimdata, sizeof(aimdata));
        break;
      case 'T':
      BT_Serial.write(resdata, sizeof(resdata));
        break;
      case 'P':
      BT_Serial.write(stopdata, sizeof(stopdata));
        break;
      case 'X':
      SEND_DATA2 = false;
       SEND_DATA1 = false;
       recording = false;
       break;
      case 'Y':
      SEND_DATA2 = false;
       SEND_DATA1 = true;
       initial_Z = AngleData_t.Z_Angle;
       recording = true;
        break;
      case 'C':
       SEND_DATA1 = false;
      SEND_DATA2 = true; 
       recording = false;
        break;
      case 'Z':
       uint8_t inte = uint8_t(Battry_Voltage);
       AS_Serial.write(0x01); AS_Serial.write(0x04);
       AS_Serial.write(inte); AS_Serial.write(uint8_t((Battry_Voltage - inte) * 100));
        break;
      }
    }
if (SEND_DATA1){
   if (recording) {
    float current_z = AngleData_t.Z_Angle;
    float delta = current_z - initial_Z;
    // Serial.println(initial_Z);
    // Serial.print("current_z:");
    // Serial.println(current_z);
    // Serial.print("delta");
    // Serial.println(delta);
    // delay(100);
    if (delta > 15 && delta < 30){BEHACD_LEFT = true;}
    if (delta > -15 && delta < 15){BEHACD_LEFT = false;BEHACD_RUGHT = false;}
    if (delta > -30 && delta < -15){BEHACD_RUGHT = true;}
   }
  
    if (BEHACD_LEFT) {
    if (MOTOR_LEFT){
    BT_Serial.write(smalldata1, sizeof(smalldata1));
    MOTOR_LEFT = false;
    MOTOR_RIGHT = true;
    MOTOR_STOP = true;}
  } 
   else if (BEHACD_RUGHT) {
    if (MOTOR_RIGHT){
    BT_Serial.write(smalldata2, sizeof(smalldata2));
    MOTOR_RIGHT = false;
    MOTOR_LEFT = true;
    MOTOR_STOP = true;}
  }
  else {
    if (MOTOR_STOP){
    BT_Serial.write(stopdata, sizeof(stopdata));
    MOTOR_STOP = false;
    MOTOR_LEFT = true;
    MOTOR_RIGHT = true;}
    }
  
    
    }
if (SEND_DATA2){if (BrainData_t.attention > 55 && GUN_STOP)
  {
    BT_Serial.write(shootdata, sizeof(shootdata));
    GUN_SHOOT = true; 
    GUN_STOP = false;
  }
  else if (BrainData_t.attention > 1 && BrainData_t.attention < 55 && GUN_SHOOT)
  {
   BT_Serial.write(shitdata, sizeof(shitdata));
   GUN_STOP = true;
   GUN_SHOOT = false;   
  }}
  vTaskDelay(100 / portTICK_PERIOD_MS);
  } 
  }

