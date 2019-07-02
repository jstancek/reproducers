#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/node.h>
#include <linux/compaction.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/page-flags.h>
#include <linux/sched.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jstancek");
MODULE_DESCRIPTION("buddy/slab allocate/free stress test, BZ 1723689");

/* 
 * There are two worker types:
 *   1. kmalloc / kfree
 *   2. get_pages / free_pages 
 * Spawn one worker (either 1 or 2) on each cpu and see if system
 * can survive.
 */

static int do_write = 0;
static long nr_pages;
static long testtime = 15;
static long testmemory = 1L*1024*1024*1024;

module_param(testtime, long, 0000);
MODULE_PARM_DESC(testtime, "test time in seconds");

static void __slab_worker(struct work_struct *work)
{
	long i;
	int **addr;
	int randint = get_random_int();
	long objects_per_page = 16 << (get_random_int() % 5);
	long object_size = PAGE_SIZE / objects_per_page;
	long objects = objects_per_page * nr_pages;

	if (do_write && object_size < sizeof(int))
		object_size = sizeof(int);

	addr = vzalloc(objects * sizeof(int *));
	if (!addr) {
		printk(KERN_ERR "buddyslab: vmalloc addr ENOMEM\n");
		return;
	}

	for (i = 0; i < objects; i++) {
		addr[i] = kmalloc(object_size, GFP_KERNEL);
		if (do_write && addr[i])
			memcpy(addr[i], &randint, sizeof(int));
	}
	for (i = 0; i < objects; i++) {
		if (!addr[i])
			continue;

		if (do_write && memcmp(addr[i], &randint,  sizeof(int))) {
			printk(KERN_ERR "buddyslab: corruption detected\n");
			printk(KERN_ERR "  page content expected: %d, found: %x\n", randint, addr[i][0]);
			dump_page(virt_to_page(addr[i]), NULL);	
		}
		kfree(addr[i]);
	}

	vfree(addr);
}

static void __buddy_worker(struct work_struct *work)
{
	long i;
	struct page **pages;
	int randint = get_random_int();
	int order = 0;

	pages = vzalloc(nr_pages * sizeof(struct page *));
	if (!pages) {
		printk(KERN_ERR "buddyslab: vmalloc pages ENOMEM\n");
		return;
	}

	for (i = 0; i < nr_pages; i++) {
		pages[i] = alloc_pages(GFP_KERNEL, order);
		if (do_write && pages[i]) {
			unsigned long *addr = page_address(pages[i]);
			memcpy(addr, &randint, sizeof(int));
		}
	}
	for (i = 0; i < nr_pages; i++) {
		int *addr;

		if (!pages[i])
			continue;

		addr = page_address(pages[i]);
		if (do_write && memcmp(addr, &randint, sizeof(int))) {
			printk(KERN_ERR "buddyslab: corruption detected\n");
			printk(KERN_ERR "  page content expected: %d, found: %x\n", randint, *addr);
			dump_page(pages[i], NULL);	
		}
		__free_pages(pages[i], order);
	}

	vfree(pages);
}

static void slab_worker(struct work_struct *work)
{
	struct timespec start, now;

	getnstimeofday(&start);
	while (1) {	
		__slab_worker(work);
		cond_resched();
		getnstimeofday(&now);
		if (now.tv_sec - start.tv_sec > testtime)
			break;
	}
	printk("slab_worker %d done\n", smp_processor_id());
}

static void buddy_worker(struct work_struct *work)
{
	struct timespec start, now;

	getnstimeofday(&start);
	while (1) {	
		__buddy_worker(work);
		cond_resched();
		getnstimeofday(&now);
		if (now.tv_sec - start.tv_sec > testtime)
			break;
	}
	printk("buddy_worker %d done\n", smp_processor_id());
}

static DEFINE_PER_CPU(struct work_struct, works);

static int teststart_proc_open(struct inode *inode, struct file *file)
{
	long cpu;

	nr_pages = testmemory / PAGE_SIZE / num_online_cpus();
	printk(KERN_ERR "buddyslab: running, pages_nr: %ld, cpus: %d, testtime: %ld\n",
		nr_pages, num_online_cpus(), testtime);

	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(works, cpu);

		if ((cpu % 2) == 0) {
			INIT_WORK(work, buddy_worker);
			schedule_work_on(cpu, work);
		} else {
			INIT_WORK(work, slab_worker);
			schedule_work_on(cpu, work);
		}
	}
	flush_scheduled_work();

	printk(KERN_ERR "buddyslab test done\n");
	return -EINVAL;
}

static const struct file_operations test_proc_fops = {
	.owner = THIS_MODULE,
	.open = teststart_proc_open,
};

static int __init buddyslab_init(void)
{
	proc_create("buddyslab", 0, NULL, &test_proc_fops);
	return 0;
}

static void __exit buddyslab_cleanup(void)
{
	remove_proc_entry("buddyslab", NULL);
}

module_init(buddyslab_init);
module_exit(buddyslab_cleanup);
