/*
 * Copyright (c) International Business Machines Corp., 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/fsp/console.h>

static struct fsptrace_ops *fsptrace_ops;
static int fsptrace_console_td = -1;
int silent_console;

#define CONSTRACE_LINE_SIZE 256

/*
 * This is for silent console support.  To enable, uboot will pass in
 * "silent=yes" on the kernel command line.
 */
static int __init silent_console_setup(char *str)
{
	if (str[0] == 'y')
		silent_console = 1;

	return 1;
}
__setup("silent=", silent_console_setup);


int register_fsptrace_callbacks(struct fsptrace_ops *ops)
{
	if (fsptrace_ops) {
		printk("register_trace_callbacks: ops (%p) already registered",
		       ops);
		return -EBUSY;
	}

	fsptrace_ops = ops;

	fsptrace_console_td = fsptrace_ops->trace_register("CONSOLE", 8192);

	if (fsptrace_console_td < 0) {
		printk("CONSOLE trace register failed: rc=%d\n",
		       fsptrace_console_td);
		return fsptrace_console_td;
	}

	return 0;
}
EXPORT_SYMBOL(register_fsptrace_callbacks);

int unregister_fsptrace_callbacks(struct fsptrace_ops *ops)
{
	if (fsptrace_ops != ops)
		panic("unregister_trace_callbacks: ops mismatch %p != %p",
		      fsptrace_ops, ops);

	fsptrace_ops->trace_unregister(fsptrace_console_td);

	fsptrace_ops = NULL;
	fsptrace_console_td = -1;

	return 0;
}
EXPORT_SYMBOL(unregister_fsptrace_callbacks);

/*
 * If the fsptrace module has been loaded, this function traces the console
 * output to the CONSOLE buffer in fsptrace, via trace_binary(), provided by
 * the fsptrace module.  Otherwise it does nothing.
 */
void fsp_console_trace(struct tty_struct *tty, struct file *file,
			  const unsigned char *buf, size_t nr)
{
	static char line[CONSTRACE_LINE_SIZE];
	static int pos = 0;
	int i;

	if (!fsptrace_ops || fsptrace_console_td < 0)
		return;

	/*
	 * Buffer the console output into a global buffer, replacing
	 * non-printable characters with ".".  When we get a newline,
	 * send the buffer to fsp-trace so we only trace one line at a
	 * time.
	 */
	for (i = 0; i < nr; i++) {
		if (pos < CONSTRACE_LINE_SIZE) {
			if ((buf[i] >= ' ' && buf[i] <= '~') || buf[i] == '\n')
				line[pos++] = buf[i];
			else
				line[pos++] = '.';
		}
		if (buf[i] == '\n') {
			fsptrace_ops->trace_binary(fsptrace_console_td, 0, -1,
						   __LINE__, line, pos);
			pos = 0;
		}
	}
}
