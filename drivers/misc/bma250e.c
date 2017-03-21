
/* drivers/misc/bma250e.c
 *
 * Copyright (c) 2014  Ingenic Semiconductor Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <linux/kthread.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <asm/div64.h>
#include <linux/linux_sensors.h>
#include <linux/i2c/bma250e.h>

#define DATA_BUF_SIZE	(1 * 1024 * 1024)
#define BMA250e_BW_7_81_sel	64
#define BMA250e_GET_DATA (1 << 30) | 		\
	(data.single.accel_x << 20) | 		\
	(data.single.accel_y << 10) | 		\
	data.single.accel_z

struct bma250e_dev {
	struct device *dev;
	int irq;
	struct i2c_client *client;
	struct task_struct *kthread;
	struct miscdevice mdev;
	atomic_t in_use;
	struct completion done;
	spinlock_t lock;
	struct regulator *power;
	int gpio;
	int wakeup;
	int suspend_t;
	unsigned int *data_buf;
	unsigned int *data_buf_copy;
	unsigned int *cur_producer_p;
	unsigned int *cur_consumer_p;
	unsigned int *data_buf_base;
	unsigned int *data_buf_top;
	unsigned int *data_buf_copy_base;
	unsigned int *data_buf_copy_link;
	u32 cur_timestamp;
	u32 rtc_base;
	u64 time_base;
};

typedef union bma250_accel_data {

	/** raw register data */
	uint32_t d32;

	/** register bits */
	struct {
		unsigned accel_z:10;
		unsigned accel_y:10;
		unsigned accel_x:10;
		unsigned flag:2;
	} single;
} bma250_accel_data_t;

typedef struct accel_info_ptr_t {
	int threshold;
	int frequency;
	int ranges;
} accel_info_ptr;

#define BMA250_I2C_RETRYS	5

static inline u32 get_time(struct bma250e_dev *b)
{
	unsigned long long t = sched_clock();
	do_div(t, 1000000);
	return (u32) t;
}

int bma250_ic_read(struct i2c_client *ic_dev, u8 reg, u8 * buf, int len)
{
	int rc;
	int i;

	for (i = 0; i < BMA250_I2C_RETRYS; i++) {
		rc = i2c_smbus_read_i2c_block_data(ic_dev, reg, len, buf);
		if (rc > 0)
			return 0;
	}
	printk("read error %d, %d \n", __LINE__, rc);
	return rc;
}

static u32 __get_rtc_time(void)
{
	const char *rtc_patch = "/dev/rtc0";
	static int rtc_fd = -1;
	struct rtc_time time_rtc;

	if (rtc_fd < 0)
		rtc_fd = sys_open(rtc_patch, O_RDONLY, 0);

	if (rtc_fd < 0) {
		printk("Error when open /dev/rtc0, is RTC in kernel enabled? ");
		return 0;
	}

	sys_ioctl(rtc_fd, (unsigned int)RTC_RD_TIME, (unsigned long)&time_rtc);
	sys_close(rtc_fd);
	return mktime(time_rtc.tm_year, time_rtc.tm_mon, time_rtc.tm_mday,
		      time_rtc.tm_hour, time_rtc.tm_min, time_rtc.tm_sec);
}

static void set_rtc_base(struct bma250e_dev *bma250e)
{
	bma250e->rtc_base = __get_rtc_time();
}

static u32 get_rtc_time(struct bma250e_dev *bma250e)
{
	return __get_rtc_time() - bma250e->rtc_base;
}

#if 0
#define NEW_BMA250_ACCEL_DATA(D)		\
	bma250_accel_data_t D##;		\
	D##.d32 = 0;				\
	D##.flag = 1
#endif

