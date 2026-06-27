#include <kernel/serial.h>
#include <kernel/panic.h>
#include <arch/io.h>
#include <arch/plic.h>
#include <arch/csr.h>
#include <arch/spinlock.h>
#include <kernel/mm.h>

#define SERIAL_VA ((void *)(KERNEL_DIRECT_MAP_START + 0x10000000UL))

static inline void serial_write_reg(u64 reg, u8 val)
{
	iowrite8(val, (void *)((u64)SERIAL_VA + reg));
}

static inline u8 serial_read_reg(u64 reg)
{
	return ioread8((void *)((u64)SERIAL_VA + reg));
}

#define SERIAL_BUF_SIZE 256

struct serialdev {
	char buf[SERIAL_BUF_SIZE];
	size_t len;
	struct spinlock lock;
};

static struct serialdev dev;

void serial_init()
{
	spin_init(&dev.lock);
	dev.len = 0;

	serial_write_reg(SERIAL_IER, 0x00);
	serial_write_reg(SERIAL_FCR,
			 SERIAL_FCR_FIFO_ENABLE |
			 SERIAL_FCR_RX_FIFO_CLEAR |
			 SERIAL_FCR_TX_FIFO_CLEAR);
	serial_write_reg(SERIAL_LCR, 0x03);
	serial_write_reg(SERIAL_IER, SERIAL_IER_ERBFI);
}

void serial_irq_enable()
{
	plic_irq_set_priority(IRQ_SERIAL, 1);
	plic_hart_set_threshold(0, 0);
	plic_hart_enable_irq(0, IRQ_SERIAL);
	csr_set(CSR_SIE, CSR_SIE_SEIE);
}

void serial_irq_disable()
{
	csr_clear(CSR_SIE, CSR_SIE_SEIE);
	serial_write_reg(SERIAL_IER, 0x00);
}

void serial_irq()
{
	while (serial_read_reg(SERIAL_LSR) & SERIAL_LSR_DTR) {
		char c = (char)serial_read_reg(SERIAL_RBR);

		u64 flags = spin_lock_irqsave(&dev.lock);
		if (dev.len < SERIAL_BUF_SIZE) {
			dev.buf[dev.len++] = c;
		}
		spin_unlock_irqrestore(&dev.lock, flags);
	}
}

size_t serial_read(char *buf)
{
	u64 flags = spin_lock_irqsave(&dev.lock);
	size_t n = dev.len;
	for (size_t i = 0; i < n; i++)
		buf[i] = dev.buf[i];
	dev.len = 0;
	spin_unlock_irqrestore(&dev.lock, flags);
	return n;
}

void serial_putc(char c)
{
	while (!(serial_read_reg(SERIAL_LSR) & SERIAL_LSR_THRE))
		;
	serial_write_reg(SERIAL_THR, (u8)c);
}

void serial_puts(char *str)
{
	while (*str) {
		if (*str == '\n')
			serial_putc('\r');
		serial_putc(*str++);
	}
}
