#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <mach/ovisp-v4l2.h>
#include <mach/regs-isp.h>
#include "sensor.h"

#define OV8856_MAX_WIDTH		3264
#define OV8856_MAX_HEIGHT		2448

#define OV8856_RG_Ratio_Typical        0x13f
#define OV8856_BG_Ratio_Typical        0x14d

#define OV8856_SETTING_PROFILE_2    //sys 144M

#define OV8856_DEFAULT_REG_WRTIE_MODE   ISP_CMD_GROUP_WRITE
/* #define OV8856_DEFAULT_REG_WRTIE_MODE   ISP_GROUP_WRITE */

#if defined OV8856_SETTING_PROFILE_2

static struct regval_list ov8856_init_default[] = {

	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x031e, 0x0c},
	{0x3000, 0x00},
	{0x300e, 0x00},
	{0x3010, 0x00},
	{0x3015, 0x84},
	{0x3018, 0x32},
	{0x3033, 0x24},
	{0x3500, 0x00},
	{0x3501, 0x4c},
	{0x3502, 0xe0},
	{0x3503, 0x08},
	{0x3505, 0x83},
	{0x3508, 0x01},
	{0x3509, 0x80},
	{0x350c, 0x00},
	{0x350d, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3600, 0x72},
	{0x3601, 0x40},
	{0x3602, 0x30},
	{0x3610, 0xc5},
	{0x3611, 0x58},
	{0x3612, 0x5c},
	{0x3613, 0x5a},
	{0x3614, 0x60},
	{0x3628, 0xff},
	{0x3629, 0xff},
	{0x362a, 0xff},
	{0x3633, 0x10},
	{0x3634, 0x10},
	{0x3635, 0x10},
	{0x3636, 0x10},
	{0x3663, 0x08},
	{0x3669, 0x34},
	{0x366e, 0x08},
	{0x3706, 0x86},
	{0x370b, 0x7e},
	{0x3714, 0x27},
	{0x3730, 0x12},
	{0x3733, 0x10},
	{0x3764, 0x00},
	{0x3765, 0x00},
	{0x3769, 0x62},
	{0x376a, 0x2a},
	{0x3780, 0x00},
	{0x3781, 0x24},
	{0x3782, 0x00},
	{0x3783, 0x23},
	{0x3798, 0x2f},
	{0x37a1, 0x60},
	{0x37a8, 0x6a},
	{0x37ab, 0x3f},
	{0x37c2, 0x14},
	{0x37c3, 0xf1},
	{0x37c9, 0x80},
	{0x37cb, 0x03},
	{0x37cc, 0x0a},
	{0x37cd, 0x16},
	{0x37ce, 0x1f},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0e},
	{0x380d, 0xa0},
	{0x380e, 0x04},
	{0x380f, 0xde},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x382a, 0x03},
	{0x382b, 0x01},
	{0x3830, 0x06},
	{0x3836, 0x02},
	{0x3862, 0x04},
	{0x3863, 0x08},
	{0x3cc0, 0x33},
	{0x3d85, 0x17},
	{0x3d8c, 0x73},
	{0x3d8d, 0xde},
	{0x4001, 0xe0},
	{0x4003, 0x40},
	{0x4008, 0x00},
	{0x4009, 0x05},
	{0x400f, 0x80},
	{0x4010, 0xf0},
	{0x4011, 0xff},
	{0x4012, 0x02},
	{0x4013, 0x01},
	{0x4014, 0x01},
	{0x4015, 0x01},
	{0x4042, 0x00},
	{0x4043, 0x80},
	{0x4044, 0x00},
	{0x4045, 0x80},
	{0x4046, 0x00},
	{0x4047, 0x80},
	{0x4048, 0x00},
	{0x4049, 0x80},
	{0x4041, 0x03},
	{0x404c, 0x20},
	{0x404d, 0x00},
	{0x404e, 0x20},
	{0x4203, 0x80},
	{0x4307, 0x30},
	{0x4317, 0x00},
	{0x4503, 0x08},
	{0x4601, 0x80},
	{0x4816, 0x53},
	{0x481b, 0x58},
	{0x481f, 0x27},
	{0x4825, 0x4a},
	{0x4826, 0x40},
	{0x4837, 0x16},
	{0x5000, 0x77},
	{0x5030, 0x41},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579a, 0x00},
	{0x579b, 0x28},
	{0x579c, 0x00},
	{0x579d, 0x16},
	{0x579e, 0x06},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
	{0x5780, 0x14},
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x59f8, 0x3d},
	{0x5a08, 0x02},
	{0x5b00, 0x02},
	{0x5b01, 0x10},
	{0x5b02, 0x03},
	{0x5b03, 0xcf},
	{0x5b05, 0x6c},
	{0x5e00, 0x00},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov8856_3264_2448_profile_2[] = {

	{0x0302, 0x35},
	{0x0303, 0x00},
	{0x366e, 0x10},
	{0x3714, 0x23},
	{0x37c2, 0x04},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x0f},
	{0x380d, 0x20},
	{0x380e, 0x09},
	{0x380f, 0xb2},
	{0x3811, 0x10},
	{0x3813, 0x04},
	{0x3814, 0x01},
	{0x382a, 0x01},
	{0x4009, 0x0b},
	{0x4837, 0x0c},
	{0x5795, 0x02},
	{0x5796, 0x20},
	{0x5797, 0x20},
	{0x5798, 0xd5},
	{0x5799, 0xd5},
	{0x579b, 0x50},
	{0x579d, 0x2c},
	{0x579e, 0x0c},
	{0x579f, 0x40},
	{0x57a0, 0x09},
	{0x57a1, 0x40},
