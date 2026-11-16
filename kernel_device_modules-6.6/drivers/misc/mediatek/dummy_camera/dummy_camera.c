#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/sysfs.h>
#include <asm/io.h>

#include <linux/input.h>
#include <linux/i2c.h>

#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/device.h> 
#include <linux/cdev.h>
#include <linux/input.h>

#include <linux/mutex.h>
#include <asm/atomic.h>

#include <linux/module.h>
#include <linux/init.h>

#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#endif

#include <linux/sched.h>
#include <linux/kthread.h>	

#include "dummy_camera.h"

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <generated/autoconf.h>
#include <linux/kobject.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

struct cam2_device {
    struct device *cam2_dev;
	struct regulator *cam2_iovdd_reg;
	struct regulator *cam2_avdd_reg;
	struct regulator *cam2_dvdd_reg;

};
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/ctype.h>

static struct class *dcam_light_core_class;
static struct device *dcam_light_core_dev;

static int dcam_light_val;
static struct cam2_device *cam2_tpd;

#define DCAM_TAG                  	"[D_CAMERA] "
#define DCAM_FUN(f)               	printk(KERN_INFO DCAM_TAG"%s %d \n", __FUNCTION__ , __LINE__)
#define DCAM_ERR(fmt, args...)    	printk(KERN_ERR DCAM_TAG "%s %d error: "fmt, __FUNCTION__, __LINE__, ##args)
#define DCAM_LOG(fmt, args...)    	printk(KERN_INFO DCAM_TAG "%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define DCAM_DBG(fmt, args...)    	printk(KERN_INFO DCAM_TAG "%s %d : "fmt, __FUNCTION__, __LINE__, ##args)   

static DEFINE_MUTEX(dcam_mutex);
/**********dummy camera report key param start**********/

//extern void kpd_dcam_report(int val);
/**********dummy camera report key param start**********/

#define DCAM_CAM2_I2C_NAME "dcam_cam2"


static struct i2c_client *new_client;
static struct task_struct *main2_light = NULL;  
#ifdef CONFIG_DCAM_CAM2_SP0A08
static int sp0a08_kthread_flag = -1;
#endif
#ifdef CONFIG_DCAM_CAM2_GC0310
static int gc0310_kthread_flag = -1;
#endif
#ifdef CONFIG_DCAM_CAM2_GC030A
static int gc030a_kthread_flag = -1;
#endif
#ifdef CONFIG_DCAM_CAM2_GC032A
static int gc032a_kthread_flag = -1;
#endif

#ifdef CONFIG_DCAM_CAM2_GC6133
static int gc6133_kthread_flag = -1;
#endif

struct pinctrl *dcam_pinctrl; //gpio pinctrl
static struct pinctrl_state *pins_default = NULL;
static struct pinctrl_state *pins_cam2_rst0;
static struct pinctrl_state *pins_cam2_rst1;
static struct pinctrl_state *pins_cam2_pnd0;
static struct pinctrl_state *pins_cam2_pnd1;
//static struct pinctrl_state *pins_cam2_d_vdd0;
//static struct pinctrl_state *pins_cam2_d_vdd1;
//static struct pinctrl_state *pins_cam2_a_vdd0;
//static struct pinctrl_state *pins_cam2_a_vdd1;
static struct pinctrl_state *pins_cam2_mclk1;
static struct pinctrl_state *pins_cam2_mclk0;

static struct input_dev *dummy_cam_input_dev;
/*************custom define start**************/
//don't support compat sensor, now
//dummy camera2:

/*************custom define end**************/


static int I2C_RxData(unsigned char reg_addr)
{
	char buffer[1],reg_value[1];
	int ret = 0;
	DCAM_LOG(" start\n");
	mutex_lock(&dcam_mutex);
	buffer[0] = reg_addr;

	ret = i2c_master_send(new_client,buffer,0x1);
	DCAM_LOG(" ret = %d\n",ret);
	
	ret = i2c_master_recv(new_client,reg_value,0x1);
	DCAM_LOG(" reg_value = 0x%x ret = %d\n",reg_value[0],ret);
	mutex_unlock(&dcam_mutex);
	DCAM_LOG(" end\n");
	return reg_value[0];
}	

static int I2C_TxData(unsigned char reg_addr,unsigned char value)
{
	int ret = 0;
	unsigned char databuf[2]; 
	mutex_lock(&dcam_mutex);

	databuf[0] = reg_addr;   
	databuf[1] = value;
	ret = i2c_master_send(new_client, databuf, 0x2);
	DCAM_LOG(" ret = %d\n",ret);
	mutex_unlock(&dcam_mutex);
	
	return 1;
}

int read_main2_device_light(void)
{
	unsigned char light_reg_addr = 0;
	unsigned char switch_page_light_addr = 0;
	unsigned char switch_page_light_num = 0;

	int read_data = 0;
	printk("read main2 device light start\n");
	DCAM_LOG(" start\n");
#ifdef CONFIG_DCAM_CAM2_SP0A08
	switch_page_light_addr = SP0A08MAIN2MIPI_SENSOR_SWITCH_PAGE_LIGHT_ADDR;
	switch_page_light_num = SP0A08MAIN2MIPI_SENSOR_SWITCH_PAGE_LIGHT_NUM;
	light_reg_addr = SP0A08MAIN2MIPI_SENSOR_LIGHT_ADDR;
#endif
#ifdef CONFIG_DCAM_CAM2_GC0310
	switch_page_light_addr = GC0310_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_ADDR;
	switch_page_light_num = GC0310_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_NUM;
	light_reg_addr = GC0310_MIPI_YUV_MAIN2_SENSOR_LIGHT_ADDR;
#endif

#ifdef CONFIG_DCAM_CAM2_GC030A
	switch_page_light_addr = GC030A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_ADDR;
	switch_page_light_num = GC030A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_NUM;
	light_reg_addr = GC030A_MIPI_YUV_MAIN2_SENSOR_LIGHT_ADDR;
#endif

#ifdef CONFIG_DCAM_CAM2_GC032A
	switch_page_light_addr = GC032A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_ADDR;
	switch_page_light_num = GC032A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_NUM;
	light_reg_addr = GC032A_MIPI_YUV_MAIN2_SENSOR_LIGHT_ADDR;
#endif

#ifdef CONFIG_DCAM_CAM2_GC6133
	switch_page_light_addr = GC6133_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_ADDR;
	switch_page_light_num = GC6133_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_LIGHT_NUM;
	light_reg_addr = GC6133_MIPI_YUV_MAIN2_SENSOR_LIGHT_ADDR;
#endif
	I2C_TxData(switch_page_light_addr,switch_page_light_num);
	read_data = I2C_RxData(light_reg_addr);
	DCAM_LOG(" read_data = %d\n",read_data);
	DCAM_LOG(" end\n");
	return read_data;
}

static void dcam_keyevent_report(int val)
{
	if(val<200){
		// input_report_key(dummy_cam_input_dev, KEY_F11, 1);
		// input_sync(dummy_cam_input_dev);
		// input_report_key(dummy_cam_input_dev, KEY_F11, 0);
		// input_sync(dummy_cam_input_dev);
		DCAM_LOG("---sunritel---dcam---input_report_key_1=%d\n",val);
	}else{
		// input_report_key(dummy_cam_input_dev, KEY_F12, 1);
		// input_sync(dummy_cam_input_dev);
		// input_report_key(dummy_cam_input_dev, KEY_F12, 0);
		// input_sync(dummy_cam_input_dev);
		DCAM_LOG("---sunritel---dcam---input_report_key_0=%d\n",val);
	}
}
static int  read_main2_light_worker_kthread(void *data)
{
	int main2_light_data = 0;
	DCAM_LOG(" start\n");
	while(!kthread_should_stop())  
	{  
		main2_light_data = read_main2_device_light();
		DCAM_LOG(" main2_light_data = %d\n",main2_light_data);
		dcam_light_val = main2_light_data;
		dcam_keyevent_report(main2_light_data);
		msleep(250);
	}  
	return 0;
}




