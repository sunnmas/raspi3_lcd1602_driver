#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/delay.h> 
#include <linux/miscdevice.h>
#include <linux/uaccess.h>


MODULE_LICENSE("GPL");

struct GpioRegisters
{
    uint32_t GPFSEL[6];
    uint32_t Reserved1;
    uint32_t GPSET[2];
    uint32_t Reserved2;
    uint32_t GPCLR[2];
};

struct GpioRegisters *s_pGpioRegisters;


// Чтобы понять, читаем BCM2835 ARM Peripherals раздел 6 General Purpose I/O (GPIO)
static void SetGPIOFunction(int GPIO, int functionCode)
{
    int registerIndex = GPIO / 10;
    int bit = (GPIO % 10) * 3;

    uint32_t oldValue = s_pGpioRegisters->GPFSEL[registerIndex];
    uint32_t mask = 0b111 << bit;
    // printk(KERN_INFO "register value:\n");
    s_pGpioRegisters->GPFSEL[registerIndex] = (oldValue & ~mask) | ((functionCode << bit) & mask);
}

static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
    if (outputValue)
        s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
    else
        s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
}

// static struct timer_list s_BlinkTimer;
// static int s_BlinkPeriod = 1000;
// static const int LedGpioPin = 18;

// static void BlinkTimerHandler(struct timer_list *unused)
// {
//     static bool on = false;
//     on = !on;
//     SetGPIOOutputValue(LedGpioPin, on);
//     mod_timer(&s_BlinkTimer, jiffies + msecs_to_jiffies(s_BlinkPeriod));
// }

// static ssize_t set_period_callback(struct device* dev,
//     struct device_attribute* attr,
//     const char* buf,
//     size_t count)
// {
//     long period_value = 0;
//     if (kstrtol(buf, 10, &period_value) < 0)
//         return -EINVAL;
//     if (period_value < 10)	//Safety check
//     	return - EINVAL;

//     s_BlinkPeriod = period_value;
//     return count;
// }

// static DEVICE_ATTR(period, S_IRWXU | S_IRWXG, NULL, set_period_callback);

static struct class *s_pDeviceClass;
static struct device *s_pDeviceObject;

#define BCM2708_PERI_BASE   0x3f000000
#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000)
int OUTPUT = 1;
int INPUT = 0;
int PIN_RS = 4;
int PIN_E = 17;
int PIN_D4 = 18;
int PIN_D5 = 27;
int PIN_D6 = 22;
int PIN_D7 = 23;
int LCD_DELAY_MS = 5;

static inline int delayMicroseconds(int value)
{
  if (value > 1000) msleep(value/1000);
  udelay(value%1000);
  return 0;
}

void LCDSend(char isCommand, unsigned char data) {
    SetGPIOOutputValue(PIN_RS, isCommand ? 0 : 1);
    delayMicroseconds(LCD_DELAY_MS*1000);

    SetGPIOOutputValue(PIN_D7, (data >> 7) & 1);
    SetGPIOOutputValue(PIN_D6, (data >> 6) & 1);
    SetGPIOOutputValue(PIN_D5, (data >> 5) & 1);
    SetGPIOOutputValue(PIN_D4, (data >> 4) & 1);

    // Wnen writing to the display, data is transfered only
    // on the high to low transition of the E signal.
    SetGPIOOutputValue(PIN_E, 1);
    delayMicroseconds(LCD_DELAY_MS*1000);
    SetGPIOOutputValue(PIN_E, 0);

    SetGPIOOutputValue(PIN_D7, (data >> 3) & 1);
    SetGPIOOutputValue(PIN_D6, (data >> 2) & 1);
    SetGPIOOutputValue(PIN_D5, (data >> 1) & 1);
    SetGPIOOutputValue(PIN_D4, (data >> 0) & 1);

    SetGPIOOutputValue(PIN_E, 1);
    delayMicroseconds(LCD_DELAY_MS*1000);
    SetGPIOOutputValue(PIN_E, 0);
}

