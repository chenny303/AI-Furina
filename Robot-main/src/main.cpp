#include <Arduino.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
// 定义软件串口的引脚
#define AS_RX_PIN 7
#define AS_TX_PIN 6
#define GUND_PIN 12
#define ELEOUT_PIN A0
#define ELEC_PIN A5

//电量播报功能
//启动音效功能

bool isMotorRunning = false;  // 初始状态为停止
SoftwareSerial ASRPROSerial(AS_RX_PIN, AS_TX_PIN);
int address = 0;
static int armdata = 0;

// 电机类，用于控制电机的方向和速度
class Motor {
public:
  // 构造函数，初始化电机引脚
  Motor(int directionPin, int speedPin);

  // 初始化电机引脚模式
  void attach();

  // 控制电机运行
  void run(int direction, int speed);

  // 停止电机
  void stop();

private:
  int _directionPin;  // 方向引脚
  int _speedPin;      // 速度引脚
};
// 构造函数实现
Motor::Motor(int directionPin, int speedPin)
  : _directionPin(directionPin), _speedPin(speedPin) {}

// 初始化电机引脚模式
void Motor::attach() {
  pinMode(_directionPin, OUTPUT);
  pinMode(_speedPin, OUTPUT);
}
// 控制电机运行
void Motor::run(int direction, int speed) {
  digitalWrite(_directionPin, direction);
  analogWrite(_speedPin, speed);
}

// 停止电机
void Motor::stop() {
  digitalWrite(_directionPin, LOW);
  analogWrite(_speedPin, 0);
}
/**************************************************************************************************
*******************************************定义函数*********************************************
**************************************************************************************************/
// 机器人类，包含多个电机对象，用于控制机器人的各个部分
class RobotClass {
public:
  // 构造函数，初始化各个电机对象
  RobotClass();

  // 初始化所有电机
  void Motor_init();

  // 控制左腿电机
  void left_leg(int direction, int speed);

  // 控制右腿电机
  void right_leg(int direction, int speed);

  // 控制左臂电机
  void left_arm(int direction, int speed);

  // 控制右臂电机
  void right_arm(int direction, int speed);

  // 控制左臂电机向上运动
  void left_arm_up(int speed);

  // 控制左臂电机向下运动
  void left_arm_down(int speed);

  // 控制右臂电机向上运动
  void right_arm_up(int speed);

  // 控制右臂电机向下运动
  void right_arm_down(int speed);

  // 双腿向前移动
  void legs_front(int speed);

  // 双腿向后移动
  void legs_back(int speed);

  // 双腿向左移动
  void legs_left(int speed);

  // 双腿向右移动
  void legs_right(int speed);

  // 控制枪电机
  void gun(int speed);

  // 停止所有电机
  void legs_stop();

  // 停止枪电机
  void gun_stop();


private:
  Motor M2;          // 右腿电机
  Motor M3;          // 左腿电机
  Motor M4;          // 左臂电机
  Motor M5;          // 枪电机
  Motor M6;          // 右臂电机
  int CarDirection;  // 机器人方向标志
};

// 构造函数实现，初始化各个电机对象
RobotClass::RobotClass()
  : M2(2, 3), M3(4, 5), M4(8, 9), M5(10, 11), M6(A1, A0), CarDirection(0) {}

// 初始化所有电机
void RobotClass::Motor_init() {
  M2.attach();
  M3.attach();
  M4.attach();
  M5.attach();
  M6.attach();
}
/**************************************************************************************************
*******************************************动作函数集*********************************************
**************************************************************************************************/
// 控制左腿电机
void RobotClass::left_leg(int direction, int speed) {
  M3.run(direction, speed);
}

// 控制右腿电机
void RobotClass::right_leg(int direction, int speed) {
  M2.run(direction, speed);
}

// 控制左臂电机
void RobotClass::left_arm(int direction, int speed) {
  M4.run(direction, speed);
}

// 控制右臂电机
void RobotClass::right_arm(int direction, int speed) {
  M6.run(direction, speed);
}

// 双腿向前移动
void RobotClass::legs_front(int speed) {
  M2.run(0, -speed);
  M3.run(1, speed);
}

// 双腿向后移动
void RobotClass::legs_back(int speed) {
  M2.run(1, speed);
  M3.run(0, -speed);
}

// 双腿向左移动
void RobotClass::legs_left(int speed) {
  M2.run(0, -speed);
  M3.run(0, -speed);
}

// 双腿向右移动
void RobotClass::legs_right(int speed) {
  M2.run(1, speed);
  M3.run(1, speed);
}

// 停止所有电机
void RobotClass::legs_stop() {
  M2.stop();
  M3.stop();
  M4.stop();
  M6.stop();
}

// 控制左臂电机向上运动
void RobotClass::left_arm_up(int speed) {
  M4.run(1, speed);  // 1 表示向前（向上），speed 是速度
}

// 控制左臂电机向下运动
void RobotClass::left_arm_down(int speed) {
  M4.run(0, speed);  // 0 表示向后（向下），speed 是速度
}

// 控制右臂电机向上运动
void RobotClass::right_arm_up(int speed) {
  M6.run(0, speed);  // 1 表示向前（向上），speed 是速度
}

