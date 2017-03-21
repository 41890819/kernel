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
#include "sensor.h"

#define GC1004_IIC_ADDR (0x3c >> 1)

#define OV5640_CHIP_ID_H	(0x56)
#define OV5640_CHIP_ID_L	(0x40)

#define OV5640_CHIP_ID_H_REG	(0x300a)
#define OV5640_CHIP_ID_L_REG	(0x300b)

#define MAX_WIDTH		2592
#define MAX_HEIGHT		1944

static struct regval_list ov5640_stream_on[] = {
	{0x4202, 0x00},
	
	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5640_stream_off[] = {
	{0x4202, 0x0f},

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list ov5640_init_regs[] = {
	/* {0x3103, 0x11}, // SCCB system control */
	/* {0x3008, 0x82}, // software reset */

	{0x4202, 0x0f}, // The spec has not set this reg.But is necessary.
	// delay 5ms
	{0x3008, 0x42}, // software power down
	{0x3103, 0x03}, // SCCB system control
	{0x3017, 0x00}, // set Frex, Vsync, Href, PCLK, D[9:6] input
	{0x3018, 0x00}, // set d[5:0], GPIO[1:0] input
	{0x3034, 0x18}, // MIPI 8-bit mode
	{0x3037, 0x13}, // PLL
	{0x3108, 0x01}, // system divider
	{0x3630, 0x36},
	{0x3631, 0x0e},
	{0x3632, 0xe2},
	{0x3633, 0x12},
	{0x3621, 0xe0},
	{0x3704, 0xa0},
	{0x3703, 0x5a},
	{0x3715, 0x78},
	{0x3717, 0x01},
	{0x370b, 0x60},
	{0x3705, 0x1a},
	{0x3905, 0x02},
	{0x3906, 0x10},

	/* OV5640 Auto FocusCamera Module Application Notes */
	{0x3901, 0x0a},
	{0x3731, 0x12},
	{0x3600, 0x08}, // VCM debug mode
	{0x3601, 0x33}, // VCM debug mode
	{0x302d, 0x60}, // system control
	{0x3620, 0x52},
	{0x371b, 0x20},
	{0x471c, 0x50},
	{0x3a13, 0x43}, // AGC pre-gain, 0x40 = 1x
	{0x3a18, 0x00}, // gain ceiling
	{0x3a19, 0xf8}, // gain ceiling
	{0x3635, 0x13},
	{0x3636, 0x03},
	{0x3634, 0x40},
	{0x3622, 0x01},
	// 50Hz/60Hz
	{0x3c01, 0x34},
	{0x3c04, 0x28},
	{0x3c05, 0x98},
	{0x3c06, 0x00},
	{0x3c08, 0x00},
	{0x3c09, 0x1c},
	{0x3c0a, 0x9c},
	{0x3c0b, 0x40},
	// 50/60Hz
	// threshold for low sum
	// threshold for high sum
	// light meter 1 threshold high
	// light meter 2 threshold high
	// light meter 2 threshold low
	// sample number high
	// sample number low
	// timing
	{0x3800, 0x00}, // HS
	{0x3801, 0x00}, // HS
	{0x3802, 0x00}, // VS
	{0x3804, 0x0a}, // HW
	{0x3805, 0x3f}, // HW
	{0x3810, 0x00}, // H offset high
	{0x3811, 0x10}, // H offset low
	{0x3812, 0x00}, // V offset high
	{0x3708, 0x64},
	{0x3a08, 0x01}, // B50
	{0x4001, 0x02}, // BLC start line
	{0x4005, 0x18}, // BLC gain/level trigger
	{0x4050, 0x6e}, //
	{0x4051, 0x8f}, //
	{0x3000, 0x00}, // system reset 0
	{0x3002, 0x1c}, // system reset 2
	{0x3004, 0xff}, // clock enable 00
	{0x3006, 0xc3}, // clock enable 2
	{0x300e, 0x45}, // MIPI control, 2 lane, MIPI enable
	{0x302e, 0x08},
	{0x4300, 0x30}, // YUV 422, YUYV

	{0x501f, 0x00}, // ISP YUV 422
	{0x4407, 0x04}, // JPEG QS
	{0x440e, 0x00},
	{0x5000, 0xa7}, // ISP control, Lenc on, gamma on, BPC on, WPC on, CIP on
	// AWB
	{0x5180, 0xff},
	{0x5181, 0xf2},
	{0x5182, 0x00},
	{0x5183, 0x14},
	{0x5184, 0x25},
	{0x5185, 0x24},
	{0x5186, 0x09},
	{0x5187, 0x09},
	{0x5188, 0x09},
	{0x5189, 0x75},
	{0x518a, 0x54},
	{0x518b, 0xe0},
	{0x518c, 0xb2},
	{0x518d, 0x42},
	{0x518e, 0x3d},
	{0x518f, 0x56},
	{0x5190, 0x46},
	{0x5191, 0xf8},
	{0x5192, 0x04},
	{0x5193, 0x70},
	{0x5194, 0xf0},
	{0x5195, 0xf0},
	{0x5196, 0x03},
	{0x5197, 0x01},
	{0x5198, 0x04},
	{0x5199, 0x12},
	{0x519a, 0x04},
	{0x519b, 0x00},
	{0x519c, 0x06},
	{0x519d, 0x82},
	{0x519e, 0x38},
	// color matrix
	{0x5381, 0x1e},
	{0x5382, 0x5b},
	{0x5383, 0x08},
	{0x5384, 0x0a},
	{0x5385, 0x7e},
	{0x5386, 0x88},
	{0x5387, 0x7c},
	{0x5388, 0x6c},
	{0x5389, 0x10},

	{0x538a, 0x01},
	{0x538b, 0x98},
	// CIP
	{0x5300, 0x08}, // sharpen MT th1
	{0x5301, 0x30}, // sharpen MT th2
	{0x5302, 0x10}, // sharpen MT offset 1
	{0x5303, 0x00}, // sharpen MT offset 2
	{0x5304, 0x08}, // DNS threshold 1
	{0x5305, 0x30}, // DNS threshold 2
	{0x5306, 0x08}, // DNS offset 1
	{0x5307, 0x16}, // DNS offset 2
	{0x5309, 0x08}, // sharpen TH th1
	{0x530a, 0x30}, // sharpen TH th2
	{0x530b, 0x04}, // sharpen TH offset 1
	{0x530c, 0x06}, // sharpen Th offset 2
	// gamma
	{0x5480, 0x01},
	{0x5481, 0x08},
	{0x5482, 0x14},
	{0x5483, 0x28},
	{0x5484, 0x51},
	{0x5485, 0x65},
	{0x5486, 0x71},
	{0x5487, 0x7d},
	{0x5488, 0x87},
	{0x5489, 0x91},
	{0x548a, 0x9a},
	{0x548b, 0xaa},
	{0x548c, 0xb8},
	{0x548d, 0xcd},
	{0x548e, 0xdd},
	{0x548f, 0xea},
	{0x5490, 0x1d},
	// UV adjust
	{0x5580, 0x06}, // sat on, contrast on
	{0x5583, 0x40}, // sat U
	{0x5584, 0x10}, // sat V
	{0x5589, 0x10}, // UV adjust th1
	{0x558a, 0x00}, // UV adjust th2[8]
	{0x558b, 0xf8}, // UV adjust th2[7:0]
	{0x501d, 0x04}, // enable manual offset of contrast
	// lens correction
	{0x5800, 0x23},
	{0x5801, 0x14},

	{0x5802, 0x0f},
	{0x5803, 0x0f},
	{0x5804, 0x12},
	{0x5805, 0x26},
	{0x5806, 0x0c},
	{0x5807, 0x08},
	{0x5808, 0x05},
	{0x5809, 0x05},
	{0x580a, 0x08},
	{0x580b, 0x0d},
	{0x580c, 0x08},
	{0x580d, 0x03},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x03},
	{0x5811, 0x09},
	{0x5812, 0x07},
	{0x5813, 0x03},
	{0x5814, 0x00},
	{0x5815, 0x01},
	{0x5816, 0x03},
	{0x5817, 0x08},
	{0x5818, 0x0d},
	{0x5819, 0x08},
	{0x581a, 0x05},
	{0x581b, 0x06},
	{0x581c, 0x08},
	{0x581d, 0x0e},
	{0x581e, 0x29},
	{0x581f, 0x17},
	{0x5820, 0x11},
	{0x5821, 0x11},
	{0x5822, 0x15},
	{0x5823, 0x28},
	{0x5824, 0x46},
	{0x5825, 0x26},
	{0x5826, 0x08},
	{0x5827, 0x26},
	{0x5828, 0x64},
	{0x5829, 0x26},
	{0x582a, 0x24},
	{0x582b, 0x22},
	{0x582c, 0x24},
	{0x582d, 0x24},
	{0x582e, 0x06},
	{0x582f, 0x22},
	{0x5830, 0x40},
	{0x5831, 0x42},

	{0x5832, 0x24},
	{0x5833, 0x26},
	{0x5834, 0x24},
	{0x5835, 0x22},
	{0x5836, 0x22},
	{0x5837, 0x26},
	{0x5838, 0x44},
	{0x5839, 0x24},
	{0x583a, 0x26},
	{0x583b, 0x28},
	{0x583c, 0x42},
	{0x583d, 0xce},
	{0x5025, 0x00},
	{0x3a0f, 0x30}, // stable in high
	{0x3a10, 0x28}, // stable in low
	{0x3a1b, 0x30}, // stable out high
	{0x3a1e, 0x26}, // stable out low
	{0x3a11, 0x60}, // fast zone high
	{0x3a1f, 0x14}, // fast zone low
	{0x3008, 0x02}, // wake up

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list ov5640_init_480p_yuyv_regs[] = {
	//input 24M
	//output vga 30fps bit rate 224M bps
	{0x3035, 0x14}, // pll
	{0x3036, 0x38}, // pll
	{0x3c07, 0x08}, // light meter 1 threshold
	{0x3820, 0x41}, // ISP flip off, sensor flip off
	{0x3821, 0x07}, // ISP mirror on, sensor mirror on
	// timing
	{0x3814, 0x31}, // X inc
	{0x3815, 0x31}, // Y inc
	{0x3803, 0x04}, // VS
	{0x3806, 0x07}, // VH
	{0x3807, 0x9b}, // VH
	{0x3808, 0x02}, // DVPHO
	{0x3809, 0x80}, // DVPHO
	{0x380a, 0x01}, // DVPVO
	{0x380b, 0xe0}, // DVPVO
	{0x380c, 0x07}, // HTS
	{0x380d, 0x68}, // HTS
	{0x380e, 0x03}, // VTS
	{0x380f, 0xd8}, // VTS
	{0x3813, 0x06}, // V offset

	{0x3618, 0x00},
	{0x3612, 0x29},
	{0x3709, 0x52},
	{0x370c, 0x03},
	{0x3a02, 0x03},
	{0x3a03, 0xd8},
	{0x3a09, 0x27},
	{0x3a0a, 0x00},
	{0x3a0b, 0xf6},
	{0x3a0e, 0x03},
	{0x3a0d, 0x04},
	{0x3a14, 0x03},
	{0x3a15, 0xd8},
	// 60Hz max exposure
	// 60Hz max exposure
	// B50 low
	// B60 high
	// B60 low
	// B50 max
	// B60 max
	// 50Hz max exposure
	// 50Hz max exposure
	{0x4004, 0x02}, // BLC line number
	{0x4005, 0x18}, // BLC gain/level trigger
	{0x4713, 0x03}, // JPEG mode 3
	{0x460b, 0x35}, // debug
	{0x460c, 0x22}, // VFIFO, PCLK manual
	{0x4837, 0x44}, // MIPI global timing
	{0x3824, 0x02}, // PCLK divider
	{0x5001, 0xa3}, // SDE on, scale on, UV average off, CMX on, AWB on

	{SENSOR_REG_END, 0x00},	/* END MARKER */
};

static struct sensor_win_setting ov5640_win_sizes[] = {
	/* 640*480 */
	{
		.width		= 640,
		.height		= 480,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= ov5640_init_480p_yuyv_regs,
		.reg_sum        = ARRAY_SIZE(ov5640_init_480p_yuyv_regs),
	}
};
#define N_WIN_SIZES (ARRAY_SIZE(ov5640_win_sizes))

static struct sensor_format_struct ov5640_formats[] = {
	{
		/*YUYV FORMAT, 16 bit per pixel*/
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	/*add other format supported*/
};
#define N_ov5640_FMTS ARRAY_SIZE(ov5640_formats)

int ov5640_read(struct v4l2_subdev *sd, unsigned short reg, unsigned char *value)
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

static int ov5640_write(struct v4l2_subdev *sd, unsigned short reg, unsigned char value)
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

static int ov5640_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SENSOR_REG_END || vals->value != 0) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = ov5640_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, val);
		//mdelay(200);
		vals++;
	}
	//printk("vals->reg_num:%x, vals->value:%x\n", vals->reg_num, vals->value);
	ov5640_write(sd, vals->reg_num, vals->value);
	return 0;
}