void sp0a08_sensor_init(void)
{
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x63,0x1e);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x7c,0x6c);
	I2C_TxData(0x71,0x02);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x69,0x07);
	I2C_TxData(0x6d,0x03);
	I2C_TxData(0x71,0x02);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x1C,0x00);
	I2C_TxData(0x32,0x00);
	I2C_TxData(0x0e,0x00);
	I2C_TxData(0x0f,0x40);
	I2C_TxData(0x10,0x40);
	I2C_TxData(0x11,0x10);
	I2C_TxData(0x12,0xa0);
	I2C_TxData(0x13,0xf0);
	I2C_TxData(0x14,0x30);
	I2C_TxData(0x15,0x00);
	I2C_TxData(0x16,0x08);
	I2C_TxData(0x1A,0x37);
	I2C_TxData(0x1B,0x17);
	I2C_TxData(0x1C,0x2f);
	I2C_TxData(0x1d,0x00);
	I2C_TxData(0x1E,0x57);
	I2C_TxData(0x21,0x34);
	I2C_TxData(0x22,0x12);
	I2C_TxData(0x24,0x80);
	I2C_TxData(0x25,0x02);
	I2C_TxData(0x26,0x03);
	I2C_TxData(0x27,0xeb);
	I2C_TxData(0x28,0x5f);
	I2C_TxData(0x2f,0x01);
	I2C_TxData(0x5f,0x02);
	I2C_TxData(0xfb,0x33);
	I2C_TxData(0xf4,0x09);
	I2C_TxData(0xe7,0x03);
	I2C_TxData(0xe7,0x00);
	I2C_TxData(0x65,0x18);
	I2C_TxData(0x66,0x18);
	I2C_TxData(0x67,0x18);
	I2C_TxData(0x68,0x18);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x03,0x01);
	I2C_TxData(0x04,0x2c);
	I2C_TxData(0x05,0x00);
	I2C_TxData(0x06,0x00);
	I2C_TxData(0x09,0x01);
	I2C_TxData(0x0a,0x54);
	I2C_TxData(0xf0,0x64);
	I2C_TxData(0xf1,0x00);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x90,0x64);
	I2C_TxData(0x92,0x01);
	I2C_TxData(0x98,0x64);
	I2C_TxData(0x99,0x00);
	I2C_TxData(0x9a,0x01);
	I2C_TxData(0x9b,0x00);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0xce,0x58);
	I2C_TxData(0xcf,0x02);
	I2C_TxData(0xd0,0x58);
	I2C_TxData(0xd1,0x02);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0xc4,0x6c);
	I2C_TxData(0xc5,0x7c);
	I2C_TxData(0xca,0x30);
	I2C_TxData(0xcb,0x45);
	I2C_TxData(0xcc,0x60);
	I2C_TxData(0xcd,0x60);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x45,0x00);
	I2C_TxData(0x46,0x99);
	I2C_TxData(0x79,0xff);
	I2C_TxData(0x7a,0xff);
	I2C_TxData(0x7b,0x10);
	I2C_TxData(0x7c,0x10);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x35,0x14);
	I2C_TxData(0x36,0x14);
	I2C_TxData(0x37,0x1c);
	I2C_TxData(0x38,0x1c);
	I2C_TxData(0x39,0x10);
	I2C_TxData(0x3a,0x10);
	I2C_TxData(0x3b,0x1a);
	I2C_TxData(0x3c,0x1a);
	I2C_TxData(0x3d,0x10);
	I2C_TxData(0x3e,0x10);
	I2C_TxData(0x3f,0x15);
	I2C_TxData(0x40,0x20);
	I2C_TxData(0x41,0x0a);
	I2C_TxData(0x42,0x08);
	I2C_TxData(0x43,0x08);
	I2C_TxData(0x44,0x0a);
	I2C_TxData(0x45,0x00);
	I2C_TxData(0x46,0x00);
	I2C_TxData(0x47,0x00);
	I2C_TxData(0x48,0xfd);
	I2C_TxData(0x49,0x00);
	I2C_TxData(0x4a,0x00);
	I2C_TxData(0x4b,0x04);
	I2C_TxData(0x4c,0xfd);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xa1,0x20);
	I2C_TxData(0xa2,0x20);
	I2C_TxData(0xa3,0x20);
	I2C_TxData(0xa4,0xff);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0xde,0x0f);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x57,0x08);
	I2C_TxData(0x58,0x0e);
	I2C_TxData(0x56,0x10);
	I2C_TxData(0x59,0x10);
	I2C_TxData(0x89,0x08);
	I2C_TxData(0x8a,0x0e);
	I2C_TxData(0x9c,0x10);
	I2C_TxData(0x9d,0x10);
	I2C_TxData(0x81,0xd8);
	I2C_TxData(0x82,0x98);
	I2C_TxData(0x83,0x80);
	I2C_TxData(0x84,0x80);
	I2C_TxData(0x85,0xd8);
	I2C_TxData(0x86,0x98);
	I2C_TxData(0x87,0x80);
	I2C_TxData(0x88,0x80);
	I2C_TxData(0x5a,0xff);
	I2C_TxData(0x5b,0xb8);
	I2C_TxData(0x5c,0xa0);
	I2C_TxData(0x5d,0xa0);
	I2C_TxData(0xa7,0xff);
	I2C_TxData(0xa8,0xff);
	I2C_TxData(0xa9,0xff);
	I2C_TxData(0xaa,0xff);
	I2C_TxData(0x9e,0x10);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0xe2,0x30);
	I2C_TxData(0xe4,0xa0);
	I2C_TxData(0xe5,0x08);
	I2C_TxData(0xd3,0x10);
	I2C_TxData(0xd7,0x08);
	I2C_TxData(0xe6,0x08);
	I2C_TxData(0xd4,0x10);
	I2C_TxData(0xd8,0x08);
	I2C_TxData(0xe7,0x10);
	I2C_TxData(0xd5,0x10);
	I2C_TxData(0xd9,0x10);
	I2C_TxData(0xd2,0x10);
	I2C_TxData(0xd6,0x10);
	I2C_TxData(0xda,0x10);
	I2C_TxData(0xe8,0x28);
	I2C_TxData(0xec,0x38);
	I2C_TxData(0xe9,0x20);
	I2C_TxData(0xed,0x38);
	I2C_TxData(0xea,0x10);
	I2C_TxData(0xef,0x20);
	I2C_TxData(0xeb,0x10);
	I2C_TxData(0xf0,0x10);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0xa0,0x8a);
	I2C_TxData(0xa1,0xfb);
	I2C_TxData(0xa2,0xfc);
	I2C_TxData(0xa3,0xfe);
	I2C_TxData(0xa4,0x93);
	I2C_TxData(0xa5,0xf0);
	I2C_TxData(0xa6,0x0b);
	I2C_TxData(0xa7,0xd8);
	I2C_TxData(0xa8,0x9d);
	I2C_TxData(0xa9,0x3c);
	I2C_TxData(0xaa,0x33);
	I2C_TxData(0xab,0x0c);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x8b,0x00);
	I2C_TxData(0x8c,0x0c);
	I2C_TxData(0x8d,0x19);
	I2C_TxData(0x8e,0x2c);
	I2C_TxData(0x8f,0x49);
	I2C_TxData(0x90,0x61);
	I2C_TxData(0x91,0x77);
	I2C_TxData(0x92,0x8a);
	I2C_TxData(0x93,0x9b);
	I2C_TxData(0x94,0xa9);
	I2C_TxData(0x95,0xb5);
	I2C_TxData(0x96,0xc0);
	I2C_TxData(0x97,0xca);
	I2C_TxData(0x98,0xd4);
	I2C_TxData(0x99,0xdd);
	I2C_TxData(0x9a,0xe6);
	I2C_TxData(0x9b,0xef);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x8d,0xf7);
	I2C_TxData(0x8e,0xff);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x28,0xc4);
	I2C_TxData(0x29,0x9e);
	I2C_TxData(0x11,0x13);
	I2C_TxData(0x12,0x13);
	I2C_TxData(0x2e,0x13);
	I2C_TxData(0x2f,0x13);
	I2C_TxData(0x16,0x1c);
	I2C_TxData(0x17,0x1a);
	I2C_TxData(0x18,0x1a);
	I2C_TxData(0x19,0x54);
	I2C_TxData(0x1a,0xa5);
	I2C_TxData(0x1b,0x9a);
	I2C_TxData(0x2a,0xef);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xe0,0x3a);
	I2C_TxData(0xe1,0x2c);
	I2C_TxData(0xe2,0x26);
	I2C_TxData(0xe3,0x22);
	I2C_TxData(0xe4,0x22);
	I2C_TxData(0xe5,0x20);
	I2C_TxData(0xe6,0x20);
	I2C_TxData(0xe8,0x20);
	I2C_TxData(0xe9,0x20);
	I2C_TxData(0xea,0x20);
	I2C_TxData(0xeb,0x1e);
	I2C_TxData(0xf5,0x1e);
	I2C_TxData(0xf6,0x1e);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x94,0x60);
	I2C_TxData(0x95,0x1e);
	I2C_TxData(0x9c,0x60);
	I2C_TxData(0x9d,0x1e);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xed,0x94);
	I2C_TxData(0xf7,0x90);
	I2C_TxData(0xf8,0x88);
	I2C_TxData(0xec,0x84);
	I2C_TxData(0xef,0x88);
	I2C_TxData(0xf9,0x84);
	I2C_TxData(0xfa,0x7c);
	I2C_TxData(0xee,0x78);
	I2C_TxData(0xfd,0x01);
	I2C_TxData(0x30,0x40);
	I2C_TxData(0x31,0x70);
	I2C_TxData(0x32,0x20);
	I2C_TxData(0x33,0xef);
	I2C_TxData(0x34,0x02);
	I2C_TxData(0x4d,0x40);
	I2C_TxData(0x4e,0x15);
	I2C_TxData(0x4f,0x13);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xbe,0x5a);
	I2C_TxData(0xc0,0xff);
	I2C_TxData(0xc1,0xff);
	I2C_TxData(0xd3,0x78);
	I2C_TxData(0xd4,0x78);
	I2C_TxData(0xd6,0x70);
	I2C_TxData(0xd7,0x60);
	I2C_TxData(0xd8,0x78);
	I2C_TxData(0xd9,0x78);
	I2C_TxData(0xda,0x70);
	I2C_TxData(0xdb,0x60);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xdc,0x00);
	I2C_TxData(0xdd,0x88);
	I2C_TxData(0xde,0x90);
	I2C_TxData(0xdf,0x80);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xc2,0x08);
	I2C_TxData(0xc3,0x08);
	I2C_TxData(0xc4,0x08);
	I2C_TxData(0xc5,0x10);
	I2C_TxData(0xc6,0x80);
	I2C_TxData(0xc7,0x80);
	I2C_TxData(0xc8,0x80);
	I2C_TxData(0xc9,0x80);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0xb2,0x18);
	I2C_TxData(0xb3,0x1f);
	I2C_TxData(0xb4,0x30);
	I2C_TxData(0xb5,0x45);
	I2C_TxData(0xfd,0x00);
	I2C_TxData(0x32,0x0d);
	I2C_TxData(0x31,0x70);
	I2C_TxData(0x34,0x7e);
	I2C_TxData(0x33,0xef);
	I2C_TxData(0x35,0x00);
	
}


