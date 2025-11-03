#include "jtag/interface.h"
#include "jtag/drivers/bitbang.h"
#include "helper/log.h"
#include <stdint.h>


#ifndef TCK_ADDR
#define TCK_ADDR 0x40000000
#endif
#ifndef TMS_ADDR
#define TMS_ADDR 0x40000004
#endif
#ifndef TDI_ADDR
#define TDI_ADDR 0x40000008
#endif
#ifndef TDO_ADDR
#define TDO_ADDR 0x4000000C
#endif

static volatile uint32_t *const reg_tck = (uint32_t *)TCK_ADDR;
static volatile uint32_t *const reg_tms = (uint32_t *)TMS_ADDR;
static volatile uint32_t *const reg_tdi = (uint32_t *)TDI_ADDR;
static volatile uint32_t *const reg_tdo = (uint32_t *)TDO_ADDR;

static int ptc_write(int tck, int tms, int tdi)
{
	*reg_tck = (uint32_t)tck;
	*reg_tms = (uint32_t)tms;
	*reg_tdi = (uint32_t)tdi;
	return ERROR_OK;
}

static bb_value_t ptc_read(void)
{
	return (*reg_tdo & 1U) ? BB_HIGH : BB_LOW;
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
	LOG_INFO("  TCK=%p  TMS=%p  TDI=%p  TDO=%p",
	         (void *)reg_tck, (void *)reg_tms,
	         (void *)reg_tdi, (void *)reg_tdo);

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