static int ov5640_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SENSOR_REG_END || vals->value != 0) {
		if (vals->reg_num == SENSOR_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
			//printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value);
		} else {
			ret = ov5640_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		//printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value);
		//mdelay(200);
		vals++;
	}
	//printk("vals->reg_num:%x, vals->value:%x\n", vals->reg_num, vals->value);
	//ov5640_write(sd, vals->reg_num, vals->value);
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x58;
static unsigned int bg_ratio_typical = 0x5a;

static int ov5640_reset(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;
	/* ov5640_write(sd, 0xfffd, 0x80); */
	/* ov5640_write(sd, 0xfffe, 0x80); */
	/* ov5640_write(sd, 0x0018, 0xff); */
	return ret;
}

static int ov5640_detect(struct v4l2_subdev *sd);
static int ov5640_init(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;

	ov5640_write(sd, 0x3103, 0x11);
	ov5640_write(sd, 0x3008, 0x82);
	mdelay(5);
	
	ov5640_write_array(sd, ov5640_init_regs);

	if (ret < 0)
		return ret;

	return 0;
}
static int ov5640_get_sensor_vts(struct v4l2_subdev *sd, unsigned short *value)
{
	unsigned char h,l;
	int ret = 0;
	ret = ov5640_read(sd, 0x380e, &h);
	if (ret < 0)
		return ret;
	ret = ov5640_read(sd, 0x380f, &l);
	if (ret < 0)
		return ret;
	*value = h;
	*value = (*value << 8) | l;

	return ret;
}

static int ov5640_get_sensor_lans(struct v4l2_subdev *sd, unsigned char *pvalue)
{
	int ret = 0;
	unsigned char v = 0;
	ret = ov5640_read(sd, 0x300e, &v);
	if (ret < 0)
		return ret;
	*pvalue = (v >> 5);
	if(*pvalue > 2 || *pvalue < 1)
		ret = -EINVAL;

	return ret;
}

static int ov5640_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	ret = ov5640_read(sd, OV5640_CHIP_ID_H_REG, &v);
	if (ret < 0)
		return ret;
	printk("-----%s: %d v = %08X\n", __func__, __LINE__, v);
	if (v != OV5640_CHIP_ID_H)
		return -ENODEV;
	ret = ov5640_read(sd, OV5640_CHIP_ID_L_REG, &v);
	if (ret < 0)
		return ret;
	printk("-----%s: %d v = %08X\n", __func__, __LINE__, v);
	if (v != OV5640_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static int ov5640_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_ov5640_FMTS)
		return -EINVAL;

	*code = ov5640_formats[index].mbus_code;
	return 0;
}

