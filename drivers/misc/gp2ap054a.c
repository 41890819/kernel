#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/input/gp2ap054a.h>
#include <linux/earlysuspend.h>
#include "gp2ap054a_lib.h"

struct gp2ap_data
{
	struct mutex			  mutex ;
	struct i2c_client		  *client ;
	
	u8				  regData[20] ;

/* GS & PS */
	struct input_dev		  *input_dev ;
	int				  mode ;
	int				  gs_enabled ;
	struct work_struct		  gs_polling_work ; 
	struct hrtimer			  gs_polling_timer ;
	int				  gs_delay ;
	ktime_t				  gs_polling_delay ;

	int				  ps_gpio ;
	int				  ps_irq ;
	struct work_struct		  ps_int_work ;
	int				  ps_distance ;

/* ALS */
	struct input_dev	   	  *als_input_dev ;
	struct delayed_work		  als_work ; 
	int				  als_enabled ;
	int				  als_mode ;
	int				  als_delay ;
} ;

static struct gp2ap_data *gngp2ap = NULL;
#define DEVICE_NAME 			"GP2AP054A"
#define GESTURE_SENSOR_NAME		"gesture_sensor"
#define LIGHT_SENSOR_NAME		"light_sensor"

#define SENSOR_MODE_OFF			 ( 0 )		/* Sensor OFF */
#define SENSOR_MODE_GESTURE		 ( 1 )		/* Gesture mode */
#define SENSOR_MODE_PROXIMITY		 ( 2 )		/* Proximity mode */
#define SENSOR_MODE_PULSE_COUNTER        ( 3 )		/* Pulse Counter */

#define SENSOR_DEFAULT_DELAY		 ( 200 )		/* 10 ms unit=1/10ms(100us) */
#define SENSOR_MIN_DELAY		 (  30 )		/*  3 ms unit=1/10ms(100us) */
#define SENSOR_MAX_DELAY		 ( 200 )		/* 20 ms unit=1/10ms(100us) */

/* * * Ambient Light Sensor * * */
#define SENSOR_DEFAULT_DELAY_ALS	 ( 200 )		/* 200 ms */
#define SENSOR_MAX_DELAY_ALS		 ( 5000 )	/* 5000 ms */

#define LOW_LUX_MODE			 ( 0 )
#define	MIDDLE_LUX_MODE			 ( 1 )
#define	HIGH_LUX_MODE			 ( 2 )

#define LOW_LUX_RANGE	 		 ( ALS1_RANGEX1 )
#define MIDDLE_LUX_RANGE 		 ( ALS1_RANGEX16 )
#define HIGH_LUX_RANGE	 		 ( ALS1_RANGEX512 )

#define ALS_L_to_M_counts		 35000
#define ALS_M_to_H_counts		 35000

#define ALS_H_to_M_counts		 900
#define ALS_M_to_L_counts		 1800

/* Reflective cancellation */ 
#ifdef WITH_PANEL_GS
#define REFLECTIVE_CANCEL_GS		 0x00
#define REFLECTIVE_CANCEL_PULSE		 0x2D
#else
#define REFLECTIVE_CANCEL_GS		 0x00
#define REFLECTIVE_CANCEL_PULSE		 0x2D
#endif

/* PS Threshold */
/* HTH:1500(0x05DC), LTH:1000(0x03E8) */
static u8 gp2ap_ps_th[4] = {
	/* Reg.4 INT_LT[7:0]:0xE8 */
	0xE8,
	/* Reg.5 INT_LT[15:8]:0x03 */
	0x03,
	/* Reg.6 INT_HT[7:0]:0xDC */
	0xDC,
	/* Reg.7 INT_HT[15:8]:0x05 */
	0x05
} ;

