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

#define OV5648_MAX_WIDTH		2592
#define OV5648_MAX_HEIGHT		1944

#define OV5648_RG_Ratio_Typical        0x01c0
#define OV5648_BG_Ratio_Typical        0x01c0

/* #define OV5648_CAMERA_RAW_8 */
#define OV5648_DEFAULT_REG_WRTIE_MODE   ISP_CMD_GROUP_WRITE
/* #define OV5648_DEFAULT_REG_WRTIE_MODE   ISP_GROUP_WRITE */

static struct regval_list ov5648_init_1944_1944_raw10_regs[] = {

	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0xcc},	// changed by AM05d

	{0x3800, 0x01},	// xstart = 0
	{0x3801, 0x44}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x08}, // xend = 2623
	{0x3805, 0xfb}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x07}, // x output size = 2592
	{0x3809, 0x80}, // x output size
	{0x380a, 0x07}, // y output size = 1944 check to 1920
	{0x380b, 0x80}, // y output size
	//b00 8c0
	{0x380c, 0x08}, // hts = 2816
	{0x380d, 0x90}, // hts
	{0x380e, 0x07}, // vts = 1984
	{0x380f, 0xb0}, // vts

	{0x3810, 0x00}, // isp x win = 16
	{0x3811, 0x10}, // isp x win
	{0x3812, 0x00}, // isp y win = 6
	{0x3813, 0x06}, // isp y win
	{0x3814, 0x11}, // x inc
	{0x3815, 0x11}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x40}, // flip off, v bin off
	{0x3821, 0x06}, // mirror on, v bin off
	{0x4004, 0x04}, // black line number
	{0x4005, 0x1a}, // blc always update

	/* {0x350b, 0x40}, // gain = 4x */

	{0x4837, 0x18}, // MIPI global timing


	{0x4202, 0x00},
	{0x0100, 0x01}, // black line number

	//test pattern
	/* {0x503d, 0x80}, */
	/* {0x503e, 0x00}, */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_init_raw10_regs[] = {
	/* {0x0100, 0x00}, */
	/* {0x0103, 0x01}, */
	//delay 5ms
	/* {0x4202, 0x0f}, */

	{0x3001, 0x00},
	{0x3002, 0x00},
	{0x3011, 0x02},
	{0x3017, 0x05},
	{0x3018, 0x4c},
	{0x301c, 0xd2},
	{0x3022, 0x00},
	{0x3034, 0x1a},
	{0x3035, 0x21},
	{0x3036, 0x69},
	{0x3037, 0x03},
	{0x3038, 0x00},
	{0x3039, 0x00},
	{0x303a, 0x00},
	{0x303b, 0x19},
	{0x303c, 0x11},
	{0x303d, 0x30},
	{0x3105, 0x11},
	{0x3106, 0x05},
	{0x3304, 0x28},
	{0x3305, 0x41},
	{0x3306, 0x30},
	{0x3308, 0x00},
	{0x3309, 0xc8},
	{0x330a, 0x01},
	{0x330b, 0x90},
	{0x330c, 0x02},
	{0x330d, 0x58},
	{0x330e, 0x03},
	{0x330f, 0x20},
	{0x3300, 0x00},
	
	/* {0x3500, 0x00}, // */
	/* {0x3501, 0x3d}, // */
	/* {0x3502, 0x00}, // */

	{0x3503, 0x07},
	{0x4825, 0x52},

	/* {0x350a, 0x00}, // */
	/* {0x350b, 0x40}, // */

	{0x3601, 0x33},
	{0x3602, 0x00},
	{0x3611, 0x0e},
	{0x3612, 0x2b},
	{0x3614, 0x50},
	{0x3620, 0x33},
	{0x3622, 0x00},
	{0x3630, 0xad},
	{0x3631, 0x00},
	{0x3632, 0x94},
	{0x3633, 0x17},
	{0x3634, 0x14},
	{0x3704, 0xc0},
	{0x3705, 0x2a},

	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370b, 0x23},
	{0x370c, 0xcf},
	{0x370d, 0x00},
	{0x370e, 0x00},
	{0x371c, 0x07},
	{0x3739, 0xd2},
	{0x373c, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0a},
	{0x3805, 0x3f},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x05},
	{0x3809, 0x10},
	{0x380a, 0x03},
	{0x380b, 0xcc},
	{0x380c, 0x0b},
	{0x380d, 0x00},
	{0x380e, 0x03},
	{0x380f, 0xe0},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3817, 0x00},
	{0x3820, 0x08},
	{0x3821, 0x07},
	{0x3826, 0x03},
	{0x3829, 0x00},
	{0x382b, 0x0b},
	{0x3830, 0x00},
	{0x3836, 0x00},
	{0x3837, 0x00},
	{0x3838, 0x00},
	{0x3839, 0x04},
	{0x383a, 0x00},
	{0x383b, 0x01},
	{0x3b00, 0x00},
	{0x3b02, 0x08},
	{0x3b03, 0x00},
	{0x3b04, 0x04},
	{0x3b05, 0x00},
	{0x3b06, 0x04},
	{0x3b07, 0x08},
	{0x3b08, 0x00},
	{0x3b09, 0x02},
	{0x3b0a, 0x04},
	{0x3b0b, 0x00},
	{0x3b0c, 0x3d},
	{0x3f01, 0x0d},
	{0x3f0f, 0xf5},
	{0x4000, 0x89},
	{0x4001, 0x02},
	{0x4002, 0x45},
	{0x4004, 0x02},
	{0x4005, 0x18},
	{0x4006, 0x08},
	{0x4007, 0x10},
	{0x4008, 0x00},
	{0x4050, 0x6e},
	{0x4051, 0x8f},
	{0x4300, 0xf8},
	{0x4303, 0xff},
	{0x4304, 0x00},
	{0x4307, 0xff},
	{0x4520, 0x00},
	{0x4521, 0x00},
	{0x4511, 0x22},
	{0x4801, 0x0f},
	{0x4814, 0x2a},
	{0x481f, 0x3c},
	{0x4823, 0x3c},
	{0x4826, 0x00},
	{0x481b, 0x3c},
	{0x4827, 0x32},
	{0x4837, 0x17},
	{0x4b00, 0x06},
	{0x4b01, 0x0a},//raw8 or raw10
	{0x4b04, 0x10},
	{0x5000, 0xff},
	{0x5001, 0x00},
	{0x5002, 0x41},
	{0x5003, 0x0a},
	{0x5004, 0x00},
	{0x5043, 0x00},
	{0x5013, 0x00},
	{0x501f, 0x03},
	{0x503d, 0x00},
	{0x5780, 0xfc},
	{0x5781, 0x1f},
	{0x5782, 0x03},
	{0x5786, 0x20},
	{0x5787, 0x40},
	{0x5788, 0x08},
	{0x5789, 0x08},
	{0x578a, 0x02},
	{0x578b, 0x01},
	{0x578c, 0x01},
	{0x578d, 0x0c},
	{0x578e, 0x02},
	{0x578f, 0x01},
	{0x5790, 0x01},
	{0x5a00, 0x08},
	{0x5b00, 0x01},
	{0x5b01, 0x40},
	{0x5b02, 0x00},
	{0x5b03, 0xf0},
	/* {0x0100, 0x01}, */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

