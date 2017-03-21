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

#ifdef CONFIG_VIDEO_OV5648
#include "ov5648.h"
#endif
#ifdef CONFIG_VIDEO_OV8858
#include "ov8858.h"
#endif
#ifdef CONFIG_VIDEO_OV8865
#include "ov8865.h"
#endif
#ifdef CONFIG_VIDEO_OV8856
#include "ov8856.h"
#endif
#ifdef CONFIG_VIDEO_OV4689
#include "ov4689.h"
#endif
#ifdef CONFIG_VIDEO_OV6211
#include "ov6211.h"
#endif

static struct v4l2_subdev_core_ops back_camera_core_ops;
static struct v4l2_subdev_video_ops back_camera_video_ops;
struct sensor_info *hstp_camera_info;

static const struct v4l2_subdev_ops back_camera_ops = {
	.core = &back_camera_core_ops,
	.video = &back_camera_video_ops,
};

static int m200_sensor_read(struct v4l2_subdev *sd, unsigned short reg,
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
	int ret = -1;
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}


/* static  */
static int m200_sensor_write(struct v4l2_subdev *sd, unsigned short reg,
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
	int ret = -1;
	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int m200_sensor_type_detect(struct v4l2_subdev *sd)
{
	struct sensor_info *p = container_of(sd,struct sensor_info,sd);
    	unsigned char chip_id_0, chip_id_1, chip_id_2;
	int ret;

	ret = m200_sensor_read(sd, 0x300a, &chip_id_0);
	if (ret < 0)
		return ret;
	ret = m200_sensor_read(sd, 0x300b, &chip_id_1);
	if (ret < 0)
		return ret;
	ret = m200_sensor_read(sd, 0x300c, &chip_id_2);
	if (ret < 0)
		return ret;

	if (chip_id_0 == OV5648_CHIP_ID_H && chip_id_1 == OV5648_CHIP_ID_L) {
		p->chip_id = (chip_id_0 << 8) | chip_id_1;
	} else if (chip_id_1 == OV8858_CHIP_ID_H && chip_id_2 == OV8858_CHIP_ID_L) {
		p->chip_id = (chip_id_1 << 8) | chip_id_2;
	} else if (chip_id_1 == OV8856_CHIP_ID_H && chip_id_2 == OV8856_CHIP_ID_L) {
		p->chip_id = 0x8856;
	} else if (chip_id_1 == OV8865_CHIP_ID_H && chip_id_2 == OV8865_CHIP_ID_L) {
		p->chip_id = (chip_id_1 << 8) | chip_id_2;
	} else if (chip_id_0 == OV4689_CHIP_ID_H && chip_id_1 == OV4689_CHIP_ID_L) {
		p->chip_id = (chip_id_0 << 8) | 0x89;
	} else if (chip_id_0 == OV6211_CHIP_ID_H && chip_id_1 == OV6211_CHIP_ID_L) {
		p->chip_id = 0x6211;
	} else {
		SENSOR_PRINT(CAMERA_ERROR,"-----%s: none supported camera detect.\n", __func__);
		return -1;
	}
	if (p->chip_id == 0x8858) {
		ret = m200_sensor_read(sd, 0x302a, &chip_id_0);
		if (ret < 0)
			return ret;
		SENSOR_PRINT(CAMERA_WARNING,"-----%s: %d ov8858 reg 302a = %08X\n", 
			     __func__, __LINE__, chip_id_0);
	}

	return 0;
}

static int m200_sensor_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	struct sensor_info *p = container_of(sd,struct sensor_info,sd);           
	if ((p->chip_id >> 8) == 0x56) {
		a->c.width	= 2592;
		a->c.height	= 1920;
	} else if ((p->chip_id >> 8) == 0x88) {
		a->c.width	= 3264;
		a->c.height	= 2448;
	} else if (p->chip_id == 0x6211) {
		a->c.width	= 400;
		a->c.height	= 400;		
	} else {
		SENSOR_PRINT(CAMERA_ERROR,"functiong:%s no  line:%d ~~~\n", __func__,  __LINE__);
		return -1;
	}
	a->c.left	= 0;
	a->c.top	= 0;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int m200_sensor_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	struct sensor_info *p = container_of(sd,struct sensor_info,sd);           
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);

	if ((p->chip_id >> 8) == 0x56) {
		a->bounds.width = 2592;
		a->bounds.height = 1920;
	} else if ((p->chip_id >> 8) == 0x88) {
		a->bounds.width	= 3264;
		a->bounds.height = 2448;
	} else if ((p->chip_id >> 8) == 0x46) {
		a->bounds.width = 2688;
		a->bounds.height = 1520;
	} else if (p->chip_id == 0x6211) {
		a->bounds.width = 400;
		a->bounds.height = 400;
	} else {
		SENSOR_PRINT(CAMERA_ERROR,"functiong:%s, line:%d no ~~~\n", __func__, __LINE__);
		return -1;
	}
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int m200_sensor_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	return 0;
}

static int m200_sensor_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	return 0;
}

