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

#define XMC_JTAG_REG_BASE   0x8002002C  // TCK/TMS/TDI
#define XMC_JTAG_MAP_SIZE   0x1000
#define XMC_TDO_OFFSET  (0x8002012C - 0x8002002C) // TDO
#define XMC_TDO_MASK        (1U << 4)

// individual bit masks
#define XMC_JTAG_EN_MASK    (1U << 0)
#define XMC_TDI_MASK        (1U << 4)
#define XMC_TMS_MASK        (1U << 5)
#define XMC_TCK_MASK        (1U << 6)
#define XMC_RESETN_MASK     (1U << 8)

static volatile uint32_t *jtag_reg;

__attribute__((unused))
static void *map_register(off_t phys_addr, size_t size)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		LOG_ERROR("PTC: cannot open /dev/mem: %s", strerror(errno));
		return MAP_FAILED;
	}

	off_t page_base = phys_addr & ~(sysconf(_SC_PAGE_SIZE) - 1);
	off_t page_off  = phys_addr - page_base;

	void *map = mmap(NULL, size + page_off,
	                 PROT_READ | PROT_WRITE, MAP_SHARED, fd, page_base);
	close(fd);

	if (map == MAP_FAILED) {
		LOG_ERROR("PTC: mmap failed: %s", strerror(errno));
		return MAP_FAILED;
	}

	return (uint8_t *)map + page_off;
}

static int ptc_speed(int speed)
{
    (void)speed;
    return ERROR_OK;
}

static int ptc_khz(int speed, int *khz)
{
    (void)speed;
    if (khz)
        *khz = 0;  /* 0 = fixed/unknown speed */
    return ERROR_OK;
}

static int ptc_write(int tck, int tms, int tdi)
{
	if (!jtag_reg)
		return ERROR_FAIL;

	uint32_t reg = *jtag_reg;

	if (tck)
		reg |= XMC_TCK_MASK;
	else
		reg &= ~XMC_TCK_MASK;

	if (tms)
		reg |= XMC_TMS_MASK;
	else
		reg &= ~XMC_TMS_MASK;

	if (tdi)
		reg |= XMC_TDI_MASK;
	else
		reg &= ~XMC_TDI_MASK;

	*jtag_reg = reg;
	return ERROR_OK;
}

static bb_value_t ptc_read(void)
{
    volatile uint32_t *tdo_reg = jtag_reg + (XMC_TDO_OFFSET / sizeof(uint32_t));
    return (*tdo_reg & XMC_TDO_MASK) ? BB_HIGH : BB_LOW;
}

__attribute__((unused))
static struct bitbang_interface ptc_bitbang = {
	.write = ptc_write,
	.read  = ptc_read,
};

static int ptc_execute_queue(void)

{
	return bitbang_execute_queue();
}

static int ptc_reset(int trst, int srst)
{
    if (!jtag_reg)
        return ERROR_FAIL;

    uint32_t reg = *jtag_reg;

    /* TRST not supported, but keep logic symmetrical */
    (void)trst;

    if (srst)
        reg &= ~XMC_RESETN_MASK;  // assert reset (active low)
    else
        reg |= XMC_RESETN_MASK;   // deassert/reset released

    *jtag_reg = reg;
    return ERROR_OK;
}

static struct jtag_interface ptc_jtag_interface = {
	.supported     = DEBUG_CAP_TMS_SEQ,
	.execute_queue = ptc_execute_queue,
};

static int ptc_init(void)
{
    fprintf(stderr, "PTC: init() starting\n");

    bitbang_interface = &ptc_bitbang;

    jtag_reg = map_register(XMC_JTAG_REG_BASE, XMC_JTAG_MAP_SIZE);
    if (jtag_reg == MAP_FAILED || !jtag_reg) {
        LOG_ERROR("PTC: unable to map registers");
        return ERROR_FAIL;
    }

    LOG_INFO("PTC bit‑bang adapter initialised, reg=%p", jtag_reg);
    return ERROR_OK;
}

/*
static int ptc_init(void)
{
	fprintf(stderr, "PTC: init() starting\n");


	if (!bitbang_interface)
		bitbang_interface = calloc(1, sizeof(*bitbang_interface));
	
	if (!bitbang_interface) {
		LOG_ERROR("PTC: failed to allocate bitbang_interface");
		return ERROR_FAIL;
	}

	if (!bitbang_interface)
		bitbang_interface = calloc(1, sizeof(struct bitbang_interface));

	bitbang_interface->write = ptc_write;
	bitbang_interface->read  = ptc_read;

	jtag_reg = map_register(XMC_JTAG_REG_BASE, XMC_JTAG_MAP_SIZE);
	if (jtag_reg == MAP_FAILED || !jtag_reg) {
		LOG_ERROR("PTC: unable to map registers");
		return ERROR_FAIL;
	}
	fprintf(stderr, "PTC: mapped jtag_reg=%p\n", (void *)jtag_reg);

	LOG_INFO("PTC bit‑bang adapter initialised");
	return ERROR_OK;
}
*/


__attribute__((weak)) struct bitbang_interface *bitbang_interface;

__attribute__((unused))
static const char * const ptc_transports[] = { "jtag", NULL };

struct adapter_driver ptc_adapter_driver = {
	.name     = "ptc",
	.transports = ptc_transports,
	.init     = ptc_init,
	.reset	  = ptc_reset,
	.quit	  = NULL,
	.speed	  = ptc_speed,
	.khz	  = ptc_khz,
	.jtag_ops = &ptc_jtag_interface,
};