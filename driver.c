/* Crea un dispositivo para leer/escribir de gpio */

#include <linux/init.h>
#include <linux/kernel.h>		/* printk() */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>		/* error codes */
#include <linux/types.h>		/* size_t */
#include <linux/stat.h> 
#include <linux/slab.h>			/* kmalloc() */
#include <linux/proc_fs.h>
#include <linux/string.h>		/* for strlen*/
#include <linux/mman.h>			/* mmap() */
#include <linux/fcntl.h>		/* O_ACCMODE */
#include <linux/ioport.h>		/* check_mem_region() */
#include <asm/uaccess.h>		/* copy_from/to_user */
#include <linux/delay.h>
#include <linux/mutex.h>        /* Required for the mutex functionality */
#include <linux/gpio.h>
#include <asm/io.h>
#include <linux/interrupt.h>    /* Required for the IRQ code */
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <linux/buffer_head.h>

#define LICENSE				"GPL3"
#define DRIVER_AUTHOR		"Roberto de la Cruz, Juan Samper"
#define DRIVER_DESC			"Allows comunication between a RBPi and an FPGA SPARTAN 6 using de GPIO RBPi pins"
#define SUPORTED_DEVICE		"Tested on a Raspberry PI model A"

static DEFINE_MUTEX(gpio_mutex);  /// A macro that is used to declare a new mutex that is visible in this file
                                  /// results in a semaphore variable ebbchar_mutex with value 1 (unlocked)
                                  /// DEFINE_MUTEX_LOCKED() results in a variable with value 0 (locked)

MODULE_LICENSE("Dual BSD/GPL");

#define BLOCK_SIZE              4096


volatile unsigned *gpio;

/* GPIO macros  */

#define INP_GPIO_04(b)   *(volatile unsigned int *)(0x00000000+b) &= (unsigned int)0xFFFF8FFF /* &= Binary AND assignment*/
#define OUT_GPIO_04(b)   *(volatile unsigned int *)(0x00000000+b) |= (unsigned int)0x00001000 /* |= Binary OR  assignment*/

#define GPIO_SET_04(b)  *(volatile unsigned int *)(0x0000001C+b) |= 0x10
#define GPIO_CLR_04(b)  *(volatile unsigned int *)(0x00000028+b) |= 0x10

#define GPIO_BASE 0xf2200000 /*This is the kernel memory adress for the PIN _04*/




