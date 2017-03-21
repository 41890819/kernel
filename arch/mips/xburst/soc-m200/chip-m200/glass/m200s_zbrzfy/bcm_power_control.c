#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <mach/jzmmc.h>
#include <linux/bcm_pm_core.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/wlan_plat.h>

#include <board_base.h>
#define IMPORT_WIFIMAC_BY_SELF

static int ENABLE_32K_CNT = 0;
void enable_clk32k(void)
{
    ENABLE_32K_CNT++;
    jzrtc_enable_clk32k();
}
EXPORT_SYMBOL(enable_clk32k);

void disable_clk32k(void)
{
    if (ENABLE_32K_CNT > 0)
        ENABLE_32K_CNT--;

    if (ENABLE_32K_CNT == 0)
        jzrtc_disable_clk32k();
}
EXPORT_SYMBOL(disable_clk32k);

#ifdef CONFIG_BCM_PM_CORE
static struct bcm_power_platform_data bcm_power_platform_data = {
	.wlan_pwr_en = WLAN_PWR_EN,
	.clk_enable = enable_clk32k,
	.clk_disable = disable_clk32k,
};

struct platform_device	bcm_power_platform_device = {
	.name = "bcm_power",
	.id = -1,
	.num_resources = 0,
	.dev = {
		.platform_data = &bcm_power_platform_data,
	},
};
#endif

/*For BlueTooth*/
#ifdef CONFIG_BROADCOM_RFKILL
#include <linux/bt-rfkill.h>

#if 0
static void set_pin_status(int bt_power_state)
{
#if 0
	if(bt_power_state){
		/*set UART0_RXD, UART0_CTS_N ,2 pins to input nopull*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_INPUT, 0x3);
		/*set UART0_TXD to output low*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_INPUT, 0x8);

		/*set PCM0_DO ,PCM0_CLK, PCM0_SYN ,PCM0_DI 4 pins to OUTPUT_LOW*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_OUTPUT0, 0xF << 12);
	}else{
#if defined(GPIO_BT_RST_N)
		jzgpio_set_func(GPIO_BT_RST_N / 32, GPIO_OUTPUT0,
				1 << (GPIO_BT_RST_N % 32));
#endif
		jzgpio_set_func(GPIO_BT_INT / 32, GPIO_OUTPUT0,
				1 << (GPIO_BT_INT % 32));
		jzgpio_set_func(GPIO_BT_WAKE / 32, GPIO_OUTPUT0,
				1 << (GPIO_BT_WAKE % 32));

		/*set BT_RST_N ,BT_INT, BT_WAKE , BT_REG_ON 4 pins to OUTPUT_LOW*/
		jzgpio_set_func(GPIO_BT_REG_ON / 32, GPIO_OUTPUT0,
				1 << (GPIO_BT_REG_ON % 32));

		/*set UART0_RXD, UART0_CTS_N, UART0_RTS_N 3 pins to OUTPUT_LOW*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_OUTPUT0 , 0x7);

		/*set UART0_TXD to INPUT_NOPULL*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_INPUT , 1 << 3);

		/*set PCM0_DO ,PCM0_CLK, PCM0_SYN ,PCM0_DI 4 pins to OUTPUT_LOW*/
		jzgpio_set_func(GPIO_PORT_F, GPIO_OUTPUT0 , 0xF << 12);
	}
#endif
}
#endif

static void restore_pin_status(int bt_power_state)
{
	jzgpio_set_func(BLUETOOTH_UART_GPIO_PORT, BLUETOOTH_UART_GPIO_FUNC, BLUETOOTH_UART_FUNC_SHIFT);
}

static struct bt_rfkill_platform_data  bt_gpio_data = {
	.gpio = {
		.bt_rst_n = -1,
		.bt_reg_on = BT_REG_EN,
		.bt_wake = HOST_WAKE_BT,
		.bt_int = BT_WAKE_HOST,
		.bt_uart_rts = BT_UART_RTS,
#if 0
		.bt_int_flagreg = -1,
		.bt_int_bit = -1,
#endif
	},