void gc0310_sensor_init(void)
{
	I2C_TxData(0xfe,0xf0);
	I2C_TxData(0xfe,0xf0);
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0xfc,0x0e);
	I2C_TxData(0xfc,0x0e);
	I2C_TxData(0xf2,0x80);
	I2C_TxData(0xf3,0x00);
	I2C_TxData(0xf7,0x1b);
	I2C_TxData(0xf8,0x04);
	I2C_TxData(0xf9,0x8e);
	I2C_TxData(0xfa,0x11);
	
	I2C_TxData(0xfe,0x03);
	I2C_TxData(0x40,0x08);
	I2C_TxData(0x42,0x00);
	I2C_TxData(0x43,0x00);
	I2C_TxData(0x01,0x03);
	I2C_TxData(0x10,0x84);
	
	I2C_TxData(0x01,0x03);
	I2C_TxData(0x02,0x11);
	I2C_TxData(0x03,0x94);
	I2C_TxData(0x04,0x01);
	I2C_TxData(0x05,0x00);
	I2C_TxData(0x06,0x80);
	I2C_TxData(0x11,0x1e);
	I2C_TxData(0x12,0x00);
	I2C_TxData(0x13,0x05);
	I2C_TxData(0x15,0x10);
	I2C_TxData(0x21,0x10);
	I2C_TxData(0x22,0x01);
	I2C_TxData(0x23,0x10);
	I2C_TxData(0x24,0x02);
	I2C_TxData(0x25,0x10);
	I2C_TxData(0x26,0x03);
	I2C_TxData(0x29,0x02);
	I2C_TxData(0x2a,0x0a);
	I2C_TxData(0x2b,0x04);
	I2C_TxData(0xfe,0x00);
	
	I2C_TxData(0x00,0x2f);	
	I2C_TxData(0x01,0x0f);
	I2C_TxData(0x02,0x04);
	I2C_TxData(0x03,0x03);
	I2C_TxData(0x04,0x50);
	I2C_TxData(0x09,0x00);
	I2C_TxData(0x0a,0x00);
	I2C_TxData(0x0b,0x00);
	I2C_TxData(0x0c,0x04);
	I2C_TxData(0x0d,0x01);
	I2C_TxData(0x0e,0xe8);
	I2C_TxData(0x0f,0x02);
	I2C_TxData(0x10,0x88);
	I2C_TxData(0x16,0x00);
	I2C_TxData(0x17,0x14);
	I2C_TxData(0x18,0x1a);
	I2C_TxData(0x19,0x14);
	I2C_TxData(0x1b,0x48);
	I2C_TxData(0x1c,0x1c);
	I2C_TxData(0x1e,0x6b);
	I2C_TxData(0x1f,0x28);
	I2C_TxData(0x20,0x8b);
	I2C_TxData(0x21,0x49);
	I2C_TxData(0x22,0xb0);
	I2C_TxData(0x23,0x04);
	I2C_TxData(0x24,0x16);
	I2C_TxData(0x34,0x20);
	
	I2C_TxData(0x26,0x23);
	I2C_TxData(0x28,0xff);
	I2C_TxData(0x29,0x00);
	I2C_TxData(0x32,0x00);
	I2C_TxData(0x33,0x10);
	I2C_TxData(0x37,0x20);
	I2C_TxData(0x38,0x10);
	I2C_TxData(0x47,0x80);
	I2C_TxData(0x4e,0x66);
	I2C_TxData(0xa8,0x02);
	I2C_TxData(0xa9,0x80);
	
	I2C_TxData(0x40,0xff);
	I2C_TxData(0x41,0x21);
	I2C_TxData(0x42,0xcf);
	I2C_TxData(0x44,0x02);
	I2C_TxData(0x45,0xa8);
	I2C_TxData(0x46,0x02);
	I2C_TxData(0x4a,0x11);
	I2C_TxData(0x4b,0x01);
	I2C_TxData(0x4c,0x20);
	I2C_TxData(0x4d,0x05);
	I2C_TxData(0x4f,0x01);
	I2C_TxData(0x50,0x01);
	I2C_TxData(0x55,0x01);
	I2C_TxData(0x56,0xe0);
	I2C_TxData(0x57,0x02);
	I2C_TxData(0x58,0x80);
	
	I2C_TxData(0x70,0x70);
	I2C_TxData(0x5a,0x84);
	I2C_TxData(0x5b,0xc9);
	I2C_TxData(0x5c,0xed);
	I2C_TxData(0x77,0x74);
	I2C_TxData(0x78,0x40);
	I2C_TxData(0x79,0x5f);
	
	I2C_TxData(0x82,0x14);
	I2C_TxData(0x83,0x0b);
	I2C_TxData(0x89,0xf0);
	
	I2C_TxData(0x8f,0xaa);
	I2C_TxData(0x90,0x8c);
	I2C_TxData(0x91,0x90);
	I2C_TxData(0x92,0x03);
	I2C_TxData(0x93,0x03);
	I2C_TxData(0x94,0x05);
	I2C_TxData(0x95,0x65);
	I2C_TxData(0x96,0xf0);
	
	I2C_TxData(0xfe,0x00);

	I2C_TxData(0x9a,0x20);
	I2C_TxData(0x9b,0x80);
	I2C_TxData(0x9c,0x40);
	I2C_TxData(0x9d,0x80);

	I2C_TxData(0xa1,0x30);
	I2C_TxData(0xa2,0x32);
	I2C_TxData(0xa4,0x30);
	I2C_TxData(0xa5,0x30);
	I2C_TxData(0xaa,0x10);
	I2C_TxData(0xac,0x22);

	I2C_TxData(0xfe,0x00);
	I2C_TxData(0xbf,0x08);
	I2C_TxData(0xc0,0x16);
	I2C_TxData(0xc1,0x28);
	I2C_TxData(0xc2,0x41);
	I2C_TxData(0xc3,0x5a);
	I2C_TxData(0xc4,0x6c);
	I2C_TxData(0xc5,0x7a);
	I2C_TxData(0xc6,0x96);
	I2C_TxData(0xc7,0xac);
	I2C_TxData(0xc8,0xbc);
	I2C_TxData(0xc9,0xc9);
	I2C_TxData(0xca,0xd3);
	I2C_TxData(0xcb,0xdd);
	I2C_TxData(0xcc,0xe5);
	I2C_TxData(0xcd,0xf1);
	I2C_TxData(0xce,0xfa);
	I2C_TxData(0xcf,0xff);
	
	I2C_TxData(0xd0,0x40);
	I2C_TxData(0xd1,0x34);
	I2C_TxData(0xd2,0x34);
	I2C_TxData(0xd3,0x40);
	I2C_TxData(0xd6,0xf2);
	I2C_TxData(0xd7,0x1b);
	I2C_TxData(0xd8,0x18);
	I2C_TxData(0xdd,0x03);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x05,0x30);
	I2C_TxData(0x06,0x75);
	I2C_TxData(0x07,0x40);
	I2C_TxData(0x08,0xb0);
	I2C_TxData(0x0a,0xc5);
	I2C_TxData(0x0b,0x11);
	I2C_TxData(0x0c,0x00);
	I2C_TxData(0x12,0x52);
	I2C_TxData(0x13,0x38);
	I2C_TxData(0x18,0x95);
	I2C_TxData(0x19,0x96);
	I2C_TxData(0x1f,0x20);
	I2C_TxData(0x20,0xc0);
	I2C_TxData(0x3e,0x40);
	I2C_TxData(0x3f,0x57);
	I2C_TxData(0x40,0x7d);
	I2C_TxData(0x03,0x60);
	I2C_TxData(0x44,0x02);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x1c,0x91);
	I2C_TxData(0x21,0x15);
	I2C_TxData(0x50,0x80);
	I2C_TxData(0x56,0x04);
	I2C_TxData(0x59,0x08);
	I2C_TxData(0x5b,0x02);
	I2C_TxData(0x61,0x8d);
	I2C_TxData(0x62,0xa7);
	I2C_TxData(0x63,0xd0);
	I2C_TxData(0x65,0x06);
	I2C_TxData(0x66,0x06);
	I2C_TxData(0x67,0x84);
	I2C_TxData(0x69,0x08);
	I2C_TxData(0x6a,0x25);
	I2C_TxData(0x6b,0x01);
	I2C_TxData(0x6c,0x00);
	I2C_TxData(0x6d,0x02);
	I2C_TxData(0x6e,0xf0);
	I2C_TxData(0x6f,0x80);
	I2C_TxData(0x76,0x80);
	I2C_TxData(0x78,0xaf);
	I2C_TxData(0x79,0x75);
	I2C_TxData(0x7a,0x40);
	I2C_TxData(0x7b,0x50);
	I2C_TxData(0x7c,0x0c);
	
	I2C_TxData(0x90,0xc9);
	I2C_TxData(0x91,0xbe);
	I2C_TxData(0x92,0xe2);
	I2C_TxData(0x93,0xc9);
	I2C_TxData(0x95,0x1b);
	I2C_TxData(0x96,0xe2);
	I2C_TxData(0x97,0x49);
	I2C_TxData(0x98,0x1b);
	I2C_TxData(0x9a,0x49);
	I2C_TxData(0x9b,0x1b);
	I2C_TxData(0x9c,0xc3);
	I2C_TxData(0x9d,0x49);
	I2C_TxData(0x9f,0xc7);
	I2C_TxData(0xa0,0xc8);
	I2C_TxData(0xa1,0x00);
	I2C_TxData(0xa2,0x00);
	I2C_TxData(0x86,0x00);
	I2C_TxData(0x87,0x00);
	I2C_TxData(0x88,0x00);
	I2C_TxData(0x89,0x00);
	I2C_TxData(0xa4,0xb9);
	I2C_TxData(0xa5,0xa0);
	I2C_TxData(0xa6,0xba);
	I2C_TxData(0xa7,0x92);
	I2C_TxData(0xa9,0xba);
	I2C_TxData(0xaa,0x80);
	I2C_TxData(0xab,0x9d);
	I2C_TxData(0xac,0x7f);
	I2C_TxData(0xae,0xbb);
	I2C_TxData(0xaf,0x9d);
	I2C_TxData(0xb0,0xc8);
	I2C_TxData(0xb1,0x97);
	I2C_TxData(0xb3,0xb7);
	I2C_TxData(0xb4,0x7f);
	I2C_TxData(0xb5,0x00);
	I2C_TxData(0xb6,0x00);
	I2C_TxData(0x8b,0x00);
	I2C_TxData(0x8c,0x00);
	I2C_TxData(0x8d,0x00);
	I2C_TxData(0x8e,0x00);
	I2C_TxData(0x94,0x55);
	I2C_TxData(0x99,0xa6);
	I2C_TxData(0x9e,0xaa);
	I2C_TxData(0xa3,0x0a);
	I2C_TxData(0x8a,0x00);
    I2C_TxData(0xa8,0x55);
    I2C_TxData(0xad,0x55);
    I2C_TxData(0xb2,0x55);
    I2C_TxData(0xb7,0x05);
    I2C_TxData(0x8f,0x00);
    I2C_TxData(0xb8,0xcb);
    I2C_TxData(0xb9,0x9b);

	I2C_TxData(0xfe,0x01);

	I2C_TxData(0xd0,0x38);
	I2C_TxData(0xd1,0x00);
	I2C_TxData(0xd2,0x02);
	I2C_TxData(0xd3,0x04);
	I2C_TxData(0xd4,0x38);
	I2C_TxData(0xd5,0x12);

	I2C_TxData(0xd6,0x30);
	I2C_TxData(0xd7,0x00);
	I2C_TxData(0xd8,0x0a);
	I2C_TxData(0xd9,0x16);
	I2C_TxData(0xda,0x39);
	I2C_TxData(0xdb,0xf8);

	I2C_TxData(0xfe,0x01);
	I2C_TxData(0xc1,0x3c);
	I2C_TxData(0xc2,0x50);
	I2C_TxData(0xc3,0x00);
	I2C_TxData(0xc4,0x40);
	I2C_TxData(0xc5,0x30);
    I2C_TxData(0xc6,0x30);
    I2C_TxData(0xc7,0x10);
    I2C_TxData(0xc8,0x00);
    I2C_TxData(0xc9,0x00);
    I2C_TxData(0xdc,0x20);
    I2C_TxData(0xdd,0x10);
    I2C_TxData(0xdf,0x00);
    I2C_TxData(0xde,0x00);
	
    I2C_TxData(0x01,0x10);	
	I2C_TxData(0x0b,0x31);	
	I2C_TxData(0x0e,0x50);	
	I2C_TxData(0x0f,0x0f);	
	I2C_TxData(0x10,0x6e);	
	I2C_TxData(0x12,0xa0);	
	I2C_TxData(0x15,0x60);	
	I2C_TxData(0x16,0x60);	
	I2C_TxData(0x17,0xe0);	

	I2C_TxData(0xcc,0x0c);
	I2C_TxData(0xcd,0x10);
	I2C_TxData(0xce,0xa0);
	I2C_TxData(0xcf,0xe6);
	
	I2C_TxData(0x45,0xf7);
	I2C_TxData(0x46,0xff);
	I2C_TxData(0x47,0x15);
	I2C_TxData(0x48,0x03);
	I2C_TxData(0x4f,0x60);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x05,0x02);
	I2C_TxData(0x06,0xd1);
	I2C_TxData(0x07,0x00);
	I2C_TxData(0x08,0x22);
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x25,0x00);
	I2C_TxData(0x26,0x6a);
	I2C_TxData(0x27,0x02);
	I2C_TxData(0x28,0x12);
	I2C_TxData(0x29,0x03);
	I2C_TxData(0x2a,0x50);
	I2C_TxData(0x2b,0x05);
	I2C_TxData(0x2c,0xcc);
	I2C_TxData(0x2d,0x07);
	I2C_TxData(0x2e,0x74);
	I2C_TxData(0x3c,0x20);
	I2C_TxData(0xfe,0x00);

	I2C_TxData(0xfe,0x03);
	I2C_TxData(0x10,0x94);
	I2C_TxData(0xfe,0x00);	
	
}