static int bma250e_read_continue(struct bma250e_dev *bma250e)
{
	int rc = 0;
	u8 rx_buf[6];
	bma250_accel_data_t data;
	data.d32 = 0;
	data.single.flag = 1;
	rc = bma250_ic_read(bma250e->client, BMA250_X_AXIS_LSB_REG, rx_buf, 6);
	if (rc)
		goto get_data_error;
	/* 10bit signed to 16bit signed  */
#if 0
	data.single.accel_x = ((rx_buf[1] << 8) | (rx_buf[0] & 0xc0)) >> 6;
	data.single.accel_y = ((rx_buf[3] << 8) | (rx_buf[2] & 0xc0)) >> 6;
	data.single.accel_z = ((rx_buf[5] << 8) | (rx_buf[4] & 0xc0)) >> 6;
#else
	data.single.accel_x = ((rx_buf[1] << 8) | (rx_buf[0])) >> 6;
	data.single.accel_y = ((rx_buf[3] << 8) | (rx_buf[2])) >> 6;
	data.single.accel_z = ((rx_buf[5] << 8) | (rx_buf[4])) >> 6;
#endif
//      printk("%s no_add_time_report: %d, %d, %d\n", __func__,
//             data.single.accel_x, data.single.accel_y, data.single.accel_z);

	if (bma250e->cur_producer_p == bma250e->cur_consumer_p) {
		*bma250e->cur_producer_p = BMA250e_GET_DATA;
		spin_lock(&bma250e->lock);
		bma250e->cur_producer_p++;
		spin_unlock(&bma250e->lock);
		return rc;
	}
	if ((bma250e->cur_producer_p > bma250e->cur_consumer_p)) {
		if (bma250e->cur_producer_p == bma250e->data_buf_top) {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p = bma250e->data_buf_base;
			spin_unlock(&bma250e->lock);
			return rc;
		} else {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			return rc;
		}
	}

	if (bma250e->cur_producer_p < bma250e->cur_consumer_p) {
		if ((bma250e->cur_consumer_p - bma250e->cur_producer_p) > 2) {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			return rc;
		} else {
			return 0;
		}
	}

      get_data_error:

	return rc;
}

static int bma250e_read_continue_time(struct bma250e_dev *bma250e, u32 time)
{
	//int i;
	int rc = 0;
	u8 rx_buf[6];
	bma250_accel_data_t data;
	data.d32 = 0;
	data.single.flag = 1;

	rc = bma250_ic_read(bma250e->client, BMA250_X_AXIS_LSB_REG, rx_buf, 6);
	if (rc)
		goto report_error;

	/* 10bit signed to 16bit signed  */
#if 0
	data.single.accel_x = ((rx_buf[1] << 8) | (rx_buf[0] & 0xc0)) >> 6;
	data.single.accel_y = ((rx_buf[3] << 8) | (rx_buf[2] & 0xc0)) >> 6;
	data.single.accel_z = ((rx_buf[5] << 8) | (rx_buf[4] & 0xc0)) >> 6;
#else
	data.single.accel_x = ((rx_buf[1] << 8) | (rx_buf[0])) >> 6;
	data.single.accel_y = ((rx_buf[3] << 8) | (rx_buf[2])) >> 6;
	data.single.accel_z = ((rx_buf[5] << 8) | (rx_buf[4])) >> 6;
#endif
//      printk("%s add_time_report: %d, %d, %d\n", __func__,
//             data.single.accel_x, data.single.accel_y, data.single.accel_z);

	if (bma250e->cur_producer_p == bma250e->cur_consumer_p) {
		*bma250e->cur_producer_p = ((2 << 30) | (time >> 2));
		spin_lock(&bma250e->lock);
		bma250e->cur_producer_p++;
		spin_unlock(&bma250e->lock);
		goto data__write;
	}

	if ((bma250e->cur_producer_p > bma250e->cur_consumer_p)) {
		if (bma250e->cur_producer_p == bma250e->data_buf_top) {
			*bma250e->cur_producer_p = ((2 << 30) | (time >> 2));
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p = bma250e->data_buf_base;
			spin_unlock(&bma250e->lock);
			goto data__write;
		} else {
			*bma250e->cur_producer_p = ((2 << 30) | (time >> 2));
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			goto data__write;
		}
	}

	if (bma250e->cur_producer_p < bma250e->cur_consumer_p) {
		if ((bma250e->cur_consumer_p - bma250e->cur_producer_p) > 2) {
			*bma250e->cur_producer_p = ((2 << 30) | (time >> 2));
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			goto data__write;
		} else {
			return 0;
		}
	}

      data__write:
	if (bma250e->cur_producer_p == bma250e->cur_consumer_p) {
		*bma250e->cur_producer_p = BMA250e_GET_DATA;
		spin_lock(&bma250e->lock);
		bma250e->cur_producer_p++;
		spin_unlock(&bma250e->lock);
		return rc;
	}
	if ((bma250e->cur_producer_p > bma250e->cur_consumer_p)) {
		if (bma250e->cur_producer_p == bma250e->data_buf_top) {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p = bma250e->data_buf_base;
			spin_unlock(&bma250e->lock);
			return rc;
		} else {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			return rc;
		}
	}

	if (bma250e->cur_producer_p < bma250e->cur_consumer_p) {
		if ((bma250e->cur_consumer_p - bma250e->cur_producer_p) > 2) {
			*bma250e->cur_producer_p = BMA250e_GET_DATA;
			spin_lock(&bma250e->lock);
			bma250e->cur_producer_p++;
			spin_unlock(&bma250e->lock);
			return rc;
		} else {
			return 0;
		}
	}

      report_error:
	return rc;
}

