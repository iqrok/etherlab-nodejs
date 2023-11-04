#include <etherlab-helper.h>

namespace EcatHelper::Domain {

int8_t write_bit(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint8_t& value)
{
	ecat_value_al val = { .u8 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint8_t& value)
{
	ecat_value_al val = { .u8 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int8_t& value)
{
	ecat_value_al val = { .i8 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_u16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint16_t& value)
{
	ecat_value_al val = { .u16 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_i16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int16_t& value)
{
	ecat_value_al val = { .i16 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_u32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const uint32_t& value)
{
	ecat_value_al val = { .u32 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

int8_t write_i32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex, const int32_t& value)
{
	ecat_value_al val = { .i32 = value };
	return domain_write(s_position, s_index, s_subindex, val);
}

uint8_t read_bit(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.u8 & 0x01;
}

uint8_t read_u8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.u8;
}

int8_t read_i8(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.i8;
}

uint16_t read_u16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.u16;
}

int16_t read_i16(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.i16;
}

uint32_t read_u32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.u32;
}

int32_t read_i32(const ecat_pos_al& s_position, const ecat_index_al& s_index,
	const ecat_sub_al& s_subindex)
{
	ecat_value_al val;
	domain_read(s_position, s_index, s_subindex, &val);

	return val.i32;
}

}
