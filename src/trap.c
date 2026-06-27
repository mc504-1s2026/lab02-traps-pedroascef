#include <kernel/trap.h>
#include <kernel/panic.h>
#include <kernel/printf.h>
#include <arch/csr.h>
#include <arch/plic.h>
#include <arch/timer.h>
#include <kernel/serial.h>

extern void trap_entry();

void handle_irq()
{
	u64 scause = csr_read(CSR_SCAUSE);

	if (scause == TRAP_TIMER_IRQ) {
		timer_irq();
	} else if (scause == TRAP_EXTERNAL_IRQ) {
		u32 irq = plic_hart_claim_irq(0);
		if (irq == IRQ_SERIAL) {
			serial_irq();
		} else if (irq != 0) {
			warn("handle_irq: unhandled external IRQ %d\n", irq);
		}
		plic_hart_complete_irq(0, irq);
	} else {
		warn("handle_irq: unhandled interrupt code %lu\n", scause & ~TRAP_IRQ_BIT);
	}
}

void handle_exception()
{
	u64 scause = csr_read(CSR_SCAUSE);
	u64 stval  = csr_read(CSR_STVAL);
	u64 sepc   = csr_read(CSR_SEPC);

	switch (scause) {
	case EXCEPTION_INST_PAGE_FAULT:
		error("Instruction page fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	case EXCEPTION_LOAD_PAGE_FAULT:
		error("Load page fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	case EXCEPTION_STORE_PAGE_FAULT:
		error("Store page fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	case EXCEPTION_INST_ACCESS_FAULT:
		error("Instruction access fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	case EXCEPTION_LOAD_ACCESS_FAULT:
		error("Load access fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	case EXCEPTION_STORE_ACCESS_FAULT:
		error("Store access fault at sepc=0x%lx, addr=0x%lx\n", sepc, stval);
		break;
	default:
		error("Unhandled exception: scause=0x%lx, sepc=0x%lx, stval=0x%lx\n",
		      scause, sepc, stval);
		break;
	}

	BUG();
}

void trap_setup()
{
	csr_write(CSR_STVEC, (u64)trap_entry);
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void handle_trap()
{
	u64 scause = csr_read(CSR_SCAUSE);

	if (scause & TRAP_IRQ_BIT) {
		handle_irq();
	} else {
		handle_exception();
	}
}

void hart_irq_enable()
{
	csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

u64 hart_irq_save()
{
	u64 sstatus = csr_read(CSR_SSTATUS);
	u64 sie_bit = sstatus & CSR_SSTATUS_SIE;
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
	return sie_bit;
}

void hart_irq_restore(u64 flags)
{
	if (flags & CSR_SSTATUS_SIE)
		csr_set(CSR_SSTATUS, CSR_SSTATUS_SIE);
	else
		csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}

void hart_irq_disable()
{
	csr_clear(CSR_SSTATUS, CSR_SSTATUS_SIE);
}
