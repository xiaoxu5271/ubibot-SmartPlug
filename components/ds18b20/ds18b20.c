#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "esp_timer.h"
#include "rom/ets_sys.h"
#include "ds18b20.h"


#define ds18b20_gpio            GPIO_NUM_4 

#define DATA_IO_ON()            gpio_set_level(ds18b20_gpio, 1)
#define DATA_IO_OFF()           gpio_set_level(ds18b20_gpio, 0)

#define ds18b20_io_in()         gpio_set_direction(ds18b20_gpio, GPIO_MODE_INPUT)
#define ds18b20_io_out()        gpio_set_direction(ds18b20_gpio, GPIO_MODE_OUTPUT)


/*******************************************************************************
//ds18b20 reset,return:0 reset success,return:-1,failured
*******************************************************************************/
static short ds18b20_reset(void)
{
  uint8_t retry=0;
  
  ds18b20_io_out();
  
  DATA_IO_ON();
  
  ets_delay_us(6);  //delay about 6 us
  
  DATA_IO_OFF();
  
  ets_delay_us(750);  //delay about 750 us
  
  DATA_IO_ON();
  
  ets_delay_us(15);  //delay about 15 us
  
  ds18b20_io_in();
  
  while( gpio_get_level(ds18b20_gpio))  //waite ds18b20 respon

  {
    retry++;
    
    if(retry>200)
    {
      return -1;
    }
    
    ets_delay_us(3);  //delay about 3 us
  }
  
  ets_delay_us(480);  //delay about 480 us
  
  ds18b20_io_out();  //data pin out mode
  
  DATA_IO_ON();
  
  return 0;
}



/*******************************************************************************
//ds18b20 read a bit,return 0/1 
*******************************************************************************/
static uint8_t ds18b20_read_bit(void)
{
  uint8_t data;
  
  ds18b20_io_out();
  
  DATA_IO_ON();
  
  ets_delay_us(2);  //delay about 1.5 us
  
  DATA_IO_OFF();
  
  ets_delay_us(3);  //delay about 3 us
  
  DATA_IO_ON();
  
  ds18b20_io_in();
  
  ets_delay_us(15);  //delay about 15 us
  
  if(  gpio_get_level(ds18b20_gpio))
  {
    data=1;
  }
  else
  {
    data=0;
  }
  
  ets_delay_us(60);  //delay about 60 us
  
  return data;
}



/*******************************************************************************
//ds18b20 read a byte
*******************************************************************************/
static uint8_t ds18b20_read_byte(void)
{
  uint8_t i,j,data=0;
  
  for(i=0;i<8;i++)
  {
    j=ds18b20_read_bit();
    
    data=(j<<7)|(data>>1);
  }
  
  return data;
}



/*******************************************************************************
//ds18b20 write a byte
*******************************************************************************/
static void ds18b20_write_byte(uint8_t data)
{
  uint8_t i,data_bit;
  
  ds18b20_io_out();  //data pin out mode
  
  for(i=0;i<8;i++)
  {
    data_bit=data&0x01;
    
    if(data_bit)
    {
      DATA_IO_OFF();
  
      ets_delay_us(3);  //delay about 3 us
      
      DATA_IO_ON();
      
      ets_delay_us(60);  //delay about 60 us
    }
    else
    {
      DATA_IO_OFF();
  
      ets_delay_us(60);  //delay about 60 us
      
      DATA_IO_ON();
      
      ets_delay_us(3);  //delay about 3 us
    }
    data=data>>1;
  }
}



/*******************************************************************************
//ds18b20 start convert
*******************************************************************************/
static void ds18b20_start(void)
{

  
  if(ds18b20_reset()==0)
  {

    ds18b20_write_byte(0xcc);   //skip rom
    
    ds18b20_write_byte(0x44);   //start convert
    
    DATA_IO_ON();
  }
  
 
      
  ets_delay_us(750*1000);  //delay about 750ms
      

}

/*******************************************************************************
//ds18b20 get temperature value
*******************************************************************************/
void ds18b20_get_temp(float *temp_value1)
{
  short temp;
  uint8_t data_h,data_l;
  
  *temp_value1 = 0;
  
  
  ds18b20_start();  //ds18b20 start convert
  
  if(ds18b20_reset()==0)
  {
    ds18b20_write_byte(0xcc);   //skip rom
    
    ds18b20_write_byte(0xbe);   //read data
    
    data_l=ds18b20_read_byte();  //LSB
    
    data_h=ds18b20_read_byte();  //MSB
    
    if(data_h>7)  //temperature value<0
    {
      data_l=~data_l;
      data_h=~data_h;
      
      temp=data_h;
      temp<<=8;
      temp+=data_l;
      
      *temp_value1=(float)-temp*0.0625;
      printf("temp11=%f\n",(float)temp*0.0625);
    }
    else //temperature value>=0
    {
      temp=data_h;
      temp<<=8;
      temp+=data_l;
      
      *temp_value1=(float)temp*0.0625;

      printf("temp1=%f\n",(float)temp*0.0625);
    }
  }
  else
  {
      printf("eeeeee\n");
    
  }
  
  
}


/*******************************************************************************
                                      END         
*******************************************************************************/