static u8 gp2ap_ini_data[20] = {
	/* COMMAND1(00H) */
	COMMAND1_SD,
	/* COMMAND2(01H) */
	0x00,
	/* COMMAND3(02H) */
	(COMMAND3_INT_PROX),
	/* ALS1(03H) */
	(ALS1_RES16 | MIDDLE_LUX_RANGE | 0x40) ,
	/* ALS2(04H) */
	(ALS2_ALS_INTVAL1P56),
	/* PS1(05H) */
	(PS1_PRST4 | PS1_RANGEX2 | PS1_RES12),
	/* PS2(06H) */
	(PS2_IS128 | PS2_SUM16 | 0x01),
	/* PS3(07H) */
	(0xC8 | PS3_GS_INTVAL6P25),
	/* PS_LT_LSB(08H) */
	0x00,
	/* PS_LT_MSB(09H) */
	0x00,
	/* PS_HT_LSB(0AH) */
	0xFF,
	/* PS_HT_MSB(0BH) */
	0xFF,
	/* OS_DATA0_LSB(0CH) */
	0x00,
	/* OS_DATA0_MSB(0DH) */
	0x00,
	/* OS_DATA1_LSB(0EH) */
	0x00,
	/* OS_DATA1_MSB(0FH) */
	0x00,
	/* OS_DATA2_LSB(10H) */
	0x00,
	/* OS_DATA2_MSB(11H) */
	0x00,
	/* OS_DATA3_LSB(12H) */
	0x00,
	/* OS_DATA3_MSB(13H) */
	0x00
} ;

/* *****************************************************************************
		I2C FUNCTION
***************************************************************************** */
static int gp2ap_i2c_read( u8 reg, unsigned char *rbuf, int len, struct i2c_client *client )
{

  int err = -1 ;
  struct i2c_msg i2cMsg[2] ;
  uint8_t buff ;

  if( client == NULL ){
    return -ENODEV ;
  }

  i2cMsg[0].addr = client->addr ;
  i2cMsg[0].flags = 0 ;
  i2cMsg[0].len = 1 ;
  i2cMsg[0].buf = &buff ;
  buff = reg ;
  i2cMsg[1].addr = client->addr ;
  i2cMsg[1].flags = I2C_M_RD ;
  i2cMsg[1].len = len ;
  i2cMsg[1].buf = rbuf ;

  err = i2c_transfer( client->adapter, &i2cMsg[0], 2 ) ;
  if( err < 0 ){
    printk( KERN_ERR "gp2ap054a i2c transfer error(%d)!!\n", err ) ;
  }

  return err ;
}

static int gp2ap_i2c_write( u8 reg, u8 *wbuf, struct i2c_client *client )
{
  int					err = 0 ;
  struct i2c_msg		i2cMsg ;
  unsigned char		buff[2] ;
  int					retry = 10 ;

  if( client == NULL ){
    return -ENODEV ;
  }

  while( retry-- ){
    buff[0] = reg ;
    buff[1] = *wbuf ;

    i2cMsg.addr = client->addr ;
    i2cMsg.flags = 0 ;
    i2cMsg.len = 2 ;
    i2cMsg.buf = buff ;

    err = i2c_transfer( client->adapter, &i2cMsg, 1 ) ;
    //pr_debug( "gp2ap054a_i2c_write : 0x%x, 0x%x\n", reg, *wbuf ) ;

    if( err >= 0 ){
      return 0 ;
    }
  }
  printk( KERN_ERR "gp2ap054a i2c transfer error(%d)!!\n", err ) ;

  return err ;
}

static enum hrtimer_restart gp2ap_gs_polling_timer_func( struct hrtimer *timer ){
  struct gp2ap_data  *data = container_of( timer, struct gp2ap_data, gs_polling_timer ) ;

  if( data != NULL ){
    schedule_work( &data->gs_polling_work ) ;
    hrtimer_forward_now( &data->gs_polling_timer, data->gs_polling_delay ) ;
  }

  return HRTIMER_RESTART ;
}

static int resume_flag = 0;
static void updater_sensor(struct gp2ap_data *data, int x , int y, int z)
{
  struct input_dev *input = data->input_dev;

  if (resume_flag == 1){
    resume_flag = 0;
    input_report_rel(input, REL_X, x);
    input_report_rel(input, REL_Y, y);
    input_report_rel(input, REL_Z, 17);
    input_sync(input);
  }

  input_report_rel(input, REL_X, x);
  input_report_rel(input, REL_Y, y);
  input_report_rel(input, REL_Z, z);
  input_sync(input);

  return ;
}

