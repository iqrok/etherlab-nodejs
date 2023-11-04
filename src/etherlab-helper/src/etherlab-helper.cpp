#include <sched.h> /* sched_setscheduler() */
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime> /* clock_gettime() */
#include <filesystem>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

#include <TimespecHelper.hpp>

#include "config-parser.h"
#include "etherlab-helper.h"

/****************************************************************************/
/* The maximum stack size which is
  guranteed safe to access without
  faulting */
#define MAX_SAFE_STACK (8 * 1024)

/** Task period in ns. **/
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000
#endif

namespace EcatHelper {

namespace fs = std::filesystem;

/****************************************************************************/

struct op_status_s {
	bool master;
	bool slaves;
};

/****************************************************************************/

// EtherCAT
static ec_master_state_t master_state = {};
static ec_master_t* master = nullptr;

static ec_domain_t* DomainN = nullptr;
static ec_domain_state_t DomainN_state = {};
static ec_pdo_entry_reg_t* DomainN_regs = nullptr;
static ecat_size_io_al DomainN_length;

// process data
static uint8_t* DomainN_pd = nullptr;

static uint32_t counter = 0;
static bool is_master_ready = false;
static struct op_status_s is_operational = { false, false };

// slave configurations
static std::vector<ecat_slave_config_al> slaves;
static ecat_size_slave_al slaves_length = 0;

static ecat_entries_al slave_entries;
static ecat_size_slave_al slave_entries_length = 0;

static std::vector<ecat_startup_config_al> startup_parameters;
static ecat_size_param_al startup_parameters_length = 0;

static ecat_entries_al IOs;

// SM startup config
inline static uint32_t convert_index_sub_size(const ecat_index_al& index,
	const ecat_sub_al& subindex, const ecat_size_al& size);

// mapped domain
static ecat_domain_map_al mapped_domains;
inline static uint32_t convert_pos_index_sub(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex);
static ecat_size_io_al get_domain_index(ecat_size_io_al* dmn_idx,
	const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex);
static void assign_domain_identifier();

// Periodic task timing
static uint16_t frequency = 1000;
static uint32_t period_ns = NSEC_PER_SEC / frequency;

// configuration
static std::string json_path;

// SDO error message
static std::map<uint32_t, std::string> sdo_abort_message = {
	{ 0x05030000, "Toggle bit not changed" },
	{ 0x05040000, "SDO protocol timeout" },
	{ 0x05040001, "Client/Server command specifier not valid or unknown" },
	{ 0x05040005, "Out of memory" },
	{ 0x06010000, "Unsupported access to an object" },
	{ 0x06010001, "Attempt to read a write-only object" },
	{ 0x06010002, "Attempt to write a read-only object" },
	{ 0x06020000, "This object does not exist in the object directory" },
	{ 0x06040041, "The object cannot be mapped into the PDO" },
	{ 0x06040042,
		"The number and length of the objects to be mapped would"
		" exceed the PDO length" },
	{ 0x06040043, "General parameter incompatibility reason" },
	{ 0x06040047, "Gerneral internal incompatibility in device" },
	{ 0x06060000, "Access failure due to a hardware error" },
	{ 0x06070010,
		"Data type does not match, length of service parameter does"
		" not match" },
	{ 0x06070012,
		"Data type does not match, length of service parameter too"
		" high" },
	{ 0x06070013,
		"Data type does not match, length of service parameter too"
		" low" },
	{ 0x06090011, "Subindex does not exist" },
	{ 0x06090030, "Value range of parameter exceeded" },
	{ 0x06090031, "Value of parameter written too high" },
	{ 0x06090032, "Value of parameter written too low" },
	{ 0x06090036, "Maximum value is less than minimum value" },
	{ 0x08000000, "General error" },
	{ 0x08000020, "Data cannot be transferred or stored to the application" },
	{ 0x08000021,
		"Data cannot be transferred or stored to the application"
		" because of local control" },
	{ 0x08000022,
		"Data cannot be transferred or stored to the application"
		" because of the present device state" },
	{ 0x08000023,
		"Object dictionary dynamic generation fails or no object"
		" dictionary is present" }
};

#if VERBOSE > 0
struct timespec epoch;
#endif

/*****************************************************************************/

void delay_ns(uint64_t ns)
{
	struct timespec timer;

	clock_gettime(CLOCK_MONOTONIC, &timer);
	timer.tv_nsec += ns;
	Timespec::normalize_upper(&timer);
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &timer, NULL);
}

