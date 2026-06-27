#include <arch/timer.h>
#include <arch/csr.h>
#include <kernel/panic.h>
#include <kernel/printf.h>

static volatile u64 alarm_deadline = 0;

u64 timer_read()
{
	return csr_read(CSR_TIME);
}

void timer_irq_enable()
{
	csr_set(CSR_SIE, CSR_SIE_STIE);
	csr_write(CSR_STIMECMP, (u64)-1);
	hart_irq_enable();
}

void timer_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_STIE);
}

void timer_set_alarm(u64 secs)
{
	u64 now = csr_read(CSR_TIME);
	alarm_deadline = now + secs * TIMER_FREQ;
	csr_write(CSR_STIMECMP, alarm_deadline);
}

void timer_irq()
{
	csr_write(CSR_STIMECMP, (u64)-1);

	if (alarm_deadline != 0 && csr_read(CSR_TIME) >= alarm_deadline) {
		alarm_deadline = 0;
		extern void on_alarm_fired(void);
		on_alarm_fired();
	}
}

__attribute__((weak))
void on_alarm_fired(void)
{
	/* overridden in kmain.c */
}
