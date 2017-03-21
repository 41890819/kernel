#include <linux/fs.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mm.h>

#define SHARE_MEDIA_NAME "sharemedia"

#define SHARE_MEM_SUM  10
#define SHARE_MEM_VIDEO_SIZE 100000
#define SHARE_MEM_AUDIO_SIZE 500

/* ioct command */
#define SHARE_MEDIA_GET_VIDEO_FREE_BUFFER 0
#define SHARE_MEDIA_PUT_VIDEO_FREE_BUFFER 1
#define SHARE_MEDIA_GET_VIDEO_BUSY_BUFFER 2
#define SHARE_MEDIA_PUT_VIDEO_BUSY_BUFFER 3

#define SHARE_MEDIA_GET_AUDIO_FREE_BUFFER 4
#define SHARE_MEDIA_PUT_AUDIO_FREE_BUFFER 5
#define SHARE_MEDIA_GET_AUDIO_BUSY_BUFFER 6
#define SHARE_MEDIA_PUT_AUDIO_BUSY_BUFFER 7

struct sharemedia_global_data {
	atomic_t open_count;
	spinlock_t video_list_lock;
	struct list_head video_free_list;
	struct list_head video_busy_list;
	spinlock_t audio_list_lock;
	struct list_head audio_free_list;
	struct list_head audio_busy_list;
	struct miscdevice misc_dev;
	void* pInfoMem;
	void* pNodeMem;
};

struct media_data_info {
	int frame_size;
	unsigned long timestampUs;
	void *pData;
};

struct sharemedia_list_node {
	int num;
	struct media_data_info *pInfo;
	struct list_head node;
};

static struct sharemedia_global_data sharemedia;

static int sharemedia_open(struct inode *inode, struct file *file)
{
	if (0 == atomic_read(&sharemedia.open_count)) {
		struct sharemedia_list_node* pVideoNode = NULL;
		struct sharemedia_list_node* pAudioNode = NULL;
		void* pVideoInfo = NULL;
		void* pAudioInfo = NULL;
		int i = 0;
		int pages = 0;

		/* 防止某次关闭失败，导致驱动不可用，进而必须重启系统 */
		if ((sharemedia.pInfoMem != NULL) || (sharemedia.pNodeMem != NULL)) {
			vfree(sharemedia.pInfoMem);
			kfree(sharemedia.pNodeMem);
			sharemedia.pInfoMem = NULL;
			sharemedia.pNodeMem = NULL;

			INIT_LIST_HEAD(&sharemedia.video_free_list);
			INIT_LIST_HEAD(&sharemedia.video_busy_list);

			INIT_LIST_HEAD(&sharemedia.audio_free_list);
			INIT_LIST_HEAD(&sharemedia.audio_busy_list);
		}

		int video_all_size = SHARE_MEM_SUM * (sizeof(struct media_data_info) + SHARE_MEM_VIDEO_SIZE);
		int audio_all_size = SHARE_MEM_SUM * (sizeof(struct media_data_info) + SHARE_MEM_AUDIO_SIZE);
		pages = PAGE_ALIGN(video_all_size + audio_all_size);
		pVideoInfo = vmalloc_user(pages);
		pAudioInfo = pVideoInfo + video_all_size;
		pVideoNode = kzalloc(2 * SHARE_MEM_SUM * sizeof(struct sharemedia_list_node), GFP_KERNEL);
		pAudioNode = pVideoNode + SHARE_MEM_SUM;
		sharemedia.pInfoMem = pVideoInfo;
		sharemedia.pNodeMem = pVideoNode;

		for (i = 0; i < SHARE_MEM_SUM; i++) {
			/* initial video list */
			pVideoNode->num = i;
			pVideoNode->pInfo = pVideoInfo;
			pVideoNode->pInfo->pData = pVideoInfo + sizeof(struct media_data_info);
			spin_lock(&sharemedia.video_list_lock);
			list_add_tail(&(pVideoNode->node), &sharemedia.video_free_list);
			spin_unlock(&sharemedia.video_list_lock);

			pVideoInfo += (sizeof(struct media_data_info) + SHARE_MEM_VIDEO_SIZE);
			pVideoNode++;

			/* initial audio list */
			pAudioNode->num = i;
			pAudioNode->pInfo = pAudioInfo;
			pAudioNode->pInfo->pData = pAudioInfo + sizeof(struct media_data_info);
			spin_lock(&sharemedia.audio_list_lock);
			list_add_tail(&(pAudioNode->node), &sharemedia.audio_free_list);
			spin_unlock(&sharemedia.audio_list_lock);

			pAudioInfo += (sizeof(struct media_data_info) + SHARE_MEM_AUDIO_SIZE);
			pAudioNode++;
		}
	}
	atomic_inc(&sharemedia.open_count);
	return 0;
}