/*******************************************************************************
		GS DATA POLLING
*******************************************************************************/
static void gp2ap_gs_data_polling( struct work_struct *work )
{
  struct gp2ap_data  *data = container_of( work, struct gp2ap_data, gs_polling_work ) ;
  int					adc_data[4] ;
  u8					rdata[8] ;
  int					i ;
  int udata[2] = { 0, 0 };

  if( data != NULL ){
    mutex_lock( &data->mutex ) ;

    gp2ap_i2c_read( REG_D0_LSB, rdata, sizeof( rdata ), data->client ) ;

    mutex_unlock( &data->mutex ) ;

    for( i = 0 ; i < 4 ; i++ ){
      adc_data[i] = ( ( rdata[i*2+1] << 8 ) | rdata[i*2] ) ;
    }
		
    udata[0] = (adc_data[0] + (adc_data[1] << 16));
    udata[1] = (adc_data[2] + (adc_data[3] << 16));
    //printk("gs poll update %x %x\n", udata[0], udata[1]);
    updater_sensor(data, udata[0], udata[1], 0x10);
  }
}

/* on/off function */
static int als_onoff_simplified( u8 onoff, struct gp2ap_data *data )
{
  u8		value ;

  pr_debug( "light_sensor onoff = %d\n", onoff ) ;

  if( onoff ){
    if( !data->gs_enabled ){
      value = ( COMMAND1_WAKEUP | COMMAND1_ALS ) ;			// ALS mode
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }else{
      value = ( COMMAND1_SD	 ) ; 							// Shutdown
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
      value = ( COMMAND1_WAKEUP | COMMAND1_ALS_GS ) ;			// GS & ALS mode
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }

  }else{
    if( !data->gs_enabled ){
      value = ( COMMAND1_SD ) ; 								// Shutdown
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }else{
      value = ( COMMAND1_SD	 ) ; 							// Shutdown
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
      value = ( COMMAND1_WAKEUP | COMMAND1_GS ) ; 			// GS mode
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }
  }
  return 0 ;
}

static unsigned int
als_mode_change(  u32 *data_als, struct gp2ap_data *data )
{
  u8 value ;

  /*  Lux mode (Range) change */
  /*  LOW ---> MIDDLE */
  if( ( data_als[0] >= ALS_L_to_M_counts ) && ( data->als_mode == LOW_LUX_MODE ) ){
    data->als_mode = MIDDLE_LUX_MODE ;
		
    printk( KERN_INFO "change lux mode. MIDDLE_LUX_MODE!! \n" ) ;
		
    /* ALS Shutdown */
    als_onoff_simplified( 0, data ) ;
		
    /* change Range of ALS */
    data->regData[REG_ALS1] = ( data->regData[REG_ALS1] & 0x78 ) | MIDDLE_LUX_RANGE;
    value = data->regData[REG_ALS1];
    gp2ap_i2c_write( REG_ALS1, &value, data->client ) ;

    /* Start */
    als_onoff_simplified( 1, data ) ;

    return 1 ;
  }/*  MIDDLE ---> HIGH */else if( ( data_als[0] > ALS_M_to_H_counts ) && ( data->als_mode == MIDDLE_LUX_MODE ) ){
    data->als_mode = HIGH_LUX_MODE ;
		
    printk( KERN_INFO "change lux mode. HIGH_LUX_MODE!! \n" ) ;
		
    /* ALS Shutdown */
    als_onoff_simplified( 0, data ) ;
		
    /* change Range of ALS */
    data->regData[REG_ALS1] = ( data->regData[REG_ALS1] & 0x78 ) | HIGH_LUX_RANGE;
    value = data->regData[REG_ALS1];
    gp2ap_i2c_write( REG_ALS1, &value, data->client ) ;

    /* Start */
    als_onoff_simplified( 1, data ) ;
		
    return 1 ;
  }/*  HIGH ---> MIDDLE */else if( ( data_als[0] < ALS_H_to_M_counts ) && ( data->als_mode == HIGH_LUX_MODE ) ){
    data->als_mode = MIDDLE_LUX_MODE ;
		
    printk( KERN_INFO "change lux mode. MIDDLE_LUX_MODE!! \n" ) ;
		
    /* ALS Shutdown */
    als_onoff_simplified( 0, data ) ;
		
    /* change Range of ALS */
    data->regData[REG_ALS1] = ( data->regData[REG_ALS1] & 0x78 ) | MIDDLE_LUX_RANGE;
    value = data->regData[REG_ALS1];
    gp2ap_i2c_write( REG_ALS1, &value, data->client ) ;

    /* Start */
    als_onoff_simplified( 1, data ) ;
		
    return 1 ;
  }/*  MIDDLE ---> LOW */else if( ( data_als[0] < ALS_M_to_L_counts ) && ( data->als_mode == MIDDLE_LUX_MODE ) ){
    data->als_mode = LOW_LUX_MODE ;
		
    printk( KERN_INFO "change lux mode. LOW_LUX_MODE!! \n" ) ;
		
    /* ALS Shutdown */
    als_onoff_simplified( 0, data ) ;
		
    /* change Range of ALS */
    data->regData[REG_ALS1] = ( data->regData[REG_ALS1] & 0x78 ) | LOW_LUX_RANGE;
    value = data->regData[REG_ALS1];
    gp2ap_i2c_write( REG_ALS1, &value, data->client ) ;

    /* Start */
    als_onoff_simplified( 1, data ) ;
		
    return 1 ;
  }

  return 0 ;

}