void gc032A_sensor_init(void)
{
	I2C_TxData(0xf3,0xff);
	I2C_TxData(0xf5,0x06);
	I2C_TxData(0xf7,0x01);
	I2C_TxData(0xf8,0x03);
	I2C_TxData(0xf9,0xce);
	I2C_TxData(0xfa,0x00);
	I2C_TxData(0xfc,0x02);
	I2C_TxData(0xfe,0x02);
	I2C_TxData(0x81,0x03);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x77,0x64);
	I2C_TxData(0x78,0x40);
	I2C_TxData(0x79,0x60);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x03,0x01);
	I2C_TxData(0x04,0xce);
	I2C_TxData(0x05,0x01);
	I2C_TxData(0x06,0xad);
	I2C_TxData(0x07,0x00);
	I2C_TxData(0x08,0x10);
	I2C_TxData(0x0a,0x00);
	I2C_TxData(0x0c,0x00);
	I2C_TxData(0x0d,0x01);
	I2C_TxData(0x0e,0xe8);
	I2C_TxData(0x0f,0x02);
	I2C_TxData(0x10,0x88);
	I2C_TxData(0x17,0x54);
	I2C_TxData(0x19,0x08);
	I2C_TxData(0x1a,0x0a);
	I2C_TxData(0x1f,0x40);
	I2C_TxData(0x20,0x30);
	I2C_TxData(0x2e,0x80);
	I2C_TxData(0x2f,0x2b);
	I2C_TxData(0x30,0x1a);
	I2C_TxData(0xfe,0x02);
	I2C_TxData(0x03,0x02);
	I2C_TxData(0x05,0xd7);
	I2C_TxData(0x06,0x60);
	I2C_TxData(0x08,0x80);
	I2C_TxData(0x12,0x89);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x18,0x02);
	I2C_TxData(0xfe,0x02);
	I2C_TxData(0x40,0x22);
	I2C_TxData(0x45,0x00);
	I2C_TxData(0x46,0x00);
	I2C_TxData(0x49,0x20);
	I2C_TxData(0x4b,0x3c);
	I2C_TxData(0x50,0x20);
	I2C_TxData(0x42,0x10);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x0a,0xc5);
	I2C_TxData(0x45,0x00);
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x40,0xff);
	I2C_TxData(0x41,0x25);
	I2C_TxData(0x42,0xcf);
	I2C_TxData(0x43,0x10);
	I2C_TxData(0x44,0x83);
	I2C_TxData(0x46,0x22);
	I2C_TxData(0x49,0x03);
	I2C_TxData(0x52,0x02);
	I2C_TxData(0x54,0x00);
	I2C_TxData(0xfe,0x02);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0xc1,0x38);
	I2C_TxData(0xc2,0x4c);
	I2C_TxData(0xc3,0x00);
	I2C_TxData(0xc4,0x32);
	I2C_TxData(0xc5,0x24);
	I2C_TxData(0xc6,0x16);
	I2C_TxData(0xc7,0x08);
	I2C_TxData(0xc8,0x08);
	I2C_TxData(0xc9,0x00);
	I2C_TxData(0xca,0x20);
	I2C_TxData(0xdc,0x8a);
	I2C_TxData(0xdd,0xa0);
	I2C_TxData(0xde,0xa6);
	I2C_TxData(0xdf,0x75);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x7c,0x09);
	I2C_TxData(0x65,0x06);
	I2C_TxData(0x7c,0x08);
	I2C_TxData(0x56,0xf4);
	I2C_TxData(0x66,0x0f);
	I2C_TxData(0x67,0x84);
	I2C_TxData(0x6b,0x80);
	I2C_TxData(0x6d,0x12);
	I2C_TxData(0x6e,0xb0);
	I2C_TxData(0x86,0x00);
	I2C_TxData(0x87,0x00);
	I2C_TxData(0x88,0x00);
	I2C_TxData(0x89,0x00);
	I2C_TxData(0x8a,0x00);
	I2C_TxData(0x8b,0x00);
	I2C_TxData(0x8c,0x00);
	I2C_TxData(0x8d,0x00);
	I2C_TxData(0x8e,0x00);
	I2C_TxData(0x8f,0x00);
	I2C_TxData(0x90,0x00);
	I2C_TxData(0x91,0x00);
	I2C_TxData(0x92,0xf4);
	I2C_TxData(0x93,0xd5);
	I2C_TxData(0x94,0x50);
	I2C_TxData(0x95,0x0f);
	I2C_TxData(0x96,0xf4);
	I2C_TxData(0x97,0x2d);
	I2C_TxData(0x98,0x0f);
	I2C_TxData(0x99,0xa6);
	I2C_TxData(0x9a,0x2d);
	I2C_TxData(0x9b,0x0f);
	I2C_TxData(0x9c,0x59);
	I2C_TxData(0x9d,0x2d);
	I2C_TxData(0x9e,0xaa);
	I2C_TxData(0x9f,0x67);
	I2C_TxData(0xa0,0x59);
	I2C_TxData(0xa1,0x00);
	I2C_TxData(0xa2,0x00);
	I2C_TxData(0xa3,0x0a);
	I2C_TxData(0xa4,0x00);
	I2C_TxData(0xa5,0x00);
	I2C_TxData(0xa6,0xd4);
	I2C_TxData(0xa7,0x9f);
	I2C_TxData(0xa8,0x55);
	I2C_TxData(0xa9,0xd4);
	I2C_TxData(0xaa,0x9f);
	I2C_TxData(0xab,0xac);
	I2C_TxData(0xac,0x9f);
	I2C_TxData(0xad,0x55);
	I2C_TxData(0xae,0xd4);
	I2C_TxData(0xaf,0xac);
	I2C_TxData(0xb0,0xd4);
	I2C_TxData(0xb1,0xa3);
	I2C_TxData(0xb2,0x55);
	I2C_TxData(0xb3,0xd4);
	I2C_TxData(0xb4,0xac);
	I2C_TxData(0xb5,0x00);
	I2C_TxData(0xb6,0x00);
	I2C_TxData(0xb7,0x05);
	I2C_TxData(0xb8,0xd6);
	I2C_TxData(0xb9,0x8c);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0xd0,0x40);
	I2C_TxData(0xd1,0xf8);
	I2C_TxData(0xd2,0x00);
	I2C_TxData(0xd3,0xfa);
	I2C_TxData(0xd4,0x45);
	I2C_TxData(0xd5,0x02);
	
	I2C_TxData(0xd6,0x30);
	I2C_TxData(0xd7,0xfa);
	I2C_TxData(0xd8,0x08);
	I2C_TxData(0xd9,0x08);
	I2C_TxData(0xda,0x58);
	I2C_TxData(0xdb,0x02);
	I2C_TxData(0xfe,0x00);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0xba,0x00);
	I2C_TxData(0xbb,0x04);
	I2C_TxData(0xbc,0x0a);
	I2C_TxData(0xbd,0x0e);
	I2C_TxData(0xbe,0x22);
	I2C_TxData(0xbf,0x30);
	I2C_TxData(0xc0,0x3d);
	I2C_TxData(0xc1,0x4a);
	I2C_TxData(0xc2,0x5d);
	I2C_TxData(0xc3,0x6b);
	I2C_TxData(0xc4,0x7a);
	I2C_TxData(0xc5,0x85);
	I2C_TxData(0xc6,0x90);
	I2C_TxData(0xc7,0xa5);
	I2C_TxData(0xc8,0xb5);
	I2C_TxData(0xc9,0xc2);
	I2C_TxData(0xca,0xcc);
	I2C_TxData(0xcb,0xd5);
	I2C_TxData(0xcc,0xde);
	I2C_TxData(0xcd,0xea);
	I2C_TxData(0xce,0xf5);
	I2C_TxData(0xcf,0xff);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x5a,0x08);
	I2C_TxData(0x5b,0x0f);
	I2C_TxData(0x5c,0x15);
	I2C_TxData(0x5d,0x1c);
	I2C_TxData(0x5e,0x28);
	I2C_TxData(0x5f,0x36);
	I2C_TxData(0x60,0x45);
	I2C_TxData(0x61,0x51);
	I2C_TxData(0x62,0x6a);
	I2C_TxData(0x63,0x7d);
	I2C_TxData(0x64,0x8d);
	I2C_TxData(0x65,0x98);
	I2C_TxData(0x66,0xa2);
	I2C_TxData(0x67,0xb5);
	I2C_TxData(0x68,0xc3);
	I2C_TxData(0x69,0xcd);
	I2C_TxData(0x6a,0xd4);
	I2C_TxData(0x6b,0xdc);
	I2C_TxData(0x6c,0xe3);
	I2C_TxData(0x6d,0xf0);
	I2C_TxData(0x6e,0xf9);
	I2C_TxData(0x6f,0xff);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x70,0x50);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x4f,0x01);
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x0d,0x00);
	I2C_TxData(0x12,0xa0);
	I2C_TxData(0x13,0x3a);
	I2C_TxData(0x44,0x04);
	I2C_TxData(0x1f,0x30);
	I2C_TxData(0x20,0x40);
	I2C_TxData(0x26,0x9a);
	I2C_TxData(0x3e,0x20);
	I2C_TxData(0x3f,0x2d);
	I2C_TxData(0x40,0x40);
	I2C_TxData(0x41,0x5b);
	I2C_TxData(0x42,0x82);
	I2C_TxData(0x43,0xb7);
	I2C_TxData(0x04,0x0a);
	I2C_TxData(0x02,0x79);
	I2C_TxData(0x03,0xc0);
	
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0xcc,0x08);
	I2C_TxData(0xcd,0x08);
	I2C_TxData(0xce,0xa4);
	I2C_TxData(0xcf,0xec);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x81,0xb8);
	I2C_TxData(0x82,0x12);
	I2C_TxData(0x83,0x0a);
	I2C_TxData(0x84,0x01);
	I2C_TxData(0x86,0x50);
	I2C_TxData(0x87,0x18);
	I2C_TxData(0x88,0x10);
	I2C_TxData(0x89,0x70);
	I2C_TxData(0x8a,0x20);
	I2C_TxData(0x8b,0x10);
	I2C_TxData(0x8c,0x08);
	I2C_TxData(0x8d,0x0a);
	
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x8f,0xaa);
	I2C_TxData(0x90,0x9c);
	I2C_TxData(0x91,0x52);
	I2C_TxData(0x92,0x03);
	I2C_TxData(0x93,0x03);
	I2C_TxData(0x94,0x08);
	I2C_TxData(0x95,0x44);
	I2C_TxData(0x97,0x00);
	I2C_TxData(0x98,0x00);
	                      
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0xa1,0x30);
	I2C_TxData(0xa2,0x41);
	I2C_TxData(0xa4,0x30);
	I2C_TxData(0xa5,0x20);
	I2C_TxData(0xaa,0x30);
	I2C_TxData(0xac,0x32);
	                      
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0xd1,0x3c);
	I2C_TxData(0xd2,0x3c);
	I2C_TxData(0xd3,0x38);
	I2C_TxData(0xd6,0xf4);
	I2C_TxData(0xd7,0x1d);
	I2C_TxData(0xdd,0x73);
	I2C_TxData(0xde,0x84);
	                      
	I2C_TxData(0xfe,0x00);
	I2C_TxData(0x05,0x01);
	I2C_TxData(0x06,0xad);
	I2C_TxData(0x07,0x00);
	I2C_TxData(0x08,0x10);
	           
	I2C_TxData(0xfe,0x01);
	I2C_TxData(0x25,0x00);
	I2C_TxData(0x26,0x9a);
	           
	I2C_TxData(0x27,0x01);
	I2C_TxData(0x28,0xce);
	I2C_TxData(0x29,0x03);
	I2C_TxData(0x2a,0x02);
	I2C_TxData(0x2b,0x04);
	I2C_TxData(0x2c,0x36);
	I2C_TxData(0x2d,0x07);
	I2C_TxData(0x2e,0xd2);
	I2C_TxData(0x2f,0x0b);
	I2C_TxData(0x30,0x6e);
	I2C_TxData(0x31,0x0e);
	I2C_TxData(0x32,0x70);
	I2C_TxData(0x33,0x12);
	I2C_TxData(0x34,0x0c);
	I2C_TxData(0x3c,0x30);
	I2C_TxData(0xfe,0x00);
		
	
}

