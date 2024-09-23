#include <linux/fs.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include<linux/moduleparam.h>

/*Define address of GPIO*/
#define GPIO_ADDR_BASE  0x44E07000 // ADDR of GPIO chapter2 memorymap
#define ADDR_SIZE       (0x1000) //size of GPIO reg(4KB)
#define GPIO_SETDATAOUT_OFFSET          0x194 // Offset to ADDr of GPIO set data out chap25
#define GPIO_CLEARDATAOUT_OFFSET        0x190//Offset to  ADDr of GPIO clear data out chap25
#define GPIO_OE_OFFSET                  0x134
#define LED                             ~(1 << 20)
#define DATA_OUT			(1 << 20)

void __iomem *base_addr;

int freq;
module_param(freq, int, S_IRUSR|S_IWUSR);

static int status;
static bool first;
struct timer_list my_timer;

static void timer_function(struct timer_list *t)
{
    static int count = 0;

    pr_info("delay time is %d\n", freq);

    // Blink led
    if ((count % freq) == 0)
    {
        writel_relaxed(DATA_OUT,  base_addr + GPIO_SETDATAOUT_OFFSET); //turn on led
        pr_info(" turn on led count =  %d\n", count);
    }
    else
    {
        writel_relaxed(DATA_OUT, base_addr + GPIO_CLEARDATAOUT_OFFSET); // turn off led
        pr_info(" turn off led count =  %d\n", count);
        }
    count++;
    mod_timer(t, jiffies + HZ);
    if(count == 5)
    {
        del_timer_sync(&my_timer);
    }
}

int gpio_open(struct inode *node, struct file *filep)
{
    pr_info("gpio driver open \n");
    first = true;
    return 0;
}

int gpio_release(struct inode *node, struct file *filep)
{
    pr_info("%s, %d\n", __func__, __LINE__);
    first = false;
    return 0;
}

//Doc data tu storage va tra ve cho application
static ssize_t gpio_read(struct file *filp, char __user *buf, size_t count,
                         loff_t *f_pos)
{
    int ret = 0;

    if (first == false) {
        pr_info("\r\nNo more storage to read\n");
        return 0;
    }
    pr_info("gpio_read run \n");

    switch (status)
    {
    case 2:
        ret = copy_to_user(buf, "2", 1);
        break;
    case 1:
        ret = copy_to_user(buf, "1", 1);
        break;
    case 0:
        ret = copy_to_user(buf, "0", 1);
    
        break;
    }
    first = false;

    return 1;
}

//Ham write: ghi du lieu vao vung storage
static ssize_t gpio_write(struct file *filp, const char __user *buf,
                          size_t count, loff_t *f_pos)
{
    int ret = 0;
    char internal[10];
    pr_info("gpio_write run\n");
      if (first == false) {
        pr_info("No more storage to write\n");
        return 0;
    }

    memset(internal, 0, sizeof(internal));
    ret = copy_from_user(internal, buf, 1);

    switch (internal[0])
    {
    case '2':
        status = 2;
        //Khoi tao timer
        my_timer.expires = jiffies + freq * HZ;
        timer_setup(&my_timer, timer_function, 0);
        add_timer(&my_timer);
        pr_info("blink led \r\n");
        break;
    case '1':
        writel_relaxed(DATA_OUT,  base_addr + GPIO_SETDATAOUT_OFFSET);
        status = 1;
        del_timer_sync(&my_timer);
        pr_info("Turn on led \r\n");
        break;
    case '0':
        writel_relaxed(DATA_OUT, base_addr + GPIO_CLEARDATAOUT_OFFSET);
        status = 0;
        del_timer_sync(&my_timer);
        pr_info("Turn off led \r\n");
        break;
    default:
        pr_info("Pls enter 0,1 or 2\n");
        break;
    }
    first = false;
    return count;
}

static long gpio_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{

	return 0;
}

struct file_operations gpio_fops = {
    .owner = THIS_MODULE,
    .open = gpio_open,
    .release = gpio_release,
    .read = gpio_read,
    .write = gpio_write,
    .unlocked_ioctl = gpio_ioctl,
};

static struct miscdevice gpio_example = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "gpio_driver",
    .fops = &gpio_fops,
};

static int gpio_init(void)
{
    uint32_t reg_data = 0;
    pr_info("GPIO driver init\n");
    base_addr = ioremap(GPIO_ADDR_BASE, ADDR_SIZE);
    pr_info("Config GPIO20 is output\n");
    /*config GPIO mode output*/

    reg_data = readl_relaxed(base_addr + GPIO_OE_OFFSET);
	reg_data &= LED;
	writel_relaxed(reg_data, base_addr + GPIO_OE_OFFSET);

    if (misc_register(&gpio_example) == 0) // register device file
    {
        pr_info("registered device file success\n");
    }

	writel_relaxed(DATA_OUT, base_addr + GPIO_CLEARDATAOUT_OFFSET); //turn off led when init
    status = 0;

    return 0;
}

static void gpio_exit(void)
{
    pr_info("gpio module exit\n");
    misc_deregister(&gpio_example);
}

module_init(gpio_init);
module_exit(gpio_exit);

MODULE_AUTHOR("Ta Minh Quan");
MODULE_DESCRIPTION("Example gpio driver.");
MODULE_LICENSE("GPL");
