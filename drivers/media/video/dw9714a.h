
#define GPIO_I2C4_SDA GPIO_PB(07)
#define GPIO_I2C4_SCK GPIO_PB(08)

#define GPIO_PA(n) 	(0*32 + n)
#define GPIO_PB(n) 	(1*32 + n)
#define GPIO_PC(n) 	(2*32 + n)
#define GPIO_PD(n) 	(3*32 + n)
#define GPIO_PE(n) 	(4*32 + n)
#define GPIO_PF(n) 	(5*32 + n)

#define TRUE true
#define FALSE false


#define dw9714a_SLAVE_ADDR  0x18
#define GPIO_I2C4_SDA  GPIO_PB(7)
#define GPIO_I2C4_SCL  GPIO_PB(8)

#define Delay2us                     udelay(10)
#define Delay10us                    udelay(20)

#define iic_set_sda_out_1               gpio_direction_output(GPIO_I2C4_SDA, 1)
#define iic_set_sda_out_0               gpio_direction_output(GPIO_I2C4_SDA, 0)
#define iic_set_sda_in                gpio_direction_input(GPIO_I2C4_SDA)

#define iic_sda_high                  __gpio_set_value(GPIO_I2C4_SDA, 1)
#define iic_sda_low                   __gpio_set_value(GPIO_I2C4_SDA, 0)
#define iic_sda_is_high               __gpio_get_value(GPIO_I2C4_SDA)

#define iic_set_scl_in                gpio_direction_input(GPIO_I2C4_SCL)
#define iic_set_scl_out_1               gpio_direction_output(GPIO_I2C4_SCL, 1)
#define iic_set_scl_out_0               gpio_direction_output(GPIO_I2C4_SCL, 0)
#define iic_scl_high                  __gpio_set_value(GPIO_I2C4_SCL, 1)
#define iic_scl_low                   __gpio_set_value(GPIO_I2C4_SCL, 0)

#define VCM_GPIO_I2C

extern int gpio_direction_input(unsigned gpio);
extern int gpio_direction_output(unsigned gpio, int value);
extern int gpio_request(unsigned gpio, const char *label);
extern int __gpio_set_value(unsigned gpio, int value);
extern int __gpio_get_value(unsigned gpio);



#ifdef VCM_GPIO_I2C
void dw9714a_IICInit(void)
{
  Delay10us;
  iic_set_sda_out_1;
  /* iic_sda_high;  */
  iic_set_scl_out_1;
	
  /* iic_scl_high;; */
	
  Delay10us;   
  Delay10us;  
  Delay10us;   
  Delay10us;  
  Delay10us;   
  //USDELAY(10)
	
  iic_sda_low;
  Delay10us;   
  Delay10us;  
  Delay10us;  
  iic_scl_low;
  Delay10us;   
  Delay10us;  
  Delay10us;   
  Delay10us;  
  Delay10us;   
}

void dw9714a_IICEnd(void)
{
  Delay10us;
  Delay10us;
  Delay10us;
	
  iic_set_sda_out_0;
  Delay10us;
  Delay10us;
  iic_scl_high;
  Delay10us; 
  iic_sda_high;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
  Delay10us;
}

void dw9714a_IICClk(void)
{

  Delay10us;
  iic_scl_high;
  Delay10us;
  iic_scl_low;
  /* Delay10us; */
}

int dw9714a_IICReadAck(void)
{
  int  ret=1;

  Delay10us;

  iic_set_sda_in;
  Delay10us;
  Delay10us;   

  iic_scl_high;
  Delay10us; 
  Delay10us;
  Delay10us; 
  Delay10us;

  if (!iic_sda_is_high)
    {
      ret = TRUE;
    }
  else
    {
      ret =  FALSE;
    }

  iic_scl_low;
  Delay10us;
  Delay10us;   
  Delay10us;
  Delay10us;
  Delay10us;   
  /* iic_set_sda_out_1; */
  Delay10us;
  Delay10us;   

  return ret;
}

