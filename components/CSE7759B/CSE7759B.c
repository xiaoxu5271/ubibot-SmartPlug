#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "Json_parse.h"
#include "CSE7759B.h"


#define UART1_TXD  (UART_PIN_NO_CHANGE)
#define UART1_RXD  (GPIO_NUM_26)
#define UART1_RTS  (UART_PIN_NO_CHANGE)
#define UART1_CTS  (UART_PIN_NO_CHANGE)

#define BUF_SIZE    100
//static const char *TAG = "CSE7759b";

#define SAMPLE_RESISTANCE_MR	1//使用的采样锰铜电阻mR值

#define UART_IND_HD			0
#define UART_IND_5A			1
#define UART_IND_VK			2
#define UART_IND_VT			5
#define UART_IND_IK			8
#define UART_IND_IT			11
#define UART_IND_PK			14
#define UART_IND_PT			17
#define UART_IND_FG			20
#define UART_IND_EN			21
#define UART_IND_SM			23

#define ARRAY_LEN   	3//平滑滤波buf长度
#define COUNT_NUM   	1//超时更新数据次数

//7759B电能计数脉冲溢出时的数据
#define ENERGY_FLOW_NUM			65536   //电量采集，电能溢出时的脉冲计数值

typedef struct RuningInf_s{
	unsigned short  voltage;//当前电压值，单位为0.1V
	unsigned short  electricity;//当前电流值,单位为0.01A
	unsigned short  power;//当前电流值,单位为0.01A
	
	unsigned long   energy;//当前消耗电能值对应的脉冲个数
	unsigned short  energyCurrent;//电能脉冲当前统计值
	unsigned short  energyLast;//电能脉冲上次统计值
	unsigned char   energyFlowFlag;//电能脉冲溢出标致
	
	unsigned long   energyUnit;//0.001度点对应的脉冲个数 
}RuningInf_t;

RuningInf_t runingInf;


//获取电压、电流、功率的有限数据
unsigned long getVIPvalue(unsigned long *arr)//更新电压、电流、功率的列表
{
	int maxIndex = 0;
	int minIndex = 0;
	//unsigned long sum = 0;
	int j = 0;
	for(j = 1; j<ARRAY_LEN; j++)
    {
		if(arr[maxIndex] <= arr[j])
        {//避免所有数据一样时minIndex等于maxIndex
			maxIndex = j;
		}
		if(arr[minIndex] > arr[j])
        {
			minIndex = j;
		}
	}
	
	for(j = 0; j<ARRAY_LEN; j++)
    {
		if((maxIndex == j)||(minIndex == j))
        {
			continue;
		}
		else
        {
			return arr[j];
		}
	}
    return arr[j];
}

int isUpdataNewData(unsigned long *arr,unsigned long dat)//检测电压电流功率是否需要更新
{
	if(arr[0] == dat)
    {
		return 0;
	}
	else
    {
		return 1;
	}
}

void updataVIPvalue(unsigned long *arr,unsigned long dat)//更新电压、电流、功率的列表
{
	int ii = ARRAY_LEN-1;
	for(ii = ARRAY_LEN-1; ii > 0; ii--)
    {
		arr[ii] = arr[ii-1];
	}
	arr[0] = dat;
}

void resetVIPvalue(unsigned long *arr,unsigned long dat)//更新所有电压、电流、功率的列表
{	
	int ii = ARRAY_LEN-1;
	for(ii = ARRAY_LEN-1; ii >= 0; ii--)
    {
		arr[ii] = dat;
	}
}