static int m200_sensor_queryctrl(struct v4l2_subdev *sd,
				 struct v4l2_queryctrl *qc)
{
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	return 0;
}

static int m200_sensor_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
        return 0;
}

static int m200_sensor_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	SENSOR_PRINT(CAMERA_INFO,"functiong:%s, line:%d\n", __func__, __LINE__);
	return 0;
}

static int m200_sensor_s_check_id(struct v4l2_subdev *sd,struct v4l2_control *ctrl)
{
	struct sensor_info *p = container_of(sd,struct sensor_info,sd);           
	return p->chip_id;
}

static int m200_sensor_g_chip_ident(struct v4l2_subdev *sd,
				    struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("functiong:%s, line:%d\n", __func__, __LINE__);
	return v4l2_chip_ident_i2c_client(client, chip, 123, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int m200_sensor_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = m200_sensor_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int ov8856_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	m200_sensor_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static int back_camera_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct sensor_info *info;
	int ret;
	info = kzalloc(sizeof(struct sensor_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;

	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &back_camera_ops);
	/* Make sure it's an supported camera */
	ret = m200_sensor_type_detect(sd);
	if (ret) {
		v4l_err(client, "chip found @ 0x%x (%s) is not an supported type.\n",
			client->addr, client->adapter->name);
		kfree(info);
		return ret;
	}
	v4l_info(client, "back camera type OV%04x found @ 0x%02x (%s)\n",
		 info->chip_id, client->addr, client->adapter->name);


	back_camera_core_ops.g_ctrl = m200_sensor_g_ctrl;
	back_camera_core_ops.s_ctrl = m200_sensor_s_ctrl;
	back_camera_core_ops.queryctrl = m200_sensor_queryctrl;
	back_camera_core_ops.s_ctrl_iic = m200_sensor_s_check_id;
	back_camera_core_ops.g_chip_ident = m200_sensor_g_chip_ident;
#ifdef CONFIG_VIDEO_ADV_DEBUG
	back_camera_core_ops.g_register = m200_sensor_g_register;
	back_camera_core_ops.s_register = m200_sensor_s_register;
#endif
	back_camera_video_ops.s_parm = m200_sensor_s_parm;
	back_camera_video_ops.g_parm = m200_sensor_g_parm;
	back_camera_video_ops.cropcap = m200_sensor_cropcap;
	back_camera_video_ops.g_crop = m200_sensor_g_crop;

	if (info->chip_id == 0x8858) {
#ifdef CONFIG_VIDEO_OV8858
		info->win = ov8858_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov8858_formats;
		info->win_size = OV8858_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;

		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV8858_DEFAULT_REG_WRTIE_MODE;
#ifdef CONFIG_VIDEO_JZ_HS_ISP
		info->hstp_preview_setting = &ov8858_win_sizes[2];
#else
		info->hstp_preview_setting = &ov8858_win_sizes[1];
#endif
		info->hstp_photo_setting = &ov8858_win_sizes[0];

		back_camera_core_ops.s_ctrl_1 = ov8858_s_ctrl_1;
		back_camera_core_ops.reset = ov8858_reset;
		back_camera_core_ops.init = ov8858_init;

		back_camera_video_ops.enum_mbus_fmt = ov8858_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov8858_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov8858_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov8858_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov8858_s_stream;
		back_camera_video_ops.enum_frameintervals = ov8858_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov8858_enum_framesizes;
#endif
	} else if (info->chip_id == 0x8856) {
#ifdef CONFIG_VIDEO_OV8856
		info->win = ov8856_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov8856_formats;
		info->win_size = OV8856_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;
	    
		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV8856_DEFAULT_REG_WRTIE_MODE;
		info->hstp_preview_setting = &ov8856_win_sizes[1];
		info->hstp_photo_setting = &ov8856_win_sizes[0];

		back_camera_core_ops.s_ctrl_1 = ov8856_s_ctrl_1;
		back_camera_core_ops.reset = ov8856_reset;
		back_camera_core_ops.init = ov8856_init;

		back_camera_video_ops.enum_mbus_fmt = ov8856_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov8856_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov8856_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov8856_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov8856_s_stream;
		back_camera_video_ops.enum_frameintervals = ov8856_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov8856_enum_framesizes;
#endif
	} else if (info->chip_id == 0x8865) {
#ifdef CONFIG_VIDEO_OV8865
		info->win = ov8865_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov8865_formats;
		info->win_size = OV8865_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;
	    
		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV8865_DEFAULT_REG_WRTIE_MODE;
		info->hstp_preview_setting = &ov8865_win_sizes[1];
		info->hstp_photo_setting = &ov8865_win_sizes[0];

		back_camera_core_ops.s_ctrl_1 = ov8865_s_ctrl_1;
		back_camera_core_ops.reset = ov8865_reset;
		back_camera_core_ops.init = ov8865_init;

		back_camera_video_ops.enum_mbus_fmt = ov8865_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov8865_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov8865_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov8865_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov8865_s_stream;
		back_camera_video_ops.enum_frameintervals = ov8865_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov8865_enum_framesizes;
#endif
	} else if (info->chip_id == 0x5648) {
#ifdef CONFIG_VIDEO_OV5648
		info->win = ov5648_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov5648_formats;
		info->win_size = OV5648_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;
	    
		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV5648_DEFAULT_REG_WRTIE_MODE;
		info->hstp_preview_setting = &ov5648_win_sizes[1];
		info->hstp_photo_setting = &ov5648_win_sizes[0];

		back_camera_core_ops.s_ctrl_1 = NULL;
		back_camera_core_ops.reset = ov5648_reset;
		back_camera_core_ops.init = ov5648_init;

		back_camera_video_ops.enum_mbus_fmt = ov5648_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov5648_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov5648_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov5648_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov5648_s_stream;
		back_camera_video_ops.enum_frameintervals = ov5648_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov5648_enum_framesizes;
#endif
	} else if (info->chip_id == 0x4689) {
		v4l_err(client, "chip found ov%x \n",info->chip_id);
#ifdef CONFIG_VIDEO_OV4689
		info->win = ov4689_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov4689_formats;
		info->win_size = OV4689_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;
	    
		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV4689_DEFAULT_REG_WRTIE_MODE;
		info->hstp_preview_setting = &ov4689_win_sizes[1];
		info->hstp_photo_setting = &ov4689_win_sizes[0];

		back_camera_core_ops.s_power = ov4689_s_power;
		back_camera_core_ops.s_ctrl_1 = NULL;
		back_camera_core_ops.reset = ov4689_reset;
		back_camera_core_ops.soft_reset = ov4689_soft_reset;
		back_camera_core_ops.init = ov4689_init;
		v4l_err(client, "chip found ov%x \n",info->chip_id);
		back_camera_video_ops.enum_mbus_fmt = ov4689_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov4689_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov4689_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov4689_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov4689_s_stream;
		back_camera_video_ops.enum_frameintervals = ov4689_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov4689_enum_framesizes;
#endif		
	} else if (info->chip_id == 0x6211) {
		v4l_err(client, "chip found ov%x \n",info->chip_id);
#ifdef CONFIG_VIDEO_OV6211
		info->win = ov6211_win_sizes;
		info->using_win = NULL;
		info->trying_win = NULL;
		info->fmt = ov6211_formats;
		info->win_size = OV6211_N_WIN_SIZES;
		info->i2c_flag = SELECT_I2C_16BIT_ADDR | SELECT_I2C_8BIT_DATA;
		info->i2c_addr = client->addr;

		info->group_hold_regs = NULL;
		info->group_hold_regs_num = 0;
		info->support_group_lunch = false;
		info->write_mode = OV6211_DEFAULT_REG_WRTIE_MODE;
		info->hstp_preview_setting = &ov6211_win_sizes[1];
		info->hstp_photo_setting = &ov6211_win_sizes[0];

		back_camera_core_ops.s_power = ov6211_s_power;
		back_camera_core_ops.s_ctrl_1 = NULL;
		back_camera_core_ops.reset = ov6211_reset;
		back_camera_core_ops.soft_reset = ov6211_soft_reset;
		back_camera_core_ops.init = ov6211_init;
		v4l_err(client, "chip found ov%x \n",info->chip_id);
		back_camera_video_ops.enum_mbus_fmt = ov6211_enum_mbus_fmt;
		back_camera_video_ops.g_mbus_fmt = ov6211_g_mbus_fmt;
		back_camera_video_ops.try_mbus_fmt = ov6211_try_mbus_fmt;
		back_camera_video_ops.s_mbus_fmt = ov6211_s_mbus_fmt;
		back_camera_video_ops.s_stream = ov6211_s_stream;
		back_camera_video_ops.enum_frameintervals = ov6211_enum_frameintervals;
		back_camera_video_ops.enum_framesizes = ov6211_enum_framesizes;
#endif
	} else {
		v4l_err(client, "chip found ov%x but not supported, why??? you can't be here.\n",
			info->chip_id);
		kfree(info);
	}
	hstp_camera_info = info;
	return 0;
}

static int back_camera_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct sensor_info *info = container_of(sd, struct sensor_info, sd);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id back_camera_id[] = {
	{"m200-back-camera", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, back_camera_id);

static struct i2c_driver back_camera_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "m200-back-camera",
	},
	.probe		= back_camera_probe,
	.remove		= back_camera_remove,
	.id_table	= back_camera_id,
};

static __init int init_back_camera(void)
{
	SENSOR_PRINT(CAMERA_WARNING," ********** init_back_camera ************\n");
	return i2c_add_driver(&back_camera_driver);
}

static __exit void exit_back_camera(void)
{
	SENSOR_PRINT(CAMERA_WARNING," ********** exit_back_camera ************\n");
	i2c_del_driver(&back_camera_driver);
}

module_init(init_back_camera);
module_exit(exit_back_camera);

MODULE_DESCRIPTION("A low-level driver for m200 back_camera .");
MODULE_LICENSE("GPL");