static int ov5640_try_fmt_internal(struct v4l2_subdev *sd,
				   struct v4l2_mbus_framefmt *fmt,
				   struct sensor_win_setting **ret_wsize)
{
	struct sensor_win_setting *wsize;

	if(fmt->width > MAX_WIDTH || fmt->height > MAX_HEIGHT)
		return -EINVAL;
	for (wsize = ov5640_win_sizes; wsize < ov5640_win_sizes + N_WIN_SIZES; wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;
	if (wsize >= ov5640_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->mbus_code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = wsize->colorspace;
	return 0;
}

static int ov5640_g_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	int ret;
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);

	data->i2cflags = V4L2_I2C_ADDR_16BIT;
	data->sensor_sys_pclk = 282;
	ret = ov5640_get_sensor_vts(sd, &(data->vts));
	if (ret < 0) {
		printk("[ov5640], problem function:%s, line:%d\n", __func__, __LINE__);
		return ret;
	}
	ret = ov5640_get_sensor_lans(sd, &(data->lans));
	if (ret < 0) {
		printk("[ov5640], problem function:%s, line:%d\n", __func__, __LINE__);
		return ret;
	}

	return 0;
}

static int ov5640_try_mbus_fmt(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt)
{
	return ov5640_try_fmt_internal(sd, fmt, NULL);
}