/* preview_HTS = 3780; */
/* preview_VTS = 742; */

/* int hts = (int)(preview_HTS / 256); */
/* int vts = (int)(preview_VTS / 256); */

/* int hts_1 = (int)(preview_HTS % 256); */
/* int vts_1 = (int)(preview_VTS % 256); */

static struct regval_list ov5648_init_5M_raw10_regs[] = {

	// 2592x1944 15fps 2 lane MIPI 420Mbps/lane
	/* {0x0100, 0x00}, */

	/* {0x3501, 0x7b}, // exposure */
	/* {0x3502, 0x00}, // exposure */

	{0x3708, 0x63},
	{0x3709, 0x12},
	{0x370c, 0xcc},	// changed by AM05d
	{0x3800, 0x00},	// xstart = 0
	{0x3801, 0x00}, // xstart
	{0x3802, 0x00}, // ystart = 0
	{0x3803, 0x00}, // ystart
	{0x3804, 0x0a}, // xend = 2623
	{0x3805, 0x3f}, // xend
	{0x3806, 0x07}, // yend = 1955
	{0x3807, 0xa3}, // yend
	{0x3808, 0x0a}, // x output size = 2592
	{0x3809, 0x20}, // x output size
	{0x380a, 0x07}, // y output size = 1944 check to 1920
	{0x380b, 0x80}, // y output size

	{0x380c, 0x0b}, // hts = 2816
	{0x380d, 0x00}, // hts
	{0x380e, 0x08}, // vts = 1984
	{0x380f, 0xc0}, // vts

