#ifndef _SENSOR_H_
#define _SENSOR_H_

#define SENSOR_REG_END		0xffff
#define SENSOR_REG_DELAY	0xfffe

#define SENSOR_REG_TESTMODE       0x5e00
#define SENSOR_TESTMODE_VALUE     0x80
#define SENSOR_NORMALMODE_VALUE   0X00
#define TURN_OVSENSOR_TESTMODE    0xAA
#define TURN_OVSENSOR_NORMALMODE  0xA0
#define CHECK_IIC_CHIP_ID         0xA1

#define CAMERA_DEBUG_LEVEL	CAMERA_WARNING
/* #define CAMERA_DEBUG_LEVEL	CAMERA_INFO */
#define CAMERA_INFO		0x0
#define CAMERA_WARNING		0x1
#define CAMERA_ERROR		0x2

#define OV8865_CHIP_ID         (0x8865)
#define OV8865_CHIP_ID_H	(0x88)
#define OV8865_CHIP_ID_L	(0x65)

#define OV8856_CHIP_ID_H	(0x88)
#define OV8856_CHIP_ID_L	(0x5A)

#define OV5648_CHIP_ID_H	(0x56)
#define OV5648_CHIP_ID_L	(0x48)

#define OV8858_CHIP_ID_H	(0x88)
#define OV8858_CHIP_ID_L	(0x58)

#define OV4689_CHIP_ID_H        (0x46)
#define OV4689_CHIP_ID_L        (0x88)

#define OV6211_CHIP_ID_H        (0x67)
#define OV6211_CHIP_ID_L        (0x10)

#define SENSOR_PRINT(level, ...) do { if (level >= CAMERA_DEBUG_LEVEL) printk(__VA_ARGS__); \
	if(level >= CAMERA_ERROR) dump_stack();} while (0)

/* #define CAPTURE_INITAL_MODE */

enum reg_write_mode {
	I2C_WRITE,
	ISP_GROUP_WRITE,
	ISP_CMD_GROUP_WRITE,
	GROUP_LUNCH,
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};

struct sensor_info {
	struct v4l2_subdev sd;
	struct sensor_format_struct *fmt;
	struct sensor_win_setting *win;
	struct sensor_win_setting *using_win;
	struct sensor_win_setting *trying_win;
	int win_size;

	bool support_group_lunch;
	struct regval_list *group_hold_regs;
	int group_hold_regs_num; 
	enum reg_write_mode write_mode; 

	unsigned int sensor_state; 
	unsigned int sensor_otp_flag; 
	unsigned short chip_id;
 
	unsigned char i2c_flag;
	unsigned short i2c_addr;
        /* hstp supported */
        struct sensor_win_setting *hstp_preview_setting;   
        struct sensor_win_setting *hstp_photo_setting;   
};

struct sensor_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
};

struct sensor_win_setting {
	unsigned short	width;
	unsigned short	height;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs;            /* Regs to tweak */
	struct regval_list *lunch_regs;            /* Regs for lunch group n, if camera support this*/
	struct sensor_win_setting *low_fps_win;            /* Regs for lunch group n, if camera support this*/

	unsigned short     vts;
	unsigned short     hts;
	int     fps;
	int     reg_sum;
	int     sysclk;
};

struct ov885x_otp_struct{
	int module_integrator_id;
	int lens_id;
	int production_year;
	int production_month;
	int production_day;
	int rg_ratio;
	int bg_ratio;
	int g_ave;
	int user_data;
	int lenc[240];
	int VCM_start;
	int VCM_end;
	int VCM_dir;
};
#endif