static irqreturn_t bma250e_interrupt(int irq, void *dev)
{
	struct bma250e_dev *bma250e = dev;

	complete(&bma250e->done);
	return IRQ_HANDLED;
}

static int bma250e_daemon(void *d)
{
	struct bma250e_dev *bma250e = d;
	u32 time_base = 0, time_now = 0;
	u32 time_delay = 0;
	u32 tmp = 0, time_base_ms = 0;

	time_delay = BMA250e_BW_7_81_sel * 2 + 20;

	if (bma250e->suspend_t == 1) {
		time_base_ms = get_time(bma250e);
		bma250e->suspend_t = 0;
	}

	while (!kthread_should_stop()) {
		if (get_time(bma250e) > 0xFFFF0000) {
			time_base = 0;
		}
		wait_for_completion_interruptible(&bma250e->done);
		if (bma250e->rtc_base == 0) {
			set_rtc_base(bma250e);
		}
		if (time_base_ms == 0) {
			time_base_ms = get_time(bma250e);
		}
		tmp = get_time(bma250e);
		if (((tmp - time_base) >= time_delay) && (tmp > time_base)) {
			time_now = get_rtc_time(bma250e) * 1000;
			time_base = tmp;
			if (tmp > time_base_ms) {
				bma250e_read_continue_time(bma250e,
							   time_now + tmp -
							   time_base_ms);
			} else {
				bma250e_read_continue_time(bma250e, time_now);

			}
		} else {
			time_base = time_base + time_delay;
			bma250e_read_continue(bma250e);
		}
	}
	return 0;
}

static int bma250e_set(struct i2c_client *client)
{
	int rc;

	/*bma250e_reset */
	rc = i2c_smbus_write_byte_data(client, BMA250_RESET_REG, BMA250_RESET);
	if (rc)
		goto config_exit;
	msleep(200);
	rc = i2c_smbus_write_byte_data(client, BMA250_BW_SEL_REG, 8);
	if (rc)
		goto config_exit;
	/*set_range */
	rc = i2c_smbus_write_byte_data(client, BMA250_RANGE_REG, 0x03);
	if (rc)
		goto config_exit;

	/*set_int_filter_ slope_triger */
	rc = i2c_smbus_write_byte_data(client, 0x1E, 0);
	if (rc)
		goto config_exit;
	/*set_get_accd_register */
	rc = i2c_smbus_write_byte_data(client, 0x13, 0x40);
	if (rc)
		goto config_exit;
	/* threshold definition for the slope int, g-range dependant */
	rc = i2c_smbus_write_byte_data(client, BMA250_SLOPE_THR, 10);
	if (rc)
		goto config_exit;
	/* number of samples (n + 1) to be evaluted for slope int */
	rc = i2c_smbus_write_byte_data(client, BMA250_SLOPE_DUR, 0x01);
	if (rc)
		goto config_exit;
	/*set_int_x_y_z_canuse */
	rc = i2c_smbus_write_byte_data(client, 0x16, 0x07);
	if (rc)
		goto config_exit;

	/* maps interrupt to INT1 pin */
	rc = i2c_smbus_write_byte_data(client, 0x19, 0x04);
	if (rc)
		goto config_exit;

	/*set_int_mode */
	rc = i2c_smbus_write_byte_data(client, 0x21, 0);
	if (rc)
		goto config_exit;
	/*set_int_mode_active */
	rc = i2c_smbus_write_byte_data(client, 0x20, 0x01);
	if (rc)
		goto config_exit;

      config_exit:
	return rc;
}

