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
  * 版权声明 COPYRIGHT 2025 xxx@126.com
  ***************************************************************************************
**/


#include "buffer.h"
#define VERSION "1.1.5"

//GPIO输入
#define SIGNAL_COUNT_READ_DIR_IO()	(SIGNAL_COUNT_DIR_GPIO_Port -> IDR & SIGNAL_COUNT_DIR_Pin)
//TIM输入
#define SIGNAL_COUNT_READ_COUNT()		(SIGNAL_COUNT_Get_TIM -> CNT)
//TIM输出
#define SIGNAL_COUNT_UP()						(SIGNAL_COUNT_Get_TIM -> CR1 &= ~(TIM_CR1_DIR))
#define SIGNAL_COUNT_DOWN()					(SIGNAL_COUNT_Get_TIM -> CR1 |=  (TIM_CR1_DIR))




TMC2209Stepper driver(UART, UART, R_SENSE, DRIVER_ADDRESS);
Buffer buffer={0};//存储个传感器状态
Motor_State motor_state=Stop;
static Motor_State last_motor_state=Stop;

bool is_front=false;//前进标志位
uint32_t front_time=0;//前进时间
const uint32_t DEFAULT_TIMEOUT = 60000;
uint32_t timeout=60000;//超时时间，单位：ms;
bool is_error=false;//错误标志位，如果连续60s推送耗材没停过，则认为错误
String serial_buf;

static HardwareTimer timer(TIM6);//超时出错
TIM_HandleTypeDef htim2;//硬件定时器接收脉冲

bool key1_press_flag=false;
bool key2_press_flag=false;
bool key1_release_flag=false;
bool key2_release_flag=false;
uint32_t key1_press_times=0;
uint32_t key2_press_times=0;
uint32_t key1_release_times=0;
uint32_t key2_release_times=0;
uint8_t key1_press_cnt=0;
uint8_t key2_press_cnt=0;

uint32_t inform_flag=false;
uint32_t inform_times=0;

const uint32_t DEFAULT_STEPS = 916;
uint32_t steps=916;//每毫米脉冲数;
BlockageDetect blockage_detect={0};//堵料检测结构体
bool connet_mdm_flag=false;
uint32_t blockage_inform_times=0;


const float DEFAULT_ENCODER_LENGTH = 1.73;
float encoder_length=1.73;//MDM段堵料模块每脉冲对应的线材移动量（mm/pulse）


const float DEFAULT_ALLOW_ERROR_SCALE = 2;
float allow_error_scale=2;//允许误差比例

uint32_t I_CURRENT = 500;		//电流

const int EEPROM_ADDR_TIMEOUT = 0;
const int EEPROM_ADDR_STEPS = 4;
const int EEPROM_ADDR_ENCODER_LENGTH = 8;
const int EEPROM_ADDR_ERROR_SCALE = 12;
const int EEPROM_ADDR_SPEED = 16;
const int EEPROM_ADDR_I_CURRENT = 20;
const int EEPROM_ADDR_ENDSTOP_OUT = 24;



//独立看门狗
#include "stm32f0xx_hal_iwdg.h"

IWDG_HandleTypeDef hiwdg;
static volatile uint32_t g_run_cnt=0;
bool DUANLIAO_OUT_STATE = 0; //断料输出状 0
Buffer_Parameter buffer_para;

void iwdg_init(void)
{
	// 1. 使能写访问
	IWDG->KR = 0x5555;     

	// 2. 设置分频
	IWDG->PR = IWDG_PRESCALER_256;  

	// 3. 设置重载值
	IWDG->RLR = 125*10-1;  //超时时间2s

	// 4. 启动 IWDG
	IWDG->KR = 0xCCCC;     	
}



//函数声明
void key1_it_callback(void);
void key2_it_callback(void);

void USB_Serial_Analys(void);
bool Check_Connet_MDM(void);
void REIN_TIM_SIGNAL_COUNT_Init(void);
void REIN_TIM_SIGNAL_COUNT_DeInit(void);
void Pulse_Receive_Init(void);
void Blockage_Detect(void);
void Main_Logic(void);
float fastAtof(const char *s);
void Signal_Dir_Init(void);
// void Buffer_S3_IT_Callback(void);
// void Buffer_S2_IT_Callback(void);
// void Buffer_S1_IT_Callback(void);

void buffer_parameter_init(Buffer_Parameter &buffer_para){
	EEPROM.get(0, buffer_para);
	if(buffer_para.magic_number!=0x55AA){
		buffer_para=Buffer_Parameter{DEFAULT_TIMEOUT,DEFAULT_STEPS,DEFAULT_ENCODER_LENGTH,DEFAULT_ALLOW_ERROR_SCALE,260,I_CURRENT,DUANLIAO_OUT_STATE,0x55AA};
		EEPROM.put(0, buffer_para);
	}
	timeout=buffer_para.timeout;
	steps=buffer_para.steps;
	encoder_length=buffer_para.encoder_length;
	allow_error_scale=buffer_para.allow_error_scale;
	SPEED=buffer_para.SPEED;
	I_CURRENT=buffer_para.I_CURRENT;
	DUANLIAO_OUT_STATE=buffer_para.DUANLIAO_OUT_STATE;
}

