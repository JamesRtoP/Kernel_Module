#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include "kmlab_given.h"

#include <linux/string.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Richards-Perhatch"); // Change with your lastname
MODULE_DESCRIPTION("CPTS360 KM PA");

#define DEBUG 1

// store per process data (you may modify it as needed)
typedef struct {
    struct list_head list;
    unsigned int pid;
    unsigned long cpu_time;
} proc_list;

static struct proc_dir_entry* kmlab_dir = NULL;
static struct proc_dir_entry* status_entry = NULL;

static struct list_head kll = LIST_HEAD_INIT(kll);

static DEFINE_SPINLOCK(sl); 
static unsigned long sl_flags;


void add_node(int pid)
{
   proc_list * new_node = kmalloc(sizeof(proc_list),GFP_KERNEL);
   if (!new_node) 
   {
      printk(KERN_INFO
      "Memory allocation failed\n");
   }
   new_node->pid = pid;
   new_node->cpu_time = 0;
   spin_lock_irqsave(&sl, sl_flags); 
   list_add_tail(&(new_node->list), &kll);
   spin_unlock_irqrestore(&sl, sl_flags);
}

void empty(void)
{
   proc_list *operate_on_me;
   proc_list *next;
   list_for_each_entry_safe(operate_on_me, next, &kll, list) 
   {
      list_del(&operate_on_me->list);
      kfree(operate_on_me);
   }
}

static ssize_t when_user_writes_to_status(struct file *file, const char __user *buffer, size_t count, loff_t *pos)
{
   long int pid = 0;
   char buf[100];
   ssize_t failure_flag = copy_from_user(buf, buffer,count);
   if(failure_flag != 0)
   {
      pr_info("COPY FROM USER ERROR");
   }
   buf[count] = '\0';

   failure_flag = kstrtol(buf, 10, &pid);
   add_node(pid); 
   pr_info("%ld",pid);
   return 14;
}

void create_process_string(char* string, proc_list *e)
{
   char pid_str[100] = "";
   char cpu_time_str[100] = "";
   snprintf(pid_str, sizeof(pid_str), "%d", e->pid);
   snprintf(cpu_time_str, sizeof(cpu_time_str), "%ld", e->cpu_time);
   strcat(string,pid_str);
   strcat(string,":");
   strcat(string,cpu_time_str);
   strcat(string,"\n");
}

static ssize_t when_user_reads_status(struct file *file_pointer, char __user *buffer,
 size_t buffer_length, loff_t *offset)
{ 
   char string[200] = "";
   int string_size = 0;
   proc_list *operate_on_me;
   struct list_head *cur;

   spin_lock_irqsave(&sl, sl_flags); 
   list_for_each(cur, &kll)
   {
      operate_on_me = list_entry(cur, proc_list, list);
      create_process_string(string,operate_on_me);
      
      string_size = strlen(string);
   }
   spin_unlock_irqrestore(&sl, sl_flags);

   if (*offset >= string_size) 
   {
      return 0;
   }
   if (copy_to_user(buffer, string, string_size) != 0)
   {
      pr_info("copy_to_user failed\n");
      return 0;
   }
   *offset += string_size;
   return string_size;
}

static const struct proc_ops proc_fops = {
   .proc_read = when_user_reads_status,
   .proc_write = when_user_writes_to_status,
};



static struct workqueue_struct *work_queue = NULL;
static struct work_struct work;

static void work_function(struct work_struct *data) 
{
   proc_list *cur;
   proc_list *next;
   long unsigned int new_cpu_time = -1;
   int ended; 

   spin_lock_irqsave(&sl, sl_flags); 
   list_for_each_entry_safe(cur, next, &kll, list) 
   {
      ended = get_cpu_use(cur->pid, &new_cpu_time);
      if(ended == 0)
      {
         cur->cpu_time = new_cpu_time;
      }
      else
      {
         list_del(&cur->list);
         kfree(cur);
      }
   }
   spin_unlock_irqrestore(&sl, sl_flags);
}


void init_workqueue(void)
{
   work_queue = alloc_workqueue("timer_queue",WQ_UNBOUND,1);
}
void schedule_timer_work(void)
{
   INIT_WORK(&work, work_function);
   schedule_work(&work);
}


static struct timer_list my_timer;
const int time_interval = 5000;

void timer_callback(struct timer_list *timer) 
{
 schedule_timer_work();
 mod_timer(&my_timer, jiffies + msecs_to_jiffies(time_interval));
}






// kmlab_init - Called when the module is loaded
int __init kmlab_init(void)
{

   kmlab_dir = proc_mkdir("kmlab",NULL);
   status_entry = proc_create("status",0666, kmlab_dir,&proc_fops);
   init_workqueue();
   timer_setup(&my_timer, timer_callback, 0);
   mod_timer(&my_timer, jiffies + msecs_to_jiffies(time_interval));
   pr_info("KMLAB MODULE LOADED\n");
   return 0;   
}

// kmlab_exit - Called when the module is unloaded
void __exit kmlab_exit(void)
{
   del_timer(&my_timer);
   destroy_workqueue(work_queue);
   remove_proc_entry("status", kmlab_dir);
   remove_proc_entry("kmlab", NULL);
   empty();
   pr_info("KMLAB MODULE UNLOADED\n");
}

// Register init and exit functions
module_init(kmlab_init);
module_exit(kmlab_exit);
