#include "jtag/interface.h"
#include "jtag/drivers/bitbang.h"
#include "helper/log.h"
#include <stdint.h>

#define XMC_JTAG_REG_BASE   0x8002002C  // all JTAG bits live here

// individual bit masks
#define XMC_JTAG_EN_MASK    (1U << 0)
#define XMC_TDI_MASK        (1U << 4)
#define XMC_TMS_MASK        (1U << 5)
#define XMC_TCK_MASK        (1U << 6)
#define XMC_RESETN_MASK     (1U << 8)
#define XMC_TDO_MASK        (1U << 16)

static volatile uint32_t *const jtag_reg = (volatile uint32_t *)XMC_JTAG_REG_BASE;

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