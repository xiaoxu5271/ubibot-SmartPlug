
#ifndef _SWITCH_H_
#define _SWITCH_H_

#define GPIO_RLY (GPIO_NUM_32)
#define GPIO_CP (GPIO_NUM_12)

void Switch_Init(void);
void Switch_Relay(int8_t set_value);

#endif