void buffer_init(){

	NVIC_SetPriority(TIM6_DAC_IRQn,0);//超时出错，独立看门狗
	// NVIC_SetPriority(TIM2_IRQn,1);//接收高频脉冲
	NVIC_SetPriority(EXTI4_15_IRQn,1);//按键1 按键2 dir 断堵料脉冲

	if(Check_Connet_MDM()){
		connet_mdm_flag=true;
		Serial.println("有连接");
	}
	else{
		connet_mdm_flag=false;
		Serial.println("无连接");
		
	}

  buffer_parameter_init(buffer_para);
  Pulse_Receive_Init();
  buffer_sensor_init();
  buffer_motor_init();
  Signal_Dir_Init();
//   delay(1000);

  VACTRUAL_VALUE=(uint32_t)(SPEED*Move_Divide_NUM*200/60/0.715) ;  //VACTUAL寄存器值



  timer.pause();
  timer.setPrescaleFactor(4800);//4800分频  48000000/4800=10000
  timer.setOverflow(1000);//100ms
  timer.attachInterrupt(&timer_it_callback);
  timer.resume();

  iwdg_init();
}

void buffer_loop()
{
	uint32_t lastToggleTime = 0; // 记录上次切换的时间
	
	while (1)
	{
		uint32_t nowTime=millis();
		if (blockage_detect.blockage_flag &&nowTime - lastToggleTime >= 50)
		{							   // 堵料，爆闪
			lastToggleTime = millis(); // 记录当前时间
			digitalToggle(ERR_LED);
			
		}
		else{
			if(connet_mdm_flag){//连接了MDM模块，每秒闪两次
				static uint8_t led_state=0;
				if(led_state==0){
					digitalWrite(ERR_LED,HIGH);
					led_state=1;
					lastToggleTime = millis();
				}
				else if(led_state==1&&nowTime-lastToggleTime>=100){
					digitalWrite(ERR_LED,LOW);
					led_state=2;
					lastToggleTime = millis();
				}
				else if(led_state==2&&nowTime-lastToggleTime>=100){
					digitalWrite(ERR_LED,HIGH);
					led_state=3;
					lastToggleTime = millis();
				}
				else if(led_state==3&&nowTime-lastToggleTime>=100){
					digitalWrite(ERR_LED,LOW);
					led_state=4;
					lastToggleTime = millis();
				}
				else if(led_state==4&&nowTime-lastToggleTime>=600){
					led_state=0;
				}

			}
			else if(millis() - lastToggleTime >= 500) //没有连接，每秒闪一次
			{
				lastToggleTime = millis(); // 记录当前时间
				digitalToggle(ERR_LED);
				// Serial.println("CNT:"+String(TIM2->CNT));
				// Serial.println("	mdm_pulse_cnt:"+String(blockage_detect.mdm_pulse_cnt));
			}


		}
		// 1、读取各传感器的值
		read_sensor_state();
		if(connet_mdm_flag) Blockage_Detect();
		motor_control();
		USB_Serial_Analys();

		// 堵料输出3s后关断
		if (blockage_detect.blockage_flag)
		{
			if (millis() - blockage_inform_times > 3000)
			{
				digitalWrite(DULIAO, HIGH);
				blockage_detect.blockage_flag = false;
			}
		}

		g_run_cnt++;
	}
}

void buffer_sensor_init(){
  //传感器初始化
  pinMode(HALL1,INPUT);
  pinMode(HALL2,INPUT);
  pinMode(HALL3,INPUT);
  pinMode(ENDSTOP_3,INPUT);

//   attachInterrupt(HALL1,&Buffer_S3_IT_Callback,RISING);
//   attachInterrupt(HALL2,&Buffer_S2_IT_Callback,RISING);
//   attachInterrupt(HALL3,&Buffer_S1_IT_Callback,RISING);

  pinMode(KEY1,INPUT);
  pinMode(KEY2,INPUT);

  attachInterrupt(KEY1,&key1_it_callback,CHANGE);
  attachInterrupt(KEY2,&key2_it_callback,CHANGE);

  //耗材指示灯初始化
  pinMode(DUANLIAO,OUTPUT);
  pinMode(ERR_LED,OUTPUT);
  pinMode(START_LED,OUTPUT);
  pinMode(DULIAO,OUTPUT);

  //扩展引脚初始化
  pinMode(EXTENSION_PIN1,OUTPUT);
  pinMode(EXTENSION_PIN2,OUTPUT);

  digitalWrite(EXTENSION_PIN1,LOW);
  digitalWrite(EXTENSION_PIN2,HIGH);
  digitalWrite(DULIAO,HIGH);
  digitalWrite(DUANLIAO,HIGH);

  //配置为上拉输入，信号控制缓冲器，检测到对应引脚低电平，执行进料或退料动作
  pinMode(FRONT_SIGNAL_PIN,INPUT_PULLUP);
  pinMode(BACK_SIGNAL_PIN,INPUT_PULLUP);

}