// 控制右臂电机向下运动
void RobotClass::right_arm_down(int speed) {
  M6.run(1, speed);  // 0 表示向后（向下），speed 是速度
}

// 控制枪电机
void RobotClass::gun(int speed) {
  M5.run(1, speed);
}

// 停止枪电机
void RobotClass::gun_stop() {
  M5.stop();
}



// 创建RobotClass的全局实例
RobotClass robot;

/**************************************************************************************************
*******************************************数据包控制函数*******************************************
***************************************************************************************************
**************************************************************************************************/

void process_Buffer(unsigned char* Buffer) {
  unsigned long startTime;
  delay(100);
  // 如果电机正在运行，则先停止电机
  if (isMotorRunning) {
    robot.legs_stop();
    isMotorRunning = false;  // 更新状态为停止
    delay(100);              // 稍作延时，确保电机完全停止
  }

  switch (Buffer[1]) {
    case 0xA1:
      // 持续前进
      robot.legs_front(Buffer[2]);
      isMotorRunning = true;  // 更新状态为运动
      Serial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
      break;
    case 0xA2:
      // 前进5秒
      startTime = millis();
      robot.legs_front(Buffer[2]);
      while (millis() - startTime < 5000) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      Serial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
      break;
    case 0xB1:
      // 持续后退
      robot.legs_back(Buffer[2]);
      isMotorRunning = true;  // 更新状态为运动
      Serial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
      break;
    case 0xB2:
      // 后退5秒
      startTime = millis();
      robot.legs_back(Buffer[2]);
      while (millis() - startTime < 5000) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      Serial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
      break;
    case 0xC1:
      // 左转半圈
      startTime = millis();
      robot.legs_left(Buffer[2]);
      while (millis() - startTime < 500) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    case 0xC2:
      // 左转一段
      startTime = millis();
      robot.legs_left(Buffer[2]);
      while (millis() - startTime < 80) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    case 0xD1:
      // 右转半圈
      startTime = millis();
      robot.legs_right(Buffer[2]);
      while (millis() - startTime < 500) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    case 0xD2:
      // 右转一段
      startTime = millis();
      robot.legs_right(Buffer[2]);
      while (millis() - startTime < 80) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    case 0xE1:
      // 停止全部动作
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      Serial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
      break;
    case 0xE2:
      // 发射1秒
      digitalWrite(GUND_PIN, 1);
      delay(100);
      robot.gun(Buffer[2]);
      startTime = millis();
      while (millis() - startTime < 230) {}
      robot.gun_stop();
      digitalWrite(GUND_PIN, 0);
      break;
    case 0xE3:
      // 停止发射
      robot.gun_stop();
      break;
    case 0xE4:
      //查询电压
        ASRPROSerial.println(analogRead(ELEC_PIN) * (4.9/ 1024) * 6);
        delay(100); 
      break;
    case 0xF1:
      // 手臂向上
      armdata = true;
      EEPROM.write(address, armdata);
      robot.left_arm_down(Buffer[2]);
      startTime = millis();
      while (millis() - startTime < 300) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    case 0xF2:
      // 手臂向下
      armdata = false;
      EEPROM.write(address, armdata);
      robot.left_arm_up(Buffer[2]);
      startTime = millis();
      while (millis() - startTime < 300) {}
      robot.legs_stop();
      isMotorRunning = false;  // 更新状态为停止
      break;
    default:
      break;
  }
}
/**************************************************************************************************
*******************************************串口解析函数*********************************************
**************************************************************************************************/
void check_Serial(SoftwareSerial& serial) {
  while (serial.available() >= 5) {              // 确保至少有5个字节数据可用
    unsigned char Frame_header = serial.peek();  // 获取当前字节但不移除
    // 当帧头是0x55时，读取5个字节的数据包
    if (Frame_header == 0x55 && serial.available() >= 5) {
      unsigned char Buffer[5];
      serial.readBytes(Buffer, 5);
      if (Buffer[3] == 0xFF && Buffer[4] == 0xFF) {
        process_Buffer(Buffer);
      }
    } else {
      serial.read();  // 丢弃无效数据
    }
  }
}

/**************************************************************************************************
*******************************************主函数*********************************************
**************************************************************************************************/


void setup() {
  Serial.begin(9600);
  ASRPROSerial.begin(9600);
  robot.Motor_init();
  while (!Serial);  // 等待串口连接
  pinMode(GUND_PIN, OUTPUT);
  pinMode(ELEC_PIN, INPUT);
  int readValue = EEPROM.read(address);  // 从EEPROM的地址0读取数据
  if (readValue == true) {
    armdata = false;
    robot.left_arm_up(1);
    delay(500);  // 发射持续2秒
    robot.legs_stop();
    EEPROM.write(address, armdata);
  }
}

void loop() {
  float elecdata = analogRead(ELEC_PIN) * (4.9f / 1024.0f) * 6.0f;
  Serial.println(analogRead(ELEC_PIN));
  Serial.println(elecdata);
  if (elecdata < 6.1)
  {digitalWrite(ELEOUT_PIN, 1);}
  else{digitalWrite(ELEOUT_PIN, 0);}
  digitalWrite(ELEOUT_PIN, 0);
  digitalWrite(GUND_PIN, 0);
  check_Serial(ASRPROSerial);
}