	{0x3810, 0x00}, // isp x win = 16
	{0x3811, 0x10}, // isp x win
	{0x3812, 0x00}, // isp y win = 6
	{0x3813, 0x06}, // isp y win
	{0x3814, 0x11}, // x inc
	{0x3815, 0x11}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x40}, // flip off, v bin off
	{0x3821, 0x06}, // mirror on, v bin off
	{0x4004, 0x04}, // black line number
	{0x4005, 0x1a}, // blc always update

	/* {0x350b, 0x40}, // gain = 4x */

	/* {0x4837, 0x17}, // MIPI global timing */


	{0x4202, 0x00},
	{0x0100, 0x01}, // black line number

	//test pattern
	/* {0x503d, 0x80}, */
	/* {0x503e, 0x00}, */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_init_1296_960_raw10_regs[] = {

	// 1296x972 30fps 2 lane MIPI 420Mbps/lane
	/* {0x0100, 0x00}, */
	/* {0x3501, 0x3d},// exposure */
	/* {0x3502, 0x00},// exposure */
	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370c, 0xcf},
	{0x3800, 0x00},// xstart = 0
	{0x3801, 0x00},// x start
	{0x3802, 0x00},// y start = 0
	{0x3803, 0x00},// y start
	{0x3804, 0x0a},// xend = 2623
	{0x3805, 0x3f},// xend
	{0x3806, 0x07},// yend = 1955
	{0x3807, 0xa3},// yend
	{0x3808, 0x05},// x output size = 1296
	{0x3809, 0x10},// x output size
	//{0x380a, 0x03},// y output size = 972
	//{0x380b, 0xcc},// y output size
	{0x380a, 0x03},// y output size = 960
	{0x380b, 0xc0},// y output size

	{0x380c, 0x08},
	{0x380d, 0xa0},
	{0x380e, 0x06},
	{0x380f, 0x30},
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x04}, // isp y win
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x08}, // flip off, v bin off
	{0x3821, 0x07}, // mirror on, h bin on
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // blc level trigger
	/* {0x350b, 0x80}, // gain = 8x */
	/* {0x4837, 0x17}, // MIPI global timing */

	{0x4202, 0x00},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_1296_960_lowfps_regs[] = {

	// 1296x972 30fps 2 lane MIPI 420Mbps/lane
	/* {0x0100, 0x00}, */
	/* {0x3501, 0x3d},// exposure */
	/* {0x3502, 0x00},// exposure */
	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370c, 0xcf},
	{0x3800, 0x00},// xstart = 0
	{0x3801, 0x00},// x start
	{0x3802, 0x00},// y start = 0
	{0x3803, 0x00},// y start
	{0x3804, 0x0a},// xend = 2623
	{0x3805, 0x3f},// xend
	{0x3806, 0x07},// yend = 1955
	{0x3807, 0xa3},// yend
	{0x3808, 0x05},// x output size = 1296
	{0x3809, 0x10},// x output size
	{0x380a, 0x03},// y output size = 972
	{0x380b, 0xcc},// y output size
	/* {0x380a, 0x03},// y output size = 960 */
	/* {0x380b, 0xc0},// y output size */

	{0x380c, 0x08},
	{0x380d, 0xa0},
	{0x380e, 0x09},
	{0x380f, 0xe0},
	{0x3810, 0x00}, // isp x win = 8
	{0x3811, 0x08}, // isp x win
	{0x3812, 0x00}, // isp y win = 4
	{0x3813, 0x04}, // isp y win
	{0x3814, 0x31}, // x inc
	{0x3815, 0x31}, // y inc
	{0x3817, 0x00}, // hsync start
	{0x3820, 0x08}, // flip off, v bin off
	{0x3821, 0x07}, // mirror on, h bin on
	{0x4004, 0x02}, // black line number
	{0x4005, 0x18}, // blc level trigger
	/* {0x350b, 0x80}, // gain = 8x */
	/* {0x4837, 0x17}, // MIPI global timing */

	{0x4202, 0x00},
	{0x0100, 0x01},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_init_720p_raw10_regs[] = {
	// 1280x720 30fps 2 lane MIPI 420Mbps/lane
	/* {0x0100, 0x00}, */
	/* {0x3501, 0x2d},	 // exposure */
	/* {0x3502, 0xc0},	 // exposure */
	{0x3708, 0x66},
	{0x3709, 0x52},
	{0x370c, 0xcf},
	{0x3800, 0x00},	 // xstart = 16
	{0x3801, 0x10},	 // xstart
	{0x3802, 0x00},	 // ystart = 254
	{0x3803, 0xfe},	 // ystart
	{0x3804, 0x0a},	 // xend = 2607
	{0x3805, 0x2f},	 // xend
	{0x3806, 0x06},	 // yend = 1701
	{0x3807, 0xa5},	 // yend
	{0x3808, 0x05},	 // x output size = 12280
	{0x3809, 0x00},	 // x output size
	{0x380a, 0x02},	 // y output size = 720
	{0x380b, 0xd0},	 // y output size
	{0x380c, 0x08},
	{0x380d, 0xa0},
	//{0x380e, 0x02},
	//{0x380f, 0xe6},
	{0x380e, 0x06},
	{0x380f, 0x30},
	{0x3810, 0x00},	 // isp x win = 8
	{0x3811, 0x08},	 // isp x win
	{0x3812, 0x00},	 // isp y win = 2
	{0x3813, 0x02},	 // isp y win
	{0x3814, 0x31},	 // x inc
	{0x3815, 0x31},	 // y inc
	{0x3817, 0x00},	 // hsync start
	{0x3820, 0x08},	 // flip off, v bin off
	{0x3821, 0x07},	 // mirror on, h bin on
	{0x4004, 0x02},	 // number of black line

	{0x4005, 0x18}, // blc level trigger
	//{0x3b0b, 0x80}, // gain = 8x
	{0x4837, 0x17}, // MIPI global timing

	{0x4202, 0x00},
	{0x0100, 0x01},

	//test pattern
	/* {0x503d, 0x80}, */
	/* {0x503e, 0x00}, */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_on[] = {
        {0x4202, 0x00},    /* stream on when new frame start */
	{0x0100, 0x01},    /* stream on and sensor power up */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_stream_off[] = {
	/* Sensor enter LP11*/
	{0x4202, 0x0f},    /* stream off when current frame finish, both clock and data lane in LP11 mode. 
			        VCM driver still working */
	{0x0100, 0x00},    /* stream off and sensor power down immediately, both clock and data lane in LP00 */

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_softsleep[] = {
        {0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5648_softreset[] = {
	{0x0103, 0x01},
        {0x0100, 0x00},
        {0x0100, 0x00},
        {0x0100, 0x00},
        {0x0100, 0x00},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct sensor_win_setting ov5648_1296_960_lowfps_win = {
		.width		= 1296,
		.height		= 972,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5648_1296_960_lowfps_regs,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov5648_1296_960_lowfps_regs),
		.sysclk         = 84,//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x9e0,
		.hts            = 0x8a0,
};

#ifdef CONFIG_BOARD_DISATCH
#define OV5648_1920_SETTING 1
#endif
static struct sensor_win_setting ov5648_win_sizes[] = {
#ifndef OV5648_1920_SETTING
	/* 2592*1944 */
	{
	        .width		= 2592,
		.height		= 1920,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5648_init_5M_raw10_regs,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov5648_init_5M_raw10_regs),
		.sysclk         = 84,//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x8c0,
		.hts            = 0xb00,
	},
#endif

#ifdef CONFIG_BOARD_DISATCH
#ifndef OV5648_1920_SETTING
	/* 1296*960 */
	{
	        .width		= 1296,
		.height		= 960,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5648_init_1296_960_raw10_regs,
		.lunch_regs 	= NULL,
		.reg_sum        = ARRAY_SIZE(ov5648_init_1296_960_raw10_regs),
		.sysclk         = 84,//M for caculate banding step
		.low_fps_win    = &ov5648_1296_960_lowfps_win,
		.vts            = 0x630,
		.hts            = 0x8a0,
	},
#else
	/* 1920*1920 */
	{
	        .width		= 1920,
		.height		= 1920,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs = ov5648_init_1944_1944_raw10_regs,
		.lunch_regs 	= NULL,
		.reg_sum = ARRAY_SIZE(ov5648_init_1944_1944_raw10_regs),
		.sysclk         = 84,//M for caculate banding step
		.vts            = 0x7b0,
		.hts            = 0x890,
	},
#endif
#else
	1280*720
	{
		.width		= 1280,
		.height		= 720,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5648_init_720p_raw10_regs,
		.reg_sum        = ARRAY_SIZE(ov5648_init_720p_raw10_regs),
		.lunch_regs 	= NULL,
		.sysclk         = 84,//M for caculate banding step
		.low_fps_win    = NULL,
		.vts            = 0x630,
		.hts            = 0x8a0,
	}
#endif
};

#define OV5648_N_WIN_SIZES (ARRAY_SIZE(ov5648_win_sizes))

static struct sensor_format_struct ov5648_formats[] = {
	{
		/*RAW10 FORMAT, 10 bit per pixel*/
		//.mbus_code	= V4L2_MBUS_FMT_SBGGR8_1X8,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	{
		.mbus_code      = V4L2_MBUS_FMT_YUYV10_2X10,
		.colorspace	= V4L2_COLORSPACE_BT878,/*don't know*/

	}
	/*add other format supported*/
};

#define N_OV5648_FMTS ARRAY_SIZE(ov5648_formats)

struct ov5648_otp_struct {
	int module_integrator_id;
	int lens_id;
	int rg_ratio;
	int bg_ratio;
	int user_data[2];
	int light_rg;
	int light_bg;
};

int ov5648_read(struct v4l2_subdev *sd, unsigned short reg, unsigned char *value)
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

int ov5648_write(struct v4l2_subdev *sd, unsigned short reg,  unsigned char value)
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

static int ov5648_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
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
			ret = ov5648_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	ov5648_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov5648_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov5648_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		//mdelay(200);
		vals++;
	}
	//ov5648_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov5648_read_otp(struct v4l2_subdev *sd, unsigned short reg)
{
	int ret = 0;
	int value = 0;
	unsigned char tmp = 0;
	ret = ov5648_read(sd, reg, &tmp);
	if(ret < 0) {
		return ret;
	}
	value = tmp;
	return value;
}

static int ov5648_write_otp(struct v4l2_subdev *sd, unsigned short reg, unsigned char value)
{
	int ret = 0;
	unsigned char tmp = value;
	ret = ov5648_write(sd, reg, tmp);
	if(ret < 0){
		return ret;
	}
	return ret;
}

// index: index of otp group. (1, 2, 3)
// return: 0, group index is empty
//         1, group index has invalid data
//         2, group index has valid data
static int check_otp_info(struct v4l2_subdev *sd, int index)
{
	int flag = 0, i = 0;
	int rg = 0, bg = 0;
	int lens_id = 0;
	if(index==1) {
		// read otp bank 0
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x00); // OTP strat address, bank 0
		ov5648_write_otp(sd, 0x3d86, 0x0f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01); // Bit[0] : OTP_rd, Read start signal 
		                                    //          Change from 0 to 1 initiates OTP read
		mdelay(1);
		flag = ov5648_read_otp(sd, 0x3d05);
		lens_id = ov5648_read_otp(sd, 0x3d06);
		rg = ov5648_read_otp(sd, 0x3d07);
		bg = ov5648_read_otp(sd, 0x3d08);
	}
	else if(index==2) {
		// read otp bank 0
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x00); // OTP strat address, bank 0
		ov5648_write_otp(sd, 0x3d86, 0x0f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		flag = ov5648_read_otp(sd, 0x3d0e);
		// read otp bank 1
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x10); // OTP strat address, bank 1
		ov5648_write_otp(sd, 0x3d86, 0x1f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		rg = ov5648_read_otp(sd, 0x3d00);
		bg = ov5648_read_otp(sd, 0x3d01);
	}else if(index==3) {
		// read otp into buffer
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x10); // OTP strat address, bank 1
		ov5648_write_otp(sd, 0x3d86, 0x1f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		flag = ov5648_read_otp(sd, 0x3d07);
		rg = ov5648_read_otp(sd, 0x3d09);
		bg = ov5648_read_otp(sd, 0x3d0a);
	}
	// disable otp read
	ov5648_write_otp(sd, 0x3d81, 0x00);
	// clear otp buffer
	for (i=0;i<16;i++) {
		ov5648_write_otp(sd, 0x3d00 + i, 0x00);
	}
	flag = flag & 0x80;
	if (flag) {
		return 1;
	} else {
		if ((rg == 0) | (bg == 0)) {
			return 0;
		}else {
			return 2;
		}
	}
}

// index: index of otp group. (1, 2, 3)
// otp_ptr: pointer of otp_struct
//          return: 0,
static int read_otp_info(struct v4l2_subdev *sd, int index, struct ov5648_otp_struct *otp_ptr)
{
	int i, temp;
	int address;
	if(index==1) {
		// read otp into buffer
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x00); // OTP strat address, bank 0
		ov5648_write_otp(sd, 0x3d86, 0x0f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		address = 0x3d05;
		(*otp_ptr).module_integrator_id = ov5648_read_otp(sd, address) & 0x7f;
		(*otp_ptr).lens_id = ov5648_read_otp(sd, (address + 1));
		(*otp_ptr).user_data[0] = ov5648_read_otp(sd, (address + 4));
		(*otp_ptr).user_data[1] = ov5648_read_otp(sd, (address + 5));
		temp = ov5648_read_otp(sd, (address + 6));
		(*otp_ptr).rg_ratio = ((ov5648_read_otp(sd, (address + 2)))<<2) + (temp>>6);
		(*otp_ptr).bg_ratio = ((ov5648_read_otp(sd, (address + 3)))<<2) + ((temp>>4) & 0x03);
		(*otp_ptr).light_rg = ((ov5648_read_otp(sd, (address + 7)))<<2) + ((temp>>2) & 0x03);
		(*otp_ptr).light_bg = ((ov5648_read_otp(sd, (address + 8)))<<2) + (temp & 0x03);

		// disable otp read
		ov5648_write_otp(sd, 0x3d81, 0x00);
		// clear otp buffer
		for (i=0;i<16;i++) {
			ov5648_write_otp(sd, 0x3d00 + i, 0x00);
		}
	}else if(index==2) {
		// read otp into buffer
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x00); // OTP strat address, bank 0
		ov5648_write_otp(sd, 0x3d86, 0x0f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		address = 0x3d0e;
		(*otp_ptr).module_integrator_id = ov5648_read_otp(sd, address) & 0x7f;
		(*otp_ptr).lens_id = ov5648_read_otp(sd, (address + 1));
		// clear otp buffer
		for (i=0;i<16;i++) {
			ov5648_write_otp(sd, 0x3d00 + i, 0x00);
		}
		// read otp into buffer
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x10); // OTP strat address, bank 1
		ov5648_write_otp(sd, 0x3d86, 0x1f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(10);
		address = 0x3d00;
		(*otp_ptr).user_data[0] = ov5648_read_otp(sd, (address + 2));
		(*otp_ptr).user_data[1] = ov5648_read_otp(sd, (address + 3));
		temp = ov5648_read_otp(sd, (address + 4));
		(*otp_ptr).rg_ratio = ((ov5648_read_otp(sd, address))<<2) + (temp>>6);
		(*otp_ptr).bg_ratio = ((ov5648_read_otp(sd, (address + 1)))<<2) + ((temp>>4) & 0x03);
		(*otp_ptr).light_rg = ((ov5648_read_otp(sd, (address + 5)))<<2) + ((temp>>2) & 0x03);
		(*otp_ptr).light_bg = ((ov5648_read_otp(sd, (address + 6)))<<2) + (temp & 0x03);
		
		// disable otp read
		ov5648_write_otp(sd, 0x3d81, 0x00);
		// clear otp buffer
		for (i=0;i<16;i++) {
			ov5648_write_otp(sd, 0x3d00 + i, 0x00);
		}
	}else if (index==3) {
		// read otp into buffer
		ov5648_write_otp(sd, 0x3d84, 0xc0);
		ov5648_write_otp(sd, 0x3d85, 0x10); // OTP strat address, bank 1
		ov5648_write_otp(sd, 0x3d86, 0x1f); // OTP end address
		ov5648_write_otp(sd, 0x3d81, 0x01);
		mdelay(1);
		address = 0x3d07;
		(*otp_ptr).module_integrator_id = ov5648_read_otp(sd,(address)) & 0x7f;
		(*otp_ptr).lens_id = ov5648_read_otp(sd, (address + 1));
		(*otp_ptr).user_data[0] = ov5648_read_otp(sd, (address + 4));
		(*otp_ptr).user_data[1] = ov5648_read_otp(sd, (address + 5));
		temp = ov5648_read_otp(sd, (address + 6));
		(*otp_ptr).rg_ratio = ((ov5648_read_otp(sd, (address + 2)))<<2) + (temp>>6);
		(*otp_ptr).bg_ratio = ((ov5648_read_otp(sd, (address + 3)))<<2) + ((temp>>4) & 0x03);
		(*otp_ptr).light_rg = ((ov5648_read_otp(sd, (address + 7)))<<2) + ((temp>>2) & 0x03);
		(*otp_ptr).light_bg = ((ov5648_read_otp(sd, (address + 8)))<<2) + (temp & 0x03);

		// disable otp read
		ov5648_write_otp(sd, 0x3d81, 0x00);
		// clear otp buffer
		for (i=0;i<16;i++) {
			ov5648_write_otp(sd, 0x3d00 + i, 0x00);
		}
	}
	
	return 0;
}

// R_gain, sensor red gain of AWB, 0x400 =1
// G_gain, sensor green gain of AWB, 0x400 =1
// B_gain, sensor blue gain of AWB, 0x400 =1
// return 0;
static int ov5648_update_awb_gain(struct v4l2_subdev *sd, int R_gain, int G_gain, int B_gain)
{
	if (R_gain > 0x400) {
		ov5648_write_otp(sd, 0x5186, R_gain>>8);
		ov5648_write_otp(sd, 0x5187, R_gain & 0x00ff);
	}
	if (G_gain > 0x400) {
		ov5648_write_otp(sd, 0x5188, G_gain>>8);
		ov5648_write_otp(sd, 0x5189, G_gain & 0x00ff);
	}
	if (B_gain > 0x400) {
		ov5648_write_otp(sd, 0x518a, B_gain>>8);
		ov5648_write_otp(sd, 0x518b, B_gain & 0x00ff);
	}
	return 0;
}

// call this function after OV5648 initialization
// return value: 0 update success
//               1, no OTP
static int ov5648_update_otp(struct v4l2_subdev *sd)
{
	struct ov5648_otp_struct current_otp;
	int i;
	int otp_index;
	int temp;
	int R_gain, G_gain, B_gain, G_gain_R, G_gain_B;
	int rg,bg;
	// R/G and B/G of current camera module is read out from sensor OTP
	// check first OTP with valid data
	for(i=1;i<=3;i++) {
		temp = check_otp_info(sd, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	printk("otp_info valid num = %d\n",i);
	if (i>3) {
		// no valid wb OTP data
		return 1;
	}
	read_otp_info(sd, otp_index, &current_otp);
	if (current_otp.light_rg == 0) {
		rg = current_otp.rg_ratio;
	}
	else {
		rg = current_otp.rg_ratio * (current_otp.light_rg +512) / 1024;
	}
	if (current_otp.light_bg == 0) {
		bg = current_otp.bg_ratio;
	}
	else {
		bg = current_otp.bg_ratio * (current_otp.light_bg +512) / 1024;
	}
	//calculate G gain
	//0x400 = 1x gain
	if(bg < OV5648_BG_Ratio_Typical) {
		if (rg< OV5648_RG_Ratio_Typical) {
			G_gain = 0x400;
			B_gain = 0x400 * OV5648_BG_Ratio_Typical / bg;
			R_gain = 0x400 * OV5648_RG_Ratio_Typical / rg;
		}
		else {
			R_gain = 0x400;
			G_gain = 0x400 * rg / OV5648_RG_Ratio_Typical;
			B_gain = G_gain * OV5648_BG_Ratio_Typical /bg;
		}
	}
	else {
		if (rg < OV5648_RG_Ratio_Typical) {
			B_gain = 0x400;
			G_gain = 0x400 * bg / OV5648_BG_Ratio_Typical;
			R_gain = G_gain * OV5648_RG_Ratio_Typical / rg;
		}
		else {
			G_gain_B = 0x400 * bg / OV5648_BG_Ratio_Typical;
			G_gain_R = 0x400 * rg / OV5648_RG_Ratio_Typical;
			if(G_gain_B > G_gain_R ) {
				B_gain = 0x400;
				G_gain = G_gain_B;
				R_gain = G_gain * OV5648_RG_Ratio_Typical /rg;
			}
			else {
				R_gain = 0x400;
				G_gain = G_gain_R;
				B_gain = G_gain * OV5648_BG_Ratio_Typical / bg;
			}
		}
	}
	printk("R_gain = %d, G_gain = %d, B_gain = %d \n",R_gain, G_gain, B_gain);
	ov5648_update_awb_gain(sd, R_gain, G_gain, B_gain);
	return 0;
}

static int ov5648_reset(struct v4l2_subdev *sd, u32 val)
{
#if 0
	/* int ret; */
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	ret = ov5648_write_array(sd, ov5648_softreset);
	if(ret < 0){
		return ret;
	}
#endif
	return 0;
}

static int ov5648_init(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;
	/* struct sensor_info *info = container_of(sd, struct sensor_info, sd); */

	//software sleep
	/* ov5648_write_array(sd, ov5648_softsleep); */

	//software reset
	ov5648_write_array(sd, ov5648_softreset);

	/* mdelay(5); */

	/* ov5648_write(sd, 0x0100, 0x00); */
	/* ov5648_write(sd, 0x0103, 0x01); */
	mdelay(5);

	ret = ov5648_write_array(sd, ov5648_init_raw10_regs);
	if (ret < 0)
		return ret;

	ret = ov5648_write_array(sd, ov5648_stream_on);
	if(ret < 0){
	    SENSOR_PRINT(CAMERA_ERROR,"ov5648 stream on failed in init period err = %d\n",ret);
		return ret;
	}

	ov5648_update_otp(sd);

	ret = ov5648_write_array(sd, ov5648_stream_off);
	if(ret < 0){
	    SENSOR_PRINT(CAMERA_ERROR,"ov5648 stream off failed in init period err = %d\n",ret);
		return ret;
	}

	return 0;
}

static int ov5648_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
				enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_OV5648_FMTS)
		return -EINVAL;

	*code = ov5648_formats[index].mbus_code;
	return 0;
}

static int ov5648_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   struct sensor_win_setting **ret_wsize)
{
	struct sensor_win_setting *wsize;

	if (fmt->width > OV5648_MAX_WIDTH || fmt->height > OV5648_MAX_HEIGHT)
		return -EINVAL;
	for (wsize = ov5648_win_sizes; wsize < ov5648_win_sizes + OV5648_N_WIN_SIZES; wsize++){
	  //printk("try fmt %d %d %d %d\n", fmt->width, wsize->width, fmt->height, wsize->height);
		if (fmt->width > wsize->width && fmt->height > wsize->height)
			break;
	}
	/* if (wsize >= ov5648_win_sizes + OV5648_N_WIN_SIZES) */
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
	/* printk("%s:------->%d fmt->code,%08X , fmt->width%d fmt->height%d\n", __func__, __LINE__, fmt->code, fmt->width, fmt->height); */
	return 0;
}

static int ov5648_g_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	data->i2cflags = V4L2_I2C_ADDR_16BIT;
	/* data->sensor_sys_pclk = info->using_win->sysclk; */
	/* data->vts = info->using_win->vts; */
	/* data->hts = info->using_win->hts; */
	data->sensor_sys_pclk = info->trying_win->sysclk;
	data->vts = info->trying_win->vts;
	data->hts = info->trying_win->hts;
	data->lans = 2;

	return 0;
}
static int ov5648_try_mbus_fmt(struct v4l2_subdev *sd,
			       struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	ret = ov5648_try_fmt_internal(sd, fmt, &info->trying_win);
	return ret;
}