#ifdef CONFIG_DCAM_CAM2_GC6133
void gc6133_sensor_init(void)
{
	//SYS
	I2C_TxData(0xfe, 0xa0);
	I2C_TxData(0xfe, 0xa0);
	I2C_TxData(0xfe, 0xa0);

	I2C_TxData(0xf6, 0x00);
	I2C_TxData(0xfa, 0x11);
	I2C_TxData(0xfc, 0x12); //clock enable

	I2C_TxData(0xfe , 0x00); //Page select
	I2C_TxData(0x49, 0x70);  //AWB r gain
	I2C_TxData(0x4a, 0x40);  //AWB g gain
	I2C_TxData(0x4b, 0x5d);  //AWB b gain

	//ANALOG & CISCTL
	I2C_TxData(0x03, 0x00);
	I2C_TxData(0x04, 0xfa);
	I2C_TxData(0x01, 0x41); //hb
	I2C_TxData(0x02, 0x12); //vb
	I2C_TxData(0x0f, 0x01);
	I2C_TxData(0x0d, 0x30);
	I2C_TxData(0x12, 0xc8);
	I2C_TxData(0x14, 0x54); //dark CFA
	I2C_TxData(0x15, 0x32); //1:sdark 0:ndark
	I2C_TxData(0x16, 0x04);
	I2C_TxData(0x17, 0x19);
	I2C_TxData(0x1d, 0xb9);
	I2C_TxData(0x1f, 0x35); //PAD_drv
	I2C_TxData(0x7a, 0x00);
	I2C_TxData(0x7b, 0x14);
	I2C_TxData(0x7d, 0x36);
	I2C_TxData(0xfe, 0x10);  //add by 20160217 CISCTL rst [4]

	//ISP
	I2C_TxData(0x20, 0x7e);
	I2C_TxData(0x22, 0xb8);
	I2C_TxData(0x24, 0x54); //output_format
	I2C_TxData(0x26, 0x87); //[5]Y_switch [4]UV_switch [2]skin_en
	//I2C_TxData(0x29, 0x10);// disable isp quiet mode

	I2C_TxData(0x39, 0x00); //crop window
	I2C_TxData(0x3a, 0x80); 
	I2C_TxData(0x3b, 0x01);  //width
	I2C_TxData(0x3c, 0x40); 
	I2C_TxData(0x3e, 0xf0); //height

	//BLK

	I2C_TxData(0x2a, 0x2f);
	I2C_TxData(0x37, 0x46); //[4:0]blk_select_row


	//GAIN
	I2C_TxData(0x3f, 0x18); //global gain 20160901


	//DNDD
	I2C_TxData(0x50, 0x3c); 
	I2C_TxData(0x52, 0x4f); 
	I2C_TxData(0x53, 0x81);
	I2C_TxData(0x54, 0x43);
	I2C_TxData(0x56, 0x78); 
	I2C_TxData(0x57, 0xaa);//20160901 
	I2C_TxData(0x58, 0xff);//20160901


	//ASDE
	I2C_TxData(0x5b, 0x60); //dd&ee th
	I2C_TxData(0x5c, 0x80); //60/OT_th
	I2C_TxData(0xab, 0x28); 
	I2C_TxData(0xac, 0xb5);

	//INTPEE
	I2C_TxData(0x60, 0x45); 
	I2C_TxData(0x62, 0x68); //20160901
	I2C_TxData(0x63, 0x13); //edge effect
	I2C_TxData(0x64, 0x43);

	//CC
	I2C_TxData(0x65, 0x13); //Y
	I2C_TxData(0x66, 0x26);
	I2C_TxData(0x67, 0x07);
	I2C_TxData(0x68, 0xf5); //Cb
	I2C_TxData(0x69, 0xea);
	I2C_TxData(0x6a, 0x21);
	I2C_TxData(0x6b, 0x21); //Cr
	I2C_TxData(0x6c, 0xe4);
	I2C_TxData(0x6d, 0xfb);

	//YCP
	I2C_TxData(0x81, 0x30); //cb
	I2C_TxData(0x82, 0x30); //cr
	I2C_TxData(0x83, 0x4a); //luma contrast
	I2C_TxData(0x85, 0x06);  //luma offset
	I2C_TxData(0x8d, 0x78); //edge dec sa
	I2C_TxData(0x8e, 0x25); //autogray

	//AEC
	I2C_TxData(0x90, 0x38);//20160901
	I2C_TxData(0x92, 0x50); //target
	I2C_TxData(0x9d, 0x32);//STEP
	I2C_TxData(0x9e, 0x61);//[7:4]margin  10fps
	I2C_TxData(0x9f, 0xf4);
	I2C_TxData(0xa3, 0x28); //pregain
	I2C_TxData(0xa4, 0x01); 

	//AWB
#if 0//AWB2
	I2C_TxData(0xb0, 0xf2); //Y_to_C_diff
	I2C_TxData(0xb1, 0x10); //Y_to_C_diff
	I2C_TxData(0xb2, 0x08); //AWB_Y_to_C_diff_big
	I2C_TxData(0xb3, 0x30); //C_max   //20
	I2C_TxData(0xb4, 0x40); 
	I2C_TxData(0xb5, 0x20); //inter
	I2C_TxData(0xb6, 0x34); //inter2
	I2C_TxData(0xb7, 0x48); //AWB_C_inter3	 18
	I2C_TxData(0xba, 0x40);   // big c  20
	I2C_TxData(0xbb, 0x71); //62//AWB adjust   72
	I2C_TxData(0xbd, 0x7a); //R_limit  80
	I2C_TxData(0xbe, 0x40); //G_limit   58
	I2C_TxData(0xbf, 0x80); //B_limit    a0	
#else
	I2C_TxData(0xb1, 0x1e); //Y_to_C_diff
	I2C_TxData(0xb3, 0x20); //C_max
	I2C_TxData(0xbd, 0x70); //R_limit
	I2C_TxData(0xbe, 0x58); //G_limit
	I2C_TxData(0xbf, 0xa0); //B_limit

	I2C_TxData(0xfe, 0x00);	//20160901 update for AWB
	I2C_TxData(0x43, 0xa8); 
	I2C_TxData(0xb0, 0xf2); 
	I2C_TxData(0xb5, 0x40); 
	I2C_TxData(0xb8, 0x05); 
	I2C_TxData(0xba, 0x60); 	
#endif

	//Banding
	//I2C_TxData(0x01, 0x41); //hb
	//I2C_TxData(0x02, 0x12); //vb
	//I2C_TxData(0x0f, 0x01);
	//I2C_TxData(0x9d, 0x32); //step
	//I2C_TxData(0x9e, 0x61); //[7:4]margin  10fps
	//I2C_TxData(0x9f, 0xf4); 

	//SPI
	I2C_TxData(0xfe , 0x02);
	I2C_TxData(0x01, 0x01); //spi enable
	I2C_TxData(0x02, 0x02);   //LSB & Falling edge sample; ddr disable
	I2C_TxData(0x03, 0x20);	//1-wire
	I2C_TxData(0x04, 0x20);   //[4] master_outformat
	I2C_TxData(0x0a, 0x00);   //Data ID, 0x00-YUV422, 0x01-RGB565
	I2C_TxData(0x13, 0x10);
	I2C_TxData(0x24, 0x00); //[1]sck_always [0]BT656
	I2C_TxData(0x28, 0x03); //clock_div_spi
	I2C_TxData(0xfe , 0x00);

	//output
	I2C_TxData(0x22, 0xf8); //open awb
	I2C_TxData(0xf1, 0x03); //output enable
}
#endif