int8_t set_json_path(const std::string& filepath)
{
	if (!fs::exists(filepath)) {
		return -1;
	}

	json_path = filepath;
	return 0;
}

void check_domain_state()
{
	ec_domain_state_t ds;
	ecrt_domain_state(DomainN, &ds);

#if VERBOSE > 1
	if (ds.working_counter != DomainN_state.working_counter) {
		timespec_get(&epoch, TIME_UTC);
		printf("%ld.%09ld | Domain1: WC %u.\n", epoch.tv_sec, epoch.tv_nsec,
			ds.working_counter);
	}

	if (ds.wc_state != DomainN_state.wc_state) {
		timespec_get(&epoch, TIME_UTC);
		printf("%ld.%09ld | Domain1: State %u.\n", epoch.tv_sec, epoch.tv_nsec,
			ds.wc_state);
	}
#endif

	DomainN_state = ds;
}

void check_master_state(ec_master_t* master)
{
	ec_master_state_t ms;
	ecrt_master_state(master, &ms);

#if VERBOSE > 1
	if (ms.slaves_responding != master_state.slaves_responding) {
		timespec_get(&epoch, TIME_UTC);
		printf("%ld.%09ld | %u slave(s).\n", epoch.tv_sec, epoch.tv_nsec,
			ms.slaves_responding);
	}

	if (ms.al_states != master_state.al_states) {
		timespec_get(&epoch, TIME_UTC);
		printf("%ld.%09ld | AL states: 0x%02X.\n", epoch.tv_sec, epoch.tv_nsec,
			ms.al_states);
	}

	if (ms.link_up != master_state.link_up) {
		timespec_get(&epoch, TIME_UTC);
		printf("%ld.%09ld | Link is %s.\n", epoch.tv_sec, epoch.tv_nsec,
			ms.link_up ? "up" : "down");
	}
#endif

	master_state = ms;
	is_operational.master = (master_state.al_states & EC_AL_STATE_OP) != 0;
}

void check_slave_config_states()
{
	uint8_t sop_state = 1;
	for (ecat_size_slave_al slave_idx = 0; slave_idx < slaves_length;
		 slave_idx++) {
		ec_slave_config_state_t slave_state;

		ecrt_slave_config_state(slaves.at(slave_idx).sc, &slave_state);
		sop_state &= slave_state.operational;

#if VERBOSE > 1
		if (s.al_state != slaves.at(slave_idx).state.al_state) {
			timespec_get(&epoch, TIME_UTC);
			printf("%ld.%09ld | Slaves %d : State 0x%02X.\n", epoch.tv_sec,
				epoch.tv_nsec, slave_idx, s.al_state);
		}

		if (s.online != slaves.at(slave_idx).state.online) {
			timespec_get(&epoch, TIME_UTC);
			printf("%ld.%09ld | Slaves %d : %s.\n", epoch.tv_sec, epoch.tv_nsec,
				slave_idx, s.online ? "online" : "offline");
		}

		if (s.operational != slaves.at(slave_idx).state.operational) {
			timespec_get(&epoch, TIME_UTC);
			printf("%ld.%09ld | Slaves %d : %soperational.\n", epoch.tv_sec,
				epoch.tv_nsec, slave_idx, s.operational ? "" : "Not ");
		}
#endif

		slaves.at(slave_idx).state = slave_state;
	}

	is_operational.slaves = sop_state;
}

int8_t write_output_value(
	const ecat_size_io_al& dmn_idx, const ecat_value_al& value)
{
	// selected index is not output, return immediately
	if (IOs[dmn_idx].direction != EC_DIR_OUTPUT) {
		return -1;
	}

	switch (IOs[dmn_idx].size) {
	case 1: {
		EC_WRITE_BIT(DomainN_pd + IOs[dmn_idx].offset,
			IOs[dmn_idx].bit_position, value.u8 & 0x1);
		return 1;
	} break;

	case 8: {
		EC_WRITE_U8(DomainN_pd + IOs[dmn_idx].offset, value.u8);
	} break;

	case 16: {
		EC_WRITE_U16(DomainN_pd + IOs[dmn_idx].offset, value.u16);
	} break;

	case 32: {
		EC_WRITE_U32(DomainN_pd + IOs[dmn_idx].offset, value.u32);
	} break;

	case 64: {
		EC_WRITE_U64(DomainN_pd + IOs[dmn_idx].offset, value.u64);
	} break;

	default: {
		return 0;
	} break;
	}

	return 1;
}