#ifdef CONFIG_VIDEO_OV8856_FLIP
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0xc6},
	{0x3821, 0x46},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x04},
	{0x376b, 0x36},
#else
	{0x3820, 0xc6},
	{0x3821, 0x40},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x00},
	{0x376b, 0x36},
#endif
#else
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
#else
	{0x3820, 0x80},
	{0x3821, 0x40},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x00},
	{0x376b, 0x30},
#endif
#endif
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},
};

static struct regval_list ov8856_1600_1200_profile_2[] = {

	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x366e, 0x08},
	{0x3714, 0x27},
	{0x37c2, 0x14},
	{0x3808, 0x06},
	{0x3809, 0x40},
	{0x380a, 0x04},
	{0x380b, 0xb0},
#ifdef CONFIG_BOARD_SHIELD
	/* 40fps */
	{0x380c, 0x08},
	{0x380d, 0x5f},
	{0x380e, 0x06},
	{0x380f, 0x90},
	/* 50fps */
	/* {0x380c, 0x07}, */
	/* {0x380d, 0x8c}, */
	/* {0x380e, 0x05}, */
	/* {0x380f, 0xd0}, */
#else
	{0x380c, 0x0a},
	{0x380d, 0x80},
	{0x380e, 0x08},
	{0x380f, 0xa0},
#endif
	{0x3811, 0x08},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x382a, 0x03},
	{0x4009, 0x05},
	{0x4837, 0x16},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579b, 0x28},
	{0x579d, 0x16},
	{0x579e, 0x06},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
#ifdef CONFIG_VIDEO_OV8856_FLIP
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0xd6},
	{0x3821, 0x67},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x04},
	{0x376b, 0x36},
#else
	{0x3820, 0xd6},
	{0x3821, 0x61},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x00},
	{0x376b, 0x36},
#endif
#else
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0x90},
	{0x3821, 0x67},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
#else
	{0x3820, 0x90},
	{0x3821, 0x61},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x00},
	{0x376b, 0x30},
#endif
#endif
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8856_1600_1200_lowfps_profile_2[] = {

	{0x0302, 0x3c},
	{0x0303, 0x01},
	{0x366e, 0x08},
	{0x3714, 0x27},
	{0x37c2, 0x14},
	{0x3808, 0x06},
	{0x3809, 0x40},
	{0x380a, 0x04},
	{0x380b, 0xb0},
	{0x380c, 0x0a}, 
	{0x380d, 0x80}, 
	{0x380e, 0x0d},
	{0x380f, 0xcc},
	{0x3811, 0x08},
	{0x3813, 0x02},
	{0x3814, 0x03},
	{0x382a, 0x03},
	{0x4009, 0x05},
	{0x4837, 0x16},
	{0x5795, 0x00},
	{0x5796, 0x10},
	{0x5797, 0x10},
	{0x5798, 0x73},
	{0x5799, 0x73},
	{0x579b, 0x28},
	{0x579d, 0x16},
	{0x579e, 0x06},
	{0x579f, 0x20},
	{0x57a0, 0x04},
	{0x57a1, 0xa0},
#ifdef CONFIG_VIDEO_OV8856_FLIP
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0xd6},
	{0x3821, 0x67},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x04},
	{0x376b, 0x36},