void buffer_motor_init(){

  //电机驱动引脚初始化
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW);      // Enable driver in hardware

  //电机驱动初始化
  driver.begin();                  // UART: Init SW UART (if selected) with default 115200 baudrate
  driver.beginSerial(9600);
  driver.I_scale_analog(false);
  driver.toff(5);                 // Enables driver in software
  driver.rms_current(I_CURRENT);        // Set motor RMS current
  driver.microsteps(Move_Divide_NUM);          // Set microsteps to 1/16th
  driver.VACTUAL(STOP);           // Set velocity
  driver.en_spreadCycle(true);
  driver.pwm_autoscale(true);
 
}

/**
  * @brief  读取各传感器状态
  * @param  NULL
  * @retval NULL
**/
void read_sensor_state(void)
{
	buffer.buffer1_pos1_sensor_state= digitalRead(HALL3);
	buffer.buffer1_pos2_sensor_state= digitalRead(HALL2);	
	buffer.buffer1_pos3_sensor_state= digitalRead(HALL1);		
	buffer.buffer1_material_swtich_state=digitalRead(ENDSTOP_3);	
	buffer.key1=digitalRead(KEY1);
	buffer.key2=digitalRead(KEY2);
}

/**
  * @brief  电机控制
  * @param  NULL
  * @retval NULL
**/
void motor_control(void)
{
	static uint32_t cur_times=0;
	cur_times=millis();

	//通知信号关闭
	if(inform_flag&&cur_times-inform_times>=3000){
		inform_flag=false;
		digitalWrite(EXTENSION_PIN2,HIGH);
		digitalWrite(EXTENSION_PIN1,LOW);
	}
	
	//按键控制电机
	//按键1短按后松开
	if(key1_release_flag&&millis()-key1_release_times>500){

		key1_release_flag=false;
		//短按1次后松开
		if(key1_press_cnt==1){
			digitalWrite(EXTENSION_PIN2,LOW);
			inform_times=millis();
			inform_flag=true;
			is_error=false;
		}
		else if(key1_press_cnt>=2){//短按2次或两次以上后松开
			is_error=true;
		}

		key1_press_cnt=0;
	}

 	//按键2短按后松开
	if(key2_release_flag&&cur_times-key2_release_times>500){

		key2_release_flag=false;
		//短按1次后松开
		if(key2_press_cnt==1){
			digitalWrite(EXTENSION_PIN1,HIGH);
			inform_times=millis();
			inform_flag=true;
			is_error=false;
		}
		else if(key2_press_cnt>=2){//短按2次或两次以上后松开
			is_error=true;
		}
		
		key2_press_cnt=0;

	}	

	//按键1按下后长按
	if(key1_press_flag&&cur_times-key1_press_times>=500||digitalRead(BACK_SIGNAL_PIN)==LOW)
	{
		
		WRITE_EN_PIN(0);//使能
    	driver.VACTUAL(STOP);	//停止
		
    	driver.shaft(BACK);
    	driver.VACTUAL(VACTRUAL_VALUE);
		while(key1_press_flag||digitalRead(BACK_SIGNAL_PIN)==LOW){
			delay(1);
			g_run_cnt++;
			// Serial.println("key1_press_flag is true ");
		}//等待松手
					

		driver.VACTUAL(STOP);	//停止
		motor_state=Stop;

		is_front=false;
		front_time=0;
		is_error=false;
		WRITE_EN_PIN(1);//失能
		is_error=true;

	}
	else if(key2_press_flag&&cur_times-key2_press_times>=500||digitalRead(FRONT_SIGNAL_PIN)==LOW)//按键2按下后长按
	{

		WRITE_EN_PIN(0);
		driver.VACTUAL(STOP);	//停止
		
    	driver.shaft(FORWARD);
		driver.VACTUAL(VACTRUAL_VALUE);
		while(key2_press_flag||digitalRead(FRONT_SIGNAL_PIN)==LOW){
			delay(1);
			g_run_cnt++;
		};//等待松手
					

		driver.VACTUAL(STOP);	//停止
		motor_state=Stop;

		is_front=false;
		front_time=0;
		is_error=false;
		WRITE_EN_PIN(1);
	}
	
	if(connet_mdm_flag){//连接了MDM断堵料模块
		//判断耗材
		if(digitalRead(ENDSTOP_3)&&!digitalRead(MDM_DPIN))
		{
			//无耗材，停止电机
			driver.VACTUAL(STOP);	//停止
			motor_state=Stop;
			
			//断料引脚输出断料状态
			digitalWrite(DUANLIAO,DUANLIAO_OUT_STATE);
			
			//关闭指示灯
			digitalWrite(START_LED,0);

			is_front=false;
			front_time=0;
			is_error=false;
			WRITE_EN_PIN(1);

			
			return;//无耗材，结束
		}
		else if(!blockage_detect.blockage_flag){
			//有耗材，断料引脚输出非断料状态
			digitalWrite(DUANLIAO,!DUANLIAO_OUT_STATE);
			
			//开启指示灯
			digitalWrite(START_LED,1);					

		}

	}
	else{
		//判断耗材
		if(digitalRead(ENDSTOP_3))
		{
			//无耗材，停止电机
			driver.VACTUAL(STOP);	//停止
			motor_state=Stop;
			
			//断料引脚输出低电平
			digitalWrite(DUANLIAO,DUANLIAO_OUT_STATE);
			
			//关闭指示灯
			digitalWrite(START_LED,0);

			is_front=false;
			front_time=0;
			is_error=false;
			WRITE_EN_PIN(1);

			
			return;//无耗材，结束
		}		

		//有耗材，断料引脚输出非断料状态
		digitalWrite(DUANLIAO,!DUANLIAO_OUT_STATE);
		
		//开启指示灯
		digitalWrite(START_LED,1);		
	}

		


	//判断是否有错误
	if(is_error){
		//停止电机
		driver.VACTUAL(STOP);	//停止
		motor_state=Stop;
		WRITE_EN_PIN(1);
		return ;
	}

	//缓冲器位置记录
	if(buffer.buffer1_pos1_sensor_state)	//缓冲器位置为1，耗材往前推
	{
		last_motor_state=motor_state;		//记录上一次状态
		motor_state=Forward;
		is_front=true;

	}
	else if(buffer.buffer1_pos2_sensor_state)	//缓冲器位置为2,电机停止转动
	{
		last_motor_state=motor_state;		//记录上一次状态
		motor_state=Stop;
		is_front=false;
		front_time=0;
	}
	else if(buffer.buffer1_pos3_sensor_state)	//缓冲器位置为3，回退耗材
	{
		last_motor_state=motor_state;		//记录上一次状态
		motor_state=Back;
		is_front=false;
		front_time=0;
	}
			
	if(motor_state==last_motor_state)//如果上次状态跟这次状态一致，则不需要再次发送控制命令,结束此次函数
		return;
	
	static uint8_t write_cnt=0;
	uint8_t retry_count=9;
	
	//电机控制
	switch(motor_state)
	{
		case Forward://向前
		{
			WRITE_EN_PIN(0);
			if(last_motor_state==Back)	driver.VACTUAL(STOP);//上次是后退，先停下再前进
			driver.shaft(FORWARD);
			write_cnt=driver.IFCNT();
			driver.VACTUAL(VACTRUAL_VALUE);
			while(write_cnt==driver.IFCNT()&&retry_count--){//发送失败重发
				driver.VACTUAL(VACTRUAL_VALUE);
			}

		}break;
		case Stop://停止
		{
			write_cnt=driver.IFCNT();
			driver.VACTUAL(STOP);
			while(write_cnt==driver.IFCNT()&&retry_count--){//发送失败重发
				driver.VACTUAL(STOP);
			}	
			WRITE_EN_PIN(1);		

		}break;
		case Back://向后
		{
			WRITE_EN_PIN(0);
			if(last_motor_state==Forward)	driver.VACTUAL(STOP);;//上次是前进，先停下再后退
			driver.shaft(BACK);
			write_cnt=driver.IFCNT();
			driver.VACTUAL(VACTRUAL_VALUE);
			while(write_cnt==driver.IFCNT()&&retry_count--){//发送失败重发
				driver.VACTUAL(VACTRUAL_VALUE);
			}				
		}break;
		
	}
	last_motor_state=motor_state;		//记录上一次状态
	
}