void main_routine()
{
	// receive process data
	ecrt_master_receive(master);
	ecrt_domain_process(DomainN);

	// check process data state
	check_domain_state();

	// update states every 1s
	if (!counter--) {
		// reset counter
		counter = frequency;

		// update master state
		check_master_state(master);

		// update slave configuration state(s)
		check_slave_config_states();
	}

	// do nothing if master is not ready
	if (master_state.al_states & EC_AL_STATE_OP) {

		for (ecat_size_io_al dmn_idx = 0; dmn_idx < DomainN_length; dmn_idx++) {
			if (IOs[dmn_idx].direction == EC_DIR_OUTPUT) {
				write_output_value(dmn_idx, IOs[dmn_idx].written_value);
			}

			switch (IOs[dmn_idx].size) {

			// No endian difference for 1 bit variable
			case 1: {
				IOs[dmn_idx].value.u8
					= EC_READ_BIT(DomainN_pd + IOs[dmn_idx].offset,
						  IOs[dmn_idx].bit_position)
					& 0x1;
			} break;

			// No endian difference for 1 byte variable
			case 8: {
				IOs[dmn_idx].value.u8
					= EC_READ_U8(DomainN_pd + IOs[dmn_idx].offset);
			} break;

			case 16: {
				uint16_t tmp16 = EC_READ_U16(DomainN_pd + IOs[dmn_idx].offset);
				IOs[dmn_idx].value.u16
					= IOs[dmn_idx].swap_endian ? swap_endian16(tmp16) : tmp16;
			} break;

			case 32: {
				uint32_t tmp32 = EC_READ_U32(DomainN_pd + IOs[dmn_idx].offset);
				IOs[dmn_idx].value.u32
					= IOs[dmn_idx].swap_endian ? swap_endian32(tmp32) : tmp32;
			} break;

			default: {
				uint64_t tmp64 = EC_READ_U64(DomainN_pd + IOs[dmn_idx].offset);
				IOs[dmn_idx].value.u64
					= IOs[dmn_idx].swap_endian ? swap_endian32(tmp64) : tmp64;
			} break;
			}

#if VERBOSE > 2
			printf("Index %2d pos %d 0x%04x:%02x offset %d = %8x\n", dmn_idx,
				IOs[dmn_idx].position, IOs[dmn_idx].index,
				IOs[dmn_idx].subindex, IOs[dmn_idx].offset, IOs[dmn_idx].value);
#endif
		};

#if VERBOSE > 2
		printf("=====================\n");
#endif
	}

	ecrt_domain_queue(DomainN);
	ecrt_master_send(master);
}

void domain_startup_config(
	ec_pdo_entry_reg_t** DomainN_regs, ecat_size_io_al* dmn_size)
{
#if VERBOSE > 0
	fprintf(stdout, "\nConfiguring Domains...\n");
#endif

	ecat_size_slave_al length = slave_entries_length;
	ecat_size_slave_al entry_idx;
	ecat_size_io_al dmn_idx;

	// find length of valid domain inside slave_entries
	*dmn_size = 0;
	for (entry_idx = 0; entry_idx < length; entry_idx++) {
		// prevent bit-padding from being added into domains
		if (slave_entries[entry_idx].index == 0x0000) {
			slave_entries[entry_idx].add_to_domain = 0;
			continue;
		}

		if (slave_entries[entry_idx].add_to_domain) {
			(*dmn_size)++;
		}
	}

	// new size to be allocated into DomainN_regs
	ecat_size_io_al alloc_size = ((*dmn_size) + 1) * sizeof(ec_pdo_entry_reg_t);

	// allocate domain_regs
	*DomainN_regs = (ec_pdo_entry_reg_t*)malloc(alloc_size);

	// reserve vector memory allocation,
	// in order to avoid pointer address change everytime we push_back new value
	IOs.reserve(length);

	// add every valid slave process data into domain
	for (entry_idx = 0, dmn_idx = 0; entry_idx < length; entry_idx++) {
		if (slave_entries[entry_idx].add_to_domain) {
			// create IOs domain to access domain value
			IOs.push_back(slave_entries[entry_idx]);

			// register domain with IOs
			(*DomainN_regs)[dmn_idx] = {
				.alias = IOs[dmn_idx].alias,
				.position = IOs[dmn_idx].position,
				.vendor_id = IOs[dmn_idx].vendor_id,
				.product_code = IOs[dmn_idx].product_code,
				.index = IOs[dmn_idx].index,
				.subindex = IOs[dmn_idx].subindex,
				.offset = &IOs[dmn_idx].offset,
				.bit_position = &IOs[dmn_idx].bit_position,
			};

			dmn_idx++;
		}
	}

	// terminate with an empty structure
	(*DomainN_regs)[(*dmn_size)] = {};
}

