#ifndef __WAKEUP_OPS_H_
#define __WAKEUP_OPS_H_

#include <linux/circ_buf.h>

#include "ivDefine.h"
#include "ivIvwDefine.h"
#include "ivIvwErrorCode.h"
#include "ivIVW.h"
#include "ivPlatform.h"


#define TCSM_BANK_5_V	(0xb3427000)
#define TCSM_BANK_5_P	(0x13427000)

//#define TCSM_BUF_SIZE	(2048)
//#define TCSM_NR_BUF		(8)
#define TCSM_BUF_SIZE	(4096)
#define TCSM_NR_BUF		(8)

#define WAKEUP_RES_SIZE	(512 * 1024)

#define STATUS_WAKEUP	(50)

#define SYS_WAKEUP			(1)
#define SYS_WAKEUP_TIMEOUT	(2)



struct wakeup_module;

struct wakeup_ops {
	/*dma ops*/
	int (*init_dma)(struct wakeup_module *wakeup);
	int (*prepare_dma)(struct wakeup_module *wakeup);
	int (*start_dma)(struct wakeup_module *wakeup);
	int (*start_dma_3)(struct wakeup_module *wakeup);
	int (*release_dma)(struct wakeup_module *wakeup);

	int (*open)(struct wakeup_module *wakeup);
	ssize_t (*read)(struct wakeup_module *wakeup, char __user *buffer,\
						size_t count, loff_t *ppos);
	int (*close)(struct wakeup_module *wakeup);

	int (*wakeup_wait_for_complete)(struct wakeup_module *wakeup);
	/*resource ops*/
	ssize_t (*wakeup_set_res)(struct wakeup_module *wakeup, const char *buf, size_t count);
	ssize_t (*wakeup_ctl)(struct wakeup_module *wakeup, const char *buf, size_t count);

	int (*idle_tcu_int)(struct wakeup_module *wakeup);
	int (*mod_timer)(struct wakeup_module *wakeup, unsigned long timer_count);
};
enum work_mode {

	WMODE_INIT,
	WMODE_NORMAL_RECORD,
	WMODE_VOICE_TRIGGER,
};


struct dma_fifo {
	int head;
	int tail;
	size_t n_size;
	size_t len;
	char *data;
};

struct dma_fifo_2 {
	struct circ_buf xfer;
	size_t n_size;
};

enum wakeup_status {
	STATUS_RESERVED,
	STATUS_WAIT_WAKEUP,
	STATUS_WAKEUP_TIMEOUT,
	STATUS_WAKEUP_OK,
};


struct wakeup_module {

	/*dma*/
	dma_cookie_t cookie;
	enum jzdma_type dma_type;
	enum dma_status dma_status;

	struct dma_chan *chan_rx;
	struct dma_async_tx_descriptor *desc_rx;
	struct dma_slave_config dma_config;


	struct timer_list wakeup_timer;
	int mod_time_ms;
	int wakeup_timeout;
	bool recording;

	/* tcsm buf,attr */
	int nr_buf;
	int bytes_per_buf;

	/* voice process */
	unsigned char *sys_pReskey;
	unsigned int pResKeyoffset; /*sysfs test*/
	ivSize nIvwObjSize;
	ivPointer pIvwObj;
	ivUInt16  nResidentRAMSize;
	ivPointer pResidentRAM;
	ivUInt16 nWakeupNetworkID;
	ivUInt16 CMScore;
	ivUInt16 ResID;
	ivUInt16 KeywordID;
	ivUInt32 StartMS;
	ivUInt32 EndMS;
	bool IvwObjCreated;

	/* recorder */
	struct timer_list record_timer;
	struct completion read_completion;
	/*struct completion process_completion;*/

#define F_READING			1
#define F_READBLOCK			2
#define F_WAITING_WAKEUP	3
#define F_WAKEUP_ENABLED	4
#define F_OPENED			5
#define F_WAKEUP_BLOCK		6
	unsigned long flags;

	/* lock */
	struct mutex mutex;
	spinlock_t	lock;

	struct dma_fifo_2 *wakeup_fifo_2;
	spinlock_t voice_lock;

	struct dma_fifo_2 *record_fifo;
	spinlock_t record_lock;

	struct wakeup_timer *wakeup_timer_hw;

	struct tcu_device *tcu_chan;

	struct completion completion; /*wake up procedure complete*/
	struct wakeup_ops *ops;

	struct tasklet_struct wakeup_tasklet;

	enum wakeup_status wakeup_status;
	unsigned long t_notrigger;
	unsigned long t_wakeup_timeout;
};

#define wakeup_module_call(wakeup, f, args...)               \
	(!(wakeup) ? -ENODEV : (((wakeup)->ops && (wakeup)->ops->f) ?    \
							(wakeup)->ops->f((wakeup) , ##args) : -ENOIOCTLCMD))


int voice_wakeup_init(struct wakeup_module * wakeup);

#endif