static int sharemedia_release(struct inode *inode, struct file *file)
{
	atomic_dec(&sharemedia.open_count);
	if (0 == atomic_read(&sharemedia.open_count)) {
		vfree(sharemedia.pInfoMem);
		kfree(sharemedia.pNodeMem);
		sharemedia.pInfoMem = NULL;
		sharemedia.pNodeMem = NULL;

		/* Must do that */
		INIT_LIST_HEAD(&sharemedia.video_free_list);
		INIT_LIST_HEAD(&sharemedia.video_busy_list);

		INIT_LIST_HEAD(&sharemedia.audio_free_list);
		INIT_LIST_HEAD(&sharemedia.audio_busy_list);
	}
	return 0;
}

static int sharemedia_get_video_free_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;

	spin_lock(&sharemedia.video_list_lock);
	if (!list_empty(&sharemedia.video_free_list)) {
		pTmpNode = container_of(sharemedia.video_free_list.next, struct sharemedia_list_node, node);
	}
	spin_unlock(&sharemedia.video_list_lock);
	if (pTmpNode) {
		return pTmpNode->num;
	} else {
		return -EFAULT;
	}
}

static int sharemedia_put_video_free_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;
	int ret = -1;

	spin_lock(&sharemedia.video_list_lock);
	pTmpNode = container_of(sharemedia.video_free_list.next, struct sharemedia_list_node, node);
	if (pTmpNode) {
		list_move_tail(&pTmpNode->node, &sharemedia.video_busy_list);
		ret = 0;
	}
	spin_unlock(&sharemedia.video_list_lock);
	return ret;
}

static int sharemedia_get_video_busy_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;

	spin_lock(&sharemedia.video_list_lock);
	if (!list_empty(&sharemedia.video_busy_list)) {
		pTmpNode = container_of(sharemedia.video_busy_list.next, struct sharemedia_list_node, node);
	}
	spin_unlock(&sharemedia.video_list_lock);
	if (pTmpNode) {
		return pTmpNode->num;
	} else {
		return -EFAULT;
	}
}

static int sharemedia_put_video_busy_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;
	int ret = -1;
	spin_lock(&sharemedia.video_list_lock);
	pTmpNode = container_of(sharemedia.video_busy_list.next, struct sharemedia_list_node, node);
	if (pTmpNode) {
		list_move_tail(&pTmpNode->node, &sharemedia.video_free_list);
		ret = 0;
	}
	spin_unlock(&sharemedia.video_list_lock);
	return ret;
}

static int sharemedia_get_audio_free_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;

	spin_lock(&sharemedia.audio_list_lock);
	if (!list_empty(&sharemedia.audio_free_list)) {
		pTmpNode = container_of(sharemedia.audio_free_list.next, struct sharemedia_list_node, node);
	}
	spin_unlock(&sharemedia.audio_list_lock);
	if (pTmpNode) {
		return pTmpNode->num;
	} else {
		return -EFAULT;
	}
}

static int sharemedia_put_audio_free_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;
	int ret = -1;

	spin_lock(&sharemedia.audio_list_lock);
	pTmpNode = container_of(sharemedia.audio_free_list.next, struct sharemedia_list_node, node);
	if (pTmpNode) {
		list_move_tail(&pTmpNode->node, &sharemedia.audio_busy_list);
		ret = 0;
	}
	spin_unlock(&sharemedia.audio_list_lock);
	return ret;
}