uint32_t convert_index_sub_size(const ecat_index_al& index,
	const ecat_sub_al& subindex, const ecat_size_al& size)
{
	return ((index & 0xffff) << 16) | ((subindex & 0xff) << 8) | (size & 0xff);
}

void syncmanager_startup_config()
{
#if VERBOSE > 0
	fprintf(stdout, "\nConfiguring SyncManager and Mapping PDOs...\n");
#endif

	ecat_size_slave_al entry_size = slave_entries_length;

	ecat_pos_al last_position = -1;
	ecat_index_al last_pdo_index = -1;
	uint32_t last_index_sub_size = -1;

	uint8_t syncM_index = 0, last_syncM_index = -1;
	ec_direction_t direction;
	ec_watchdog_mode_t watchdog_mode;

	ecat_pos_al current_position = -1;
	ecat_index_al current_pdo_index = -1;
	ecat_index_al current_index = -1;
	ecat_sub_al current_subindex = -1;

	uint32_t processed_index_sub_size = -1;

	// add every valid slave and configure sync manager
	for (ecat_size_slave_al entry_idx = 0; entry_idx < entry_size;
		 entry_idx++) {
		processed_index_sub_size = convert_index_sub_size(
			slave_entries[entry_idx].index, slave_entries[entry_idx].subindex,
			slave_entries[entry_idx].size);

		syncM_index = slave_entries[entry_idx].sync_index;

		// invalid pdo index use default configuration, continue to next index
		if (last_position != slave_entries[entry_idx].position
			&& slave_entries[entry_idx].pdo_index) {

			// reset last SM index when encountering new slave
			last_syncM_index = -1;

			direction = static_cast<ec_direction_t>(
				slave_entries[entry_idx].direction);
			watchdog_mode = slave_entries[entry_idx].watchog_enabled
				? EC_WD_ENABLE
				: EC_WD_DISABLE;
			current_position = slave_entries[entry_idx].position;

#if VERBOSE > 0
			printf("\n# [%d] %s\n", slave_entries[entry_idx].position,
				slaves.at(current_position).info.name);
#endif

			if (ecrt_slave_config_sync_manager(slaves.at(current_position).sc,
					syncM_index, direction, watchdog_mode)) {

				fprintf(stderr, "Failed to configure SM. Slave %2d SM%d\n",
					current_position, syncM_index);
				exit(EXIT_FAILURE);
			}

			last_position = current_position;
		}

		if (last_syncM_index != syncM_index
			&& slave_entries[entry_idx].pdo_index) {

			ecrt_slave_config_pdo_assign_clear(
				slaves.at(current_position).sc, syncM_index);

			last_syncM_index = syncM_index;

#if VERBOSE > 0
			printf("  > SyncManager %d\n", syncM_index);
#endif
		}

		// invalid pdo index use default configuration, continue to next index
		if (last_pdo_index != slave_entries[entry_idx].pdo_index
			&& slave_entries[entry_idx].pdo_index) {

			current_pdo_index = slave_entries[entry_idx].pdo_index;

#if VERBOSE > 0
			printf("    + PDO 0x%04x\n", current_pdo_index);
#endif

			if (ecrt_slave_config_pdo_assign_add(slaves.at(current_position).sc,
					syncM_index, current_pdo_index)) {

				fprintf(stderr,
					"Failed to configure PDO assign. Slave %2d SM%d 0x%04x\n",
					current_position, syncM_index, current_pdo_index);

				exit(EXIT_FAILURE);
			}

			ecrt_slave_config_pdo_mapping_clear(
				slaves.at(current_position).sc, current_pdo_index);

			last_pdo_index = current_pdo_index;
		}

		// invalid pdo index use default configuration, continue to next index
		if (last_index_sub_size != processed_index_sub_size
			&& processed_index_sub_size) {
			current_index = slave_entries[entry_idx].index;
			current_subindex = slave_entries[entry_idx].subindex;

#if VERBOSE > 0
			printf("      - entry 0x%04x:%02x %2d-bit(s)\n", current_index,
				current_subindex, slave_entries[entry_idx].size);
#endif

			int8_t mapping = ecrt_slave_config_pdo_mapping_add(
				slaves.at(current_position).sc, current_pdo_index,
				current_index, current_subindex, slave_entries[entry_idx].size);

			if (mapping) {
				fprintf(stderr,
					"Failed to add PDO mapping. Slave %2d SM%d 0x%4x "
					"0x%04x:0x%02x %2d\n",
					current_position, syncM_index, current_pdo_index,
					current_index, current_subindex,
					slave_entries[entry_idx].size);

				exit(EXIT_FAILURE);
			}

			last_index_sub_size
				= convert_index_sub_size(slave_entries[entry_idx].index,
					slave_entries[entry_idx].subindex,
					slave_entries[entry_idx].size);
		}
	}
}