#else
	{0x3820, 0xd6},
	{0x3821, 0x61},
	{0x502e, 0x00},
	{0x5001, 0x0e},
	{0x5004, 0x00},
	{0x376b, 0x36},
#endif
#else
#ifdef CONFIG_VIDEO_OV8856_MIRROR
	{0x3820, 0x90},
	{0x3821, 0x67},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x04},
	{0x376b, 0x30},
#else
	{0x3820, 0x90},
	{0x3821, 0x61},
	{0x502e, 0x03},
	{0x5001, 0x0a},
	{0x5004, 0x00},
	{0x376b, 0x30},
#endif
#endif
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

#endif

static struct regval_list ov8856_softreset[] = {
	{0x0103, 0x01},

	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},
	{0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8856_stream_on[] = {
	/* {0x4201, 0x00}, */
	/* {0x4202, 0x00}, */
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov8856_stream_off[] = {
	/* Sensor enter LP11*/
	/* {0x4201, 0x00}, */
	/* {0x4202, 0x0f}, */
	{0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

#if defined  OV8856_SETTING_PROFILE_2
static struct sensor_win_setting ov8856_1600_1200_lowfps_win = {
		.width		= 1600,
		.height		= 1200,
#ifdef OV8856_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov8856_1600_1200_lowfps_profile_2,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov8856_1600_1200_lowfps_profile_2),
		.sysclk         = 144,//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0xdcc,
		.hts            = 0xa80,
};
#endif

static struct sensor_win_setting ov8856_win_sizes[] = {
#if defined  OV8856_SETTING_PROFILE_2
	/* 3264*2448 */
	{
		.width		= 3264,
		.height		= 2448,
#ifdef OV8856_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov8856_3264_2448_profile_2,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov8856_3264_2448_profile_2),
		.sysclk         = 144,//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x9b2,
		.hts            = 0xf20,
	},
	/* 1600*1200 */
	{
		.width		= 1600,
		.height		= 1200,
#ifdef OV8856_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov8856_1600_1200_profile_2,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov8856_1600_1200_profile_2),
		.sysclk         = 144,//M for caculate banding step
		.low_fps_win    = &ov8856_1600_1200_lowfps_win,
#ifdef CONFIG_BOARD_SHIELD
 		.vts            = 0x690,
		.hts            = 0x85f,
 		/* .vts            = 0x5d0, */
		/* .hts            = 0x78c, */
#else
		.vts            = 0x8a0,
		.hts            = 0xa80,
#endif
	},
	/* 1280*720 */
/* 	{ */
/* 		.width		= 1280, */
/* 		.height		= 720, */
/* #ifdef OV8856_CAMERA_RAW_8 */
/* 		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8, */
/* #else */
/* 		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10, */
/* #endif */
/* 		.colorspace	= V4L2_COLORSPACE_SRGB, */
/* 		.regs 		= ov8856_1280_720_profile_2, */
/* 		.lunch_regs 	= NULL, */
/* 		.reg_sum        = ARRAY_SIZE(ov8856_1280_720_profile_2), */
/* 		.sysclk         = 144,//M for caculate banding step */
/* 		.low_fps_win    = NULL, */
/* 		.vts            = 0x9aa, */
/* 		.hts            = 0x78c, */
/* 	}, */
#endif

};

#define OV8856_N_WIN_SIZES (ARRAY_SIZE(ov8856_win_sizes))

static struct sensor_format_struct ov8856_formats[] = {
	{
		/*RAW8 FORMAT, 8 bit per pixel*/
#ifdef OV8856_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
#endif
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	{
#ifdef OV8856_CAMERA_RAW_8
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
#else
		.mbus_code	= V4L2_MBUS_FMT_YUYV10_2X10,
#endif
		.colorspace	 = V4L2_COLORSPACE_BT878,/*don't know*/

	}
	/*add other format supported*/
};
#define N_OV8856_FMTS ARRAY_SIZE(ov8856_formats)

int ov8856_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}


/* static  */
int ov8856_write(struct v4l2_subdev *sd, unsigned short reg,
		 unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}


static int ov8856_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8856_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	ov8856_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov8856_write_testself_mode(struct v4l2_subdev *sd,struct regval_list *vals,unsigned char config_value)
{
      int ret;
      while (vals->reg_num !=SENSOR_REG_END) {
	      if(vals->reg_num == SENSOR_REG_TESTMODE) {
		      ret = ov8856_write(sd,vals->reg_num,config_value);
                      SENSOR_PRINT(CAMERA_INFO,"config_value is %d\n",config_value);
                      if (ret < 0)
			  return -1;
		      ov8856_read(sd, vals->reg_num, &config_value);
                      SENSOR_PRINT(CAMERA_INFO," This is %d\n",config_value);
              }
            
       	      vals++;
      }
      return 0;
}
static int ov8856_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov8856_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	//ov8856_write(sd, vals->reg_num, vals->value);
	return 0;
}

int ov8856_read_otp(struct v4l2_subdev *sd, unsigned short reg){
	int ret, value;
	unsigned char tmp = 0;
	ret = ov8856_read(sd, reg, &tmp);
	value = tmp;
	return value;
}

int ov8856_write_otp(struct v4l2_subdev *sd, unsigned short reg, int value){
	int ret = 0;
	unsigned char tmp = value;
	ret = ov8856_write(sd, reg, tmp);
	return ret;
}


// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
static int ov8856_check_otp_info(struct v4l2_subdev *sd, int index)
{
	int flag;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, 0x70);
	ov8856_write_otp(sd, 0x3d89, 0x10);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8a, 0x70);
	ov8856_write_otp(sd, 0x3d8b, 0x10);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	flag = ov8856_read_otp(sd, 0x7010);
	/* SENSOR_PRINT(CAMERA_WARNING,"%s flag = %x;\n",__func__, flag); */
	//select group
	if (index == 1)
		{
			flag = (flag>>6) & 0x03;
		}
	else if (index == 2)
		{
			flag = (flag>>4) & 0x03;
		}
	else if (index ==3)
		{
			flag = (flag>>2) & 0x03;
		}
	// clear otp buffer
	ov8856_write_otp(sd, 0x7010, 0x00);
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x02) {
		return 1;
	}
	else {
		return 2;
	}
}

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
static int ov8856_check_otp_wb(struct v4l2_subdev *sd, int index)
{
	int flag;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, 0x70);
	ov8856_write_otp(sd, 0x3d89, 0x10);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8a, 0x70);
	ov8856_write_otp(sd, 0x3d8b, 0x10);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	//select group
	flag = ov8856_read_otp(sd, 0x7010);

	if (index == 1)
		{
			flag = (flag>>6) & 0x03;
		}
	else if (index == 2)
		{
			flag = (flag>>4) & 0x03;
		}

	// clear otp buffer
	ov8856_write_otp(sd,  0x7010, 0x00);
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x02) {
		return 1;
	}
	else {
		return 2;
	}
}