static int bma250e_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct bma250e_dev *bma250e =
	    container_of(dev, struct bma250e_dev, mdev);

	wake_up_process(bma250e->kthread);
	/*
	 * Make sure fd be opened only onece.
	 */
	if (atomic_read(&bma250e->in_use) > 0) {
		dev_err(bma250e->dev, "Should only be opened onece\n");
		return -EBUSY;
	} else {
		atomic_inc(&bma250e->in_use);
	}

	return 0;
}

static int bma250e_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct bma250e_dev *bma250e =
	    container_of(dev, struct bma250e_dev, mdev);

	atomic_dec(&bma250e->in_use);

	return 0;
}

static ssize_t
bma250e_read(struct file *filp, char *buf, size_t size, loff_t * l)
{
	struct miscdevice *dev = filp->private_data;
	struct bma250e_dev *bma250e =
	    container_of(dev, struct bma250e_dev, mdev);
	int valid_data_len, size_move = 0;
	int len_to_read;
	int len_cannot_read;
	int size_before, size_after;

	size_move = size / 4;

	if (bma250e->cur_producer_p >= bma250e->cur_consumer_p) {
		valid_data_len =
		    (int)(bma250e->cur_producer_p - bma250e->cur_consumer_p);
		if (valid_data_len == 0)
			return 0;
		if (size_move > valid_data_len)
			len_to_read = valid_data_len;
		else
			len_to_read = size_move;
		copy_to_user(buf, bma250e->cur_consumer_p, len_to_read * 4);

		spin_lock(&bma250e->lock);
		bma250e->cur_consumer_p += len_to_read;
		if (len_to_read == valid_data_len) {
			bma250e->cur_consumer_p = bma250e->data_buf_base;
			bma250e->cur_producer_p = bma250e->data_buf_base;
		}
		spin_unlock(&bma250e->lock);

		return len_to_read * 4;
	}

	if (bma250e->cur_producer_p < bma250e->cur_consumer_p) {
		len_cannot_read =
		    (int)(bma250e->cur_consumer_p - bma250e->cur_producer_p);
		if (size_move <=
		    (bma250e->data_buf_top - bma250e->cur_consumer_p)) {
			len_to_read = size_move;
			copy_to_user(buf, bma250e->cur_consumer_p,
				     len_to_read * 4);

			spin_lock(&bma250e->lock);
			bma250e->cur_consumer_p += len_to_read;
			if (len_to_read ==
			    (bma250e->data_buf_top - bma250e->cur_consumer_p))
				bma250e->cur_consumer_p =
				    bma250e->data_buf_base;
			spin_unlock(&bma250e->lock);

			return len_to_read * 4;
		}
		if (size_move >
		    (bma250e->data_buf_top - bma250e->cur_consumer_p)
		    && (size_move <= (DATA_BUF_SIZE - len_cannot_read))) {
			size_before =
			    bma250e->data_buf_top - bma250e->cur_consumer_p;
			size_after = size_move - size_before;
			memcpy(bma250e->data_buf_copy_base,
			       bma250e->cur_consumer_p, size_before * 4);
			bma250e->data_buf_copy_link =
			    bma250e->data_buf_copy_base + size_before;
			memcpy(bma250e->data_buf_copy_link,
			       bma250e->data_buf_base, size_after * 4);
			copy_to_user(buf, bma250e->data_buf_copy_base,
				     size_move);

			spin_lock(&bma250e->lock);
			bma250e->cur_consumer_p += size_after;
			if (size_after ==
			    (bma250e->cur_producer_p - bma250e->data_buf_base))
			{
				bma250e->cur_consumer_p =
				    bma250e->data_buf_base;
				bma250e->cur_producer_p =
				    bma250e->data_buf_base;
			}
			spin_unlock(&bma250e->lock);

			return size_move * 4;

		}

		if (size_move >
		    (bma250e->data_buf_top - bma250e->cur_consumer_p)
		    && (size_move > (DATA_BUF_SIZE - len_cannot_read))) {
			size_move = DATA_BUF_SIZE - len_cannot_read;
			size_before =
			    bma250e->data_buf_top - bma250e->cur_consumer_p;
			size_after = size_move - size_before;
			memcpy(bma250e->data_buf_copy_base,
			       bma250e->cur_consumer_p, size_before * 4);
			bma250e->data_buf_copy_link =
			    bma250e->data_buf_copy_base + size_before;
			memcpy(bma250e->data_buf_copy_link,
			       bma250e->data_buf_base, size_before * 4);
			copy_to_user(buf, bma250e->data_buf_copy_base,
				     size_move);

			spin_lock(&bma250e->lock);
			bma250e->cur_consumer_p = bma250e->data_buf_base;
			bma250e->cur_producer_p = bma250e->data_buf_base;
			spin_unlock(&bma250e->lock);

			return size_move * 4;
		}
	}
	return size_move;
}

