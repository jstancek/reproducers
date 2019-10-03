#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/printk.h>

/*
On ppc64le KVM guest:

# make test
[   95.758026] reproducer loaded
[  100.762443] timer cnt: 20976764
[  105.772344] timer cnt: 20977265
[  110.782224] timer cnt: 57794680
[  115.792105] timer cnt: 60753318
[  120.801999] timer cnt: 114808006
[  125.811890] timer cnt: 168945209
[  130.821784] timer cnt: 222991696
[  135.831679] timer cnt: 276963562
[  138.751636] NMI watchdog: BUG: soft lockup - CPU#1 stuck for 22s! [swapper/1:0]
[  138.751791] Modules linked in: repro(OE) sunrpc pseries_rng virtio_balloon ip_tables xfs libcrc32c virtio_net net_failover failover virtio_blk virtio_pci virtio_ring virtio dm_mirror dm_region_hash dm_log dm_mod [last unloaded: repro]
[  138.752395] CPU: 1 PID: 0 Comm: swapper/1 Kdump: loaded Tainted: G           OE  ------------   3.10.0-1099.el7.ppc64le #1
[  138.752568] task: c0000003e8a6f150 ti: c0000003effe8000 task.ti: c0000003e8b2c000
[  138.752702] NIP: c000000000017854 LR: c000000000017854 CTR: c000000000113a10
[  138.752833] REGS: c0000003effeb9b0 TRAP: 0901   Tainted: G           OE  ------------    (3.10.0-1099.el7.ppc64le)
[  138.754106] MSR: 8000000000009033 <SF,EE,ME,IR,DR,RI,LE>  CR: 28000024  XER: 20000000
[  138.754449] CFAR: c000000000112198 SOFTE: 1
GPR00: c000000000ae7ec4 c0000003effebc30 c00000000148a200 0000000000000900
GPR04: 0000000000000001 c000000001a4b4a8 0000000000000100 0000000000000001
GPR08: 0000000000000000 c000000001a4a900 00000000ffffc0b8 d000000003f60220
GPR12: d000000003f60000 c000000007b80900
[  138.755403] NIP [c000000000017854] arch_local_irq_restore+0x104/0x150
[  138.755515] LR [c000000000017854] arch_local_irq_restore+0x104/0x150
[  138.755623] Call Trace:
[  138.755674] [c0000003effebc30] [c0000003effebc70] 0xc0000003effebc70 (unreliable)
[  138.755877] [c0000003effebc50] [c000000000ae7ec4] _raw_spin_unlock_irqrestore+0x44/0x90
[  138.756060] [c0000003effebc70] [c000000000113be8] mod_timer+0x1d8/0x3f0
[  138.756198] [c0000003effebce0] [d000000003f60080] my_func+0x80/0xd4 [repro]
[  138.756339] [c0000003effebd50] [c00000000010ffd8] call_timer_fn+0x68/0x170
[  138.756474] [c0000003effebdf0] [c0000000001127dc] run_timer_softirq+0x2dc/0x3a0
[  138.756630] [c0000003effebea0] [c000000000100fd4] __do_softirq+0x154/0x380
[  138.756771] [c0000003effebf90] [c00000000002d87c] call_do_softirq+0x14/0x24
[  138.756909] [c0000003e8b2f9b0] [c0000000000186e0] do_softirq+0x130/0x180
[  138.757042] [c0000003e8b2f9f0] [c000000000101564] irq_exit+0x1f4/0x200
[  138.757181] [c0000003e8b2fa30] [c000000000027568] timer_interrupt+0x98/0xf0
[  138.757318] [c0000003e8b2fa60] [c000000000002c14] decrementer_common+0x114/0x118
[  138.757503] --- Exception: 901 at plpar_hcall_norets+0x8c/0xdc
[  138.757503]     LR = shared_cede_loop+0xb8/0xd0
[  138.757678] [c0000003e8b2fd50] [0000000000000008] 0x8 (unreliable)
[  138.757815] [c0000003e8b2fdc0] [c0000000008a5d54] cpuidle_idle_call+0x124/0x410
[  138.757974] [c0000003e8b2fe30] [c0000000000ac3c0] pseries_lpar_idle+0x20/0x90
[  138.758107] [c0000003e8b2fe90] [c00000000001f8c0] arch_cpu_idle+0x70/0x160
[  138.758245] [c0000003e8b2fec0] [c00000000018d480] cpu_startup_entry+0x190/0x210
[  138.758403] [c0000003e8b2ff20] [c000000000057890] start_secondary+0x310/0x340
[  138.758541] [c0000003e8b2ff90] [c000000000009b6c] start_secondary_prolog+0x10/0x14
[  138.758692] Instruction dump:
[  138.758762] 60000000 e9228130 e9290000 e8690010 786307e2 3063ffff 7c631910 7863b7a6
[  138.759002] 78635624 38630e80 4bffff5c 4bfeae41 <60000000> 4bffff6c 7927dfe3 38600e60
[  140.751569] test done 317674255 1
[  146.754154] random: crng init done
[  155.763786] reproducer unloaded
*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jstancek");
MODULE_DESCRIPTION("reproduce BZ 1752885");
MODULE_VERSION("1");

struct timer_list my_timer;
static long start_jiffies, last_jiffies;
static long cnt = 0;
static long done = 0;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
void my_func(unsigned long data)
#else
void my_func(struct timer_list *t)
#endif
{
	if (jiffies - start_jiffies < 45*HZ && !done) {
		cnt++;

		if (jiffies - last_jiffies > 5*HZ) {
			printk("timer cnt: %ld\n", cnt);
			last_jiffies = jiffies;
		}
		mod_timer(&my_timer, jiffies);
	} else {
		printk("test done %ld %d\n", cnt, smp_processor_id());
	}
}

static int __init example_init(void)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,10,0)
	init_timer(&my_timer);
	my_timer.data = (long)0;
	my_timer.function = my_func;
	my_timer.expires = jiffies;
#else
	timer_setup(&my_timer, my_func, 0);
#endif
	start_jiffies = last_jiffies = jiffies;
	mod_timer(&my_timer, jiffies);	
	printk("reproducer loaded\n");

	return 0;
}

static void __exit example_exit(void)
{
	done = 1;
	del_timer_sync(&my_timer);
	printk("reproducer unloaded\n");
}

module_init(example_init);
module_exit(example_exit);