// index: index of otp group. (1, 2, 3)
// code: 0 for start code, 1 for stop code
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
static int ov8856_check_otp_VCM(struct v4l2_subdev *sd, int index, int code)
{
	int flag;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, 0x70);
	ov8856_write_otp(sd, 0x3d89, 0x30);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8A, 0x70);
	ov8856_write_otp(sd, 0x3d8B, 0x30);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	//select group
	flag = ov8856_read_otp(sd, 0x7030);
	if (index == 1)
		{
			flag = (flag>>6) & 0x03;
		}
	else if (index == 2)
		{
			flag = (flag>>4) & 0x03;
		}
	else if (index == 3)
		{
			flag = (flag>>2) & 0x03;
		}
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	// clear otp buffer
	ov8856_write_otp(sd, 0x7030, 0x00);
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x02) {
		return 1;
	}
	else {
		return 2;
	}
}
// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
// 1, group index has invalid data
// 2, group index has valid data
static int ov8856_check_otp_lenc(struct v4l2_subdev *sd, int index)
{
	int flag;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, 0x70);
	ov8856_write_otp(sd, 0x3d89, 0x28);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8A, 0x70);
	ov8856_write_otp(sd, 0x3d8B, 0x28);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	flag = ov8856_read_otp(sd, 0x7028);
	/* SENSOR_PRINT(CAMERA_WARNING,"0x7028 flag = %x;\n", flag); */
	if (index == 1)
		{
			flag = (flag>>6) & 0x03;
		}
	else if (index == 2)
		{
			flag = (flag>>4) & 0x03;
		}
	// clear otp buffer
	ov8856_write_otp(sd,  0x7028, 0x00);
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	if (flag == 0x00) {
		return 0;
	}
	else if (flag & 0x01) {
		return 2;
	}
	else {
		return 1;
	}
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of ov885x_otp_struct
// return: 0,
static int ov8856_read_otp_info(struct v4l2_subdev *sd, int index, struct ov885x_otp_struct *otp_ptr)
{
	int i = 0;
	int start_addr = 0;
	int end_addr = 0;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	if (index == 1) {
		start_addr = 0x7011;
		end_addr = 0x7015;
	}
	else if (index == 2) {
		start_addr = 0x7016;
		end_addr = 0x701a;
	}
	else if (index == 3) {
		start_addr = 0x701b;
		end_addr = 0x701f;
	}
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, (start_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d89, start_addr & 0xff);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8A, (end_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d8B, end_addr & 0xff);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	(*otp_ptr).module_integrator_id = ov8856_read_otp(sd, start_addr);
	(*otp_ptr).lens_id = ov8856_read_otp(sd, start_addr + 1);
	(*otp_ptr).production_year = ov8856_read_otp(sd, start_addr + 2);
	(*otp_ptr).production_month = ov8856_read_otp(sd, start_addr + 3);
	(*otp_ptr).production_day = ov8856_read_otp(sd, start_addr + 4);
	// clear otp buffer
	for (i=start_addr; i<=end_addr; i++) {
		ov8856_write_otp(sd, i, 0x00);
	}
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	return 0;
}
// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of ov885x_otp_struct
// return: 0,
static int ov8856_read_otp_wb(struct v4l2_subdev *sd, int index, struct ov885x_otp_struct *otp_ptr)
{
	int i = 0;
	int start_addr = 0;
	int end_addr = 0;
	int temp;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	if (index == 1) {
		start_addr = 0x7016;
		end_addr = 0x7018;
	}
	else if (index == 2) {
		start_addr = 0x701e;
		end_addr = 0x7020;
	}
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, (start_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d89, start_addr & 0xff);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8A, (end_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d8B, end_addr & 0xff);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);

	temp = ov8856_read_otp(sd, start_addr + 2);
	(*otp_ptr).rg_ratio = (ov8856_read_otp(sd, start_addr)<<2) + ((temp>>6) & 0x03);
	(*otp_ptr).bg_ratio = (ov8856_read_otp(sd, start_addr + 1)<<2) + ((temp>>4) & 0x03);
	// clear otp buffer
	for (i=start_addr; i<=end_addr; i++) {
		ov8856_write_otp(sd, i, 0x00);
	}
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	return 0;
}
// index: index of otp group. (1, 2, 3)
// code: 0 start code, 1 stop code
// return: 0
static int ov8856_read_otp_VCM(struct v4l2_subdev *sd, int index, struct ov885x_otp_struct * otp_ptr)
{
	int temp;
	int i = 0;
	int start_addr = 0;
	int end_addr = 0;
	//set 0x5001[3] to “0”
	int temp1;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	if (index == 1) {
		start_addr = 0x7031;
		end_addr = 0x7033;
	}
	else if (index == 2) {
		start_addr = 0x7034;
		end_addr = 0x7036;
	}
	else if (index == 3) {
		start_addr = 0x7037;
		end_addr = 0x7039;
	}
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, (start_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d89, start_addr & 0xff);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8A, (end_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d8B, end_addr & 0xff);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(5);
	//flag and lsb of VCM start code
	temp = ov8856_read_otp(sd, start_addr+2);
	(* otp_ptr).VCM_start = (ov8856_read_otp(sd, start_addr)<<2) | ((temp>>6) & 0x03);
	(* otp_ptr).VCM_end = (ov8856_read_otp(sd, start_addr + 1) << 2) | ((temp>>4) & 0x03);
	(* otp_ptr).VCM_dir = (temp>>2) & 0x03;
	// clear otp buffer
	for (i=start_addr; i<=end_addr; i++) {
		ov8856_write_otp(sd, i, 0x00);
	}
	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	return 0;
}

// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of ov885x_otp_struct
// return: 0,
static int ov8856_read_otp_lenc(struct v4l2_subdev *sd, int index, struct ov885x_otp_struct *otp_ptr)
{
	int i = 0;
	int start_addr = 0;
	int end_addr = 0;
	//set 0x5001[3] to “0”
	int temp1, checksum_temp;
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x00 & 0x08) | (temp1 & (~0x08)));
	if (index == 1) {
		start_addr = 0x7029;
		end_addr = 0x7119;
	}
	else if (index == 2) {
		start_addr = 0x711a;
		end_addr = 0x720a;
	}
	ov8856_write_otp(sd, 0x3d84, 0xC0);
	//partial mode OTP write start address
	ov8856_write_otp(sd, 0x3d88, (start_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d89, start_addr & 0xff);
	// partial mode OTP write end address
	ov8856_write_otp(sd, 0x3d8a, (end_addr >> 8) & 0xff);
	ov8856_write_otp(sd, 0x3d8b, end_addr & 0xff);
	// read otp into buffer
	ov8856_write_otp(sd, 0x3d81, 0x01);
	mdelay(10);
	checksum_temp = 0;
	for(i=0; i<240; i++) {
		(* otp_ptr).lenc[i]=ov8856_read_otp(sd, start_addr + i);
		checksum_temp += (* otp_ptr).lenc[i];

	}

	checksum_temp = (checksum_temp)%255 +1;
	temp1 = ov8856_read_otp(sd, 0x7119);
	//?????? 
	if(temp1 != checksum_temp)return -1;
	// clear otp buffer
	for (i=start_addr; i<=end_addr; i++) {
		ov8856_write_otp(sd, i, 0x00);
	}

	//set 0x5001[3] to “1”
	temp1 = ov8856_read_otp(sd, 0x5001);
	ov8856_write_otp(sd, 0x5001, (0x08 & 0x08) | (temp1 & (~0x08)));
	return 0;
}

// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
static int ov8856_update_awb_gain(struct v4l2_subdev *sd, int R_gain, int G_gain, int B_gain)
{
	if (R_gain>0x400) {
		ov8856_write_otp(sd, 0x5019, R_gain>>8);
		ov8856_write_otp(sd, 0x501a, R_gain & 0x00ff);
	}
	if (G_gain>0x400) {
		ov8856_write_otp(sd, 0x501b, G_gain>>8);
		ov8856_write_otp(sd, 0x501c, G_gain & 0x00ff);
	}
	if (B_gain>0x400) {
		ov8856_write_otp(sd, 0x501d, B_gain>>8);
		ov8856_write_otp(sd, 0x501e, B_gain & 0x00ff);
	}
	return 0;
}

// otp_ptr: pointer of ov885x_otp_struct
static int ov8856_update_lenc(struct v4l2_subdev *sd, struct ov885x_otp_struct * otp_ptr)
{
	int i, temp;
	temp = ov8856_read_otp(sd, 0x5000);
	temp = 0x20 | temp;
	ov8856_write_otp(sd, 0x5000, temp);
	for(i=0;i<240;i++) {
		ov8856_write_otp(sd, 0x5900 + i, (*otp_ptr).lenc[i]);
	}
	return 0;
}

// call this function after OV8856 initialization
// return value: 0 update success
// 1, no OTP
static int ov8856_update_otp_wb(struct v4l2_subdev *sd)
{
	struct ov885x_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int rg,bg;
	int nR_G_gain, nB_G_gain, nG_G_gain;
	int R_gain, B_gain, G_gain;
	int nBase_gain;

	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=2;i++) {
		temp = ov8856_check_otp_wb(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	if (i>2) {
		// no valid wb OTP data
		return 1;
	}
	ov8856_read_otp_wb(sd, otp_index, &current_otp);

	// no light source information in OTP, light factor = 1
	rg = current_otp.rg_ratio;
    
	// not light source information in OTP, light factor = 1
	bg = current_otp.bg_ratio;

	//calculate G gain
	nR_G_gain = (OV8856_RG_Ratio_Typical*1000) / rg;
	nB_G_gain = (OV8856_BG_Ratio_Typical*1000) / bg;
	nG_G_gain = 1000;
	if (nR_G_gain < 1000 || nB_G_gain < 1000)
		{
			if (nR_G_gain < nB_G_gain)
				nBase_gain = nR_G_gain;
			else
				nBase_gain = nB_G_gain;
		}
	else
		{
			nBase_gain = nG_G_gain;
		}
	R_gain = 0x400 * nR_G_gain / (nBase_gain);
	B_gain = 0x400 * nB_G_gain / (nBase_gain);
	G_gain = 0x400 * nG_G_gain / (nBase_gain);
	/* printk("rg = %d, bg = %d\n",rg, bg); */
	/* printk("nBase_gain = %d\n",nBase_gain); */
	/* printk("R_gain = %d\n",R_gain); */
	/* printk("B_gain = %d\n",B_gain); */
	/* printk("G_gain = %d\n",G_gain); */
	ov8856_update_awb_gain(sd, R_gain, G_gain, B_gain);
	return 0;
}

// call this function after OV8856 initialization
// return value: 0 update success
// 1, no OTP
static int ov8856_update_otp_lenc(struct v4l2_subdev *sd)
{
	struct ov885x_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	// check first lens correction OTP with valid data
	for(i=1;i<=3;i++) {
		temp = ov8856_check_otp_lenc(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	if (i>3) {
		SENSOR_PRINT(CAMERA_WARNING,"no valid lenc otp\n");
		// no valid WB OTP data
		return 1;
	}
	ov8856_read_otp_lenc(sd, otp_index, &current_otp);
	ov8856_update_lenc(sd, &current_otp);
	// success
	return 0;
}

static int ov8856_reset(struct v4l2_subdev *sd, u32 val)
{
#if 0
	/* int ret; */
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	ret = ov8856_write_array(sd, ov8856_softreset);
	if(ret < 0){
		SENSOR_PRINT(CAMERA_WARNING,"ov8856 failed to reset, return %d\n",ret);
		return ret;
	}
	/* info->sensor_state = OV8856_RST; */
#endif
	return 0;
}

static int ov8856_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;
	/* unsigned char value; */

	ret = ov8856_write_array(sd, ov8856_softreset);
	if(ret < 0){
	    SENSOR_PRINT(CAMERA_ERROR,"ov8856 failed to reset, return %d\n",ret);
		return ret;
	}

	/* ret = ov8856_read(sd, 0x3f0a, &value); */
	/* if (ret < 0) */
	/* 	return ret; */
	/* SENSOR_PRINT(CAMERA_WARNING,"3f0a = %02x\n",value); */
	/* ret = ov8856_read(sd, 0x4500, &value); */
	/* if (ret < 0) */
	/* 	return ret; */
	/* SENSOR_PRINT(CAMERA_WARNING,"4500 = %02x\n",value); */
	/* ret = ov8856_read(sd, 0x382d, &value); */
	/* if (ret < 0) */
	/* 	return ret; */
	/* SENSOR_PRINT(CAMERA_WARNING,"382d = %02x\n",value); */

	/* chose init setting */
	ret = ov8856_write_array(sd, ov8856_init_default);
	if(ret < 0){
	    SENSOR_PRINT(CAMERA_ERROR,"ov8856 failed to set common reg value, return %d\n",ret);
		return ret;
	}

	ret = ov8856_write_array(sd, ov8856_stream_on);
	if(ret < 0){
	    SENSOR_PRINT(CAMERA_ERROR,"ov8856 stream on failed in init period err = %d\n",ret);
		return ret;
	}
	
	ret = ov8856_update_otp_wb(sd);
	if(ret == 0){
		SENSOR_PRINT(CAMERA_WARNING,"awb otp sucess!!!\n");
	}else{
		SENSOR_PRINT(CAMERA_WARNING,"awb otp failed!!!\n");
	}
	ret = ov8856_update_otp_lenc(sd);
	if(ret == 0){
		SENSOR_PRINT(CAMERA_WARNING,"lenc otp sucess!!!\n");
	}else{
		SENSOR_PRINT(CAMERA_WARNING,"lenc otp failed!!!\n");
	}
	
	ret = ov8856_write_array(sd, ov8856_stream_off);
	if(ret < 0){
		SENSOR_PRINT(CAMERA_ERROR,"ov8856 stream off failed in init period err = %d\n",ret);
		return ret;
	}
	return 0;
}

static int ov8856_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
				enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV8856_FMTS)
		return -EINVAL;

	*code = ov8856_formats[index].mbus_code;
	return 0;
}

