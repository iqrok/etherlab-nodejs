#include <etherlab-helper.h>
#include <map>
#include <stdexcept>

namespace EcatHelper::SDO {

std::string read_string(const ecat_pos_al& s_position,
	const ecat_index_al& s_index, const ecat_sub_al& s_subindex,
	const uint16_t& size)
{
	// init with 0, so the string will be converted correctly
	ecat_value_al value = { 0 };
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return std::string(value.string);
}

uint8_t read_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(uint8_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u8;
}

int8_t read_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(int8_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u8;
}

uint16_t read_u16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(uint16_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u16;
}

int16_t read_i16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(int16_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u16;
}

uint32_t read_u32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(uint32_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u32;
}

int32_t read_i32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(int32_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u32;
}

uint64_t read_u64(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(uint64_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.u64;
}

int64_t read_i64(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al value;
	size_t size = sizeof(int64_t);
	size_t result_size;

	sdo_upload(
		s_position, s_index, s_subindex, size, &result_size, value.bytes);

	return value.i64;
}

int32_t write_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint8_t& value)
{
	ecat_value_al data = { .u8 = value };
	size_t size = sizeof(uint8_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int8_t& value)
{
	ecat_value_al data = { .i8 = value };
	size_t size = sizeof(int8_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_u16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint16_t& value)
{
	ecat_value_al data = { .u16 = value };
	size_t size = sizeof(uint16_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_i16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int16_t& value)
{
	ecat_value_al data = { .i16 = value };
	size_t size = sizeof(int16_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_u32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint16_t& value)
{
	ecat_value_al data = { .u32 = value };
	size_t size = sizeof(uint32_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_i32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int16_t& value)
{
	ecat_value_al data = { .i32 = value };
	size_t size = sizeof(int32_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_u64(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint64_t& value)
{
	ecat_value_al data = { .u64 = value };
	size_t size = sizeof(uint64_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

int32_t write_i64(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int64_t& value)
{
	ecat_value_al data = { .i64 = value };
	size_t size = sizeof(int64_t);

	return sdo_download(s_position, s_index, s_subindex, size, data.bytes);
}

}