uint8_t skip_current_slave_position(
	const ecat_pos_al& position, const std::vector<ecat_pos_al>& last_positions)
{
	ecat_size_slave_al entry_idx;
	size_t size = last_positions.size();

	for (entry_idx = 0; entry_idx < size; entry_idx++) {
		if (position == last_positions[entry_idx]) {
			return 1;
		}
	}

	return 0;
}

void slave_startup_config(ec_master_t* master)
{
#if VERBOSE > 0
	fprintf(stdout, "\nConfiguring Slaves...\n");
#endif

	ecat_size_slave_al entry_size = slave_entries_length;
	std::vector<ecat_pos_al> last_positions;

	ecat_size_slave_al entry_idx = 0;
	for (entry_idx = 0; entry_idx < entry_size; entry_idx++) {
		// skip current slave, if it's already configured
		if (skip_current_slave_position(
				slave_entries[entry_idx].position, last_positions)) {

			continue;
		}

		ec_slave_info_t slave_info;
		if (ecrt_master_get_slave(
				master, slave_entries[entry_idx].position, &slave_info)) {

			fprintf(stderr, "Failed to get Slave (%d) info!\n", entry_idx);
			exit(EXIT_FAILURE);
		}

		// current slave
		ecat_slave_config_al current = { .info = slave_info };

#if VERBOSE > 0
		printf("Slave %2d: 0x%08x 0x%08x %s\n", current.info.position,
			current.info.vendor_id, current.info.product_code,
			current.info.name);
#endif

		// configure slave
		current.sc = ecrt_master_slave_config(master, current.info.alias,
			current.info.position, current.info.vendor_id,
			current.info.product_code);

		// save current slave information in global slaves
		slaves.push_back(current);

		// update number of slaves
		slaves_length++;

		// save last slave's position
		last_positions.push_back(current.info.position);
	}
}

void startup_parameters_config()
{
#if VERBOSE > 0
	fprintf(stdout, "\nConfiguring Startup Parameters...\n");
#endif

	static ecat_size_param_al length = startup_parameters_length;
	for (ecat_size_param_al par_idx = 0; par_idx < length; par_idx++) {
		switch (startup_parameters[par_idx].size) {
		case 8: {
			ecrt_slave_config_sdo8(
				slaves.at(startup_parameters[par_idx].slavePosition).sc,
				startup_parameters[par_idx].index,
				startup_parameters[par_idx].subindex,
				startup_parameters[par_idx].value.u8);
		} break;

		case 16: {
			ecrt_slave_config_sdo16(
				slaves.at(startup_parameters[par_idx].slavePosition).sc,
				startup_parameters[par_idx].index,
				startup_parameters[par_idx].subindex,
				startup_parameters[par_idx].value.u16);
		} break;

		default: {
			ecrt_slave_config_sdo32(
				slaves.at(startup_parameters[par_idx].slavePosition).sc,
				startup_parameters[par_idx].index,
				startup_parameters[par_idx].subindex,
				startup_parameters[par_idx].value.u32);
		} break;
		}

#if VERBOSE > 0
		printf("Set Startup Parameter Slave %2d, 0x%04x:%02x = 0x%x (%d)\n",
			startup_parameters[par_idx].slavePosition,
			startup_parameters[par_idx].index,
			startup_parameters[par_idx].subindex,
			startup_parameters[par_idx].value.u32,
			startup_parameters[par_idx].value.u32);
#endif
	}
}