/* *****************************************************************************
   ALS DATA POLLING
*******************************************************************************/
static void als_data_polling( struct work_struct *work )
{
  struct gp2ap_data  *data = container_of( ( struct delayed_work * )work,
					   struct gp2ap_data, als_work ) ;
  u8 rdata[4] ;
  u32 data_als[2];
  int ret;

  if( data != NULL ){
    mutex_lock( &data->mutex ) ;

    /* get data_als[0]:clear, data_als[1]:ir */
    gp2ap_i2c_read( REG_D5_LSB, rdata, sizeof( rdata ), data->client ) ;

    data_als[0] = rdata[0]+rdata[1]*256;
    data_als[1] = rdata[2]+rdata[3]*256;
    //printk("%d %d\n", ret_data_als[1], ret_data_als[0]);
	
    ret = als_mode_change(data_als, data);
    //lux = als_get_lux( data ) ;
    mutex_unlock( &data->mutex ) ;

    /* if(ret == 0) */
    /* { */
    /* 	printk("als_mode:%d  ret_data_als[0]:%d    ret_data_als[1]:%d\n", data->als_mode, ret_data_als[0], ret_data_als[1]); */
    /* 	//input_report_abs( data->als_input_dev, ABS_LUX_REPORT, lux ) ; */
    /* 	//input_report_abs( data->als_input_dev, ABS_LUX_REPORT, ( data->als_mode << 17 ) | ( 0x0 << 16 ) | data_als[0] ) ; */
    /* 	//input_report_abs( data->als_input_dev, ABS_LUX_REPORT, ( data->als_mode << 17 ) | ( 0x1 << 16 ) | data_als[1] ) ; */
    /* 	//input_sync( data->als_input_dev ) ; */
    /* } */
	
    schedule_delayed_work( &data->als_work, msecs_to_jiffies( data->als_delay ) ) ;
  }
}