static int ov5640_s_mbus_fmt(struct v4l2_subdev *sd,
			     struct v4l2_mbus_framefmt *fmt)
{
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);
	struct sensor_win_setting *wsize;
	int ret;

	ret = ov5640_try_fmt_internal (sd, fmt, &wsize);
	if (ret)
		return ret;

	if (info->using_win)
		printk ("info->using_win->width =%d \n",info->using_win->width);
	if (wsize)
		printk ("wsize->width =%d \n",wsize->width);

	if ((info->using_win != wsize) && wsize->regs) {
		ret = ov5640_write_array (sd, wsize->regs);
		if (ret)
			return ret;
	}

	ret = ov5640_g_mbus_fmt(sd, fmt);
	if (ret)
		return ret;

	info->using_win = wsize;

	return 0;
}

static int ov5640_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		// Stream on
		ret = ov5640_write_array(sd, ov5640_stream_on);
		printk("ov5640 stream on\n");
	}
	else {
		ret = ov5640_write_array(sd, ov5640_stream_off);
		printk("ov5640 stream off\n");
	}
	return ret;
}

static int ov5640_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int ov5640_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int ov5640_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5640_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int ov5640_frame_rates[] = { 90, 60, 45, 30, 15 };

static int ov5640_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(ov5640_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = ov5640_frame_rates[interval->index];
	return 0;
}