	.restore_pin_status = restore_pin_status,
	.set_pin_status = NULL,
#if 0
	.suspend_gpio_set = NULL,
	.resume_gpio_set = NULL,
#endif
};

struct platform_device bt_power_device  = {
	.name = "bt_power" ,
	.id = -1 ,
	.dev   = {
		.platform_data = &bt_gpio_data,
	},
};

struct platform_device bluesleep_device = {
	.name = "bluesleep" ,
	.id = -1 ,
	.dev   = {
		.platform_data = &bt_gpio_data,
	},

};

#ifdef CONFIG_BT_BLUEDROID_SUPPORT
int bluesleep_tty_strcmp(const char* name)
{
	if(!strcmp(name,BLUETOOTH_UPORT_NAME)){
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(bluesleep_tty_strcmp);
#endif
#endif /* CONFIG_BROADCOM_RFKILL */

/*For WiFi*/
#ifdef CONFIG_WLAN
#define RESET               0
#define NORMAL              1

extern int jzmmc_manual_detect(int index, int on);
extern int jzmmc_clk_ctrl(int index, int on);
extern int bcm_power_on(void);
extern int bcm_power_down(void);

struct wifi_data {
	struct wake_lock                wifi_wake_lock;
	int                             wifi_reset;
};

#ifdef IMPORT_WIFIMAC_BY_SELF
#define WIFIMAC_ADDR_PATH "/data/misc/wifi/wifimac.txt"

static int get_wifi_mac_addr(unsigned char* buf)
{
	struct file *fp = NULL;
	mm_segment_t fs;

	unsigned char source_addr[18];
	loff_t pos = 0;
	unsigned char *head, *end;
	int i = 0;

	fp = filp_open(WIFIMAC_ADDR_PATH, O_RDONLY,  0444);
	if (IS_ERR(fp)) {

		printk("Can not access wifi mac file : %s\n",WIFIMAC_ADDR_PATH);
		return -EFAULT;
	}else{
		fs = get_fs();
		set_fs(KERNEL_DS);

		vfs_read(fp, source_addr, 18, &pos);
		source_addr[17] = ':';

		head = end = source_addr;
		for(i=0; i<6; i++) {
			while (end && (*end != ':') )
				end++;

			if (end && (*end == ':') )
				*end = '\0';

			buf[i] = simple_strtoul(head, NULL, 16 );

			if (end) {
				end++;
				head = end;
			}
			printk("wifi mac %02x \n", buf[i]);
		}
		set_fs(fs);
		filp_close(fp, NULL);
	}

	return 0;
}
struct wifi_platform_data bcmdhd_wlan_pdata = {
  .get_mac_addr = get_wifi_mac_addr,
};
#endif

static struct wifi_data bcm_data;

/*The function should be called iw8103,but do not modify because of compatibility */
static void wifi_le_set_io(void)
{
	/*when wifi is down, set WL_MSC1_D0 , WL_MSC1_D1, WL_MSC1_D2, WL_MSC1_D3, 
	  WL_MSC1_CLK, WL_MSC1_CMD pins to INPUT_NOPULL status*/
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 20);
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 21);
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 22);
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 23);
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 28);
	jzgpio_set_func(GPIO_PORT_E, GPIO_INPUT, 0x1 << 29);
}

static void wifi_le_restore_io(void)
{
	/*when wifi is up ,set WL_MSC1_D0 , WL_MSC1_D1, WL_MSC1_D2, WL_MSC1_D3,
		 WL_MSC1_CLK, WL_MSC1_CMD pins to GPIO_FUNC_0*/
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 20);
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 21);
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 22);
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 23);
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 28);
	jzgpio_set_func(GPIO_PORT_E, GPIO_FUNC_2, 0x1 << 29);
}