/* *****************************************************************************
		ALS Input Device
***************************************************************************** */
static int als_input_init( struct gp2ap_data *data )
{
  struct input_dev   *dev ;
  int					err = 0 ;

  dev = input_allocate_device( ) ;
  if( !dev ){
    printk( KERN_ERR "%s, input_allocate_device error(%d)!!\n", __func__, err ) ;
    return -ENOMEM ;
  }

  set_bit( EV_ABS, dev->evbit ) ;
  input_set_capability( dev, EV_ABS, ABS_LUX_REPORT ) ;
  input_set_abs_params( dev, ABS_LUX_REPORT, 0, 0x7fffffff, 0, 0 ) ;
  input_set_capability( dev, EV_ABS, ABS_WAKE ) ;
  input_set_abs_params( dev, ABS_WAKE, 0, 0x7fffffff, 0, 0 ) ;
  input_set_capability( dev, EV_ABS, ABS_CONTROL_REPORT ) ;
  input_set_abs_params( dev, ABS_CONTROL_REPORT, 0, 0x1ffff, 0, 0 ) ;

  dev->name = LIGHT_SENSOR_NAME ;

  err = input_register_device( dev ) ;
  if( err < 0 ){
    input_free_device( dev ) ;
    printk( KERN_ERR "%s, input_register_device error(%d)!!\n", __func__, err ) ;
    return err ;
  }
  input_set_drvdata( dev, data ) ;

  data->als_input_dev = dev ;

  return 0 ;
}

/* *****************************************************************************
   PS GS Input Device
   ***************************************************************************** */
static int psgs_input_init( struct gp2ap_data *data )
{
  struct input_dev *dev = data->input_dev;
  int err = 0 ;
  int retval = 0;

  dev = input_allocate_device( ) ;
  if( !dev ){
    printk( KERN_ERR "%s, input_allocate_device error(%d)!!\n", __func__, err ) ;
    return -ENOMEM ;
  }

  dev->name = GESTURE_SENSOR_NAME ;

  /* Register the device as mouse */
  __set_bit(EV_REL, dev->evbit);
  __set_bit(REL_X, dev->relbit);
  __set_bit(REL_Y, dev->relbit);
  __set_bit(REL_Z, dev->relbit);
  __set_bit(EV_KEY, dev->evbit);
  __set_bit(BTN_LEFT, dev->keybit);
  __set_bit(BTN_RIGHT, dev->keybit);

  err = input_register_device( dev ) ;
  if( err < 0 ){
    input_free_device( dev ) ;
    printk( KERN_ERR "%s, input_register_device error(%d)!!\n", __func__, err ) ;
    return err ;
  }
  input_set_drvdata( dev, data ) ;

  data->input_dev = dev ;

  return 0 ;

 input_regs_err:
  input_free_device(data->input_dev);
}

static void gp2ap_init_device(  u8 mode, struct gp2ap_data *data )
{
  int		i ;
  u8		value ;

  for( i = 1 ; i < sizeof( data->regData ) ; i++ ){
    gp2ap_i2c_write( i, &data->regData[i], data->client ) ;
  }
	
  /* VDDCOER setting & Reflective cancellation */
  if( mode == SENSOR_MODE_GESTURE ){
    value = REFLECTIVE_CANCEL_GS;
    gp2ap_i2c_write( REG_PANEL, &value, data->client ) ;
  }else if ( mode == SENSOR_MODE_PULSE_COUNTER ){
    value = REFLECTIVE_CANCEL_PULSE;
    gp2ap_i2c_write( REG_PANEL, &value, data->client ) ;
  }
}

/* *****************************************************************************
   Sysfs Interface PS & GS Sensor
   ***************************************************************************** */