static int sharemedia_get_audio_busy_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;

	spin_lock(&sharemedia.audio_list_lock);
	if (!list_empty(&sharemedia.audio_busy_list)) {
		pTmpNode = container_of(sharemedia.audio_busy_list.next, struct sharemedia_list_node, node);
	}
	spin_unlock(&sharemedia.audio_list_lock);
	if (pTmpNode) {
		return pTmpNode->num;
	} else {
		return -EFAULT;
	}
}

static int sharemedia_put_audio_busy_buffer(void)
{
	struct sharemedia_list_node* pTmpNode = NULL;
	int ret = -1;
	spin_lock(&sharemedia.audio_list_lock);
	pTmpNode = container_of(sharemedia.audio_busy_list.next, struct sharemedia_list_node, node);
	if (pTmpNode) {
		list_move_tail(&pTmpNode->node, &sharemedia.audio_free_list);
		ret = 0;
	}
	spin_unlock(&sharemedia.audio_list_lock);
	return ret;
}

static long sharemedia_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	switch (cmd) {
	case SHARE_MEDIA_GET_VIDEO_FREE_BUFFER:
		ret = sharemedia_get_video_free_buffer();
		break;
	case SHARE_MEDIA_PUT_VIDEO_FREE_BUFFER:
		ret = sharemedia_put_video_free_buffer();
		break;
	case SHARE_MEDIA_GET_VIDEO_BUSY_BUFFER:
		ret = sharemedia_get_video_busy_buffer();
		break;
	case SHARE_MEDIA_PUT_VIDEO_BUSY_BUFFER:
		ret = sharemedia_put_video_busy_buffer();
		break;

	case SHARE_MEDIA_GET_AUDIO_FREE_BUFFER:
		ret = sharemedia_get_audio_free_buffer();
		break;
	case SHARE_MEDIA_PUT_AUDIO_FREE_BUFFER:
		ret = sharemedia_put_audio_free_buffer();
		break;
	case SHARE_MEDIA_GET_AUDIO_BUSY_BUFFER:
		ret = sharemedia_get_audio_busy_buffer();
		break;
	case SHARE_MEDIA_PUT_AUDIO_BUSY_BUFFER:
		ret = sharemedia_put_audio_busy_buffer();
		break;
	default:
		printk(KERN_DEBUG "*********Unknow Command ShareMedia*********\n");
	}
	return ret;
}

static int sharemedia_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = -1;
	ret = remap_vmalloc_range(vma, (void *)sharemedia.pInfoMem, 0);
	return ret;
}

struct file_operations sharemedia_fops = {
	.owner          =    THIS_MODULE,
	.open           =    sharemedia_open,
	.release        =    sharemedia_release,
	.unlocked_ioctl =    sharemedia_ioctl,
	.mmap           =    sharemedia_mmap,
};

static int __init sharemedia_module_init(void)
{
	int err = 0;

	atomic_set(&sharemedia.open_count, 0);

	spin_lock_init(&sharemedia.video_list_lock);
	INIT_LIST_HEAD(&sharemedia.video_free_list);
	INIT_LIST_HEAD(&sharemedia.video_busy_list);

	spin_lock_init(&sharemedia.audio_list_lock);
	INIT_LIST_HEAD(&sharemedia.audio_free_list);
	INIT_LIST_HEAD(&sharemedia.audio_busy_list);

	sharemedia.misc_dev.name = SHARE_MEDIA_NAME;
	sharemedia.misc_dev.minor = MISC_DYNAMIC_MINOR;
	sharemedia.misc_dev.fops = &sharemedia_fops;

	sharemedia.pInfoMem = NULL;
	sharemedia.pNodeMem = NULL;

	err = misc_register(&sharemedia.misc_dev);
	if (err < 0) {
		dev_err(NULL, "Unable to register sharemedia driver!\n");
		return -1;
	}
	return 0;
}

static void __exit sharemedia_module_exit(void)
{
	misc_deregister(&sharemedia.misc_dev);
}

module_init(sharemedia_module_init);
module_exit(sharemedia_module_exit);
