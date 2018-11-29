#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kallsyms.h>
#include <crypto/public_key.h>
#include <crypto/hash.h>
#include <crypto/sha1_base.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jstancek");
MODULE_DESCRIPTION("reproduce sha1 avx2 read beyond");

static void *calc_hash(const char *hashname, u8 *d, int len1, int len2)
{
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	int ret = -ENOMEM;
	u8 *digest;
	unsigned int desc_size;
	struct sha1_state *sctx;
	static int digest_size;

	tfm = crypto_alloc_shash(hashname, 0, 0);
	if (IS_ERR(tfm)) {
		printk("failed to alloc %s\n", hashname);
		return (PTR_ERR(tfm) == -ENOENT) ? ERR_PTR(-ENOPKG) : ERR_CAST(tfm);
	}

	desc_size = crypto_shash_descsize(tfm) + sizeof(*desc);
	digest_size = crypto_shash_digestsize(tfm);

	desc = kzalloc(desc_size + digest_size, GFP_KERNEL);
	desc->tfm   = tfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;

	digest = (u8 *) desc + desc_size;

	ret = crypto_shash_init(desc);
	if (ret < 0)
		goto error;

	ret = crypto_shash_update(desc, d, len1);
	if (ret < 0)
		goto error;
	d += len1;

	sctx = shash_desc_ctx(desc);

	ret = crypto_shash_update(desc, d, len2);
	if (ret < 0)
		goto error;
	d += len2;

error:
	kfree(desc);
	crypto_free_shash(tfm);
	return ERR_PTR(ret);
}

int (*set_memory_np)(unsigned long addr, int numpages);

void test_hash(char *hashname, u8 *data, int maxlen)
{
	int so, chunk1, chunk2, chunk2_min, chunk2_max;
	long hashes = 0;
	void *ptr;

	printk("sha_test testing hash: %s, maxlen: %d\n", hashname, maxlen);
	for (so = PAGE_SIZE - 128; so < PAGE_SIZE; so++) {
		// printk("  start_offset: %d\n", so);

		for (chunk1 = PAGE_SIZE - 128; chunk1 < PAGE_SIZE; chunk1++) {
			chunk2_max = maxlen - so - chunk1;
			chunk2_min = chunk2_max > 128 ? chunk2_max - 128 : 1;
			for (chunk2 = chunk2_min; chunk2 <= chunk2_max; chunk2++) {
				ptr = calc_hash(hashname, data + so, chunk1, chunk2);
				if (IS_ERR(ptr))
					return;
				hashes++;
			}
		}
	}
	printk("sha_test hash: %s calculated %ld hashes\n", hashname, hashes);
}

static int __init sha1test_init(void)
{
	int maxlen;
	u8 *page_after_data;
	u8 *data;
	char *hashes[] = { "sha1-generic", "sha1-ni", "sha1-avx", "sha1-avx2", "sha1-ssse3", NULL };
	char **hash = hashes;

	printk("sha_test module loaded\n");
	set_memory_np = (void *) kallsyms_lookup_name("set_memory_np");
	if (!set_memory_np) {
		printk("failed to find set_memory_np\n");
		return -EINVAL;
	}

	if (!kallsyms_lookup_name("sha1_transform_avx2")) {
		printk("failed to find sha1_transform_avx2\n");
		return -EINVAL;
	}

	maxlen = PAGE_SIZE * 2;
	data = kmalloc(maxlen + PAGE_SIZE, GFP_KERNEL);
	page_after_data = data + maxlen;
	printk("data is at 0x%p, page_after_data: 0x%p\n",
		data, page_after_data);

	/*
 	 * We have X pages, which hash should use, we marked page after
 	 * last one as not present.
 	 *  +----------------+...------------------+-----------------+
 	 *         PAGE1      ...       PAGEX             PAGEX+1
 	 *  +----------------+...------------------+-----------------+
 	 *  ^ data                                    (not present)
 	 *                 ^ start_offset
 	 *                                   ^ last_byte shash should use
 	 *
 	 */
	set_memory_np((unsigned long)page_after_data, 1);

	while (*hash) {
		test_hash(*hash, data, maxlen);
		hash++;
	}

	printk("sha_test done\n");

	/* yes, it leaks memory */
	return 0;
}

static void __exit sha1test_cleanup(void)
{
	printk("sha_test module unloaded\n");
}

module_init(sha1test_init);
module_exit(sha1test_cleanup);