static void gp2ap_start_sensor( u8 mode, struct gp2ap_data *data )
{
  u8		value ;
  int		i ;

  pr_debug( "sensor mode = %d\n", mode ) ;

  if( ( mode == SENSOR_MODE_GESTURE ) || ( mode == SENSOR_MODE_PULSE_COUNTER ) ){		/* GS or Pulse counter mode */
    data->regData[REG_PS_LT_LSB] = 0x00 ;
    data->regData[REG_PS_LT_MSB] = 0x00 ;
    data->regData[REG_PS_HT_LSB] = 0xFF ;
    data->regData[REG_PS_HT_MSB] = 0xFF ;
    data->regData[REG_PS3] = ( 0xC8  | PS3_GS_INTVAL6P25 ) ;
    //		data->regData[REG_COMMND3]  = (COMMAND3_INT_GS | COMMAND3_INT_GS_PULSE | COMMAND3_INT_ALS_PULSE | COMMAND3_INT_PS_PULSE) ;
    //		data->regData[REG_ADR_02] = ( data->regData[REG_ADR_02] & 0x1F ) | PRST_1 ;

    if( mode == SENSOR_MODE_GESTURE ){
      data->regData[REG_PS2] = (PS2_IS128 | PS2_SUM16 | 0x01) ;
    }
    else if( mode == SENSOR_MODE_PULSE_COUNTER ){
      data->regData[REG_PS2] = (PS2_IS32  | PS2_SUM16 | 0x01) ;
    }
		
    gp2ap_init_device( mode,  data ); 
    /* Start */
    if( !data->als_enabled ){
      value = ( COMMAND1_WAKEUP | COMMAND1_GS ) ; 			/* GS mode */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }else{
      value = ( COMMAND1_SD	 ) ; 							/* Shutdown */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
      value = ( COMMAND1_WAKEUP | COMMAND1_ALS_GS ) ; 		/* ALS_GS mode */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }
  }
  else if( mode == SENSOR_MODE_PROXIMITY ){						/* PS mode */
    for( i = 0 ; i < 4 ; i++ ){
      data->regData[REG_PS_LT_LSB+i] = gp2ap_ps_th[i] ;
    }
    data->regData[REG_PS3] = ( 0xC8 | PS3_GS_INTVAL25 ) ;
    //		data->regData[REG_COMMND3]  = (COMMAND3_INT_PS | COMMAND3_INT_GS_PULSE | COMMAND3_INT_ALS_PULSE | COMMAND3_INT_PS_PULSE) ;
    //		data->regData[REG_ADR_02] = ( data->regData[REG_ADR_02] & 0x1F ) | PRST_4 ;
    gp2ap_init_device( mode,  data ); 

    /* Start */
    if( !data->als_enabled ){
      value = ( COMMAND1_WAKEUP | COMMAND1_GS ) ; 			/* PS mode */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }else{
      value = ( COMMAND1_SD	 ) ; 							/* Shutdown */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
      value = ( COMMAND1_WAKEUP | COMMAND1_ALS_GS ) ; 		/* ALS_PS mode */
      gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    }
  }
}

static void gp2ap_stop_sensor( u8 mode, struct gp2ap_data *data )
{
  u8		value ;
  int		i ;

  mutex_lock( &data->mutex ) ;
  /* Shutdown */
  if( !data->als_enabled ){
    value = ( COMMAND1_SD	 ) ; 								/* Shutdown */
    gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
  }else{
    value = ( COMMAND1_SD	 ) ; 								/* Shutdown */
    gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
    value = ( COMMAND1_WAKEUP | COMMAND1_ALS	 ) ; 			/* ALS mode */
    gp2ap_i2c_write( REG_COMMND1, &value, data->client ) ;
  }
	
  mutex_unlock( &data->mutex ) ;

  if( ( mode == SENSOR_MODE_GESTURE ) || ( mode == SENSOR_MODE_PULSE_COUNTER ) ){
    hrtimer_cancel( &data->gs_polling_timer ) ;
    flush_work( &data->gs_polling_work ) ;
    cancel_work_sync( &data->gs_polling_work ) ;
    //		disable_irq( data->ps_irq ) ;
    //		disable_irq_wake( data->ps_irq ) ;
  }else if( mode == SENSOR_MODE_PROXIMITY ){
    disable_irq( data->ps_irq ) ;
    disable_irq_wake( data->ps_irq ) ;
    data->ps_distance = 1 ;
    for( i = 0 ; i < 4 ; i++ ){
      gp2ap_ps_th[i] = data->regData[REG_PS_LT_LSB+i] ;
    }
  }
}