#if 0
static ssize_t
bma250e_write(struct file *filp, const char *buf, size_t size, loff_t * l)
{
	return 0;
}
#endif

static long
bma250e_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct bma250e_dev *bma250e =
	    container_of(dev, struct bma250e_dev, mdev);
	accel_info_ptr *accel_info = NULL;
	int rc;

	accel_info = kzalloc(sizeof(*accel_info), GFP_KERNEL);
	if (copy_from_user
	    (accel_info, (accel_info_ptr*) arg, sizeof(*accel_info))) {
		printk("copy_from_user failed\n");
		return -EFAULT;
	}

	switch (cmd) {
	case SENSOR_IOCTL_SET:
		if (accel_info->frequency > 0 && accel_info->frequency <= 8) {
			accel_info->frequency = 0x08;
		} else if (accel_info->frequency <= 16) {
			accel_info->frequency = 0x09;
		} else if (accel_info->frequency <= 32) {
			accel_info->frequency = 0x0A;
		} else if (accel_info->frequency <= 63) {
			accel_info->frequency = 0x0B;
		} else if (accel_info->frequency <= 125) {
			accel_info->frequency = 0x0C;
		} else if (accel_info->frequency <= 250) {
			accel_info->frequency = 0x0D;
		} else if (accel_info->frequency <= 500) {
			accel_info->frequency = 0x0E;
		} else if (accel_info->frequency > 500) {
			accel_info->frequency = 0x0F;
		} else {
			printk("FREQUENCY cannot Less than zero");
			return -1;
		}

		switch (accel_info->ranges) {
		case 2:
			accel_info->ranges = 0x03;
			break;
		case 4:
			accel_info->ranges = 0x05;
			break;
		case 8:
			accel_info->ranges = 0x08;
			break;
		case 16:
			accel_info->ranges = 0x0c;
			break;
		default:
			accel_info->ranges = 0x03;
		}
		/*bma250e_reset */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RESET_REG, BMA250_RESET);
		if (rc)
			goto config_exit;
		msleep(200);
		bma250e->cur_producer_p = bma250e->data_buf_base;
		bma250e->cur_consumer_p = bma250e->data_buf_base;
		/* set frequency */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x10,
					       accel_info->frequency);
		if (rc)
			goto config_exit;
		/*set_sensor_ranges */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RANGE_REG,
					       accel_info->ranges);
		if (rc)
			goto config_exit;
		/*set_int_filter_ slope_triger */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x01E, 0);
		if (rc)
			goto config_exit;
		/*set_get_accd_register */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x13, 0x40);
		if (rc)
			goto config_exit;
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_THR,
					       accel_info->threshold);
		if (rc)
			goto config_exit;
		/* number of samples (n + 1) to be evaluted for slope int */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_DUR, 0x01);
		if (rc)
			goto config_exit;
		/*set_int_x_y_z_canuse */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x16, 0x07);
		if (rc)
			goto config_exit;
		/* maps interrupt to INT1 pin */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x19, 0x04);
		if (rc)
			goto config_exit;

		/*set_int_mode */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x21, 0);
		if (rc)
			goto config_exit;
		/*set_int_mode_active */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x20, 0x01);
		if (rc)
			goto config_exit;
		break;
	case SENSOR_IOCTL_SET_THRESHOLD:
		/*bma250e_reset */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RESET_REG, BMA250_RESET);
		if (rc)
			goto config_exit;
		msleep(200);
		bma250e->cur_producer_p = bma250e->data_buf_base;
		bma250e->cur_consumer_p = bma250e->data_buf_base;
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_BW_SEL_REG, 8);
		if (rc)
			goto config_exit;
		/*set_range */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RANGE_REG, 0x03);
		if (rc)
			goto config_exit;
		/*set_int_filter_ slope_triger */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x01E, 0);
		if (rc)
			goto config_exit;
		/*set_get_accd_register */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x13, 0x40);
		if (rc)
			goto config_exit;
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_THR,
					       accel_info->threshold);
		if (rc)
			goto config_exit;
		/* number of samples (n + 1) to be evaluted for slope int */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_DUR, 0x01);
		if (rc)
			goto config_exit;
		/*set_int_x_y_z_canuse */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x16, 0x07);
		if (rc)
			goto config_exit;
		/* maps interrupt to INT1 pin */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x19, 0x04);
		if (rc)
			goto config_exit;

		/*set_int_mode */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x21, 0);
		if (rc)
			goto config_exit;
		/*set_int_mode_active */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x20, 0x01);
		if (rc)
			goto config_exit;
		break;
	case SENSOR_IOCTL_SET_FREQUENCY:
		if (accel_info->frequency > 0 && accel_info->frequency <= 8) {
			accel_info->frequency = 0x08;
		} else if (accel_info->frequency <= 16) {
			accel_info->frequency = 0x09;
		} else if (accel_info->frequency <= 32) {
			accel_info->frequency = 0x0A;
		} else if (accel_info->frequency <= 63) {
			accel_info->frequency = 0x0B;
		} else if (accel_info->frequency <= 125) {
			accel_info->frequency = 0x0C;
		} else if (accel_info->frequency <= 250) {
			accel_info->frequency = 0x0D;
		} else if (accel_info->frequency <= 500) {
			accel_info->frequency = 0x0E;
		} else if (accel_info->frequency > 500) {
			accel_info->frequency = 0x0F;
		} else {
			printk("FREQUENCY cannot Less than zero");
			return -1;
		}

		/*bma250e_reset */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RESET_REG, BMA250_RESET);
		if (rc)
			goto config_exit;
		msleep(200);
		bma250e->cur_producer_p = bma250e->data_buf_base;
		bma250e->cur_consumer_p = bma250e->data_buf_base;
		/* set frequency */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x10,
					       accel_info->frequency);
		if (rc)
			goto config_exit;
		/*set_range */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RANGE_REG, 0x03);
		if (rc)
			goto config_exit;

		/*set_int_filter_ slope_triger */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x01E, 0);
		if (rc)
			goto config_exit;
		/*set_get_accd_register */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x13, 0x40);
		if (rc)
			goto config_exit;
		/* threshold definition for the slope int, g-range dependant */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_THR, 5);
		if (rc)
			goto config_exit;
		/* number of samples (n + 1) to be evaluted for slope int */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_DUR, 0x01);
		if (rc)
			goto config_exit;
		/*set_int_x_y_z_canuse */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x16, 0x07);
		if (rc)
			goto config_exit;
		/* maps interrupt to INT1 pin */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x19, 0x04);
		if (rc)
			goto config_exit;

		/*set_int_mode */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x21, 0);
		if (rc)
			goto config_exit;
		/*set_int_mode_active */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x20, 0x01);
		if (rc)
			goto config_exit;
		break;
	case SENSOR_IOCTL_SET_RANGES:
		switch (accel_info->ranges) {
		case 2:
			accel_info->ranges = 0x03;
			break;
		case 4:
			accel_info->ranges = 0x05;
			break;
		case 8:
			accel_info->ranges = 0x08;
			break;
		case 16:
			accel_info->ranges = 0x0c;
			break;
		default:
			accel_info->ranges = 0x03;
		}

		/*bma250e_reset */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RESET_REG, BMA250_RESET);
		if (rc)
			goto config_exit;
		msleep(200);
		bma250e->cur_producer_p = bma250e->data_buf_base;
		bma250e->cur_consumer_p = bma250e->data_buf_base;
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_BW_SEL_REG, 8);
		if (rc)
			goto config_exit;
		/*set_sensor_ranges */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RANGE_REG,
					       accel_info->ranges);
		if (rc)
			goto config_exit;
		/*set_int_filter_ slope_triger */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x01E, 0);
		if (rc)
			goto config_exit;
		/*set_get_accd_register */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x13, 0x40);
		if (rc)
			goto config_exit;
		/* threshold definition for the slope int, g-range dependant */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_THR, 5);
		if (rc)
			goto config_exit;
		/* number of samples (n + 1) to be evaluted for slope int */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_DUR, 0x01);
		if (rc)
			goto config_exit;
		/*set_int_x_y_z_canuse */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x16, 0x07);
		if (rc)
			goto config_exit;
		/* maps interrupt to INT1 pin */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x19, 0x04);
		if (rc)
			goto config_exit;
		/*set_int_mode */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x21, 0);
		if (rc)
			goto config_exit;
		/*set_int_mode_active */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x20, 0x01);
		if (rc)
			goto config_exit;
		break;

	case SENSOR_IOCTL_GET_ACTIVE:
		/*set_int_x_y_z_canuse */
		rc = i2c_smbus_write_byte_data(bma250e->client, 0x16, arg);
		if (rc)
			goto config_exit;
		break;

	case SENSOR_IOCTL_GET_DATA:
		/*set_range */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_RANGE_REG, arg);
		if (rc)
			goto config_exit;
		break;

	case SENSOR_IOCTL_WAKE:
		/* threshold definition for the slope int, g-range dependant */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_THR, arg);
		if (rc)
			goto config_exit;
		break;
	case SENSOR_IOCTL_GET_DATA_RESOLUTION:
		/* number of samples (n + 1) to be evaluted for slope int */
		rc = i2c_smbus_write_byte_data(bma250e->client,
					       BMA250_SLOPE_DUR, arg);
		if (rc)
			goto config_exit;
		break;
	default:
		return -EINVAL;
	}

	return 0;
      config_exit:
	kfree(accel_info);
	return rc;

}

