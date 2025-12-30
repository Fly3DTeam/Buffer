/**
  ***************************************************************************************
  * @file    buffer.cpp
  * @author  lijihu
  * @version V1.0.0
  * @date    2025/05/10
  * @brief   实现缓冲器功能
			  *缓冲器说明
				光感：遮挡1，不遮挡0；
				耗材开关：有耗材0，无耗材1；
				按键：按下0，松开1；

				引脚：
				HALL1 --> PB2 (光感3)
				HALL2 --> PB3 (光感2)
				HALL3 --> PB4 (光感1)
				ENDSTOP_3 --> PB7(耗材开关)
				KEY1 --> PB13(后退)	
				KEY2 --> PB12(前进)
  *
  * @note    
  ***************************************************************************************
  * 版权声明 COPYRIGHT 2024 xxx@126.com
  ***************************************************************************************
**/

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <TMCStepper.h>
#include <Arduino.h>
#include <EEPROM.h>

#define HALL1       PB2 //光感3
#define HALL2       PB3 //光感2   
#define HALL3       PB4 //光感1

#define ENDSTOP_3   PB7 //耗材开关

#define KEY1        PB13 //后退
#define KEY2        PB12 //前进

#define EN_PIN      PA6 //使能
#define DIR_PIN     PA7 //方向
#define STEP_PIN    PC13 //步
#define UART        PB1 //软串口

#define DUANLIAO    PB15 //断料
#define DULIAO    	PB15 //堵料
#define ERR_LED     PA15 //指示灯
#define START_LED   PA8  //指示灯

#define EXTENSION_PIN1 PA2 //扩展引脚1
#define EXTENSION_PIN2 PA3 //扩展引脚2
#define EXTENSION_PIN3 PB11 //扩展引脚3
#define EXTENSION_PIN4 PB10 //扩展引脚4
#define EXTENSION_PIN5 PA5 //扩展引脚5
#define EXTENSION_PIN6 PA4 //扩展引脚6
#define EXTENSION_PIN7 PB14 //扩展引脚7

//堵料检测
#define PULSE1_PIN EXTENSION_PIN5  	//接收脉冲引脚1，接收主控脉冲
#define SIG_DIR_PIN EXTENSION_PIN3	//方向引脚,挤出1，回抽0
#define PULSE2_PIN EXTENSION_PIN4 	//接收脉冲引脚2，接收段断料模块脉冲
#define MDM_DPIN EXTENSION_PIN6 	//MDM断料引脚 1：有耗材 0：无耗材

//信号检测
#define FRONT_SIGNAL_PIN PB5 //前信号引脚
#define BACK_SIGNAL_PIN PB6 //后信号引脚

//SIGNAL_COUNT(GPIO)
#define SIGNAL_COUNT_DIR_CLK_ENABLE()		__HAL_RCC_GPIOB_CLK_ENABLE()	//PB11
#define SIGNAL_COUNT_DIR_GPIO_Port			(GPIOB)
#define SIGNAL_COUNT_DIR_Pin				(GPIO_PIN_11)
#define SIGNAL_COUNT_DIR_Get_IRQn			(EXTI4_15_IRQn)	//EXTI11中断

//SIGNAL_COUNT(AFIO & TIM)
#define SIGNAL_COUNT_PUL_CLK_ENABLE()		__HAL_RCC_GPIOA_CLK_ENABLE()	//PA5
#define SIGNAL_COUNT_PUL_GPIO_Port			(GPIOA)
#define SIGNAL_COUNT_PUL_Pin						(GPIO_PIN_5)
#define SIGNAL_COUNT_TIM_CLK_ENABLE()		__HAL_RCC_TIM2_CLK_ENABLE()		//TIM2
#define	SIGNAL_COUNT_Get_TIM						(TIM2)
#define	SIGNAL_COUNT_Get_HTIM						(htim2)




#define DRIVER_ADDRESS 0b00 // TMC Driver address according to MS1 and MS2
#define R_SENSE 0.11f // Match to your driver

static int32_t SPEED=260;  //转速(单位：r/min)
#define Move_Divide_NUM			((int32_t)(64))		//(每步柔性件控制细分量)
static int32_t VACTRUAL_VALUE=(uint32_t)(SPEED*Move_Divide_NUM*200/60/0.715) ;  //VACTUAL寄存器值

#define STOP 0				//停止
#define WRITE_EN_PIN(x) digitalWrite(EN_PIN,x)//使能EN引脚
#define FORWARD		1//耗材方向
#define BACK		0


#define DEBUG 0

//定义结构体存储缓冲器中各传感器的的状态
typedef struct Buffer
{
	//buffer1
	bool buffer1_pos1_sensor_state;	
	bool buffer1_pos2_sensor_state;		
	bool buffer1_pos3_sensor_state;		
	bool buffer1_material_swtich_state;	
	bool key1;
	bool key2;
	
}Buffer;

//电机状态控制枚举
typedef enum
{
	Forward=0,//向前
	Stop,		//停止
	Back		//后退
}Motor_State;

//堵料检测结构体
typedef struct BlockageDetect
{
	int32_t target_distance; //目标距离
	int32_t actual_distance; //实际距离
	int32_t distance_error; //距离误差
	float allow_error; //允许误差
	bool blockage_flag; //堵料标志位

	int32_t mdm_pulse_cnt=0;//接收到段堵料模块发送的脉冲数
	int16_t last_pulse_cnt=0;//上次脉冲数
	int16_t pulse_cnt=0;//接收到主控板发送的脉冲数
	int16_t pulse_cnt_sub=0;//脉冲差值
	int32_t extrusion_pulse_cnt=0;//挤出脉冲数
	float encoder_length; //编码器长度

}BlockageDetect;

struct Buffer_Parameter{
	uint32_t timeout;
	uint32_t steps;
	float encoder_length;
	float allow_error_scale;
	int32_t SPEED;
	uint32_t I_CURRENT;
	bool DUANLIAO_OUT_STATE;
	uint16_t magic_number;
};

extern void buffer_sensor_init();
extern void buffer_motor_init();

extern void read_sensor_state(void);
extern void motor_control(void);

extern void buffer_init();
extern void buffer_loop(void);
extern void timer_it_callback();
extern void buffer_debug(void);

extern bool is_error;
extern uint32_t front_time;//前进时间
extern uint32_t timeout;
extern bool is_front;
extern TMC2209Stepper driver;


#endif