void reset_global_vars()
{
	IOs.clear();
	DomainN_length = 0;

	slaves.clear();
	slaves_length = 0;

	slave_entries_length = 0;
	startup_parameters_length = 0;

	is_master_ready = false;
	counter = 0;
}

/****************************************************************************/

void stack_prefault()
{
	unsigned char dummy[MAX_SAFE_STACK];
	memset(dummy, 0, MAX_SAFE_STACK);
}

void assign_domain_identifier()
{
#if VERBOSE > 0
	printf("\nAssigning Domain identifier...\n");
#endif

	for (ecat_size_io_al dmn_idx = 0; dmn_idx < DomainN_length; dmn_idx++) {
		uint32_t identifier = convert_pos_index_sub(
			IOs[dmn_idx].position, IOs[dmn_idx].index, IOs[dmn_idx].subindex);

		mapped_domains[identifier] = dmn_idx;
	}
}

uint32_t convert_pos_index_sub(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex)
{
	return (s_position << 24) | (s_index << 8) | (s_subindex << 0);
}

ecat_size_io_al get_domain_index(ecat_size_io_al* dmn_idx,
	const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	uint32_t key = convert_pos_index_sub(s_position, s_index, s_subindex);

	try {
		*dmn_idx = mapped_domains.at(key);
		return 0;
	} catch (const std::out_of_range& err) {
		fprintf(stderr, "Error %s: Index not found for pos %2d 0x%04x:%02x\n",
			err.what(), s_position, s_index, s_subindex);

		return -1;
	}
}

int8_t init_slaves()
{
#if VERBOSE > 0
	fprintf(stderr, "\nInitialize slaves from JSON '%s' %d\n",
		json_path.c_str(), VERBOSE);
#endif

	int8_t retval;
	std::string contents;

	if ((retval = ConfigParser::get_file_contents(json_path, &contents))) {
		return retval;
	}

	return ConfigParser::parse(&contents[0], &slave_entries,
		&slave_entries_length, &startup_parameters, &startup_parameters_length);
}

void init_master_and_domain()
{
	if (is_master_ready) {
#if VERBOSE > 0
		fprintf(stderr, "\nMaster is already initialized!\n");
#endif
		return;
	}

#if VERBOSE > 0
	fprintf(stdout, "\nInitializing Master and Domains\n");
#endif

	DomainN_regs = nullptr;
	master = nullptr;
	DomainN_length = 0;

	if (slave_entries_length == 0) {
		if (init_slaves() != 0) {
			fprintf(stderr, "Slave(s) must be configured first!\n");
			exit(EXIT_FAILURE);
		}
	}

#if VERBOSE > 0
	fprintf(stdout, "\nRequesting ethercat master\n");
#endif
	// request ethercat master
	master = ecrt_request_master(0);
	if (!master) {
		fprintf(stderr, "Failed at requesting master!\n");
		exit(EXIT_FAILURE);
	}

	// Configure Slaves at startup
	slave_startup_config(master);

	// Configure PDO at startup
	syncmanager_startup_config();

	// Startup parameters
	startup_parameters_config();

	// Configuring Domain
	domain_startup_config(&DomainN_regs, &DomainN_length);

	// Create a new process data domain
	if (!(DomainN = ecrt_master_create_domain(master))) {
		fprintf(stderr, "Domain Creation failed!\n");
		exit(EXIT_FAILURE);
	}

	if (ecrt_domain_reg_pdo_entry_list(DomainN, DomainN_regs)) {
		fprintf(stderr, "PDO entry registration failed!\n");
		exit(EXIT_FAILURE);
	}

#if VERBOSE > 0
	for (ecat_size_io_al dmn_idx = 0; dmn_idx < DomainN_length; dmn_idx++) {
		printf("  > Domain %3d: Slave %2d 0x%04x:%02x offset %3d, bitpos %2d, "
			   "%s\n",
			dmn_idx, IOs[dmn_idx].position, IOs[dmn_idx].index,
			IOs[dmn_idx].subindex, IOs[dmn_idx].offset,
			IOs[dmn_idx].bit_position,
			IOs[dmn_idx].direction % 2 ? "OUT" : "IN");
	}
#endif

	// free allocated memories from startup configurations
	free(DomainN_regs);
	slave_entries.clear();
	startup_parameters.clear();

	// map domain indexes
	assign_domain_identifier();

#if VERBOSE > 0
	fprintf(stdout, "\nMaster & Domain have been initialized.\n");
	fprintf(stdout, "Number of Slaves : %3ld\n", slaves.size());
	fprintf(stdout, "Number of Domains: %3d\n", DomainN_length);
#endif

	is_master_ready = true;
}