int check_main2_device(void)
{
	unsigned char reg_addr = 0;
	unsigned char switch_page_addr = 0;
	unsigned char switch_page_num = 0;
	int ret;
	int read_data = 0;

//	int read_data1 = 0;
	DCAM_LOG(" start\n");
#ifdef CONFIG_DCAM_CAM2_SP0A08	
	reg_addr = SP0A08MAIN2MIPI_SENSOR_ID_ADDR;
	switch_page_addr = SP0A08MAIN2MIPI_SENSOR_SWITCH_PAGE_ADDR;
	switch_page_num = SP0A08MAIN2MIPI_SENSOR_SWITCH_PAGE_NUM;
#endif
#ifdef CONFIG_DCAM_CAM2_GC0310	
	reg_addr = GC0310_MIPI_YUV_MAIN2_SENSOR_ID_ADDR;
	switch_page_addr = GC0310_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_ADDR;
	switch_page_num = GC0310_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_NUM;
#endif
#ifdef CONFIG_DCAM_CAM2_GC030A	
	reg_addr = GC030A_MIPI_YUV_MAIN2_SENSOR_ID_ADDR;
	switch_page_addr = GC030A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_ADDR;
	switch_page_num = GC030A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_NUM;
#endif
#ifdef CONFIG_DCAM_CAM2_GC032A  
        reg_addr = GC032A_MIPI_YUV_MAIN2_SENSOR_ID_ADDR;
        switch_page_addr = GC032A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_ADDR;
        switch_page_num = GC032A_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_NUM;
#endif

#ifdef CONFIG_DCAM_CAM2_GC6133  
        reg_addr = GC6133_MIPI_YUV_MAIN2_SENSOR_ID_ADDR;
        switch_page_addr = GC6133_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_ADDR;
        switch_page_num = GC6133_MIPI_YUV_MAIN2_SENSOR_SWITCH_PAGE_NUM;
#endif
#ifdef CONFIG_DCAM_CAM2_GC6133 
	//power on
//	pinctrl_select_state(dcam_pinctrl,pins_cam2_mclk1);	//mclk  high 
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd1);   //pdn high 
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_rst1);   //rst high 
	msleep(10);
	
    cam2_tpd->cam2_iovdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcamio_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_iovdd_reg, 1800000, 1800000);
    printk("dsy D_CAMERA  %s %d regulator_set_voltage cam2_iovdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator vcamio_main2 voltage: %d\n", ret);
    }

   ret = regulator_enable(cam2_tpd->cam2_iovdd_reg);     //iovdd  high 
   printk("dsy D_CAMERA  %s %d regulator_enable cam2_iovdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(0==regulator_is_enabled(cam2_tpd->cam2_iovdd_reg)){
        printk("D_CAMERA error::Failed to enable regulator :%d\n", ret);

    }
	msleep(10);
	
	cam2_tpd->cam2_avdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcama_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_avdd_reg, 2800000, 2800000);
    printk("dsy D_CAMERA  %s %d regulator_set_voltage cam2_avdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator cam2_avdd_reg voltage: %d\n", ret);
    }

   ret = regulator_enable(cam2_tpd->cam2_avdd_reg);     //avdd  high 
   printk("dsy D_CAMERA  %s %d regulator_enable cam2_avdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(0==regulator_is_enabled(cam2_tpd->cam2_avdd_reg)){
        printk("D_CAMERA error::Failed to enable cam2_avdd_reg regulator :%d\n", ret);

    }



	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd0);   //pdn low 
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_rst0);   //rst low 
	msleep(10);
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd1);   //pdn high 
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_rst1);   //rst high 
	msleep(10);
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd0);   //pdn low 
	
	pinctrl_select_state(dcam_pinctrl,pins_cam2_rst0);   //rst low 
	msleep(50);