void timer_it_callback(){

	//喂狗(每100ms喂狗一次)
	static uint32_t i=0;
	i++;
	//每5秒检测g_run_cnt的值
	if(i>=50)
	{
		//主程序存在异常
		if(g_run_cnt == 0)
		{
			Serial.println("program excepiton,iwdg trigger reset cpu\r\n");
			//等待看门狗超时触发复位
			while(1){
				delay(1);
			}
		}
		
		g_run_cnt=0;
		
		i=0;
	}
	// HAL_IWDG_Refresh(&hiwdg);
	IWDG->KR = 0xAAAA;   // 相当于 HAL_IWDG_Refresh(&hiwdg);





	//长时间进料出错
	if (is_front)
	{ // 如果往前推
		front_time+=100;
		if (front_time > timeout)
		{ // 如果超时
			is_error = true;
		}
	}
}

void key1_it_callback(void){
	if(!digitalRead(KEY1)){//下降沿
		key1_press_times=millis();
		key1_press_cnt++;
		key1_press_flag=true;
	}
	else{//上升沿
		if(millis()-key1_press_times<=500){
			key1_release_flag=true;
			key1_release_times=millis();
		}
		else{
			key1_release_flag=false;
			key1_press_cnt=0;

		}
		key1_press_flag=false;
	}
}


