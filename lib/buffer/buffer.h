/**
  ***************************************************************************************
  * @file    buffer.h
  * @author  lijihu
  * @version V1.0.0
  * @date    2025/05/10
  * @brief   Buffer functionality implementation
  *          实现缓冲器功能
  *
  * Buffer description / 缓冲器说明:
  *   Optical sensors: Blocked = 1, Unblocked = 0
  *   光感：遮挡1，不遮挡0
  *   
  *   Filament switch: Filament present = 0, No filament = 1
  *   耗材开关：有耗材0，无耗材1
  *   
  *   Buttons: Pressed = 0, Released = 1
  *   按键：按下0，松开1
  *
  * Pin assignments / 引脚分配:
  *   HALL1     --> PB2  (Optical sensor 3 / 光感3)
  *   HALL2     --> PB3  (Optical sensor 2 / 光感2)
  *   HALL3     --> PB4  (Optical sensor 1 / 光感1)
  *   ENDSTOP_3 --> PB7  (Filament switch / 耗材开关)
  *   KEY1      --> PB13 (Reverse / 后退)
  *   KEY2      --> PB12 (Forward / 前进)
  *
  * @note    
  ***************************************************************************************
  * Copyright 2024 xxx@126.com
  * 版权声明 COPYRIGHT 2024 xxx@126.com
  ***************************************************************************************
**/

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <TMCStepper.h>
#include <Arduino.h>
#include <EEPROM.h>

// Optical sensor pins / 光感引脚
#define HALL1       PB2  // Optical sensor 3 / 光感3
#define HALL2       PB3  // Optical sensor 2 / 光感2
#define HALL3       PB4  // Optical sensor 1 / 光感1

// Filament detection / 耗材检测
#define ENDSTOP_3   PB7  // Filament switch / 耗材开关

// Button pins / 按键引脚
#define KEY1        PB13 // Reverse button / 后退
#define KEY2        PB12 // Forward button / 前进

// Motor driver pins / 电机驱动引脚
#define EN_PIN       PA6  // Enable / 使能
#define MOTOR_DIR_PIN PA7  // Motor direction / 电机方向
#define STEP_PIN     PC13 // Step / 步
#define UART         PB1  // Software UART / 软串口

// Output signals / 输出信号
#define DUANLIAO    PB15 // Filament runout signal / 断料
#define DULIAO      PB15 // Blockage signal (same pin) / 堵料
#define ERR_LED     PA15 // Error indicator LED / 错误指示灯
#define START_LED   PA8  // Status indicator LED / 状态指示灯

// Extension pins / 扩展引脚
#define EXTENSION_PIN1 PA2  // Extension pin 1 / 扩展引脚1
#define EXTENSION_PIN2 PA3  // Extension pin 2 / 扩展引脚2
#define EXTENSION_PIN3 PB11 // Extension pin 3 / 扩展引脚3
#define EXTENSION_PIN4 PB10 // Extension pin 4 / 扩展引脚4
#define EXTENSION_PIN5 PA5  // Extension pin 5 / 扩展引脚5
#define EXTENSION_PIN6 PA4  // Extension pin 6 / 扩展引脚6
#define EXTENSION_PIN7 PB14 // Extension pin 7 / 扩展引脚7

// Blockage detection pins / 堵料检测引脚
#define PULSE1_PIN EXTENSION_PIN5  // Pulse input 1: receives controller pulses / 接收脉冲引脚1，接收主控脉冲
#define DIR_PIN EXTENSION_PIN3     // Direction pin: extrude=1, retract=0 / 方向引脚,挤出1，回抽0
#define PULSE2_PIN EXTENSION_PIN4  // Pulse input 2: receives MDM module pulses / 接收脉冲引脚2，接收段断料模块脉冲
#define MDM_DPIN EXTENSION_PIN6    // MDM runout pin: has filament=1, no filament=0 / MDM断料引脚 1：有耗材 0：无耗材

// Signal detection / 信号检测
#define FRONT_SIGNAL_PIN PB5 // Forward signal pin / 前信号引脚
#define BACK_SIGNAL_PIN PB6  // Backward signal pin / 后信号引脚

// Signal counter GPIO configuration / 信号计数GPIO配置
#define SIGNAL_COUNT_DIR_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE()	// PB11 clock enable / PB11时钟使能
#define SIGNAL_COUNT_DIR_GPIO_Port			(GPIOB)                         // GPIO port B / GPIO端口B
#define SIGNAL_COUNT_DIR_Pin				(GPIO_PIN_11)                   // Pin 11 / 引脚11
#define SIGNAL_COUNT_DIR_Get_IRQn			(EXTI4_15_IRQn)	                // EXTI11 interrupt / EXTI11中断