/* int dw9714a_IICSendAck(void) */
/* { */
/*   int  ret=1; */

/*   Delay10us; */

/*   iic_set_sda_out; */
/*   Delay10us; */
/*   iic_sda_low; */

/*   iic_scl_high; */
/*   Delay10us;  */
/*   Delay10us; */
/*   Delay10us;    */
/*   Delay10us;   */


/*   iic_scl_low; */
/*   Delay10us; */
/*   iic_set_sda_in; */
/*   Delay10us; */

/*   Delay10us;    */
/*   Delay10us;   */
/*   Delay10us;    */
/*   return ret; */
/* } */

int dw9714a_IICWrite1Byte(uint8_t  reg )
{
  int  i;
  /* iic_set_sda_out_1; */

  for(i = 0; i < 8; i++)//reg
    {
	Delay10us;
      if((reg<<i) &0x80) {
	iic_sda_high;
      } else {
	iic_sda_low;
      }
      dw9714a_IICClk();  
    }
  iic_sda_low;
  /* dw9714a_IICReadAck(); */
  return 0;
}


/*---------------------------------------------------------
  Name  :  Tpg105_IICWrite
  Desc  :  
  Params:  
  Return:
  Author:  jason 
  Date  :  
  -----------------------------------------------------------*/

void dw9714a_IICWrite(uint16_t data)
{
  dw9714a_IICInit();
  uint8_t high = data >> 8;
  uint8_t low = data & 0xff;

  dw9714a_IICWrite1Byte(dw9714a_SLAVE_ADDR); //to write  slave address to me3204
  dw9714a_IICReadAck();
  /* dw9714a_IICWrite1Byte(reg); //to write adress  */
  
  iic_set_sda_out_1;

  dw9714a_IICWrite1Byte(high); //to write data

  dw9714a_IICReadAck();

  iic_set_sda_out_1;

  dw9714a_IICWrite1Byte(low); //to write data

  dw9714a_IICReadAck();

  dw9714a_IICEnd();
}

static void inline scl_out_low(void)
{	
  iic_set_scl_out_0;
  /* iic_scl_low; */
}

static void inline sda_out_low(void)
{
  iic_set_sda_out_0;
  /* iic_sda_low; */
}

static void i2c_send_ack(int ack_s)
{
  /* scl_out_low(); */
  if (ack_s)
      iic_set_sda_out_1;
  else
      iic_set_sda_out_0;

  Delay2us;
  iic_scl_high;
  Delay10us;   
  iic_scl_low;
  Delay10us;   
  /* iic_sda_high; */
  Delay10us;   
  /* iic_set_sda_in; */
}


static void dw9714a_IICReadByte(unsigned char *value)
{
  unsigned char data = 0;
  unsigned char i;

  /* scl_out_low(); */
  iic_set_sda_in;

  for (i=0; i < 8; ++i) {
    scl_out_low();
    /* iic_scl_high; */
    iic_set_scl_in;
    Delay10us;
    data = data << 1;
    if (iic_sda_is_high) {
      data |= 1;
    }
    /* iic_scl_low; */
    scl_out_low();
    Delay10us;
  }

  value[0] = data;
  /* dump_uint(data); */
}

void dw9714a_IICRead(unsigned short *value)
{
  dw9714a_IICInit();
  unsigned char *high = (unsigned char *)value;

  dw9714a_IICWrite1Byte(dw9714a_SLAVE_ADDR + 1); //to write  slave address to me3204
  /* printk("111111111111111111111111111111111\n"); */

  dw9714a_IICReadAck();

  /* dw9714a_IICWrite1Byte(reg); //to write adress  */
  
  high++;
  /* printk("2222222222222222222222222222222222\n"); */
  dw9714a_IICReadByte(high); //to write data

  i2c_send_ack(0);
  high--;
  /* printk("33333333333333333333333333333333333\n"); */

  dw9714a_IICReadByte(high); //to write data
  /* printk("44444444444444444444444444444444444\n"); */

  dw9714a_IICEnd();
}
#endif