//	pinctrl_select_state(dcam_pinctrl,pins_cam2_mclk1);	//mclk  high 
//	msleep(10)
#endif
	DCAM_LOG("H651 [main2] reg_addr = 0x%x\n",reg_addr);
	I2C_TxData(switch_page_addr,switch_page_num);
	read_data = I2C_RxData(reg_addr);
	DCAM_LOG("[main2] device id read_data = 0x%x\n",read_data);
	// read_data1 = I2C_RxData(0xf1);
	// DCAM_LOG("[main2] device id read_data1 = 0x%x\n",read_data1);
#ifdef CONFIG_DCAM_CAM2_SP0A08	
	if(read_data == SP0A08MAIN2MIPI_SENSOR_ID)	
	{
		DCAM_LOG("MAIN2 DEVICE IS SP0A08\n");
		sp0a08_sensor_init();
		main2_light = kthread_create(read_main2_light_worker_kthread,NULL, "main2 light");
		wake_up_process(main2_light);
		sp0a08_kthread_flag = 1;
	}
#endif	
#ifdef CONFIG_DCAM_CAM2_GC0310	
	if(read_data == GC0310_MIPI_YUV_MAIN2_SENSOR_ID)	
	{
		DCAM_LOG("MAIN2 DEVICE IS GC0310\n");
		gc0310_sensor_init();
		main2_light = kthread_create(read_main2_light_worker_kthread,NULL, "main2 light");
		wake_up_process(main2_light);
		gc0310_kthread_flag = 1;
	}
#endif	
#ifdef CONFIG_DCAM_CAM2_GC030A	
	if(read_data == GC030A_MIPI_YUV_MAIN2_SENSOR_ID)	
	{
		DCAM_LOG("MAIN2 DEVICE IS GC030a\n");
		gc0310_sensor_init();
		main2_light = kthread_create(read_main2_light_worker_kthread,NULL, "main2 light");
		wake_up_process(main2_light);
		gc030a_kthread_flag = 1;
	}
#endif
#ifdef CONFIG_DCAM_CAM2_GC032A	
	DCAM_LOG("MAIN2 DEVICE IS GC032A read_data = 0x%x GC032A_MIPI_YUV_MAIN2_SENSOR_ID = 0x%x \n",read_data,GC032A_MIPI_YUV_MAIN2_SENSOR_ID);
	if(read_data == GC032A_MIPI_YUV_MAIN2_SENSOR_ID)	
	{
		DCAM_LOG("MAIN2 DEVICE IS GC032A\n");
		//gc0312_sensor_init();
		gc032A_sensor_init();
		main2_light = kthread_create(read_main2_light_worker_kthread,NULL, "main2 light");
		wake_up_process(main2_light);
		gc032a_kthread_flag = 1;
	}
#endif

#ifdef CONFIG_DCAM_CAM2_GC6133	
	DCAM_LOG("MAIN2 DEVICE IS GC6133 read_data = 0x%x GC6133_MIPI_YUV_MAIN2_SENSOR_ID = 0x%x \n",read_data,GC6133_MIPI_YUV_MAIN2_SENSOR_ID);
	if(read_data == GC6133_MIPI_YUV_MAIN2_SENSOR_ID)	
	{
		DCAM_LOG("MAIN2 DEVICE IS GC6133\n");
		gc6133_sensor_init();
		main2_light = kthread_create(read_main2_light_worker_kthread,NULL, "main2 light");
		wake_up_process(main2_light);
		gc6133_kthread_flag = 1;
	}
#endif
	//read_main2_device_light();
	DCAM_LOG(" end\n");
	return 0;
}
EXPORT_SYMBOL(check_main2_device);

void disable_main2_light_kthread(void)
{
	int retval;
	int ret = 0;
#ifdef CONFIG_DCAM_CAM2_SP0A08	
	if(sp0a08_kthread_flag == 1)
	{
		kthread_stop(main2_light);
		sp0a08_kthread_flag = -1;
	}
#endif
#ifdef CONFIG_DCAM_CAM2_GC0310
	if(gc0310_kthread_flag == 1)
	{
		kthread_stop(main2_light);
		gc0310_kthread_flag = -1;
	}
#endif
#ifdef CONFIG_DCAM_CAM2_GC030A		
	if(gc030a_kthread_flag == 1)
	{
		kthread_stop(main2_light);
		gc030a_kthread_flag = -1;
	}
#endif
#ifdef CONFIG_DCAM_CAM2_GC6133		
	if(gc6133_kthread_flag == 1)
	{
		kthread_stop(main2_light);
		gc6133_kthread_flag = -1;
	}
#endif
#ifdef CONFIG_DCAM_CAM2_GC6133	
	//power off 

	//msleep(10);
//	pinctrl_select_state(dcam_pinctrl,pins_cam2_mclk1);	//mclk  high 
//	msleep(10);
	 pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd0); //pnd  low 
	 pinctrl_select_state(dcam_pinctrl,pins_cam2_rst0); //rst  low 
	 msleep(30);
	 pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd1); //pnd  high 
	
	 pinctrl_select_state(dcam_pinctrl,pins_cam2_rst1); //rst  high 
	 msleep(10);
 cam2_tpd->cam2_avdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcama_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_avdd_reg, 2800000, 2800000);
    printk("dsy D_CAMERA  %s %d regulator_set_voltage cam2_avdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator vcamio_main2 cam2_avdd_reg voltage: %d\n", ret);
    }


      retval = regulator_disable(cam2_tpd->cam2_avdd_reg); //disable regulator   //avdd   low  
     printk("dsy D_CAMERA  %s %d retval = %d\n", __func__,__LINE__,retval);
      if(retval){
             printk("D_CAMERA error::regulator_disable() cam2_avdd_reg failed in disable_main2_light_kthread!\n");
      }
	 //pinctrl_select_state(dcam_pinctrl,pins_cam2_a_vdd0);//avdd   low  

    cam2_tpd->cam2_iovdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcamio_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_iovdd_reg, 1800000, 1800000);
    printk("dsy D_CAMERA  %s %d regulator_set_voltage cam2_iovdd_reg ret = %d\n", __func__,__LINE__,ret);
    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator vcamio_main2 cam2_iovdd_reg voltage: %d\n", ret);
    }


      retval = regulator_disable(cam2_tpd->cam2_iovdd_reg); //disable regulator   //iovdd   low  
     printk("dsy D_CAMERA  %s %d retval = %d\n", __func__,__LINE__,retval);
      if(retval){
             printk("D_CAMERA error::regulator_disable() cam2_iovdd_reg failed in disable_main2_light_kthread!\n");
      }
      msleep(10);

//	pinctrl_select_state(dcam_pinctrl,pins_cam2_mclk0);	//mclk  low

	 pinctrl_select_state(dcam_pinctrl,pins_cam2_pnd0); //pnd  low 
	 pinctrl_select_state(dcam_pinctrl,pins_cam2_rst0); //rst  low 
	 msleep(20);
	 dcam_light_val = 0;
#endif


	
}
EXPORT_SYMBOL(disable_main2_light_kthread);

/***********dummy camera i2c driver start***********/



//sys/class/dcam_light_core/dcam_light/dcam_light_core_on_off
static ssize_t dcam_light_core_show(struct device *dev,struct device_attribute *attr,char *buf)
{
	pr_info("dsy  %s %d dcam_light_val = %d \n", __func__,__LINE__,dcam_light_val);
return sprintf(buf,"%d\n",dcam_light_val);
}
static ssize_t dcam_light_core_store(struct device *dev,struct device_attribute *attr,const char *buf,size_t size)
{
switch (buf[0])
{
	case '0':
		// dcam_light_status = 0;
		pr_info("dsy  %s %d \n", __func__,__LINE__);
		break;
	case '1':	
		// dcam_light_status = 1;
		pr_info("dsy %s %d \n", __func__,__LINE__);
		break;
	default:
		pr_info("dsy %s %d err paramater \n", __func__,__LINE__);
	break;
	}
	return size;
}
static struct device_attribute dcam_light_core_on_off_attr = {
	.attr = {
	.name = "dcam_light_core_on_off",
	.mode = S_IRUSR | S_IWUSR,
	},
	.show = dcam_light_core_show,
	.store = dcam_light_core_store,
};