void key2_it_callback(void){
	if(!digitalRead(KEY2)){//下降沿
		key2_press_times=millis();
		key2_press_cnt++;
		key2_press_flag=true;
	}
	else{//上升沿
		if(millis()-key2_press_times<=500){
			key2_release_flag=true;
			key2_release_times=millis();

		}
		else{
			key2_release_flag=false;
			key2_press_cnt=0;
		}
		key2_press_flag=false;
	}	
}

void Recv_MDM_Pulse_IT_Callback(void){
	blockage_detect.mdm_pulse_cnt++;
	// Serial.println("mdm_pulse_cnt:"+String(blockage_detect.mdm_pulse_cnt));
}

void Dir_IT_Callback(void){
	//修改定时器计数方向
	if(SIGNAL_COUNT_READ_DIR_IO())	SIGNAL_COUNT_UP();		//DIR高电平-配置为向上计数
	else 	SIGNAL_COUNT_DOWN();	//DIR低电平-配置为向下计数
}

// void Buffer_S3_IT_Callback(void){
// 	last_motor_state=motor_state;		//记录上一次状态
// 	motor_state=Back;
// 	is_front=false;
// 	front_time=0;
// }

// void Buffer_S2_IT_Callback(void){
// 	last_motor_state=motor_state;		//记录上一次状态
// 	motor_state=Stop;
// 	is_front=false;
// 	front_time=0;
// }

// void Buffer_S1_IT_Callback(void){
// 	last_motor_state=motor_state;		//记录上一次状态
// 	motor_state=Forward;
// 	is_front=true;	

// }



void buffer_debug(void){
	// Serial.print("buffer1_pos1_sensor_state:");Serial.println(buffer.buffer1_pos1_sensor_state);
	// Serial.print("buffer1_pos2_sensor_state:");Serial.println(buffer.buffer1_pos2_sensor_state);
	// Serial.print("buffer1_pos3_sensor_state:");Serial.println(buffer.buffer1_pos3_sensor_state);
	// Serial.print("buffer1_material_swtich_state:");Serial.println(buffer.buffer1_material_swtich_state);
	// Serial.print("key1:");Serial.println(buffer.key1);
	// Serial.print("key2:");Serial.println(buffer.key2);
	static int i=0;
	if(i<0x1ff){
		Serial.print("i:");
		Serial.println(i);
		driver.GCONF(i);
		driver.PWMCONF(i);
		i++;
	}
	uint32_t gconf = driver.GCONF();
	uint32_t chopconf=driver.CHOPCONF();
	uint32_t pwmconf = driver.PWMCONF();
	if(driver.CRCerror){
		Serial.println("CRCerror");
	}
	else{
		Serial.print("GCONF():0x");
		Serial.println(gconf,HEX);
		Serial.print("CHOPCONF():0x");
		char buf[11];  // "0x" + 8 digits + null terminator
		sprintf(buf, "%08lX", chopconf);  // %08lX -> 8位大写十六进制（long unsigned）
		Serial.println(buf);
		Serial.print("PWMCONF():0x");
		sprintf(buf, "%08lX", pwmconf);  // %08lX -> 8位大写十六进制（long unsigned）
		Serial.println(buf);
		Serial.println("");
	}
  	// delay(1000);
}


