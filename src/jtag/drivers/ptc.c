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
#define XMC_TDO_MASK        (1U << 16)

// individual bit masks
#define XMC_JTAG_EN_MASK    (1U << 0)
#define XMC_TDI_MASK        (1U << 6)
#define XMC_TMS_MASK        (1U << 4)
#define XMC_TCK_MASK        (1U << 5)
#define XMC_RESETN_MASK     (1U << 8)

#define XMC_BOOT_WAIT	50000
#define XMC_RESET_WAIT	1000

static volatile uint32_t *jtag_reg;
static volatile uint32_t *tdo_reg;
static int ptc_delay_us = 0;

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

static int ptc_write(int tck, int tms, int tdi)
{
	if (!jtag_reg)
		return ERROR_FAIL;

	uint32_t reg = *jtag_reg;
	if (ptc_delay_us > 0)
		usleep(ptc_delay_us);

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

__attribute__((unused))
static int ptc_speed(int khz)
{
    /* "khz" is user argument: adapter speed <value> */
    if (khz <= 0)
        ptc_delay_us = 10;   // slowest
    else if (khz < 10)
        ptc_delay_us = 5;
    else if (khz < 100)
        ptc_delay_us = 2;
    else
        ptc_delay_us = 0;    // fastest (no sleep)
    LOG_INFO("PTC: delay set to %d µs for adapter speed %d", ptc_delay_us, khz);
    return ERROR_OK;
}

__attribute__((unused))
static int ptc_khz(int speed, int *khz)
{
    *khz = speed;
    return ERROR_OK;
}

static int ptc_speed_div(int khz, int *speed)
{
    *speed = khz;
    return ERROR_OK;
}

static bb_value_t ptc_read(void)
{
	if (ptc_delay_us > 0)
		usleep(ptc_delay_us);
	uint32_t val = *tdo_reg;
	return (val & XMC_TDO_MASK) ? BB_HIGH : BB_LOW;
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

	tdo_reg = jtag_reg + (XMC_TDO_OFFSET / sizeof(uint32_t));

	// get JTAG out of tri-state and enable XMC JTAG control from the enclustra
	uint32_t reg = *jtag_reg;
	reg |= XMC_JTAG_EN_MASK;
	reg |= XMC_RESETN_MASK;
	*jtag_reg = reg;

	// wait for a bit to ensure the XMC is up and running 
	usleep(XMC_BOOT_WAIT);

    LOG_INFO("PTC bit‑bang adapter initialised, reg=%p", jtag_reg);

    return ERROR_OK;
}

static int ptc_quit(void)
{
	if (!jtag_reg)
		return ERROR_OK;

	uint32_t reg = *jtag_reg;

	// reset XMC
	reg &= ~XMC_RESETN_MASK;
	*jtag_reg = reg;
	// wait for a bit
	usleep(XMC_RESET_WAIT);

	// bring reset high so XMC boots
	reg |= XMC_RESETN_MASK;
	*jtag_reg = reg;
	usleep(XMC_RESET_WAIT);

	// put JTAG lines back into tri-state
	reg &= ~XMC_JTAG_EN_MASK;
	*jtag_reg = reg;

	LOG_INFO("PTC: JTAG bridge disabled, pins tri‑stated");
	return ERROR_OK;
}

__attribute__((weak)) struct bitbang_interface *bitbang_interface;

__attribute__((unused))
static const char * const ptc_transports[] = { "jtag", NULL };

struct adapter_driver ptc_adapter_driver = {
	.name     = "ptc",
	.transports = ptc_transports,
	.speed		= ptc_speed,
	.khz		= ptc_khz,
	.speed_div	= ptc_speed_div,
	.init     = ptc_init,
	.reset	  = ptc_reset,
	.quit	  = ptc_quit,
	.jtag_ops = &ptc_jtag_interface,
};