/* Declaration of gpio functions */
int gpio_open(struct inode *inode, struct file *filp);
int gpio_release(struct inode *inode, struct file *filp);
ssize_t gpio_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t gpio_write(struct file *filp, char *buf, size_t count, loff_t *f_pos);
void gpio_exit(void);
int gpio_init(void

/* Structure that declares the usual file */
/* access functions */
struct file_operations gpio_fops = {
  .read= gpio_read,
  .write= gpio_write,
  .open= gpio_open,
  .release= gpio_release
};

/* Declaration of the init and exit functions */
module_init(gpio_init);
module_exit(gpio_exit);

/* Global variables of the driver */
/* Major number */
int gpio_major = 60;

/* I/O access */
volatile unsigned long gpio_addr;
volatile unsigned *gpio_ptr;
unsigned long gpio_size=512;


int mem_fd;
void *gpio_map;

unsigned int clk = 7, sensor_temperatura = 4 , dir = 10 , data_in = 22, data_out= 17;

/*
 * This function is called when the module is loaded
 */
int gpio_init() {
        int result;

        /* register char_major */
        result = register_chrdev(gpio_major, "gpio", &gpio_fops);
        if (result < 0) {
                printk(KERN_WARNING "Registering char device failed with %d\n", result);
                return result;
        }

        OUT_GPIO_04(GPIO_BASE);

        gpio_request(clk ,"clk");
        gpio_request(data_in, "data_in");
        gpio_request(data_out, "data_out");
        gpio_request(dir, "dir");

        gpio_direction_input(data_in);
        gpio_direction_output(clk,0);
        gpio_direction_output(data_out,0);
        gpio_direction_output(dir,0);

        gpio_addr = (volatile unsigned long)GPIO_BASE;
        gpio_ptr = (volatile unsigned *)gpio_addr;
        printk(KERN_WARNING "gpio_addr = %lu\n", gpio_addr);

        printk(KERN_WARNING "Result is  %i\n", result);
        if(!request_mem_region(gpio_addr, gpio_size, "gpio")){
                printk(KERN_WARNING "Cannot allocate GPIO memory:%i\n", result);
                unregister_chrdev(gpio_major, "gpio");
                return result;
        }

        printk(KERN_INFO "Allocating GPIO memory at 0x%lu\n", gpio_addr);
        mutex_init(&gpio_mutex);
        return 0;
}

/*
 * This function is called when the module is unloaded
 */
void gpio_exit() {
        mutex_destroy(&gpio_mutex);
        printk(KERN_INFO "Exit gpio module\n");
        unregister_chrdev(gpio_major, "gpio");
        release_mem_region(gpio_addr, gpio_size);
        gpio_ptr = (volatile unsigned *)NULL;
}

/* 
 * Called when a process tries to open the device file
 */
int gpio_open(struct inode *inode, struct file *filp) {
        if(!mutex_trylock(&gpio_mutex)){
                printk(KERN_ALERT "Device in use by another process");
        }
        return 0;
}

/* 
 * Called when a process closes the device file.
 */
int gpio_release(struct inode *inode, struct file *filp) {
        mutex_unlock(&gpio_mutex);
        printk(KERN_INFO "Releasing gpio module\n");
        return 0;
}

/*
 * Devuelve base^pot
 */
int pow(int base, int pot){
        int num = 1, i;
        for(i=0; i < pot; i++){
                num = num*base;
        }
        return num;
}

/*
 * Recibe un array de enteros (representacion binaria del numero)
 * y devuelve el numero.
 */
int receiveBit(int buffer[]){
        int i, num = 0;

        for(i=0; i < 8; i++){
                num += pow(2,(7-i))*buffer[i];
        }
        //printk(KERN_INFO "RESULT READ is : %i \n", num);
        return num;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it, like "cat /dev/gpio"
 */
ssize_t gpio_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
        int i;
        int buffer[8];

        gpio_set_value(dir,0);

        msleep(500);
        //ESPERAMOS UN CICLO DE RELOJ PARA QUE EL LOAD TENGA EFECTO
        gpio_set_value(clk,1);
        gpio_set_value(clk,0);
        //msleep(500);

        for (i = 0; i < 8; i++){
                gpio_set_value(clk,1);
                gpio_set_value(clk,0);
                buffer[i] = gpio_get_value(data_in);
                printk(KERN_INFO "VALOR GET GPIO %i \n", gpio_get_value(data_in));
                //msleep(500);
        }

        int num = receiveBit(buffer);
        if(buffer[0] == 1){
                num -= 256;
        }
        sprintf(buf, "%d", num);
        return 0;
}

/*
 * Recibe 2 operandos. Va haciendo el desplazamiento de cada uno de ellos,
 * fijando el valor del gpio mientras genera un ciclo de reloj.
 */
void sendBit(int op1, int op2){
        int i;

        gpio_set_value(clk,0);
        gpio_set_value(dir,1);

        for(i = 0; i < 16; i++){
                if(i < 8){
                        gpio_set_value(data_out, op2%2);
                        op2 /= 2;
                } else {
                        gpio_set_value(data_out, op1%2);
                        op1 /= 2;
                }
                //GENERAMOS EL FLANCO DE SUBIDA
                gpio_set_value(clk, 1);
                gpio_set_value(clk, 0);
                //msleep(500);
        }
}

/*  
 * Called when a process writes to dev file: echo "hi" > /dev/gpio 
*/
ssize_t gpio_write(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
        int op1, op2;
        op1 = (buf[0] - '0')*10 + (buf[1]-'0');
        op2 = (buf[2] - '0')*10 + (buf[3]-'0');
        printk(KERN_INFO "OP1 y OP2, %i %i", op1, op2);
        sendBit(op1, op2);
        return count;
}
