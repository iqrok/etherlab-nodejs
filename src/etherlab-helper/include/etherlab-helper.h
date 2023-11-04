#ifndef _ETHERLAB_HELPER_H_
#define _ETHERLAB_HELPER_H_

#include <cstdint>
#include <map>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <ecrt.h>
}

#define swap_endian16(x)                                                       \
	((uint16_t)((((uint16_t)(x)&0x00ffU) << 8)                                 \
		| (((uint16_t)(x)&0xff00U) >> 8)))
#define swap_endian32(x)                                                       \
	((uint32_t)((((uint32_t)(x)&0x000000ffUL) << 24)                           \
		| (((uint32_t)(x)&0x0000ff00UL) << 8)                                  \
		| (((uint32_t)(x)&0x00ff0000UL) >> 8)                                  \
		| (((uint32_t)(x)&0xff000000UL) >> 24)))
#define swap_endian64(x)                                                       \
	((uint64_t)((((uint64_t)(x)&0x00000000000000ffULL) << 56)                  \
		| (((uint64_t)(x)&0x000000000000ff00ULL) << 40)                        \
		| (((uint64_t)(x)&0x0000000000ff0000ULL) << 24)                        \
		| (((uint64_t)(x)&0x00000000ff000000ULL) << 8)                         \
		| (((uint64_t)(x)&0x000000ff00000000ULL) >> 8)                         \
		| (((uint64_t)(x)&0x0000ff0000000000ULL) >> 24)                        \
		| (((uint64_t)(x)&0x00ff000000000000ULL) >> 40)                        \
		| (((uint64_t)(x)&0xff00000000000000ULL) >> 56)))

/*****************************************************************************/

namespace EcatHelper {

typedef int32_t ecat_size_io_al;
typedef uint16_t ecat_size_slave_al;
typedef uint16_t ecat_size_param_al;

typedef uint16_t ecat_pos_al;
typedef uint16_t ecat_index_al;
typedef uint8_t ecat_sub_al;
typedef uint8_t ecat_size_al;
typedef union unit_64b_u {
	uint8_t bytes[64];
	char string[64];
	uint8_t u8;
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	float f32;
	double f64;
} ecat_value_al;

typedef struct ecat_slave_config_s {
	ec_slave_info_t info;
	ec_slave_config_state_t state;
	ec_slave_config_t* sc;
} ecat_slave_config_al;

typedef struct ecat_startup_config_s {
	ecat_size_al size;
	ecat_pos_al slavePosition;
	ecat_index_al index;
	ecat_sub_al subindex;
	ecat_value_al value;
} ecat_startup_config_al;

typedef struct ecat_slave_entry_al_s {
	uint16_t alias; /**< Slave alias address. */
	ecat_pos_al position; /**< Slave position. */
	uint32_t vendor_id; /**< Slave vendor ID. */
	uint32_t product_code; /**< Slave product code. */

	uint8_t sync_index; /**< SM index. */
	ecat_index_al pdo_index; /**< PDO entry index. */

	ecat_index_al index = 0; /**< PDO entry index. */
	ecat_sub_al subindex = 0; /**< PDO entry subindex. */
	ecat_size_al size = 0;

	uint8_t add_to_domain = 0;

	uint32_t offset = 0;
	uint32_t bit_position = 0;

	ecat_value_al value;
	uint8_t direction;

	uint8_t swap_endian = 0;
	uint8_t is_signed = 0;

	ecat_value_al written_value;

	uint8_t watchog_enabled = 0;
} ecat_slave_entry_al;

typedef enum sdo_req_type_en {
	ECAT_SDO_READ = 0,
	ECAT_SDO_WRITE = 1
} sdo_req_type_al;

typedef std::vector<ecat_slave_entry_al> ecat_entries_al;
typedef std::map<uint32_t, ecat_size_io_al> ecat_domain_map_al;

/*****************************************************************************/

int8_t set_json_path(const std::string& filepath);

void set_period_ms(uint32_t milliseconds);
void set_period_us(uint32_t microseconds);
void set_period(uint32_t nanoseconds);
void set_frequency(uint32_t hertz);

uint16_t get_frequency();
uint32_t get_period();

void prerun_routine();
void main_routine();
void postrun_routine();

void init();
void start();
void stop();

bool operational_status();
uint8_t application_layer_states();

void attach_process_data(ecat_entries_al** ptr);
void attach_mapped_domain(ecat_domain_map_al** ptr);

int8_t domain_write(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const ecat_value_al& value);
int8_t domain_read(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, ecat_value_al* value);

int8_t sdo_request(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint8_t& size, void* result,
	const sdo_req_type_al& rtype, const uint32_t& timeout);
int32_t sdo_upload(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const size_t& size, size_t* result_size,
	uint8_t* value);
int32_t sdo_download(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
	const size_t& size, uint8_t* value);

namespace Domain {

	int8_t write_bit(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint8_t& value);

	int8_t write_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
		const ecat_sub_al& s_subindex, const uint8_t& value);

	int8_t write_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
		const ecat_sub_al& s_subindex, const int8_t& value);

	int8_t write_u16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint16_t& value);

	int8_t write_i16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const int16_t& value);

	int8_t write_u32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint32_t& value);

	int8_t write_i32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const int32_t& value);

	int8_t write_float(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const float& value);

	int8_t write_double(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const float& value);

	uint8_t read_bit(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	uint8_t read_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
		const ecat_sub_al& s_subindex);

	int8_t read_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
		const ecat_sub_al& s_subindex);

	uint16_t read_u16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	int16_t read_i16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	uint32_t read_u32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	int32_t read_i32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	float read_float(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	float read_double(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

}

namespace SDO {

	typedef enum sdo_req_retval_en {
		ECAT_SDO_REQ_SUCCESS = 0,
		ECAT_SDO_REQ_FAILED = -1,
		ECAT_SDO_REQ_ERR_SLAVE_NOT_FOUND = -2,
		ECAT_SDO_REQ_ERR_SLAVE_NOT_READY = -3,
		ECAT_SDO_REQ_ERR_BUSY = -4,
		ECAT_SDO_REQ_ERR = -5,
	} sdo_req_retval_al;

	int32_t write_u8(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint8_t& value);

	int32_t write_u16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint16_t& value);

	int32_t write_u32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint32_t& value);

	int32_t write_u64(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint64_t& value);

	uint8_t read_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
		const ecat_sub_al& s_subindex);

	uint16_t read_u16(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	uint32_t read_u32(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	uint64_t read_u64(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex);

	std::string read_string(const ecat_pos_al& s_position,
		const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
		const uint16_t& size);

}

}

#endif