static int ov5648_s_mbus_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	/* struct sensor_win_setting *wsize; */
	int ret;

	/* printk("[ov5648], problem function:%s, line:%d\n", __func__, __LINE__); */
	/* ret = ov5648_try_fmt_internal(sd, fmt, &wsize); */
	/* if (ret) */
	/* 	return ret; */
	/* if (info->using_win) */
	/* 	printk("info->using_win->width =%d \n",info->using_win->width); */
	/* if (wsize) */
	/* 	printk("wsize->width =%d \n",wsize->width); */
	/* if ((info->using_win != wsize) && wsize->regs) { */
	/* 	printk("pay attention : ov5648, %s:LINE:%d  size = %d\n", __func__, __LINE__, sizeof(*wsize->regs)); */
	/* 	ret = ov5648_write_array(sd, wsize->regs); */
	/* 	if (ret) */
	/* 		return ret; */
	/* } */

	if (info->using_win != info->trying_win) {
		if(info->write_mode == GROUP_LUNCH){
		    ret = ov5648_write_array(sd, info->trying_win->lunch_regs);
		}else{
		    ret = ov5648_write_array(sd, info->trying_win->regs);
		}
		if (ret)
			return ret;
	}

	info->using_win = info->trying_win;

	ret = ov5648_g_mbus_fmt(sd, fmt);
	if (ret)
		return ret;

	return 0;
}

