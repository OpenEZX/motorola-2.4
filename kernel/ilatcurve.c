/*
 * FILE NAME kernel/ilatcurve.c
 *
 * BRIEF MODULE DESCRIPTION
 * This file implements a sample based interrupt latency curve for Linux.
 *
 * Author: David Singleton
 *	MontaVista Software, Inc.
 *      support@mvista.com
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <asm/system.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/ilatency.h>
#include <asm/preem_latency.h>
#ifdef CONFIG_SMP
#include <linux/smp.h>
#endif

unsigned long bucketlog[2][BUCKETS];
unsigned long maximum_off;
unsigned int ilatency_start;
unsigned int ilatency_stop;

void inline interrupt_overhead_start()
{

	readclock(ilatency_start);
}

void inline interrupt_overhead_stop()
{
	readclock(ilatency_stop);
}


unsigned long total_ilat_samples; /* total number of samples collected */

asmlinkage void inthoff_logentry(unsigned diff)
{
	unsigned sampletime = diff;
	sampletime /= BUCKET_SIZE;
	if (maximum_off < diff) {
		maximum_off = diff;
	}

	if (sampletime > LAST_BUCKET) {
		bucketlog[1][LAST_BUCKET]++;
	} else {
		bucketlog[1][sampletime]++;
	}
	total_ilat_samples++;
	return;
}

/*
 * Writing to /proc/maxoff resets the counters
 */
static ssize_t
ilatcurve_write_proc(struct file * file, const char * buf, size_t count,
loff_t *ppos)
{
	extern interrupt_latency_log_t latency;

	total_ilat_samples = 0;
        memset(&bucketlog, 0, sizeof(unsigned long) * 2 * BUCKETS);
        memset(&latency, 0, sizeof(interrupt_latency_log_t));
	latency.logging = ON;
	return count;
}

static int g_read_completed = 0;

static int ilatcurve_read_proc(
	char *page_buffer,
	char **my_first_byte,
	off_t virtual_start,
	int length,
	int *eof,
	void *data)
{
	int my_buffer_offset = 0;
	char * const my_base = page_buffer;
	int i;
	if (virtual_start == 0) {
		/* 
		 * Just been opened so display the header information 
		 * also stop logging  BUGBUG: stop logging while we are 
		 * reading, initialize the index numbers in the log array
		 */
		unsigned long usecs;
		g_read_completed = 0;

#ifdef CLOCK_COUNTS_DOWN
		usecs = clock_to_usecs(ilatency_start - ilatency_stop);
#else
		usecs = clock_to_usecs(ilatency_stop - ilatency_start);
#endif

		my_buffer_offset += sprintf(my_base+my_buffer_offset,
		    "#%ld samples logged\n#timer measured %ld ticks per usec.\n"
		    "#interrupt overhead %u microseconds\n#usecs  samples\n"
		    "#maximum interrupt off time %u microseconds\n",
		    total_ilat_samples, (unsigned)ticks_per_usec, usecs, 
		    maximum_off);
	} else if (g_read_completed == BUCKETS) {
		 *eof = 1;
		 /* BUGBUG: start logging again */
		 return 0;
	}

	/* dump the sample log on the screen */
	for (i = 0; i < (BUCKETS-1); i++) {
		my_buffer_offset += sprintf(my_base + my_buffer_offset,
		    "%5u\t%8u\n", bucketlog[0][i],
		    bucketlog[1][i]);
		g_read_completed++;
	}
	my_buffer_offset += sprintf(my_base + my_buffer_offset,
	    "%5u\t%8u(greater than this time)\n", bucketlog[0][i],
	    bucketlog[1][i]);
	g_read_completed++;

	*my_first_byte = page_buffer;
	return  my_buffer_offset;
}

int __init
ilatcurve_init(void)
{
	int i;
	int j;
	struct proc_dir_entry *entry;

	readclock_init();
#ifndef CONFIG_PPC
	ticks_per_usec = TICKS_PER_USEC;
#endif
#ifdef CONFIG_SMP
	printk("Interrupt holdoff times are not supported on SMP systems.\n");
#else
	printk("Interrupt holdoff times measurement enabled at %u ticks per usec.\n", ticks_per_usec);
	for (j = BUCKET_SIZE, i = 0; i < BUCKETS; i++) {
		bucketlog[0][i] = j;
		j += BUCKET_SIZE;
	}	
	bucketlog[0][LAST_BUCKET] = UPPER_LIMIT;
	/*
	 * this is where each platform must turn the
	 * delta 'ticks' into microseconds.  Each architecture
	 * does it differently.
	 */
#ifdef CONFIG_PROC_FS
	entry = create_proc_read_entry("ilatcurve", S_IWUSR | S_IRUGO, NULL,
	    ilatcurve_read_proc, 0);
	if (entry) {
		entry->write_proc = ilatcurve_write_proc;
	}

#endif
#endif
	return 0;
}

__initcall(ilatcurve_init);