static int dcam2_i2c_probe(struct i2c_client *client)
{
	int ret = 0;
	int err = 0;
//	int retval;
	DCAM_LOG("start\n");

//	client->addr = 0x21; // gc030a i2c addr  0x42 >> 1

#ifdef CONFIG_DCAM_CAM2_GC6133
	client->addr = GC6133_MIPI_YUV_MAIN2_I2C_ADDR; // gc030a i2c addr  0x42 >> 1 gc6133 >> 0x80
#endif
	
	new_client = client;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -ENODEV;
		goto exit_check_functionality_failed;
	}

	dummy_cam_input_dev = input_allocate_device();
	
    if(!dummy_cam_input_dev)
	{
        return -ENOMEM;
	}
	
	dummy_cam_input_dev->name = "dummy_cam2";
    dummy_cam_input_dev->id.bustype = BUS_HOST;
    dummy_cam_input_dev->id.vendor = 0x0001;
    dummy_cam_input_dev->id.product = 0x0001;
    dummy_cam_input_dev->id.version = 0x0001;
	
	__set_bit(EV_KEY, dummy_cam_input_dev->evbit);
	__set_bit(KEY_F11, dummy_cam_input_dev->keybit);
	__set_bit(KEY_F12, dummy_cam_input_dev->keybit);

	err = input_register_device(dummy_cam_input_dev); 
    if (err){
        printk("%s regist input device failed!!!\n",__func__);
        input_free_device(dummy_cam_input_dev);
        return err;
      }

//	err = check_main2_device();
//	if(err < 0)
//	{
//		DCAM_LOG("check main2 device fail\n");
//	}
	dcam_light_core_class = class_create("dcam_light_core");
 	if(IS_ERR(dcam_light_core_class))
	{
		pr_info("Failed to create class!\n");
	}
	dcam_light_core_dev = device_create(dcam_light_core_class, NULL, 0, NULL, "dcam_light");
	if (device_create_file(dcam_light_core_dev, &dcam_light_core_on_off_attr) < 0)
	{
		pr_info("Failed to create device file(%s)!\n", dcam_light_core_on_off_attr.attr.name);
	}


	DCAM_LOG("end\n");
	return 0;

	exit_check_functionality_failed:
	return ret;	
}

static void dcam2_i2c_remove(struct i2c_client *client)
{
	i2c_unregister_device(client);
	
	return ;
}

static const struct i2c_device_id dcam2_i2c_id[] = {
	{ DCAM_CAM2_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id dcam2_of_match[] = {
	{.compatible = "mediatek,camera_main_2"},
	{},
};
#endif

static struct i2c_driver dcam2_i2c_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = DCAM_CAM2_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = dcam2_of_match,
#endif
	},

	.probe          = dcam2_i2c_probe,
	.remove         = dcam2_i2c_remove,
	.id_table       = dcam2_i2c_id,
};

static int dummy_camera_remove(struct platform_device *pdev)
{
	DCAM_FUN();
	i2c_del_driver(&dcam2_i2c_driver);
	
	return 0;
}

static int dummy_camera_gpio_pinctrl_init(struct platform_device *pdev)
{
	int ret = 0;
	DCAM_LOG("dummy_camera_gpio_pinctrl_init start\n");
	if (pdev->dev.of_node == NULL) {
		DCAM_LOG("gpio_pinctrl get dcam node failed!\n");
		return -1;
	}	
	
	dcam_pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(dcam_pinctrl)) {
		DCAM_LOG("Cannot find dcam pinctrl!\n");
		return -1;
	}
	
	pins_default = pinctrl_lookup_state(dcam_pinctrl, "default");
    if (IS_ERR(pins_default)) {
        ret = PTR_ERR(pins_default);
        DCAM_LOG("pinctrl lookup default pinctrl fail\n");
    }

	pins_cam2_rst1 = pinctrl_lookup_state(dcam_pinctrl, "cam2_rst1");
    if (IS_ERR(pins_cam2_rst1)) {
        ret = PTR_ERR(pins_cam2_rst1);
        DCAM_LOG("pinctrl lookup cam2_rst1 pinctrl fail\n");
        return ret;
    }
	pins_cam2_rst0 = pinctrl_lookup_state(dcam_pinctrl, "cam2_rst0");
    if (IS_ERR(pins_cam2_rst0)) {
        ret = PTR_ERR(pins_cam2_rst0);
        DCAM_LOG("pinctrl lookup cam2_rst0 pinctrl fail\n");
        return ret;
    }
	pins_cam2_pnd1 = pinctrl_lookup_state(dcam_pinctrl, "cam2_pnd1");
    if (IS_ERR(pins_cam2_pnd1)) {
        ret = PTR_ERR(pins_cam2_pnd1);
        DCAM_LOG("pinctrl lookup cam2_pnd1 pinctrl fail\n");
        return ret;
    }
	pins_cam2_pnd0 = pinctrl_lookup_state(dcam_pinctrl, "cam2_pnd0");
    if (IS_ERR(pins_cam2_pnd0)) {
        ret = PTR_ERR(pins_cam2_pnd0);
        DCAM_LOG("pinctrl lookup cam2_pnd0 pinctrl fail\n");
        return ret;
    }
/*	
	pins_cam2_d_vdd1 = pinctrl_lookup_state(dcam_pinctrl, "cam2_d_vdd1");
    if (IS_ERR(pins_cam2_d_vdd1)) {
        ret = PTR_ERR(pins_cam2_d_vdd1);
        printk("pinctrl lookup cam2_d_vdd1 pinctrl fail\n");
        return ret;
    }
	pins_cam2_d_vdd0 = pinctrl_lookup_state(dcam_pinctrl, "cam2_d_vdd0");
    if (IS_ERR(pins_cam2_d_vdd0)) {
        ret = PTR_ERR(pins_cam2_d_vdd0);
        printk("pinctrl lookup cam2_d_vdd0 pinctrl fail\n");
        return ret;
    }
	pins_cam2_a_vdd1 = pinctrl_lookup_state(dcam_pinctrl, "cam2_a_vdd1");
    if (IS_ERR(pins_cam2_a_vdd1)) {
        ret = PTR_ERR(pins_cam2_a_vdd1);
        printk("pinctrl lookup cam2_a_vdd1 pinctrl fail\n");
        return ret;
    }
	pins_cam2_a_vdd0 = pinctrl_lookup_state(dcam_pinctrl, "cam2_a_vdd0");
    if (IS_ERR(pins_cam2_a_vdd0)) {
        ret = PTR_ERR(pins_cam2_a_vdd0);
        printk("pinctrl lookup cam2_a_vdd0 pinctrl fail\n");
        return ret;
    }
*/
	pins_cam2_mclk1 = pinctrl_lookup_state(dcam_pinctrl, "dummy_cam2_mclk_on");
   if (IS_ERR(pins_cam2_mclk1)) {
       ret = PTR_ERR(pins_cam2_mclk1);
       DCAM_LOG("pinctrl lookup pins_cam2_mclk1 pinctrl fail\n");
       return ret;
   }
	pins_cam2_mclk0 = pinctrl_lookup_state(dcam_pinctrl, "dummy_cam2_mclk_off");
   if (IS_ERR(pins_cam2_mclk0)) {
       ret = PTR_ERR(pins_cam2_mclk0);
       DCAM_LOG("pinctrl lookup pins_cam2_mclk0 pinctrl fail\n");
       return ret;
   }



	DCAM_LOG("dummy_camera_gpio_pinctrl_init end\n");
	return ret;
}

static int dummy_camera_probe(struct platform_device *pdev)
{
	int ret;

	DCAM_LOG("start\n");
	
	
	dummy_camera_gpio_pinctrl_init(pdev);//init dummy camera gpio
	
	cam2_tpd = kmalloc(sizeof(struct cam2_device), GFP_KERNEL);
	if (cam2_tpd == NULL)
		return -ENOMEM;
	memset(cam2_tpd, 0, sizeof(struct cam2_device));

	cam2_tpd->cam2_dev = &pdev->dev;
/*
    cam2_tpd->cam2_iovdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcamio_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_iovdd_reg, 1800000, 1800000);

    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator vcamio_main2 voltage: %d\n", ret);
    }

    cam2_tpd->cam2_avdd_reg = regulator_get(cam2_tpd->cam2_dev, "vcama_main2");
    ret = regulator_set_voltage(cam2_tpd->cam2_avdd_reg, 2800000, 2800000);

    if(ret != 0){
        printk("D_CAMERA error::Failed to check_main2_deviceset regulator vcama_main2 voltage: %d\n", ret);
    }
*/
	ret = i2c_add_driver(&dcam2_i2c_driver);
	if (ret != 0) {
		DCAM_ERR("[%s] failed to register dcam2 i2c driver.\n", __func__);
		return ret;
	}

	DCAM_LOG("end\n");

	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id dummy_camera_of_match[] = {
	{.compatible = "mediatek,dummy_camera",},
	{},
};
#endif

static struct platform_driver dummy_camera_driver = {
	.probe	 = dummy_camera_probe,
	.remove	 = dummy_camera_remove,
	.driver = {
		.name  = "d_camera",
	#ifdef CONFIG_OF
		.of_match_table = dummy_camera_of_match,
	#endif
	}
};

static int __init dummy_camera_init(void) {
	int ret;
	
	DCAM_FUN();

	ret = platform_driver_register(&dummy_camera_driver);
	if (ret) {
		DCAM_LOG("Unable to register driver (%d)\n", ret);
		return ret;
	}		
	
	return 0;
}

static void __exit dummy_camera_exit(void) {
	DCAM_FUN();
	platform_driver_unregister(&dummy_camera_driver);
}

//module_init(dummy_camera_init);
late_initcall(dummy_camera_init);
module_exit(dummy_camera_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dcam driver");
MODULE_AUTHOR("main2");

