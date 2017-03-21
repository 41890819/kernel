	#ifndef __GP2AP054A_H__
	#define __GP2AP054A_H__

	
	#define REG_COMMND1    		0x00
	#define REG_COMMND2    		0x01
	#define REG_COMMND3 		0x02
	#define REG_ALS1   			0x03
	#define REG_ALS2			0x04
	#define REG_PS1				0x05
	#define REG_PS2				0x06
	#define REG_PS3				0x07
	#define REG_PS_LT_LSB	 	0x08
	#define REG_PS_LT_MSB		0x09
	#define REG_PS_HT_LSB		0x0A
	#define REG_PS_HT_MSB		0x0B
	#define REG_OS_D0_LSB		0x0C
	#define REG_OS_D0_MSB		0x0D
	#define REG_OS_D1_LSB		0x0E
	#define REG_OS_D1_MSB		0x0F
	#define REG_OS_D2_LSB		0x10
	#define REG_OS_D2_MSB   	0x11
	#define REG_OS_D3_LSB   	0x12
	#define REG_OS_D3_MSB   	0x13
	#define REG_PRE3_D0_LSB		0x14
	#define REG_PRE3_D0_MSB		0x15
	#define REG_PRE3_D1_LSB		0x16
	#define REG_PRE3_D1_MSB		0x17
	#define REG_PRE3_D2_LSB		0x18
	#define REG_PRE3_D2_MSB		0x19
	#define REG_PRE3_D3_LSB		0x1A
	#define REG_PRE3_D3_MSB		0x1B
	#define REG_PRE2_D0_LSB		0x1C
	#define REG_PRE2_D0_MSB		0x1D
	#define REG_PRE2_D1_LSB		0x1E
	#define REG_PRE2_D1_MSB		0x1F
	#define REG_PRE2_D2_LSB		0x20
	#define REG_PRE2_D2_MSB		0x21
	#define REG_PRE2_D3_LSB		0x22
	#define REG_PRE2_D3_MSB		0x23
	#define REG_PRE1_D0_LSB		0x24
	#define REG_PRE1_D0_MSB		0x25
	#define REG_PRE1_D1_LSB		0x26
	#define REG_PRE1_D1_MSB		0x27
	#define REG_PRE1_D2_LSB		0x28
	#define REG_PRE1_D2_MSB		0x29
	#define REG_PRE1_D3_LSB		0x2A
	#define REG_PRE1_D3_MSB		0x2B
	#define REG_D0_LSB			0x2C
	#define REG_D0_MSB			0x2D
	#define REG_D1_LSB			0x2E
	#define REG_D1_MSB			0x2F
	#define REG_D2_LSB			0x30
	#define REG_D2_MSB			0x31
	#define REG_D3_LSB			0x32
	#define REG_D3_MSB			0x33
	#define REG_D4_LSB			0x34
	#define REG_D4_MSB			0x35
	#define REG_D5_LSB			0x36
	#define REG_D5_MSB			0x37
	#define REG_D6_LSB			0x38
	#define REG_D6_MSB			0x39
	#define REG_D7_LSB			0x3A
	#define REG_D7_MSB			0x3B
	#define REG_D8_LSB			0x3C
	#define REG_D8_MSB			0x3D
	#define REG_DEVICE_ID		0x3E
	#define REG_PANEL   		0x41

	
	// COMMAND1
	#define COMMAND1_WAKEUP			0x80
	#define COMMAND1_SD				0x00
	#define COMMAND1_ALS_GS			0x00
	#define COMMAND1_ALS			0x10
	#define COMMAND1_GS				0x20


	// COMMAND2
	#define COMMAND2_NO_INT_CLEAR	0x0F
	#define COMMAND2_INT_CLEAR		0x00
	#define COMMAND2_GS_INT_CLEAR	0x0E
	#define COMMAND2_PS_INT_CLEAR	0x03
	#define COMMAND2_ALS_INT_CLEAR	0x0D

	
	// COMMAND3
	#define COMMAND3_INT_PROX		0x00
	#define COMMAND3_INT_PS			0x10
	#define COMMAND3_INT_ALS		0x20
	#define COMMAND3_INT_GS			0x40
	#define COMMAND3_INT_PS_LEVEL	0x00
	#define COMMAND3_INT_PS_PULSE	0x02
	#define COMMAND3_INT_ALS_LEVEL	0x00
	#define COMMAND3_INT_ALS_PULSE	0x04
	#define COMMAND3_INT_GS_LEVEL	0x00
	#define COMMAND3_INT_GS_PULSE	0x08
	#define COMMAND3_REG_RST		0x01

	
	// ALS1
	#define ALS1_RES18			0x00
	#define ALS1_RES16			0x08
	#define ALS1_RES14			0x10
	#define ALS1_RES12			0x18
	#define ALS1_RANGEX1		0x00
	#define ALS1_RANGEX2		0x01
	#define ALS1_RANGEX4		0x02
	#define ALS1_RANGEX8		0x03
	#define ALS1_RANGEX16		0x04
	#define ALS1_RANGEX32		0x05
	#define ALS1_RANGEX64		0x06
	#define ALS1_RANGEX128		0x07
	#define ALS1_RANGEX256		0x86
	#define ALS1_RANGEX512		0x87	
	
	// ALS2
	#define ALS2_ALS_INTVAL0	0x00
	#define ALS2_ALS_INTVAL1P56	0x01
	#define ALS2_ALS_INTVAL6P25	0x02
	#define ALS2_ALS_INTVAL25	0x03
	#define ALS2_ALS_INTVAL50	0x04
	#define ALS2_ALS_INTVAL100	0x05
	#define ALS2_ALS_INTVAL200	0x06
	#define ALS2_ALS_INTVAL400	0x07


	// PS1
	#define PS1_PRST1			0x00
	#define PS1_PRST2			0x20
	#define PS1_PRST3			0x40
	#define PS1_PRST4			0x60
	#define PS1_PRST5			0x80
	#define PS1_PRST6			0xA0
	#define PS1_PRST7			0xC0
	#define PS1_PRST8			0xE0
	#define PS1_RES14			0x00
	#define PS1_RES12			0x08
	#define PS1_RES10			0x10
	#define PS1_RES8			0x18
	#define PS1_RANGEX1			0x00
	#define PS1_RANGEX2			0x01
	#define PS1_RANGEX4	 		0x02
	#define PS1_RANGEX8			0x03
	#define PS1_RANGEX16		0x04
	#define PS1_RANGEX32		0x05
	#define PS1_RANGEX64		0x06
	#define PS1_RANGEX128		0x07
	
	
	// PS2	
	#define PS2_IS2				0x00
	#define PS2_IS4				0x20
	#define PS2_IS8				0x40
	#define PS2_IS16			0x60
	#define PS2_IS32			0x80
	#define PS2_IS64			0xA0
	#define PS2_IS128			0xC0
	#define PS2_IS256			0xE0
	#define PS2_SUM4			0x00
	#define PS2_SUM8			0x04
	#define PS2_SUM12			0x08
	#define PS2_SUM16			0x0C
	#define PS2_SUM20			0x10
	#define PS2_SUM24			0x14
	#define PS2_SUM28			0x18
	#define PS2_SUM32			0x1C

	
	//PS3
	#define PS3_GS_INT1			0x00
	#define PS3_GS_INT2			0x10
	#define PS3_GS_INT3			0x20
	#define PS3_GS_INT4			0x30
	#define PS3_GS_INTVAL0		0x00
	#define PS3_GS_INTVAL1P56	0x01
	#define PS3_GS_INTVAL6P25	0x02
	#define PS3_GS_INTVAL25		0x03
	#define PS3_GS_INTVAL50		0x04
	#define PS3_GS_INTVAL100	0x05
	#define PS3_GS_INTVAL200	0x06
	#define PS3_GS_INTVAL400	0x07

/* event code */
#define ABS_CONTROL_REPORT		( ABS_THROTTLE )
#define ABS_ADC_REPORT			( ABS_MISC )
#define ABS_DISTANCE_REPORT		( ABS_DISTANCE )
#define ABS_WAKE				( ABS_BRAKE )
#define ABS_LUX_REPORT			( ABS_MISC )


/* platform data */
struct gp2ap054a_platform_data
{
	int		gpio ;
} ;

#endif
