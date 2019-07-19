
#ifndef _SWITCH_H_
#define _SWITCH_H_

#define GPIO_RLY (GPIO_NUM_32)

void Switch_Init(void);
void Key_Switch_Relay(void);
void Mqtt_Switch_Relay(uint8_t set_value);

#endif
