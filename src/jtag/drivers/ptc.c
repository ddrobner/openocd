#include "jtag/interface.h"
#include "jtag/drivers/bitbang.h"
#include "helper/log.h"
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define XMC_JTAG_REG_BASE   0x8002002C  // all JTAG bits live here
#define XMC_JTAG_MAP_SIZE   0x1000

// individual bit masks
#define XMC_JTAG_EN_MASK    (1U << 0)
#define XMC_TDI_MASK        (1U << 4)
#define XMC_TMS_MASK        (1U << 5)
#define XMC_TCK_MASK        (1U << 6)
#define XMC_RESETN_MASK     (1U << 8)
#define XMC_TDO_MASK        (1U << 16)

static volatile uint32_t *jtag_reg;

static void *map_register(off_t phys_addr, size_t size)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		LOG_ERROR("PTC: cannot open /dev/mem: %s", strerror(errno));
		return NULL;
	}

	/* Page-align the mapping start */
	off_t page_base = phys_addr & ~(sysconf(_SC_PAGE_SIZE) - 1);
	off_t page_off  = phys_addr - page_base;

	void *map = mmap(NULL, size + page_off,
	                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
	close(fd);

	if (map == MAP_FAILED) {
		LOG_ERROR("PTC: mmap failed: %s", strerror(errno));
		return NULL;
	}

	return (uint8_t *)map + page_off;
}

static int ptc_write(int tck, int tms, int tdi)
{
	uint32_t reg = *jtag_reg;

	if(tck)
		reg |= XMC_TCK_MASK;
	if(tms)
		reg |= XMC_TMS_MASK;
	if(tdi)
		reg |= XMC_TDI_MASK;

	*jtag_reg = reg;
	return ERROR_OK;
}

static bb_value_t ptc_read(void)
{
	return ((*jtag_reg & XMC_TDO_MASK) ?
            BB_HIGH : BB_LOW);
}

static struct bitbang_interface ptc_bitbang = {
	.write = ptc_write,
	.read  = ptc_read,
};

static int ptc_execute_queue(void)

{
	return bitbang_execute_queue();
}

static struct jtag_interface ptc_jtag_interface = {
	.supported     = DEBUG_CAP_TMS_SEQ,
	.execute_queue = ptc_execute_queue,
};

static int ptc_init(void)
{
	LOG_INFO("PTC adapter initialised");
	// LOG_INFO("  TCK=%p  TMS=%p  TDI=%p  TDO=%p",
	//         (void *)reg_tck, (void *)reg_tms,
	//         (void *)reg_tdi, (void *)reg_tdo);

	jtag_reg = map_register(XMC_JTAG_REG_BASE, XMC_JTAG_MAP_SIZE);

	if(!jtag_reg){
		LOG_ERROR("PTC: unable to map JTAG registers");
		return ERROR_FAIL;
	}

	bitbang_interface = &ptc_bitbang;

	return ERROR_OK;
}

__attribute__((weak)) struct bitbang_interface *bitbang_interface;

/* -------------------------------------------------------------------------- */

struct adapter_driver ptc_adapter_driver = {
	.name     = "ptc",
	.init     = ptc_init,
	.jtag_ops = &ptc_jtag_interface,
};