static struct file_operations bma250e_misc_fops = {
	.open = bma250e_open,
	.release = bma250e_release,
	.read = bma250e_read,
	.unlocked_ioctl = bma250e_ioctl,
};

static int
bma250e_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	u32 rc;
	struct bma250e_dev *bma250e;
	struct bma250_platform_data *pdata =
	    (struct bma250_platform_data *)client->dev.platform_data;

	u8 rx_buf[2];

	bma250e = kzalloc(sizeof(struct bma250e_dev), GFP_KERNEL);
	if (!bma250e) {
		printk("mallc bma250e memery error %s:%d\n", __func__,
		       __LINE__);
		return -ENOMEM;
	}

	bma250e->dev = &client->dev;
	bma250e->gpio = pdata->gpio;
	bma250e->wakeup = pdata->wakeup;
	if (gpio_request_one(bma250e->gpio, GPIOF_DIR_IN, "bma250e_irq")) {
		dev_err(bma250e->dev, "no irq pin available\n");
		ret = -EBUSY;
		goto err_request_gpio;
	}

	init_completion(&bma250e->done);
	spin_lock_init(&bma250e->lock);
	atomic_set(&bma250e->in_use, 0);

	bma250e->data_buf =
	    (unsigned int *)__get_free_pages(GFP_KERNEL,
					     get_order(DATA_BUF_SIZE));
	if (bma250e->data_buf == NULL) {
		printk("malloc bma250e data buf error %s:%d\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_request_buffer;
	}

	bma250e->data_buf_copy =
	    (unsigned int *)__get_free_pages(GFP_KERNEL,
					     get_order(DATA_BUF_SIZE));
	if (bma250e->data_buf_copy == NULL) {
		printk("malloc bma250e buf for copy error %s:%d\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_request_buffer;
	}

	bma250e->data_buf_base = bma250e->data_buf;
	bma250e->data_buf_top = bma250e->data_buf + DATA_BUF_SIZE;
	bma250e->cur_producer_p = bma250e->data_buf;
	bma250e->cur_consumer_p = bma250e->data_buf;

	bma250e->data_buf_copy_base = bma250e->data_buf_copy;

	bma250e->kthread =
	    kthread_create(bma250e_daemon, bma250e, "bma250e_daemon");
	if (IS_ERR(bma250e->kthread)) {
		ret = -1;
		goto err_irq_request_failed;
	}

	client->irq = gpio_to_irq(bma250e->gpio);
	ret = request_irq(client->irq, bma250e_interrupt,
			  IRQF_TRIGGER_RISING | IRQF_DISABLED, "bma250e",
			  bma250e);
	if (ret < 0) {
		dev_err(bma250e->dev, "%s: request irq failed\n", __func__);
		goto err_irq_request_failed;
	}

	bma250e->irq = client->irq;
	bma250e->client = client;

	i2c_set_clientdata(client, bma250e);

	bma250e->power = regulator_get(bma250e->dev, "vcc_sensor1v8");
	if (!IS_ERR(bma250e->power)) {
		regulator_enable(bma250e->power);
	}
	if (IS_ERR(bma250e->power)) {
		dev_warn(bma250e->dev, "get regulator failed\n");
	}
	msleep(200);
	rc = bma250_ic_read(client, BMA250_CHIP_ID_REG, rx_buf, 2);
	printk(KERN_INFO "bma250: detected chip id %x, rev 0x%X\n", rx_buf[0],
	       rx_buf[1]);
	if (rc) {
		printk("bma250e register error\n");
		goto err_irq_request_failed;
	}
	bma250e_set(client);

	bma250e->mdev.minor = MISC_DYNAMIC_MINOR;
	bma250e->mdev.name = "bma250e";
	bma250e->mdev.fops = &bma250e_misc_fops;

	ret = misc_register(&bma250e->mdev);
	if (ret < 0) {
		dev_err(bma250e->dev, "misc_register failed\n");
		goto err_register_misc;
	}

	return 0;
      err_request_buffer:
	misc_deregister(&bma250e->mdev);
      err_register_misc:
	regulator_put(bma250e->power);
	free_irq(bma250e->irq, bma250e);
      err_irq_request_failed:
	gpio_free(bma250e->gpio);
      err_request_gpio:
	kfree(bma250e);

	return ret;
}

static int __devexit bma250e_remove(struct i2c_client *client)
{
	struct bma250e_dev *bma250e = i2c_get_clientdata(client);

	misc_deregister(&bma250e->mdev);
	regulator_put(bma250e->power);
	free_irq(bma250e->irq, bma250e);
	gpio_free(bma250e->gpio);
	i2c_set_clientdata(client, NULL);
	kfree(bma250e->data_buf);
	kfree(bma250e->data_buf_copy);
	kfree(bma250e);

	return 0;
}

static int bma250e_suspend(struct i2c_client *client, pm_message_t state)
{
	struct bma250e_dev *bma250e = i2c_get_clientdata(client);

	if (bma250e->wakeup)
		enable_irq_wake(bma250e->irq);

	return 0;
}

static int bma250e_resume(struct i2c_client *client)
{
	struct bma250e_dev *bma250e = i2c_get_clientdata(client);
	bma250e->suspend_t = 1;
	return 0;
}

static const struct i2c_device_id bma250e_id[] = {
	{"bma250e-misc", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bma250e_id);

static struct i2c_driver bma250e_driver = {
	.probe = bma250e_probe,
	.remove = __devexit_p(bma250e_remove),
	.id_table = bma250e_id,
	.driver = {
		   .name = "bma250e-misc",
		   .owner = THIS_MODULE,
		   },
	.suspend = bma250e_suspend,
	.resume = bma250e_resume,
};

static int __init bma250e_init(void)
{
	return i2c_add_driver(&bma250e_driver);
}

static void __exit bma250e_exit(void)
{
	i2c_del_driver(&bma250e_driver);
}

module_init(bma250e_init);
module_exit(bma250e_exit);

MODULE_DESCRIPTION("bma250e misc driver");
MODULE_LICENSE("GPL");