static int ov8856_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   struct sensor_win_setting **ret_wsize)
{
	struct sensor_win_setting *wsize;

	if(fmt->width > OV8856_MAX_WIDTH || fmt->height > OV8856_MAX_HEIGHT)
		return -EINVAL;
	for (wsize = ov8856_win_sizes; wsize < ov8856_win_sizes + OV8856_N_WIN_SIZES;
	     wsize++)
		if (fmt->width > wsize->width && fmt->height > wsize->height)
			break;
	/* if (wsize >= ov8856_win_sizes + OV8856_N_WIN_SIZES) */
		wsize--;   /* Take the smallest one */
	if (fmt->priv == V4L2_CAP_LOWFPS_MODE && wsize->low_fps_win != NULL){
		wsize = wsize->low_fps_win;
	}
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->mbus_code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = wsize->colorspace;
	return 0;
}

static int ov8856_g_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	data->i2cflags = V4L2_I2C_ADDR_16BIT;
	data->sensor_sys_pclk = info->trying_win->sysclk;
	data->vts = info->trying_win->vts;
	data->hts = info->trying_win->hts;
	data->lans = 2;
	
	return 0;
}

static int ov8856_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	ret = ov8856_try_fmt_internal(sd, fmt, &info->trying_win);
	SENSOR_PRINT(CAMERA_INFO,"info->trying_win- >width = %d, info->trying_win- >height = %d,\n", info->trying_win->width, info->trying_win->height);

	return ret;
}