static int ov5640_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	if (index < N_WIN_SIZES) {
		struct sensor_win_setting *win = &ov5640_win_sizes[index];
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = win->width;
		fsize->discrete.height = win->height;
		return 0;
	} else {
		return -EINVAL;
	}

	return -EINVAL;
}

static int ov5640_queryctrl(struct v4l2_subdev *sd,
			    struct v4l2_queryctrl *qc)
{
	return 0;
}

static int ov5640_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int ov5640_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int ov5640_g_chip_ident(struct v4l2_subdev *sd,
			       struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

//	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_ov5640, 0);
	return v4l2_chip_ident_i2c_client(client, chip, 123, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int ov5640_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = ov5640_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov5640_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ov5640_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static int ov5640_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static const struct v4l2_subdev_core_ops ov5640_core_ops = {
	.g_chip_ident = ov5640_g_chip_ident,
	.g_ctrl = ov5640_g_ctrl,
	.s_ctrl = ov5640_s_ctrl,
	.queryctrl = ov5640_queryctrl,
	.reset = ov5640_reset,
	.init = ov5640_init,
	.s_power = ov5640_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ov5640_g_register,
	.s_register = ov5640_s_register,
#endif
};

static const struct v4l2_subdev_video_ops ov5640_video_ops = {
	.enum_mbus_fmt = ov5640_enum_mbus_fmt,
	.try_mbus_fmt = ov5640_try_mbus_fmt,
	.s_mbus_fmt = ov5640_s_mbus_fmt,
	.g_mbus_fmt = ov5640_g_mbus_fmt,
	.s_stream = ov5640_s_stream,
	.cropcap = ov5640_cropcap,
	.g_crop	= ov5640_g_crop,
	.s_parm = ov5640_s_parm,
	.g_parm = ov5640_g_parm,
	.enum_frameintervals = ov5640_enum_frameintervals,
	.enum_framesizes = ov5640_enum_framesizes,
};

static const struct v4l2_subdev_ops ov5640_ops = {
	.core = &ov5640_core_ops,
	.video = &ov5640_video_ops,
};

static ssize_t ov5640_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t ov5640_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical = (unsigned int)value;

	return size;
}

static ssize_t ov5640_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t ov5640_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(ov5640_rg_ratio_typical, 0664, ov5640_rg_ratio_typical_show, ov5640_rg_ratio_typical_store);
static DEVICE_ATTR(ov5640_bg_ratio_typical, 0664, ov5640_bg_ratio_typical_show, ov5640_bg_ratio_typical_store);

static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int ret;

	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	
	info->win = ov5640_win_sizes;
	info->using_win = NULL;
	info->fmt = ov5640_formats;
	info->win_size = N_WIN_SIZES;

	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &ov5640_ops);

	/* Make sure it's an ov5640 */
	//aaa:
	ret = ov5640_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an ov5640 chip.\n",
			client->addr, client->adapter->name);
		//goto aaa;
		kfree(info);
		return ret;
	}
	v4l_info(client, "ov5640 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ret = device_create_file(&client->dev, &dev_attr_ov5640_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5640_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5640_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_ov5640_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_ov5640_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_ov5640_bg_ratio_typical;
	}

	printk("probe ok ------->ov5640\n");
	return 0;

err_create_dev_attr_ov5640_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_ov5640_rg_ratio_typical);
err_create_dev_attr_ov5640_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	device_remove_file(&client->dev, &dev_attr_ov5640_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_ov5640_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "ov5640",
	},
	.probe		= ov5640_probe,
	.remove		= ov5640_remove,
	.id_table	= ov5640_id,
};

static __init int init_ov5640(void)
{
	return i2c_add_driver(&ov5640_driver);
}

static __exit void exit_ov5640(void)
{
	i2c_del_driver(&ov5640_driver);
}

module_init(init_ov5640);
module_exit(exit_ov5640);

MODULE_DESCRIPTION("A low-level driver for OmniVision ov5640 sensors");
MODULE_LICENSE("GPL");
