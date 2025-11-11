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

static int ptc_dummy_write(int tck, int tms, int tdi)
{
	return ERROR_OK;
}

static bb_value_t ptc_dummy_read(void)
{
	return BB_LOW;
}

__attribute__((unused))
static struct bitbang_interface ptc_bitbang = {
	.write = ptc_dummy_write,
	.read  = ptc_dummy_read,
};

static int ptc_dummy_execute_queue(void)

{
	return bitbang_execute_queue();
}

static struct jtag_interface ptc_dummy_jtag_interface = {
	.supported     = DEBUG_CAP_TMS_SEQ,
	.execute_queue = ptc_dummy_execute_queue,
};


static int ptc_dummy_init(void)
{
	fprintf(stderr, "PTC_DUMMY: init() starting\n");


	if (!bitbang_interface)
		bitbang_interface = calloc(1, sizeof(*bitbang_interface));
	
	if (!bitbang_interface) {
		LOG_ERROR("PTC_DUMMY: failed to allocate bitbang_interface");
		return ERROR_FAIL;
	}

	if (!bitbang_interface)
		bitbang_interface = calloc(1, sizeof(struct bitbang_interface));

	bitbang_interface->write = ptc_dummy_write;
	bitbang_interface->read  = ptc_dummy_read;

	fprintf(stderr, "PTC_DUMMY: Finished ptc_init");

	LOG_INFO("PTC dummy bitbang adapter initialised");
	return ERROR_OK;
}


__attribute__((weak)) struct bitbang_interface *bitbang_interface;

/* -------------------------------------------------------------------------- */

__attribute__((unused))
static const char * const ptc_dummy_transports[] = { "jtag", NULL };

struct adapter_driver ptc_dummy_adapter_driver = {
	.name     = "ptc_dummy",
	.transports = ptc_dummy_transports,
	.init     = ptc_dummy_init,
	.jtag_ops = &ptc_dummy_jtag_interface,
};