// Signal counter timer configuration / 信号计数定时器配置
#define SIGNAL_COUNT_PUL_CLK_ENABLE()		__HAL_RCC_GPIOA_CLK_ENABLE()	// PA5 clock enable / PA5时钟使能
#define SIGNAL_COUNT_PUL_GPIO_Port			(GPIOA)                         // GPIO port A / GPIO端口A
#define SIGNAL_COUNT_PUL_Pin				(GPIO_PIN_5)                    // Pin 5 / 引脚5
#define SIGNAL_COUNT_TIM_CLK_ENABLE()		__HAL_RCC_TIM2_CLK_ENABLE()		// TIM2 clock enable / TIM2时钟使能
#define	SIGNAL_COUNT_Get_TIM				(TIM2)                          // Timer 2 / 定时器2
#define	SIGNAL_COUNT_Get_HTIM				(htim2)                         // Timer 2 handle / 定时器2句柄




// TMC2209 driver configuration / TMC2209驱动配置
#define DRIVER_ADDRESS 0b00     // TMC driver address (MS1/MS2 pins) / 驱动地址
#define R_SENSE        0.11f    // Sense resistor value / 检测电阻值

// Motor parameters (runtime adjustable) / 电机参数（运行时可调）
static int32_t SPEED = 260;     // Motor speed in RPM / 转速(单位：r/min)
#define Move_Divide_NUM ((int32_t)(64))  // Microstep subdivision / 每步柔性件控制细分量
static int32_t VACTRUAL_VALUE = (uint32_t)(SPEED*Move_Divide_NUM*200/60/0.715);  // VACTUAL register value / VACTUAL寄存器值

// Motor control constants / 电机控制常量
#define STOP       0     // Stop motor / 停止
#define I_CURRENT  (500) // Motor current in mA / 电流(mA)
#define WRITE_EN_PIN(x) digitalWrite(EN_PIN,x)  // Enable pin control / 使能EN引脚
#define FORWARD    1     // Forward direction / 前进方向
#define BACK       0     // Backward direction / 后退方向

// Debug mode / 调试模式
#define DEBUG      0     // 0 = disabled, 1 = enabled / 0=禁用, 1=启用

// Buffer sensor state structure / 定义结构体存储缓冲器中各传感器的状态
typedef struct Buffer
{
	bool buffer1_pos1_sensor_state;    // Position sensor 1 state / 位置传感器1状态
	bool buffer1_pos2_sensor_state;    // Position sensor 2 state / 位置传感器2状态
	bool buffer1_pos3_sensor_state;    // Position sensor 3 state / 位置传感器3状态
	bool buffer1_material_swtich_state; // Filament switch state / 耗材开关状态
	bool key1;                         // Button 1 state / 按键1状态
	bool key2;                         // Button 2 state / 按键2状态
}Buffer;

// Motor state enumeration / 电机状态控制枚举
typedef enum
{
	Forward = 0,  // Forward / 向前
	Stop,         // Stop / 停止
	Back          // Backward / 后退
}Motor_State;

// Blockage detection structure / 堵料检测结构体
typedef struct BlockageDetect
{
	int32_t target_distance;        // Target distance / 目标距离
	int32_t actual_distance;        // Actual distance / 实际距离
	int32_t distance_error;         // Distance error / 距离误差
	float allow_error;              // Allowed error / 允许误差
	bool blockage_flag;             // Blockage flag / 堵料标志位

	int32_t mdm_pulse_cnt = 0;      // Pulses received from MDM module / 接收到段堵料模块发送的脉冲数
	int16_t last_pulse_cnt = 0;     // Previous pulse count / 上次脉冲数
	int16_t pulse_cnt = 0;          // Pulses received from controller / 接收到主控板发送的脉冲数
	int16_t pulse_cnt_sub = 0;      // Pulse difference / 脉冲差值
	int32_t extrusion_pulse_cnt = 0; // Extrusion pulse count / 挤出脉冲数
	float encoder_length;           // Encoder length / 编码器长度
}BlockageDetect;

// Initialization functions / 初始化函数
extern void buffer_sensor_init();  // Initialize sensors / 初始化传感器
extern void buffer_motor_init();   // Initialize motor driver / 初始化电机驱动

// Sensor and control functions / 传感器和控制函数
extern void read_sensor_state(void);  // Read all sensor states / 读取所有传感器状态
extern void motor_control(void);      // Motor control logic / 电机控制逻辑

// Main functions / 主函数
extern void buffer_init();         // Main initialization / 主初始化
extern void buffer_loop(void);     // Main loop / 主循环
extern void timer_it_callback();   // Timer interrupt callback / 定时器中断回调
extern void buffer_debug(void);    // Debug output / 调试输出

// Global variables / 全局变量
extern bool is_error;              // Error flag / 错误标志
extern uint32_t front_time;        // Forward motion time counter / 前进时间
extern uint32_t timeout;           // Timeout value / 超时值
extern bool is_front;              // Forward motion flag / 前进标志
extern TMC2209Stepper driver;      // TMC2209 driver object / TMC2209驱动对象


#endif