static int ov8856_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{

	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	/* struct sensor_win_setting *wsize; */
	int ret;

	/* SENSOR_PRINT(CAMERA_WARNING,"[ov8856], problem function:%s, line:%d\n", __func__, __LINE__); */
	/* ret = ov8856_try_fmt_internal(sd, fmt, &wsize); */
	/* if (ret) */
	/* 	return ret; */
	/* if (info->using_win) */
	/* 	SENSOR_PRINT(CAMERA_WARNING,"info->using_win->width =%d \n",info->using_win->width); */
	/* if (info->trying_win) */
	/* 	SENSOR_PRINT(CAMERA_WARNING,"info->trying_win->width =%d \n",info->trying_win->width); */

	if (info->using_win != info->trying_win) {
		if(info->write_mode == GROUP_LUNCH){
		    ret = ov8856_write_array(sd, info->trying_win->lunch_regs);
		}else{
		    ret = ov8856_write_array(sd, info->trying_win->regs);
		}
		if (ret)
			return ret;
	}
	info->using_win = info->trying_win;

	ov8856_g_mbus_fmt(sd, fmt);

	return 0;
}

static int ov8856_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	if (enable) {

		ret = ov8856_write_array(sd, ov8856_stream_on);
		if(ret < 0){
			SENSOR_PRINT(CAMERA_ERROR,"ov8856 stream on failed ret = %d\n",ret);
		}
		else{
		    SENSOR_PRINT(CAMERA_INFO,"ov8856 stream on\n");
		}

	}
	else {
		ret = ov8856_write_array(sd, ov8856_stream_off);
		if(ret < 0){
			SENSOR_PRINT(CAMERA_ERROR,"ov8856 stream off failed ret = %d\n",ret);
		}
		else{
		    SENSOR_PRINT(CAMERA_INFO,"ov8856 stream off\n");
		}
		info->write_mode = OV8856_DEFAULT_REG_WRTIE_MODE;
	}
	return ret;
}

static int ov8856_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov8856_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *interval)
{
    SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	if (interval->index >= ARRAY_SIZE(ov8856_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov8856_frame_rates[interval->index];
	return 0;
}

static int ov8856_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	__u32 index = fsize->index;

	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	if (index < OV8856_N_WIN_SIZES) {
		struct sensor_win_setting *win = &ov8856_win_sizes[index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = win->width;
		fsize->discrete.height = win->height;
		return 0;
	} else {
		return -EINVAL;
	}

	return -EINVAL;
}

static int ov8856_s_ctrl_1(struct v4l2_subdev *sd,struct v4l2_control *ctrl,unsigned char select_mode)
{
        int ret = -1;
        if (select_mode == TURN_OVSENSOR_TESTMODE) {
		ret = ov8856_write_testself_mode(sd,ov8856_init_default,SENSOR_TESTMODE_VALUE);
		if(ret < 0){
			SENSOR_PRINT(CAMERA_ERROR,"functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
        }
     
        if (select_mode == TURN_OVSENSOR_NORMALMODE) {
		ret = ov8856_write_testself_mode(sd,ov8856_init_default,SENSOR_NORMALMODE_VALUE);
		if(ret < 0){
			SENSOR_PRINT(CAMERA_ERROR,"functiong:%s, line:%d failed\n", __func__, __LINE__);
			return ret;
		}
        }
 
        return 0;
}