/**
  * @brief  usb串口接收解析
  * @param  null
  * @retval null
**/
void USB_Serial_Analys(void){

	static String serial_buf;
	if(Serial.available()){
		char c=Serial.read();
		if(c=='\n'){
			if(strstr(serial_buf.c_str(),"rt")){
				Serial.print("read timeout=");
				Serial.println(timeout);				
			}
			else if(strstr(serial_buf.c_str(),"timeout")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("Error: Invalid timeout value.");
				}
				serial_buf=serial_buf.substring(index+1);
				int64_t num=serial_buf.toInt();
				if(num<0||num>0xffffffff){
					serial_buf="";
					Serial.println("Error: Invalid timeout value.");
				}
				buffer_para.timeout=num;
				timeout=num;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				Serial.print("set timeout succeed! timeout=");
				Serial.println(timeout);

			}
			else if(strstr(serial_buf.c_str(),"steps")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("Error: Invalid steps value.");
				}
				serial_buf=serial_buf.substring(index+1);
				int64_t num=serial_buf.toInt();
				if(num<0||num>51200){
					serial_buf="";
					Serial.println("Error: Invalid steps value.");
				}
				buffer_para.steps=num;
				steps=num;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				Serial.print("set steps succeed! steps=");
				Serial.println(steps);
				if(connet_mdm_flag){
					REIN_TIM_SIGNAL_COUNT_DeInit();
					REIN_TIM_SIGNAL_COUNT_Init();
				}

			}		
			else if(strstr(serial_buf.c_str(),"clear")){
				//重新计数
				blockage_detect.actual_distance=0;
				blockage_detect.target_distance=0;
				blockage_detect.mdm_pulse_cnt=0;
				blockage_detect.pulse_cnt=0;				
			}
			else if(strstr(serial_buf.c_str(),"encoder")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("Error: Invalid encoder value.");
				}
				serial_buf=serial_buf.substring(index+1);
				// float num = serial_buf.toFloat();
				float num = fastAtof(serial_buf.c_str());
				if(num<0){
					serial_buf="";
					Serial.println("Error: Invalid encoder length value.");
				}
				buffer_para.encoder_length=num;
				encoder_length=num;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				blockage_detect.allow_error = encoder_length*allow_error_scale;
				Serial.print("set encoder length succeed! encoder_length=");
				Serial.println(encoder_length);
			}
			else if(strstr(serial_buf.c_str(),"info")){
				Serial.println("encoder_length="+String(encoder_length));
				Serial.println("timeout="+String(timeout));
				Serial.println("steps="+String(steps));
				Serial.println("allow_error_scale="+String(allow_error_scale));
				Serial.println("allow_error="+String(blockage_detect.allow_error));
				Serial.println("DUANLIAO_OUT_STATE="+String(buffer_para.DUANLIAO_OUT_STATE));
			}			
			else if(strstr(serial_buf.c_str(),"scale")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("Error: Invalid scale value.");
				}
				serial_buf=serial_buf.substring(index+1);
				// float num = serial_buf.toFloat();
				float num = fastAtof(serial_buf.c_str());
				if(num<0){
					serial_buf="";
					Serial.println("Error: Invalid scale length value.");
				}
				buffer_para.allow_error_scale=num;
				allow_error_scale=num;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				blockage_detect.allow_error = encoder_length*allow_error_scale;
				Serial.print("set scale length succeed! allow_error_scale=");
				Serial.println(allow_error_scale);

			}
			else if(strstr(serial_buf.c_str(),"speed")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("speed: "+String(SPEED));
					return ;
				}
				serial_buf=serial_buf.substring(index+1);
				// float num = serial_buf.toFloat();
				float num = fastAtof(serial_buf.c_str());
				if(num<0){
					serial_buf="";
					Serial.println("Error: Invalid speed  value.");
				}
				buffer_para.SPEED=num;
				SPEED=num;
				VACTRUAL_VALUE=(uint32_t)(SPEED*Move_Divide_NUM*200/60/0.715) ;  //VACTUAL寄存器值
				EEPROM.put(0, buffer_para);
				serial_buf="";
				Serial.print("set speed  succeed! speed=");
				Serial.println(SPEED);
			}			
			else if(strstr(serial_buf.c_str(),"I")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("I_CURRENT="+String(I_CURRENT));
					return ;
				}
				serial_buf=serial_buf.substring(index+1);
				// float num = serial_buf.toFloat();
				int32_t num = atoi(serial_buf.c_str());
				if(num<0||num>3000){
					serial_buf="";
					Serial.println("Error: Invalid I_CURRENT  value,range:0-3000mA");
					return ;
				}
				buffer_para.I_CURRENT=num;
				I_CURRENT=num;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				Serial.print("set I_CURRENT  succeed! I_CURRENT=");
				Serial.println(I_CURRENT);
			}

			else if(strstr(serial_buf.c_str(),"out")){
				int index=serial_buf.indexOf(" ");
				if(index==-1){
					serial_buf="";
					Serial.println("DUANLIAO_OUT_STATE="+String(buffer_para.DUANLIAO_OUT_STATE));
					return ;
				}
				serial_buf=serial_buf.substring(index+1);
				bool state = atoi(serial_buf.c_str());
				buffer_para.DUANLIAO_OUT_STATE=state;
				DUANLIAO_OUT_STATE=state;
				EEPROM.put(0, buffer_para);
				serial_buf="";
				Serial.print("set DUANLIAO_OUT_STATE  succeed! DUANLIAO_OUT_STATE=");
				Serial.println(DUANLIAO_OUT_STATE);
			}
			else if(strstr(serial_buf.c_str(),"version")){
				Serial.println("version: "+String(VERSION));
			}



			else{
				Serial.println(serial_buf.c_str());
				Serial.println("command error!");
				Serial.print("\n+---------------------------------------------+\n");
				Serial.print("|         Fly Buffer Command Set              |\n");
				Serial.print("|     set steps per mm: <steps nnn CRLF>      |\n");
				Serial.print("|     set encoder length: <encoder nnn CRLF>  |\n");
				Serial.print("|     set timeout : <timeout nnn CRLF>        |\n");
				Serial.print("|     read timeout: <rt CRLF>                 |\n");
				Serial.print("|     show all info : <info CRLF>             |\n");
				Serial.print("|     set scale: <scale nnn CRLF>             |\n");
				Serial.print("|     set speed(r/min): <speed nnn CRLF>      |\n");
				Serial.print("|     set I_CURRENT(mA): <I nnn CRLF>         |\n");
				Serial.print("|     endstop out: <out n>                    |\n");
				Serial.print("|     View version information: <version CRLF>|\n");
				Serial.print("+-----------------------------------------------+\n\n");
			}
			serial_buf="";

		}
		else  serial_buf+=c;
	}
}



