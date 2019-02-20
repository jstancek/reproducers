#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/kallsyms.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jstancek");
MODULE_DESCRIPTION("copy compound page, set page after it as not present and run page_mapped()");

static void print_page_info(struct page *page, const char *msg)
{
	printk("%s\n", msg);
	printk(" struct page: %px\n", page);
	printk(" page addr: %px\n", page_address(page));
	printk(" PageCompound(page): %d\n", PageCompound(page));
	printk(" PageHuge(page): %d\n", PageHuge(page));
	printk(" atomic_read(compound_mapcount_ptr(page): %d\n", atomic_read(compound_mapcount_ptr(page)));
	printk(" hpage_nr_pages(page): %d\n", hpage_nr_pages(page));
	printk(" atomic_read(page->_mapcount: %d\n", atomic_read(&page->_mapcount));
	printk(" calling page_mapped(page)\n");
	printk(" page_mapped(page): %d\n\n", page_mapped(page));
}

static int __init compound_init(void)
{
	struct page *comp_page, *copy_page;
	char *copy_area;

	/* allocate compound page of order 1 */
	comp_page = alloc_pages(GFP_KERNEL | __GFP_COMP, 1);
	if (!comp_page)
		return -ENOMEM;
	//print_page_info(comp_page, "compound page");

	/* now try it on a copy, but set subsequent page as not present */
	copy_page = alloc_pages(GFP_KERNEL, 4);
	if (!copy_page)
		return -ENOMEM;
	split_page(copy_page, 1);
	copy_area = page_address(copy_page);

	printk("copy addr: %px\n", copy_area);

	memcpy(copy_area, comp_page, 2*sizeof(struct page));

	/* mapcount for subsequent struct pages is -1 */
	memset(copy_area + 2*sizeof(struct page), -1, HPAGE_PMD_NR*sizeof(struct page));
	print_page_info((struct page *)copy_area, "comp page copy");

	/* mapcount for subsequent struct pages is 0 */
	memset(copy_area + 2*sizeof(struct page), 0, HPAGE_PMD_NR*sizeof(struct page));
	print_page_info((struct page *)copy_area, "comp page copy");

	/*
	 * if we get different page_mapped() result, that means page_mapped
	 * is reading beyong what it's supposed to 
	 */

	return -EINVAL;
}

static void __exit compound_cleanup(void)
{
	printk("sha_test module unloaded\n");
}

module_init(compound_init);
module_exit(compound_cleanup);