void LCDCommand(unsigned char cmd) {
    LCDSend(1, cmd);
}

void LCDChar(const char chr) {
    LCDSend(0, (unsigned char)chr);
}

void LCDString(const char* str) {
    while(*str != '\0') {
        LCDChar(*str);
        str++;
    }
}



// static void __exit LCD1602Module_exit(void)
// {
//     SetGPIOFunction(PIN_RS, INPUT);
//     SetGPIOFunction(PIN_E,  INPUT);
//     SetGPIOFunction(PIN_D4, INPUT);
//     SetGPIOFunction(PIN_D5, INPUT);
//     SetGPIOFunction(PIN_D6, INPUT);
//     SetGPIOFunction(PIN_D7, INPUT);
//     iounmap(s_pGpioRegisters);
//     // device_remove_file(s_pDeviceObject, &dev_attr_period);
//     device_destroy(s_pDeviceClass, 0);
//     class_destroy(s_pDeviceClass);

//     // del_timer(&s_BlinkTimer);
// }

// module_init(LCD1602Module_init);
// module_exit(LCD1602Module_exit);


static int minor = 0;
module_param( minor, int, S_IRUGO );

static char *info_str = "lcd1602 device driver\nAutor Misyagin Anton masjanin@yandex.ru\n";         // buffer!

static ssize_t dev_read( struct file * file, char * buf,
            size_t count, loff_t *ppos ) {
  int len = strlen( info_str );
  if( count < len ) return -EINVAL;
  if( *ppos != 0 ) {
    return 0;
  }
  if( copy_to_user( buf, info_str, len ) ) return -EINVAL;
  *ppos = len;
  return len;
}

static ssize_t dev_write( struct file *file, const char *buf, size_t count, loff_t *ppos ) {
// clear display (optional here)
  LCDCommand(0b00000001);

  LCDCommand(0b10000000); // set address to 0x00
  LCDString(buf);
  return count;
}

static const struct file_operations lcd1602_fops = {
  .owner  = THIS_MODULE,
  .read   = dev_read,
  .write  = dev_write,
};

static struct miscdevice lcd1602_dev = {
  MISC_DYNAMIC_MINOR, "lcd1602", &lcd1602_fops
};


static int __init dev_init( void ) {
  int ret;
  if( minor != 0 ) lcd1602_dev.minor = minor;
  ret = misc_register( &lcd1602_dev );
  if( ret ) printk( KERN_ERR "=== Unable to register misc device\n");
  
 
  s_pGpioRegisters = (struct GpioRegisters *)ioremap(GPIO_BASE, sizeof(struct GpioRegisters));
  SetGPIOFunction(PIN_RS, OUTPUT);
  SetGPIOFunction(PIN_E,  OUTPUT);
  SetGPIOFunction(PIN_D4, OUTPUT);
  SetGPIOFunction(PIN_D5, OUTPUT);
  SetGPIOFunction(PIN_D6, OUTPUT);
  SetGPIOFunction(PIN_D7, OUTPUT);

  // 4-bit mode, 2 lines, 5x7 format
  LCDCommand(0b00110000);
  // display & cursor home (keep this!)
  LCDCommand(0b00000010);
  // display on, right shift, underline off, blink off
  LCDCommand(0b00001100);
  // clear display (optional here)
  LCDCommand(0b00000001);

  LCDCommand(0b10000000); // set address to 0x00
  LCDString("Using HD44780");
  LCDCommand(0b11000000); // set address to 0x40
  LCDString("as kernel module =)");
  printk(KERN_INFO "LCD1602 module init\n");

  return ret;
}

static void __exit dev_exit( void ) {
  // unmap_peripheral(&gpio);
  misc_deregister( &lcd1602_dev );
}
 
static int __init dev_init( void );
module_init( dev_init );

static void __exit dev_exit( void );
module_exit( dev_exit );

MODULE_LICENSE("GPL");
MODULE_AUTHOR( "Misyagin Anton <masjanin@yandex.ru>" );
MODULE_VERSION( "0.1" );