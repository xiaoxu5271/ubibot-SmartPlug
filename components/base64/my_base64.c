/*******************************************************************************
  * @file       Uart Application Task  
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/

/*-------------------------------- Includes ----------------------------------*/
#include "stdint.h"


const char base64char[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char padding_char = '=';

/*******************************************************************************
//  base64 encode
*******************************************************************************/
void base64_encode(char * sourcedata, uint8_t datalength, char * base64, uint8_t base64_len)
{
  int i=0, j=0; 
  unsigned char trans_index=0;
  
  for (i=0; i < datalength; i += 3)
  { 
    if((j+4) > base64_len)
    {
      break;
    }
    // Every three byte 
    // First base64 byte
    trans_index = ((sourcedata[i] >> 2) & 0x3f); 
    base64[j++] = base64char[(int)trans_index]; 
    
    // Second base64 byte 
    trans_index = ((sourcedata[i] << 4) & 0x30); 
    if (i + 1 < datalength)
    { 
      trans_index |= ((sourcedata[i + 1] >> 4) & 0x0f); 
      base64[j++] = base64char[(int)trans_index]; 
    }
    else
    { 
      base64[j++] = base64char[(int)trans_index]; 
      base64[j++] = padding_char; 
      base64[j++] = padding_char; 
      break;
    } 
    
    // Third  Fourth base64 byte  
    trans_index = ((sourcedata[i + 1] << 2) & 0x3c); 
    if (i + 2 < datalength)
    { 
      trans_index |= ((sourcedata[i + 2] >> 6) & 0x03); 
      base64[j++] = base64char[(int)trans_index];  // Third base64 byte 
      
      trans_index = sourcedata[i + 2] & 0x3f; 
      base64[j++] = base64char[(int)trans_index]; // Fourth base64 byte
    } 
    else
    { 
      base64[j++] = base64char[(int)trans_index]; 
      base64[j++] = padding_char; 
      break; 
    } 
  } 
  
  base64[j] = '\0';
}


/*******************************************************************************
                                      END         
*******************************************************************************/





















