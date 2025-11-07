#include "helper/log.h"
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <jtag/interface.h>
#include "bitbang.h"

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
		// LOG_ERROR("PTC: cannot open /dev/mem: %s", strerror(errno));
		return NULL;
	}

	/* Page-align the mapping start */
	off_t page_base = phys_addr & ~(sysconf(_SC_PAGE_SIZE) - 1);
	off_t page_off  = phys_addr - page_base;

	void *map = mmap(NULL, size + page_off,
	                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
	close(fd);

	if (map == MAP_FAILED) {
		// LOG_ERROR("PTC: mmap failed: %s", strerror(errno));
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
    fprintf(stderr, "PTC: init() starting\n");

    if (!bitbang_interface)
        bitbang_interface = calloc(1, sizeof(struct bitbang_interface));

    /* Map registers */
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) { perror("open"); return ERROR_FAIL; }

    off_t page = XMC_JTAG_REG_BASE & ~0xFFF;
    off_t off  = XMC_JTAG_REG_BASE - page;
    void *map = mmap(NULL, 0x1000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page);
    close(fd);
    if (map == MAP_FAILED) { perror("mmap"); return ERROR_FAIL; }
    jtag_reg = (uint32_t *)((uint8_t *)map + off);

    /* Register callbacks */
    bitbang_interface->write = ptc_write;
    bitbang_interface->read  = ptc_read;

    fprintf(stderr, "PTC: init() done, jtag_reg=%p\n", (void*)jtag_reg);
    return ERROR_OK;
}


__attribute__((weak)) struct bitbang_interface *bitbang_interface;

/* -------------------------------------------------------------------------- */

struct adapter_driver ptc_adapter_driver = {
	.name     = "ptc",
	.init     = ptc_init,
	.jtag_ops = &ptc_jtag_interface,
};