int DealUartInf(unsigned char *inDataBuffer,int recvlen)
{
	unsigned char     startFlag     = 0;
	
	unsigned long	  voltage_k	    = 0;
	unsigned long	  voltage_t     = 0;
	unsigned long	  voltage       = 0;
	static unsigned long voltage_a[ARRAY_LEN]  = {0};
	static unsigned int  voltageCnt	= 0;

	unsigned long	  electricity_k = 0;
	unsigned long	  electricity_t = 0;
	unsigned long	  electricity   = 0;
	static unsigned long electricity_a[ARRAY_LEN]  = {0};
	static unsigned int  electricityCnt	= 0;

	unsigned long	  power_k = 0;
	unsigned long	  power_t = 0;
	unsigned long	  power	  = 0;
	static unsigned long power_a[ARRAY_LEN]= {0};
	static unsigned int  powerCnt	= 0;
	static unsigned long powerNewFlag = 1;
	
	unsigned int	  energy_cnt     = 0;
	unsigned char	  energyFlowFlag = 0;
	unsigned long	  energy =0;//累计用电量

	startFlag = inDataBuffer[UART_IND_HD];
	switch(startFlag)
	{
		case 0x55:
			if((inDataBuffer[UART_IND_FG]&0x40) == 0x40){//获取当前电压标致，为1时说明电压检测OK
				voltage_k = ((inDataBuffer[UART_IND_VK] << 16)|(inDataBuffer[UART_IND_VK+1] << 8)|(inDataBuffer[UART_IND_VK+2]));//电压系数
				voltage_t = ((inDataBuffer[UART_IND_VT] << 16)|(inDataBuffer[UART_IND_VT+1] << 8)|(inDataBuffer[UART_IND_VT+2]));//电压周期
				
				if(isUpdataNewData(voltage_a,voltage_t)){
					updataVIPvalue(voltage_a,voltage_t);
					voltageCnt = 0;
				}
				else{
					voltageCnt++;
					if(voltageCnt >= COUNT_NUM){
						voltageCnt = 0;
						updataVIPvalue(voltage_a,voltage_t);
					}
				}
				printf("voltage:%ld,%ld,%ld\r\n",voltage_a[0],voltage_a[1],voltage_a[2]);
				voltage_t = getVIPvalue(voltage_a);
				
				if(voltage_t == 0){
					voltage = 0;
				}
				else{
					voltage = voltage_k * 100 / voltage_t;//电压10mV值，避免溢出
					voltage = voltage * 10;//电压mV值
				}
				printf("11Vk = %ld,Vt = %ld,v = %ld\r\n",voltage_k,voltage_t,voltage);
				mqtt_json_s.mqtt_Voltage=voltage/1000;
			}
			else{
				printf("%s(%d):V Flag Error\r\n",__func__,__LINE__);
			}
			
			if((inDataBuffer[UART_IND_FG]&0x20) == 0x20){
				electricity_k = ((inDataBuffer[UART_IND_IK] << 16)|(inDataBuffer[UART_IND_IK+1] << 8)|(inDataBuffer[UART_IND_IK+2]));//电流系数
				electricity_t = ((inDataBuffer[UART_IND_IT] << 16)|(inDataBuffer[UART_IND_IT+1] << 8)|(inDataBuffer[UART_IND_IT+2]));//电流周期

				if(isUpdataNewData(electricity_a,electricity_t)){
					updataVIPvalue(electricity_a,electricity_t);
					electricityCnt = 0;
				}
				else{
					electricityCnt++;
					if(electricityCnt >= COUNT_NUM){
						electricityCnt = 0;
						updataVIPvalue(electricity_a,electricity_t);
					}
				}
				printf("electricity:%ld,%ld,%ld\r\n",electricity_a[0],electricity_a[1],electricity_a[2]);
				electricity_t = getVIPvalue(electricity_a);
				
				if(electricity_t == 0){
					electricity = 0;
				}
				else{
					electricity = electricity_k * 100 / electricity_t;//电流10mA值，避免溢出
					electricity = electricity * 10;//电流mA值
					#if(SAMPLE_RESISTANCE_MR == 1)
					//由于使用1mR的电阻，电流和功率需要不除以2
					#elif(SAMPLE_RESISTANCE_MR == 2)
					//由于使用2mR的电阻，电流和功率需要除以2
					electricity >>= 1;
					#endif
				}
				printf("11Ik = %ld,It = %ld,I = %ld\r\n",electricity_k,electricity_t,electricity);
				mqtt_json_s.mqtt_Current=electricity/1000.0;
			}
			else{
				printf("%s(%d):I Flag Error\r\n",__func__,__LINE__);
			}

			if((inDataBuffer[UART_IND_FG]&0x10) == 0x10){
				powerNewFlag = 0;
				power_k = ((inDataBuffer[UART_IND_PK] << 16)|(inDataBuffer[UART_IND_PK+1] << 8)|(inDataBuffer[UART_IND_PK+2]));//功率系数
				power_t = ((inDataBuffer[UART_IND_PT] << 16)|(inDataBuffer[UART_IND_PT+1] << 8)|(inDataBuffer[UART_IND_PT+2]));//功率周期
				
				if(isUpdataNewData(power_a,power_t)){
					updataVIPvalue(power_a,power_t);
					powerCnt = 0;
				}
				else{
					powerCnt++;
					if(powerCnt >= COUNT_NUM){
						powerCnt = 0;
						updataVIPvalue(power_a,power_t);
					}
				}
				printf("power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
				power_t = getVIPvalue(power_a);
				
				if(power_t == 0){
					power = 0;
				}
				else{
					power = power_k * 100 / power_t;//功率10mw值，避免溢出
					power = power * 10;//功率mw值
					#if(SAMPLE_RESISTANCE_MR == 1)
					//由于使用1mR的电阻，电流和功率需要不除以2
					#elif(SAMPLE_RESISTANCE_MR == 2)
					//由于使用2mR的电阻，电流和功率需要除以2
					power >>= 1;
					#endif
				}
				printf("11Pk = %ld,Pt = %ld,P = %ld\r\n",power_k,power_t,power);
				mqtt_json_s.mqtt_Power=power/1000.0;
			}
			else if(powerNewFlag == 0){
				power_k = ((inDataBuffer[UART_IND_PK] << 16)|(inDataBuffer[UART_IND_PK+1] << 8)|(inDataBuffer[UART_IND_PK+2]));//功率系数
				power_t = ((inDataBuffer[UART_IND_PT] << 16)|(inDataBuffer[UART_IND_PT+1] << 8)|(inDataBuffer[UART_IND_PT+2]));//功率周期
				
				if(isUpdataNewData(power_a,power_t)){
					unsigned long powerData = getVIPvalue(power_a);
					if(power_t > powerData){
						if((power_t - powerData) > (powerData >> 2)){
							resetVIPvalue(power_a,power_t);
						}
					}
				}
				printf("power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
				power_t = getVIPvalue(power_a);
				
				if(power_t == 0){
					power = 0;
				}
				else{
					power = power_k * 100 / power_t;//功率10mw值，避免溢出
					power = power * 10;//功率mw值
					#if(SAMPLE_RESISTANCE_MR == 1)
					//由于使用1mR的电阻，电流和功率需要不除以2
					#elif(SAMPLE_RESISTANCE_MR == 2)
					//由于使用2mR的电阻，电流和功率需要除以2
					power >>= 1;
					#endif
				}
				printf("22Pk = %ld,Pt = %ld,P = %ld\r\n",power_k,power_t,power);
            	mqtt_json_s.mqtt_Power=power/1000.0;
			}
			
			energyFlowFlag = (inDataBuffer[UART_IND_FG] >> 7);//获取当前电能计数溢出标致
			runingInf.energyCurrent = ((inDataBuffer[UART_IND_EN] << 8)|(inDataBuffer[UART_IND_EN+1]));//更新当前的脉冲计数值
			if(runingInf.energyFlowFlag != energyFlowFlag){//每次计数溢出时更新当前脉冲计数值
				runingInf.energyFlowFlag = energyFlowFlag;
				if(runingInf.energyCurrent > runingInf.energyLast){
					runingInf.energyCurrent = 0;
				}
				energy_cnt = runingInf.energyCurrent + ENERGY_FLOW_NUM - runingInf.energyLast;
			}
			else{
				energy_cnt = runingInf.energyCurrent - runingInf.energyLast;
			}
			runingInf.energyLast = runingInf.energyCurrent;
			runingInf.energy += (energy_cnt * 10);//电能个数累加时扩大10倍，计算电能是除数扩大10倍，保证计算精度
			
			runingInf.energyUnit = 0xD693A400 >> 1;
			runingInf.energyUnit /= (power_k >> 1);//1mR采样电阻0.001度电对应的脉冲个数
			#if(SAMPLE_RESISTANCE_MR == 1)
			//1mR锰铜电阻对应的脉冲个数
			#elif(SAMPLE_RESISTANCE_MR == 2)
			//2mR锰铜电阻对应的脉冲个数
			runingInf.energyUnit = (runingInf.energyUnit << 1);//2mR采样电阻0.001度电对应的脉冲个数
			#endif
			runingInf.energyUnit =runingInf.energyUnit * 10;//0.001度电对应的脉冲个数(计算个数时放大了10倍，所以在这里也要放大10倍)
			
			//电能使用量
			energy=runingInf.energy/runingInf.energyUnit;//单位是0.001度
			mqtt_json_s.mqtt_Energy=energy/1000.0; //单位是度
			printf("energy=%ld\n",energy);
			break;
		
		case 0xAA:
			//芯片未校准
			printf("CSE7759B not check\r\n");
			break;

		default :
			if((startFlag & 0xF1) == 0xF1){//存储区异常，芯片坏了
				//芯片坏掉，反馈服务器
				printf("CSE7759B broken\r\n");
			}

			if((startFlag & 0xF2) == 0xF2){//功率异常
				runingInf.power = 0;//获取到的功率是以0.1W为单位
				power = 0;
				//printf("Power Error\r\n");
			}
			else{
				if((inDataBuffer[UART_IND_FG]&0x10) == 0x10){
					powerNewFlag = 0;
					power_k = ((inDataBuffer[UART_IND_PK] << 16)|(inDataBuffer[UART_IND_PK+1] << 8)|(inDataBuffer[UART_IND_PK+2]));//功率系数
					power_t = ((inDataBuffer[UART_IND_PT] << 16)|(inDataBuffer[UART_IND_PT+1] << 8)|(inDataBuffer[UART_IND_PT+2]));//功率周期
					
					if(isUpdataNewData(power_a,power_t)){
						updataVIPvalue(power_a,power_t);
						powerCnt = 0;
					}
					else{
						powerCnt++;
						if(powerCnt >= COUNT_NUM){
							powerCnt = 0;
							updataVIPvalue(power_a,power_t);
						}
					}
					//printf("power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
					power_t = getVIPvalue(power_a);
					
					if(power_t == 0){
						power = 0;
					}
					else{
						power = power_k * 100 / power_t;//功率10mw值，避免溢出
						power = power * 10;//功率mw值
						#if(SAMPLE_RESISTANCE_MR == 1)
						//由于使用1mR的电阻，电流和功率需要不除以2
						#elif(SAMPLE_RESISTANCE_MR == 2)
						//由于使用2mR的电阻，电流和功率需要除以2
						power >>= 1;
						#endif
					}
					printf("33Pk = %ld,Pt = %ld,P = %ld\r\n",power_k,power_t,power);
					mqtt_json_s.mqtt_Power=power/1000.0;
				}
				else if(powerNewFlag == 0){
					power_k = ((inDataBuffer[UART_IND_PK] << 16)|(inDataBuffer[UART_IND_PK+1] << 8)|(inDataBuffer[UART_IND_PK+2]));//功率系数
					power_t = ((inDataBuffer[UART_IND_PT] << 16)|(inDataBuffer[UART_IND_PT+1] << 8)|(inDataBuffer[UART_IND_PT+2]));//功率周期
					
					if(isUpdataNewData(power_a,power_t)){
						unsigned long powerData = getVIPvalue(power_a);
						if(power_t > powerData){
							if((power_t - powerData) > (powerData >> 2)){
								resetVIPvalue(power_a,power_t);
							}
						}
					}
					//printf("power:%ld,%ld,%ld\r\n",power_a[0],power_a[1],power_a[2]);
					power_t = getVIPvalue(power_a);
					
					if(power_t == 0){
						power = 0;
					}
					else{
						power = power_k * 100 / power_t;//功率10mw值，避免溢出
						power = power * 10;//功率mw值
						#if(SAMPLE_RESISTANCE_MR == 1)
						//由于使用1mR的电阻，电流和功率需要不除以2
						#elif(SAMPLE_RESISTANCE_MR == 2)
						//由于使用2mR的电阻，电流和功率需要除以2
						power >>= 1;
						#endif
					}
					printf("44Pk = %ld,Pt = %ld,P = %ld\r\n",power_k,power_t,power);
					mqtt_json_s.mqtt_Power=power/1000.0;
				}
			}

			if((startFlag & 0xF4) == 0xF4){//电流异常
				runingInf.electricity = 0;//获取到的电流以0.01A为单位				
				electricity = 0;
			}
			else{
				if((inDataBuffer[UART_IND_FG]&0x20) == 0x20){
					electricity_k = ((inDataBuffer[UART_IND_IK] << 16)|(inDataBuffer[UART_IND_IK+1] << 8)|(inDataBuffer[UART_IND_IK+2]));//电流系数
					electricity_t = ((inDataBuffer[UART_IND_IT] << 16)|(inDataBuffer[UART_IND_IT+1] << 8)|(inDataBuffer[UART_IND_IT+2]));//电流周期

					if(isUpdataNewData(electricity_a,electricity_t)){
						updataVIPvalue(electricity_a,electricity_t);
						electricityCnt = 0;
					}
					else{
						electricityCnt++;
						if(electricityCnt >= COUNT_NUM){
							electricityCnt = 0;
							updataVIPvalue(electricity_a,electricity_t);
						}
					}
					//printf("electricity:%ld,%ld,%ld\r\n",electricity_a[0],electricity_a[1],electricity_a[2]);
					electricity_t = getVIPvalue(electricity_a);
					
					if(electricity_t == 0){
						electricity = 0;
					}
					else{
						electricity = electricity_k * 100 / electricity_t;//电流10mA值，避免溢出
						electricity = electricity * 10;//电流mA值
						#if(SAMPLE_RESISTANCE_MR == 1)
						//由于使用1mR的电阻，电流和功率需要不除以2
						#elif(SAMPLE_RESISTANCE_MR == 2)
						//由于使用2mR的电阻，电流和功率需要除以2
						electricity >>= 1;
						#endif
					}
					printf("22Ik = %ld,It = %ld,I = %ld\r\n",electricity_k,electricity_t,electricity);
				}
				else{
					printf("%s(%d):I Flag Error\r\n",__func__,__LINE__);
				}
			}
			
			if((startFlag & 0xF8) == 0xF8){//电压异常
				runingInf.voltage = 0;//获取到的电压是以0.1V为单位				
				voltage = 0;
			}
			else{
				if((inDataBuffer[UART_IND_FG]&0x40) == 0x40){//获取当前电压标致，为1时说明电压检测OK
					voltage_k = ((inDataBuffer[UART_IND_VK] << 16)|(inDataBuffer[UART_IND_VK+1] << 8)|(inDataBuffer[UART_IND_VK+2]));//电压系数
					voltage_t = ((inDataBuffer[UART_IND_VT] << 16)|(inDataBuffer[UART_IND_VT+1] << 8)|(inDataBuffer[UART_IND_VT+2]));//电压周期
					
					if(isUpdataNewData(voltage_a,voltage_t)){
						updataVIPvalue(voltage_a,voltage_t);
						voltageCnt = 0;
					}
					else{
						voltageCnt++;
						if(voltageCnt >= COUNT_NUM){
							voltageCnt = 0;
							updataVIPvalue(voltage_a,voltage_t);
						}
					}
					//printf("voltage:%ld,%ld,%ld\r\n",voltage_a[0],voltage_a[1],voltage_a[2]);
					voltage_t = getVIPvalue(voltage_a);
					
					if(voltage_t == 0){
						voltage = 0;
					}
					else{
						voltage = voltage_k * 100 / voltage_t;//电压10mV值，避免溢出
						voltage = voltage * 10;//电压mV值
					}
					printf("22Vk = %ld,Vt = %ld,v = %ld\r\n",voltage_k,voltage_t,voltage);
				}
				else{
					printf("%s(%d):V Flag Error\r\n",__func__,__LINE__);
				} 
			}
			printf("0x%x:V = %ld;I = %ld;P = %ld;\r\n",startFlag,voltage,electricity,power);
            mqtt_json_s.mqtt_Voltage=voltage/1000;
            mqtt_json_s.mqtt_Current=0;
            mqtt_json_s.mqtt_Power=power/1000.0;

			break;
	}
	return 1;
}


void CSE7759B_Task(void *pvParameters)
{
    while(1)
    {
        CSE7759B_Read();
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


void CSE7759B_Init(void)
{
    //配置GPIO
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = 1 << UART1_RXD;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    
    uart_config_t uart_config = {
        .baud_rate = 4800,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART1_TXD, UART1_RXD, UART1_RTS, UART1_CTS);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);
    
    xTaskCreate(CSE7759B_Task, "CSE7759B_Task", 2048, NULL, 5, NULL);
}


int8_t CSE7759B_Read(void)
{
    uint8_t data_u1[BUF_SIZE];
    uint8_t data_7759b[24];

    int len_7759_start=0;
 
    int len1 = uart_read_bytes(UART_NUM_1, data_u1, BUF_SIZE, 1 / portTICK_RATE_MS);
    if(len1!=0)  //读取到数据
    {
        //查找数据头
        for(int i=0;i<len1;i++)
        {
            if((data_u1[i]==0x5a)&&(((data_u1[i-1]&0xf0)==0xf0)||(data_u1[i-1]==0xaa)||(data_u1[i-1]==0x55)))
            {
                len_7759_start=i-1;
                break;
            }
        }

        //取得24byte计量数据
        for(int i=0;i<24;i++)
        {
            data_7759b[i]=data_u1[len_7759_start+i];
        }        


        printf("7759b=");
        for(int i=0;i<24;i++)
        {
            printf("0x%02x ",data_7759b[i]);

        }
        printf("\n");  
        DealUartInf(data_7759b,24);//处理7759B数据            
    }
    len1=0;
    len_7759_start=0;
    bzero(data_u1,sizeof(data_u1));
    bzero(data_7759b,sizeof(data_7759b));
    return   1;       
}