int bcm_wlan_init(void)
{
	int reset;
	//static struct wake_lock	*wifi_wake_lock = &bcm_data.wifi_wake_lock;

	wifi_le_set_io();

	gpio_request(WL_REG_EN, "wl_reg_on");
	gpio_direction_output(WL_REG_EN, 0);

#if defined(WL_RST_EN)
	reset = WL_RST_EN;
	if (gpio_request(WL_RST_EN, "wifi_reset")) {
		pr_err("no wifi_reset pin available\n");

		return -EINVAL;
	} else {
		gpio_direction_output(reset, 1);
	}
#else
	reset = -1;
#endif
	bcm_data.wifi_reset = reset;

	return 0;
}
EXPORT_SYMBOL(bcm_wlan_init);

int IW8101_wlan_power_on(int flag)
{
        static struct wake_lock	*wifi_wake_lock = &bcm_data.wifi_wake_lock;
#ifdef WL_REG_EN
	int wl_reg_on	= WL_REG_EN;
#endif
#ifdef WL_RST_EN
	int reset = bcm_data.wifi_reset;
#endif
  printk("IW8101_wlan_power_on in\n");

	if (wifi_wake_lock == NULL)
		pr_warn("%s: invalid wifi_wake_lock\n", __func__);
#ifdef WL_RST_EN
	else if (!gpio_is_valid(reset))
		pr_warn("%s: invalid reset\n", __func__);
#endif
	else
		goto start;

	return -ENODEV;
start:
	wifi_le_restore_io();
	printk("bcm_power_on start\n");
	bcm_power_on();
	msleep(200);
	printk("enable_clk32k start");
	enable_clk32k();
	pr_warn("wlan power on end %d flag=%d\n", __LINE__, flag);
	switch(flag) {
		case RESET:
#ifdef WL_REG_EN
			gpio_direction_output(wl_reg_on,1);
			msleep(200);
#endif
			msleep(200);
#ifdef WL_RST_EN
			gpio_direction_output(reset, 0);
			msleep(200);
			gpio_direction_output(reset, 1);
			msleep(200);
#endif
			break;
		case NORMAL:
			msleep(200);
#ifdef WL_REG_EN
			printk("wl_reg_on %d\n", wl_reg_on);
			gpio_direction_output(wl_reg_on,1);
			msleep(200);
#endif
#ifdef WL_RST_EN
			gpio_direction_output(reset, 0);
			msleep(200);
			gpio_direction_output(reset, 1);
			msleep(200);
#endif
			//while(1);
			pr_warn("NORMAL mmc detect start\n");
			jzmmc_manual_detect(1, 1);
			pr_warn("NORMAL mmc detect end\n");
			
			break;
	}
	//	wake_lock(wifi_wake_lock);
	return 0;
}
EXPORT_SYMBOL(IW8101_wlan_power_on);

int IW8101_wlan_power_off(int flag)
{
	static struct wake_lock	*wifi_wake_lock = &bcm_data.wifi_wake_lock;
#ifdef WL_REG_EN
	int wl_reg_on = WL_REG_EN;
#endif
#ifdef WL_RST_EN
	int reset = bcm_data.wifi_reset;
#endif

	if (wifi_wake_lock == NULL)
		pr_warn("%s: invalid wifi_wake_lock\n", __func__);
#ifdef WL_RST_EN
	else if (!gpio_is_valid(reset))
		pr_warn("%s: invalid reset\n", __func__);
#endif
	else
		goto start;
	return -ENODEV;
start:
	disable_clk32k();
	pr_debug("wlan power off:%d\n", flag);
	switch(flag) {
		case RESET:
#ifdef WL_REG_EN
			gpio_direction_output(wl_reg_on,0);
#endif
#ifdef WL_RST_EN
			gpio_direction_output(reset, 0);
#endif
			msleep(200);
			break;

		case NORMAL:
#ifdef WL_RST_EN
			gpio_direction_output(reset, 0);
#endif
			udelay(65);

			/*
			 *  control wlan reg on pin
			 */
#ifdef WL_REG_EN
			gpio_direction_output(wl_reg_on,0);
#endif
			msleep(200);
//			jzmmc_manual_detect(1, 0);
			break;
	}

	//	wake_unlock(wifi_wake_lock);

	bcm_power_down();
	wifi_le_set_io();

	return 0;
}
EXPORT_SYMBOL(IW8101_wlan_power_off);
#endif