static int ov5648_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	printk("--------%s: %d enable:%d\n", __func__, __LINE__, enable);
	if (enable) {
		ret = ov5648_write_array(sd, ov5648_stream_on);
		if(ret < 0) {
			printk("ov5648 stream on failed ret = %d\n",ret);
		}
		else {
			printk("ov5648 stream on\n");
		}
	}
	else {
		ret = ov5648_write_array(sd, ov5648_stream_off);
		info->write_mode = OV5648_DEFAULT_REG_WRTIE_MODE;
		printk("ov5648 stream off\n");
	}
	return ret;
}

static int ov5648_frame_rates[] = { 30, 15, 10, 5, 1 };

static int ov5648_enum_frameintervals(struct v4l2_subdev *sd,
				      struct v4l2_frmivalenum *interval)
{
	printk("functiong:%s, line:%d\n", __func__, __LINE__);
	if (interval->index >= ARRAY_SIZE(ov5648_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov5648_frame_rates[interval->index];
	return 0;
}

static int ov5648_enum_framesizes(struct v4l2_subdev *sd,
				  struct v4l2_frmsizeenum *fsize)
{
	__u32 index = fsize->index;

	printk("functiong:%s, line:%d\n", __func__, __LINE__);
	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	if (index < OV5648_N_WIN_SIZES) {
		struct sensor_win_setting *win = &ov5648_win_sizes[index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = win->width;
		fsize->discrete.height = win->height;
		return 0;
	} else {
		return -EINVAL;
	}

	return -EINVAL;
}
