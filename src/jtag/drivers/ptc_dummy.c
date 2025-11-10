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

static int ptc_write(int tck, int tms, int tdi)
{
	return ERROR_OK;
}

static bb_value_t ptc_read(void)
{
	return BB_LOW;
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

static struct jtag_interface ptc_jtag_interface = {
	.supported     = DEBUG_CAP_TMS_SEQ,
	.execute_queue = ptc_execute_queue,
};


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

	fprintf(stderr, "PTC: Finished ptc_init");

	LOG_INFO("PTC bit‑bang adapter initialised");
	return ERROR_OK;
}


__attribute__((weak)) struct bitbang_interface *bitbang_interface;

/* -------------------------------------------------------------------------- */

struct adapter_driver ptc_dummy_adapter_driver = {
	.name     = "ptc",
	.init     = ptc_init,
	.jtag_ops = &ptc_jtag_interface,
};