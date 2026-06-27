#include <kernel/printf.h>
#include <kernel/mm.h>
#include <arch/timer.h>
#include <kernel/trap.h>
#include <kernel/serial.h>
#include <kernel/string.h>

extern int _hartid[];

static volatile int alarm_fired = 0;

void on_alarm_fired(void)
{
	alarm_fired = 1;
}

#define CMD_BUF_SIZE 128

static char cmd_buf[CMD_BUF_SIZE];
static size_t cmd_len = 0;

static void shell_prompt(void)
{
	serial_puts("> ");
}

static void shell_execute(void)
{
	cmd_buf[cmd_len] = '\0';

	serial_puts("\r\n");

	if (cmd_len == 0) {
		shell_prompt();
		return;
	}

	if (strcmp(cmd_buf, "uptime") == 0) {
		u64 secs = timer_read() / TIMER_FREQ;
		char nbuf[32];
		size_t i = 0;
		if (secs == 0) {
			nbuf[i++] = '0';
		} else {
			char rev[32];
			size_t j = 0;
			u64 tmp = secs;
			while (tmp > 0) { rev[j++] = '0' + (tmp % 10); tmp /= 10; }
			while (j > 0) nbuf[i++] = rev[--j];
		}
		nbuf[i++] = 's';
		nbuf[i++] = '\r';
		nbuf[i++] = '\n';
		nbuf[i]   = '\0';
		serial_puts(nbuf);

	} else if (strncmp(cmd_buf, "echo ", 5) == 0) {
		serial_puts(cmd_buf + 5);
		serial_puts("\r\n");

	} else if (strncmp(cmd_buf, "alarm ", 6) == 0) {
		u64 secs = strtou64(cmd_buf + 6, 10);
		alarm_fired = 0;
		timer_set_alarm(secs);

	} else {
		serial_puts("unknown command\r\n");
	}

	cmd_len = 0;
	shell_prompt();
}

void kmain()
{
	printk_set_level(LOG_DEBUG);
	info("entered S-mode\n");
	info("booting on hart %d\n", _hartid[0]);
	info("setting up virtual memory...\n");
	vm_init();

	info("enabling traps...\n");
	trap_setup();
	info("enabling timer...\n");
	timer_irq_enable();
	info("enabling serial...\n");
	serial_init();
	serial_irq_enable();

	shell_prompt();

	char rx_buf[128];
	while (1) {
		if (alarm_fired) {
			alarm_fired = 0;
			serial_puts("\r\nalarm\r\n");
			shell_prompt();
			if (cmd_len > 0) {
				cmd_buf[cmd_len] = '\0';
				serial_puts(cmd_buf);
			}
		}

		size_t n = serial_read(rx_buf);
		for (size_t i = 0; i < n; i++) {
			char c = rx_buf[i];
			if (c == '\r' || c == '\n') {
				shell_execute();
			} else if (c == '\b' || c == 0x7f) {
				if (cmd_len > 0) {
					cmd_len--;
					serial_puts("\b \b");
				}
			} else if (cmd_len < CMD_BUF_SIZE - 1) {
				serial_putc(c);
				cmd_buf[cmd_len++] = c;
			}
		}
	}
}