void init()
{
	init_slaves();
	init_master_and_domain();
}

void activate_master()
{

#if VERBOSE > 0
	fprintf(stdout, "\nActivating master...\n");
#endif
	if (ecrt_master_activate(master)) {
		fprintf(stderr, "Master Activation failed!\n");
		exit(EXIT_FAILURE);
	}

#if VERBOSE > 0
	fprintf(stdout, "\nInitializing Domain data...\n");
#endif
	if (!(DomainN_pd = ecrt_domain_data(DomainN))) {
		fprintf(stderr, "Domain data initialization failed!\n");
		exit(EXIT_FAILURE);
	}
}

void set_frequency(uint32_t hz)
{
	frequency = hz;
	period_ns = NSEC_PER_SEC / hz;
}

void set_period(uint32_t ns)
{
	period_ns = ns;
	frequency = NSEC_PER_SEC / ns;
}

void set_period_us(uint32_t us)
{
	set_period(us * 1'000);
}

void set_period_ms(uint32_t ms)
{
	set_period(ms * 1'000'000);
}

uint16_t get_frequency()
{
	return frequency;
}

uint32_t get_period()
{
	return period_ns;
}

void attach_process_data(ecat_entries_al** ptr)
{
	*ptr = &IOs;
}

void attach_mapped_domain(ecat_domain_map_al** ptr)
{
	*ptr = &mapped_domains;
}

void prerun_routine()
{
	if (!is_master_ready) init_master_and_domain();

	// activate master and initialize domain data
	activate_master();
}

void postrun_routine()
{
	ecrt_master_deactivate(master);

#if VERBOSE > 0
	timespec_get(&epoch, TIME_UTC);
	fprintf(stdout, "%ld.%09ld | Waiting for master deactivation...\n",
		epoch.tv_sec, epoch.tv_nsec);
#endif

	// wait until OP bit is reset after deactivation
	while (is_operational.master) {
		// update master state
		check_master_state(master);
		delay_ns(500'000);
	}

	reset_global_vars();
	ecrt_release_master(master);

#if VERBOSE > 0
	timespec_get(&epoch, TIME_UTC);
	fprintf(stdout, "%ld.%09ld | Stopped.\n", epoch.tv_sec, epoch.tv_nsec);
#endif
}

bool operational_status()
{
	return is_operational.slaves && is_operational.master;
}

uint8_t application_layer_states()
{
	return master_state.al_states;
}

int8_t domain_write(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const ecat_value_al& value)
{
	if (!(master_state.al_states & EC_AL_STATE_OP)) {
		fprintf(stderr, "Master is not OP!\n");
		return -1;
	}

	ecat_size_io_al dmn_idx;
	if (get_domain_index(&dmn_idx, s_position, s_index, s_subindex) < 0) {
		return -1;
	}

	IOs[dmn_idx].written_value = value;

	return 0;
}

int8_t domain_read(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, ecat_value_al* value)
{
	if (!(master_state.al_states & EC_AL_STATE_OP)) {
		fprintf(stderr, "Master is not OP!\n");
		return -1;
	}

	ecat_size_io_al dmn_idx;
	if (get_domain_index(&dmn_idx, s_position, s_index, s_subindex) < 0) {
		return -1;
	}

	*value = IOs[dmn_idx].value;

	return 0;
}

void sdo_print_abort_message(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
	const int32_t& retval, const uint32_t& code)
{
	try {
		fprintf(stderr, "Error Slave %d 0x%04x:%02x: 0x%08x - %s\n", s_position,
			s_index, s_subindex, code, sdo_abort_message.at(code).c_str());
	} catch (const std::out_of_range& err) {
		fprintf(stderr, "Error Slave %d 0x%04x:%02x: 0x%08x - %d\n", s_position,
			s_index, s_subindex, code, retval);
	}
}

int32_t sdo_download(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
	const size_t& size, uint8_t* value)
{
	uint32_t abort_code;
	int32_t process = ecrt_master_sdo_download(
		master, s_position, s_index, s_subindex, value, size, &abort_code);

	if (process) {
		sdo_print_abort_message(
			s_position, s_index, s_subindex, process, abort_code);
	}

	return process;
}

int32_t sdo_upload(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const size_t& size, size_t* result_size,
	uint8_t* value)
{
	uint32_t abort_code;
	int32_t process = ecrt_master_sdo_upload(master, s_position, s_index,
		s_subindex, value, size, result_size, &abort_code);

	if (process) {
		sdo_print_abort_message(
			s_position, s_index, s_subindex, process, abort_code);
	}

	return process;
}

int8_t sdo_request(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint8_t& size, void* value,
	const sdo_req_type_al& rtype, const uint32_t& timeout)
{
	struct timespec start, current;
	int64_t elapsed_ns;
	int64_t timeout_ns = timeout * 1'000'000;

	ec_slave_config_t* slave;
	ec_slave_config_state_t* state;

	// SDO Request
	ec_sdo_request_t* sdo_req;

	try {
		slave = slaves.at(s_position).sc;
		state = &slaves.at(s_position).state;

		ecrt_slave_config_state(slave, state);
	} catch (const std::out_of_range& err) {
		fprintf(stderr, "Slave pos %d doesn't exist! (max %ld)\n", s_position,
			slaves.size());

		return SDO::ECAT_SDO_REQ_ERR_SLAVE_NOT_FOUND;
	}

#if VERBOSE > 1
	fprintf(stdout,
		"%d Slave %d 0x%04x:%02x - Online %02x | OP %02x | State %02x\n", rtype,
		s_position, s_index, s_subindex, state->online, state->operational,
		state->al_state);
#endif

	// TODO: Find how Busy Request never ends once it happens.
	// this condition is to prevent infinite Busy Request, but tbh I'm not
	// sure why it happens. so for now, I simply prevent creating request
	// when slave is in INIT state
	if (state->al_state == 0x01) {
		fprintf(stderr, "Slave %d 0x%04x:%02x is in INIT state! (%02x)\n",
			s_position, s_index, s_subindex, state->al_state);

		return SDO::ECAT_SDO_REQ_FAILED;
	}

	if (!(sdo_req = ecrt_slave_config_create_sdo_request(
			  slave, s_index, s_subindex, size))) {

		fprintf(stderr, "Failed to create SDO request!\n");
		return -1;
	}

	ecrt_sdo_request_timeout(sdo_req, timeout);

	if (rtype == ECAT_SDO_READ) {
		ecrt_sdo_request_read(sdo_req);
	} else {
		// data should be set before requesting sdo write
		memcpy(ecrt_sdo_request_data(sdo_req), value, size);
		ecrt_sdo_request_write(sdo_req);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	for (;;) {
		switch (ecrt_sdo_request_state(sdo_req)) {

		case EC_REQUEST_UNUSED: {
			fprintf(stderr, "Unused request!\n");
			// request was not used yet, trigger request
			if (rtype == ECAT_SDO_READ) {
				ecrt_sdo_request_read(sdo_req);
			} else {
				ecrt_sdo_request_write(sdo_req);
			}
		} break;

		case EC_REQUEST_BUSY: {
			// there's possibility the loop stuck in busy state
			// limit the loop with the timeout
			clock_gettime(CLOCK_MONOTONIC, &current);
			Timespec::diff(current, start, &elapsed_ns);

			if (elapsed_ns > timeout_ns) {
				fprintf(stderr, "Timeout waiting for Busy Request!\n");
				return SDO::ECAT_SDO_REQ_ERR_BUSY;
			}
		} break;

		case EC_REQUEST_SUCCESS: {
			if (rtype == ECAT_SDO_READ) {
				memcpy(value, ecrt_sdo_request_data(sdo_req), size);
			}
			return SDO::ECAT_SDO_REQ_SUCCESS;
		} break;

		case EC_REQUEST_ERROR: {
			return SDO::ECAT_SDO_REQ_ERR;
		} break;
		}
	}
}

}