static void gp2ap_set_mode(struct gp2ap_data *data, int mode){
  if (mode < 0 || mode > 3)
    return;

  if (data->mode == mode){
    return;
  }

  if( mode == SENSOR_MODE_OFF ){
    gp2ap_stop_sensor( data->mode, data ) ;
    data->gs_enabled = 0; 
    mutex_lock( &data->mutex ) ;
    data->mode = mode ;
    mutex_unlock( &data->mutex ) ;
    printk( KERN_INFO "gp2ap054a00f gesture sensor disabe!! \n" ) ;
  }else if( ( mode == SENSOR_MODE_GESTURE ) || ( mode == SENSOR_MODE_PULSE_COUNTER ) ){
    if( data->mode != SENSOR_MODE_OFF ){
      gp2ap_stop_sensor( data->mode, data ) ;
    }

    mutex_lock( &data->mutex ) ;
    gp2ap_start_sensor( mode, data ) ;
    data->gs_enabled = 1;
    data->mode = mode ;
    mutex_unlock( &data->mutex ) ;

    msleep( 150 ) ;

    hrtimer_start( &data->gs_polling_timer, data->gs_polling_delay, HRTIMER_MODE_REL ) ;

    if( mode == SENSOR_MODE_GESTURE )
      printk( KERN_INFO "gp2ap054a00f gesture sensor enable!! \n" ) ;
    else if( mode == SENSOR_MODE_PULSE_COUNTER )
      printk( KERN_INFO "gp2ap054a00f pulse counter sensor enable!! \n" ) ;
  }else if( mode == SENSOR_MODE_PROXIMITY ){
    if( data->mode != SENSOR_MODE_OFF ){
      gp2ap_stop_sensor( data->mode, data ) ;
    }

    mutex_lock( &data->mutex ) ;
    gp2ap_start_sensor( mode, data ) ;
    data->gs_enabled = 1;
    data->mode = mode ;
    msleep( 200 ) ;
    mutex_unlock( &data->mutex ) ;

    printk( KERN_INFO "proximity_sensor enable!! \n" ) ;
  }

  return;
}

static int gp2ap054a_probe(struct i2c_client *client, const struct i2c_device_id *id){
  struct gp2ap_data *gp2ap;
  struct gp2ap054a_platform_data *pdata;

  u8 value = 0 ;
  int err = 0 ;
  int i;

  gp2ap = kzalloc(sizeof(struct gp2ap_data), GFP_KERNEL);;

  gp2ap->client = client;
  pdata = client->dev.platform_data ;

  mutex_init( &gp2ap->mutex);

  gp2ap->mode = SENSOR_MODE_OFF;

  gp2ap->gs_delay = SENSOR_DEFAULT_DELAY ;
  hrtimer_init( &gp2ap->gs_polling_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL ) ;
  gp2ap->gs_polling_delay = ns_to_ktime( gp2ap->gs_delay * 100 * NSEC_PER_USEC ) ;
  gp2ap->gs_polling_timer.function = gp2ap_gs_polling_timer_func ;

  INIT_WORK( &gp2ap->gs_polling_work, gp2ap_gs_data_polling ) ;

  gp2ap->ps_distance = 1 ;

  //INIT_WORK( &gp2ap->ps_int_work, gp2ap_ps_work_int_func ) ;

  gp2ap->als_enabled = 0 ;
  gp2ap->als_delay = SENSOR_DEFAULT_DELAY_ALS ;

  //INIT_DELAYED_WORK( &gp2ap->als_work, als_data_polling ) ;

  err = als_input_init( gp2ap ) ;
  if( err < 0 ){
    goto err_als_input_device ;
  }

  //err = sysfs_create_group( &gp2ap->als_input_dev->dev.kobj, &als_attribute_group ) ;
  if( err ){
    printk( KERN_ERR
	    "sysfs_create_group failed[%s]\n",
	    gp2ap->als_input_dev->name ) ;
    goto err_sysfs_create_group_als ;
  }

  //input device PS, GS
  err = psgs_input_init( gp2ap ) ;
  if( err < 0 ){
    goto err_input_device ;
  }

  // sysfs
  //err = sysfs_create_group( &gp2ap->input_dev->dev.kobj, &gp2ap_attribute_group ) ;
  if( err ){
    printk( KERN_ERR
	    "sysfs_create_group failed[%s]\n",
	    gp2ap->input_dev->name ) ;
    goto err_sysfs_create_group ;
  }
  
  i2c_set_clientdata(client, gp2ap);
  gngp2ap = gp2ap;

  id = 0;
  gp2ap_i2c_read(0x3e, &id, 1, gp2ap->client);
  printk("chip id 0x%08x\n", id);

  value = COMMAND1_SD ;	// shutdown
  err = gp2ap_i2c_write( REG_COMMND1, &value, gp2ap->client ) ;
  if( err < 0 ){
    printk( KERN_ERR "threre is no gp2ap054a device. !! \n" ) ;
    goto err_no_device ;
  }


  for( i = 0 ; i < sizeof( gp2ap->regData )  ; i++ ){
    gp2ap->regData[i] = gp2ap_ini_data[i] ;
  }

  gp2ap_set_mode(gp2ap, SENSOR_MODE_GESTURE);
  //als_set_mode(gp2ap, ALS_L_to_M_counts);
  //als_set_enable(gp2ap, 1);

  return 0;

 err_ps_request_irq:
 err_no_device:
  /* sysfs_remove_group( &gp2ap->input_dev->dev.kobj, */
  /* 		      &gp2ap_attribute_group ) ; */
 err_sysfs_create_group:
  input_unregister_device( gp2ap->input_dev ) ;
  input_free_device( gp2ap->input_dev ) ;
 err_input_device:
  /* sysfs_remove_group( &gp2ap->als_input_dev->dev.kobj, */
  /* 		      &als_attribute_group ) ;	 */
 err_sysfs_create_group_als:
  input_unregister_device( gp2ap->als_input_dev ) ;
  input_free_device( gp2ap->als_input_dev ) ;
 err_als_input_device:
  mutex_destroy( &gp2ap->mutex ) ;
  kfree( gp2ap ) ;
  return err ;
}