/**
  * @brief  检测是否有连接MDM断堵料模块
  * @param  null
  * @retval true:有连接 false:无连接
**/
bool Check_Connet_MDM(void){

	//等待上电稳定
	delay(200);

	//读取MDM断料引脚状态
	pinMode(MDM_DPIN,INPUT);
	bool mdm_state=digitalRead(MDM_DPIN);
	// Serial.print("mdm_state:");
	// Serial.println(mdm_state);

	//配置为相反的电平拉取方向，再次读取电平，如果电平状态不变，则说明有连接，否则说明无连接
	if(mdm_state){//高电平,配置为下拉
		pinMode(MDM_DPIN,INPUT_PULLDOWN);
		Serial.println(digitalRead(MDM_DPIN));
		if(digitalRead(MDM_DPIN))	return true;//电平不变,说明有连接
		else 						return false;
	}
	else{
		pinMode(MDM_DPIN,INPUT_PULLUP);
		Serial.println(digitalRead(MDM_DPIN));
		if(digitalRead(MDM_DPIN))	return false;
		else 						return true;//电平不变,说明有连接
	}
}

/**
 * @brief  TIM_SIGNAL_PUL初始化
 * @param  NULL
 * @retval NULL
 **/
void REIN_TIM_SIGNAL_COUNT_Init(void)
{
	/* GPIO初始化 */
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	/* GPIO Ports Clock Enable*/
	SIGNAL_COUNT_PUL_CLK_ENABLE(); // 启用SIGNAL_COUNT_PUL端口时钟
	/*Configure GPIO pin*/
	GPIO_InitStruct.Pin = SIGNAL_COUNT_PUL_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // 输入模式
	GPIO_InitStruct.Pull = GPIO_NOPULL;		// 禁用上下拉
	GPIO_InitStruct.Alternate = GPIO_AF2_TIM2;
	HAL_GPIO_Init(SIGNAL_COUNT_PUL_GPIO_Port, &GPIO_InitStruct);

	/* TIM初始化 */
	TIM_SlaveConfigTypeDef sSlaveConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	SIGNAL_COUNT_TIM_CLK_ENABLE(); // 启用TIM时钟
	SIGNAL_COUNT_Get_HTIM.Instance = SIGNAL_COUNT_Get_TIM;
	SIGNAL_COUNT_Get_HTIM.Init.Prescaler = 0;									   // 预分频:0
	SIGNAL_COUNT_Get_HTIM.Init.CounterMode = TIM_COUNTERMODE_UP;				   // 向上计数
	SIGNAL_COUNT_Get_HTIM.Init.Period = 65536-1;									   // 计数周期
	SIGNAL_COUNT_Get_HTIM.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;			   // 不分频
	SIGNAL_COUNT_Get_HTIM.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE; // 禁用自动重新加载
	if (HAL_TIM_Base_Init(&SIGNAL_COUNT_Get_HTIM) != HAL_OK)
	{
		Error_Handler();
	}
	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;		   // 外部时钟模式
	sSlaveConfig.InputTrigger = TIM_TS_TI1FP1;				   // TI1FP1
	sSlaveConfig.TriggerPolarity = TIM_TRIGGERPOLARITY_RISING; // 上升沿触发
	sSlaveConfig.TriggerFilter = 4;							   // 滤波参数(FDIV2_N6)
	if (HAL_TIM_SlaveConfigSynchro(&SIGNAL_COUNT_Get_HTIM, &sSlaveConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;			 // 主机模式触发复位
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE; // 禁用主机模式
	if (HAL_TIMEx_MasterConfigSynchronization(&SIGNAL_COUNT_Get_HTIM, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/*begin work*/
	// HAL_TIM_Base_Start_IT(&SIGNAL_COUNT_Get_HTIM);
	HAL_TIM_Base_Start(&SIGNAL_COUNT_Get_HTIM);

}

/**
  * @brief  TIM_SIGNAL_COUNT清理
  * @param  NULL
  * @retval NULL
**/
void REIN_TIM_SIGNAL_COUNT_DeInit(void)
{
	// HAL_TIM_Base_Stop_IT(&SIGNAL_COUNT_Get_HTIM);										//停止TIM
	HAL_TIM_Base_Stop(&SIGNAL_COUNT_Get_HTIM);										//停止TIM
	HAL_GPIO_DeInit(SIGNAL_COUNT_PUL_GPIO_Port, SIGNAL_COUNT_PUL_Pin);	//重置GPIO
}

/**
  * @brief  GPIO初始化(SIGNAL_COUNT)
  * @param  NULL
  * @retval NULL
*/
void Signal_Dir_Init(void)
{
	pinMode(SIG_DIR_PIN,INPUT);
	attachInterrupt(SIG_DIR_PIN,&Dir_IT_Callback,CHANGE);
}


/**
  * @brief  脉冲接收初始化
  * @param  null
  * @retval null
**/
void Pulse_Receive_Init(void){

	// PULSE2_PIN初始化
	if(connet_mdm_flag){
		pinMode(PULSE2_PIN,INPUT);
		attachInterrupt(PULSE2_PIN,&Recv_MDM_Pulse_IT_Callback,RISING);
		REIN_TIM_SIGNAL_COUNT_Init();
	}

	blockage_detect.allow_error = encoder_length*allow_error_scale;
}


void Blockage_Detect(void){

	static uint32_t last_target_distance=blockage_detect.target_distance;
	static uint32_t last_timer_cnt=TIM2->CNT;
	static uint32_t last_time=0;

	//500ms内每没接收到脉冲，重新计数
	if(last_timer_cnt==TIM2->CNT){//CNT计数没变化，没有脉冲
		if(millis()-last_time>=500){
			blockage_detect.actual_distance=0;
			blockage_detect.target_distance=0;
			blockage_detect.mdm_pulse_cnt=0;
			blockage_detect.extrusion_pulse_cnt=0;
		}
	}
	else{//CNT计数有变化，有脉冲，记录时间戳
		last_timer_cnt=TIM2->CNT;
		last_time=millis();

		//计算挤出脉冲数
		blockage_detect.last_pulse_cnt=blockage_detect.pulse_cnt;
		blockage_detect.pulse_cnt=TIM2->CNT;
		blockage_detect.pulse_cnt_sub=blockage_detect.pulse_cnt-blockage_detect.last_pulse_cnt;
		blockage_detect.extrusion_pulse_cnt+=blockage_detect.pulse_cnt_sub;
		

		//只计算挤出的距离，若 extrusion_pulse_cnt 为负，则认为是回退，不计算
		if(blockage_detect.extrusion_pulse_cnt<0) blockage_detect.target_distance=0;
		else blockage_detect.target_distance=(blockage_detect.extrusion_pulse_cnt)/steps;

		// Serial.println("extrusion_pulse_cnt:"+String(blockage_detect.extrusion_pulse_cnt));
	}

	blockage_detect.actual_distance=blockage_detect.mdm_pulse_cnt*encoder_length;//实际距离
	blockage_detect.distance_error=blockage_detect.actual_distance-blockage_detect.target_distance;//距离差值




	static bool detect_blockage=false;
	static uint32_t detect_blockage_time=0;
	
	
	//连续检测到两次堵料，则认为是堵料，否则判断为误触
	if(!detect_blockage){//尚未检测到堵料
		if(blockage_detect.target_distance!=last_target_distance){
			// Serial.println("dir:"+String((bool)SIGNAL_COUNT_READ_DIR_IO()));

			// Serial.print("target_distance:"+String(blockage_detect.target_distance));
			// Serial.println("	actual_distance:"+String(blockage_detect.actual_distance));

			//堵料判断
			if(abs(blockage_detect.distance_error)>blockage_detect.allow_error&&blockage_detect.target_distance>=blockage_detect.allow_error){//检测得到堵料
	
				detect_blockage=true;
				detect_blockage_time=millis();

				// Serial.print("target_distance:"+String(blockage_detect.target_distance));
				// Serial.println("	actual_distance:"+String(blockage_detect.actual_distance));
				// Serial.println("detect over error:"+String(blockage_detect.distance_error));

				//重新计数
				blockage_detect.actual_distance=0;
				blockage_detect.target_distance=0;
				blockage_detect.mdm_pulse_cnt=0;
				blockage_detect.extrusion_pulse_cnt=0;
			}

			last_target_distance=blockage_detect.target_distance;
		}

	}
	else if(blockage_detect.target_distance>blockage_detect.allow_error){//已经检测到堵料，1s后再次检测，如果还是检测到堵料，堵料触发，否则认为是误触，清空检测标志位
		if(millis()-detect_blockage_time>=100){
			if(abs(blockage_detect.distance_error)>blockage_detect.allow_error){//检测得到堵料
				//堵料触发
				// Serial.println("blockage trigger");
				blockage_detect.blockage_flag=true;
				blockage_inform_times=millis();
				digitalWrite(DULIAO,LOW);			
				detect_blockage=false;
				detect_blockage_time=0;	

				//重新计数
				blockage_detect.actual_distance=0;
				blockage_detect.target_distance=0;
				blockage_detect.mdm_pulse_cnt=0;
				blockage_detect.extrusion_pulse_cnt=0;				
			}
			else{//没有检测到堵料，认为是误触
				//清空检测标志位
				detect_blockage=false;
				detect_blockage_time=0;
			}
		}

	}

}


float fastAtof(const char *s) {
    float val = 0.0f;
    int sign = 1;

    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }

    // 整数部分
    while (*s >= '0' && *s <= '9') {
        val = val * 10.0f + (*s - '0');
        s++;
    }

    // 小数部分
    if (*s == '.') {
        s++;
        float frac = 1.0f;
        while (*s >= '0' && *s <= '9') {
            frac *= 0.1f;
            val += (*s - '0') * frac;
            s++;
        }
    }

    return sign * val;
}