static int gp2ap054a_remove(struct i2c_client *client)
{
	return 0;
}

static int gp2ap054a_suspend(struct i2c_client *client, pm_message_t mesg)
{
  struct gp2ap_data *gp2ap = i2c_get_clientdata(client);

  if(gngp2ap->mode == SENSOR_MODE_GESTURE){
    hrtimer_cancel( &gngp2ap->gs_polling_timer ) ;
    flush_work( &gngp2ap->gs_polling_work ) ;
    cancel_work_sync( &gngp2ap->gs_polling_work ) ;
    gngp2ap->mode = SENSOR_MODE_OFF;
  }

  return 0; 
}

static int gp2ap054a_resume(struct i2c_client *client)
{
  struct gp2ap054a_data *dev = i2c_get_clientdata(client);
  unsigned char id = 0;
  unsigned char value = 0;

  value = COMMAND1_SD ;	// shutdown
  gp2ap_i2c_write( REG_COMMND1, &value, gngp2ap->client ) ;

  gp2ap_set_mode(gngp2ap, SENSOR_MODE_GESTURE);

  return 0;
}

static struct early_suspend gp2ap054a_early_suspend_handler = {
	.suspend = gp2ap054a_suspend,
	.resume  = gp2ap054a_resume,
};

static const struct i2c_device_id gp2ap054a_id[] = {
	{ "gp2ap054a", 0 },
	{ }
};

static struct i2c_driver gp2ap054a_driver = {
	.driver = {
		.name	= "gp2ap054a",
	},
	.probe			= gp2ap054a_probe,
	.remove			= gp2ap054a_remove,
	.id_table		= gp2ap054a_id,
};

static int __init gp2ap054a_init(void)
{
  register_early_suspend( &gp2ap054a_early_suspend_handler ) ;

  return i2c_add_driver(&gp2ap054a_driver);
}

static void __exit gp2ap054a_exit(void)
{
	i2c_del_driver(&gp2ap054a_driver);
}

module_init(gp2ap054a_init);
module_exit(gp2ap054a_exit);
