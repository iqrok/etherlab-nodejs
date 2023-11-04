#include <algorithm>
#include <fstream>
#include <string>

#include <rapidjson/document.h>

#include "config-parser.h"

namespace ConfigParser {

std::string normalize_hex_string(const std::string& str);
uint32_t to_uint32(const rapidjson::Value&);
uint8_t member_is_valid_array(const rapidjson::Value&, const char*);

static const uint8_t SyncMEthercatDirection[] = {
	EC_DIR_OUTPUT, // SM0 EC_DIR_OUTPUT
	EC_DIR_INPUT, // SM1 EC_DIR_INPUT
	EC_DIR_OUTPUT, // SM2 EC_DIR_OUTPUT
	EC_DIR_INPUT // SM3 EC_DIR_INPUT
};

std::string normalize_hex_string(const std::string& str)
{
	std::string output;

	// avoids buffer reallocations in the loop
	output.reserve(str.size());

	size_t length = str.size();
	for (size_t i = 0; i < length; i++) {
		if (isxdigit(str[i])) {
			output += str[i];
		}
	}

	return output;
}

uint32_t to_uint32(const rapidjson::Value& val)
{
	if (val.IsString()) {
		return std::stoi(normalize_hex_string(val.GetString()), 0, 16);
	} else {
		return val.GetUint();
	}
}

uint8_t member_is_valid_array(const rapidjson::Value& doc, const char* name)
{
	if (!doc.HasMember(name)) {
		return 0;
	}

	if (!doc[name].IsArray()) {
		return 0;
	}

	return doc[name].Size() > 0;
}

int8_t get_file_contents(const std::string& filename, std::string* contents)
{
	std::FILE* fp = std::fopen(&filename[0], "rb");

	if (!fp) {
		perror("Error: Can't open file");
		return -1;
	}

	std::fseek(fp, 0, SEEK_END);
	contents->resize(std::ftell(fp));
	std::rewind(fp);
	size_t read = std::fread(&(*contents)[0], 1, contents->size(), fp);
	int8_t retval = 0;

	if (read != contents->size()) {
		if (feof(fp)) {
			fprintf(stderr, "Error reading '%s': unexpected EoF\n",
				filename.c_str());
			retval = -3;
		} else if (ferror(fp)) {
			fprintf(stderr, "Error reading '%s'", filename.c_str());
			retval = -2;
		}
	}

	std::fclose(fp);

	return retval;
}

int8_t parse(const char* json_string,
	std::vector<EcatHelper::ecat_slave_entry_al>* slave_entries,
	EcatHelper::ecat_size_slave_al* slave_length,
	std::vector<EcatHelper::ecat_startup_config_al>* slave_parameters,
	EcatHelper::ecat_size_param_al* parameters_length)
{

	rapidjson::Document document;

	// relaxed JSON config, i.e. comment and trailing comma are allowed
	document.Parse<rapidjson::kParseTrailingCommasFlag
		| rapidjson::kParseCommentsFlag | rapidjson::kParseEscapedApostropheFlag
		| rapidjson::kParseNanAndInfFlag>(json_string);

	assert(document.IsArray());

	// re-initialize length with 0
	*slave_length = 0;
	*parameters_length = 0;

	// Slave entries must be ordered by position ascendingly
#if RAPIDJSON_HAS_CXX11_RVALUE_REFS
	struct SortSlavesAsc {
		bool operator()(
			const rapidjson::Value& lhs, const rapidjson::Value& rhs) const
		{
			return lhs["position"].GetUint() < rhs["position"].GetUint();
		}
	};

	std::sort(document.Begin(), document.End(), SortSlavesAsc());
#endif

	size_t total_slaves = document.Size();
	EcatHelper::ecat_size_slave_al i_slaves = 0;

	for (i_slaves = 0; i_slaves < total_slaves; i_slaves++) {
		rapidjson::Value::Object m_slaves = document[i_slaves].GetObject();

		assert(m_slaves.HasMember("alias"));
		assert(m_slaves.HasMember("position"));
		assert(m_slaves.HasMember("vendor_id"));
		assert(m_slaves.HasMember("product_code"));

		uint16_t alias = to_uint32(m_slaves["alias"]);
		EcatHelper::ecat_pos_al position = to_uint32(m_slaves["position"]);
		uint32_t vendor_id = to_uint32(m_slaves["vendor_id"]);
		uint32_t product_code = to_uint32(m_slaves["product_code"]);

		// add new slave entry if slave doesnt have syncs
		if (!member_is_valid_array(m_slaves, "syncs")) {
			(*slave_length)++;

			(*slave_entries)
				.push_back({
					.alias = alias,
					.position = position,
					.vendor_id = vendor_id,
					.product_code = product_code,
				});

			continue;
		}

		assert(m_slaves["syncs"].IsArray());

		for (uint8_t i_syncs = 0; i_syncs < m_slaves["syncs"].Size();
			 i_syncs++) {

			rapidjson::Value::Object m_syncs
				= m_slaves["syncs"][i_syncs].GetObject();

			assert(m_syncs.HasMember("index"));
			assert(m_syncs.HasMember("pdos"));
			assert(m_syncs["pdos"].IsArray());

			uint8_t sync_index = to_uint32(m_syncs["index"]);
			uint8_t watchdog_enabled = 0;
			uint8_t direction = SyncMEthercatDirection[sync_index];

			if (m_syncs.HasMember("watchdog_enabled")) {
				assert(m_syncs["watchdog_enabled"].IsBool());
				watchdog_enabled = static_cast<uint8_t>(
					m_syncs["watchdog_enabled"].GetBool());
			}

			// override default 'direction' value if it's defined
			if (m_syncs.HasMember("direction")) {
				assert(m_syncs["direction"].IsString());

				std::string sync_direction = m_syncs["direction"].GetString();

				if (sync_direction == "input") {
					direction = EC_DIR_INPUT;
				} else if (sync_direction == "output") {
					direction = EC_DIR_OUTPUT;
				} else {
					throw std::invalid_argument("\"" + sync_direction
						+ "\" is invalid value. "
						+ "'direction' value must be \"input\" or \"output\"");
				}
			}

			rapidjson::Value::Array arr_pdos = m_syncs["pdos"].GetArray();
			EcatHelper::ecat_size_io_al size_arr_pdos = arr_pdos.Size();
			EcatHelper::ecat_size_io_al i_pdos = 0;
			for (i_pdos = 0; i_pdos < size_arr_pdos; i_pdos++) {

				rapidjson::Value::Object m_pdos = arr_pdos[i_pdos].GetObject();
				assert(m_pdos.HasMember("index"));

				EcatHelper::ecat_index_al pdo_index
					= to_uint32(m_pdos["index"]);

				// add new slave entry if sync doesnt have pdo entries
				if (!member_is_valid_array(m_pdos, "entries")) {
					(*slave_length)++;

					(*slave_entries)
						.push_back({
							.alias = alias,
							.position = position,
							.vendor_id = vendor_id,
							.product_code = product_code,
							.sync_index = sync_index,
							.pdo_index = pdo_index,
							.direction = direction,
						});

					continue;
				}

				rapidjson::Value::Array arr_entries
					= m_pdos["entries"].GetArray();
				int32_t size_arr_entries = arr_entries.Size();
				EcatHelper::ecat_size_io_al i_entries = 0;
				for (i_entries = 0; i_entries < size_arr_entries; i_entries++) {

					rapidjson::Value::Object m_entries
						= arr_entries[i_entries].GetObject();

					assert(m_entries.HasMember("index"));
					assert(m_entries.HasMember("subindex"));
					assert(m_entries.HasMember("size"));

					EcatHelper::ecat_index_al entry_index;
					EcatHelper::ecat_sub_al entry_subindex;
					EcatHelper::ecat_size_al entry_size;

					entry_index = to_uint32(m_entries["index"]);
					entry_subindex = to_uint32(m_entries["subindex"]);
					entry_size = to_uint32(m_entries["size"]);

					uint8_t entry_add_to_domain = 0;
					uint8_t entry_swap_endian = 0;
					uint8_t entry_signed = 0;

					if (m_entries.HasMember("swap_endian")) {
						assert(m_entries["swap_endian"].IsBool());
						entry_swap_endian = static_cast<uint8_t>(
							m_entries["swap_endian"].GetBool());
					}

					if (m_entries.HasMember("add_to_domain")) {
						assert(m_entries["add_to_domain"].IsBool());
						entry_add_to_domain = static_cast<uint8_t>(
							m_entries["add_to_domain"].GetBool());
					}

					if (m_entries.HasMember("signed")) {
						assert(m_entries["signed"].IsBool());
						entry_signed = static_cast<uint8_t>(
							m_entries["signed"].GetBool());
					}

					// add new slave entry
					(*slave_length)++;

					(*slave_entries)
						.push_back({
							.alias = alias,
							.position = position,
							.vendor_id = vendor_id,
							.product_code = product_code,
							.sync_index = sync_index,
							.pdo_index = pdo_index,
							.index = entry_index,
							.subindex = entry_subindex,
							.size = entry_size,
							.add_to_domain = entry_add_to_domain,
							.direction = direction,
							.swap_endian = entry_swap_endian,
							.is_signed = entry_signed,
							.watchog_enabled = watchdog_enabled,
						});
				}
			}
		}

		// start adding startup parameters if there is one
		if (member_is_valid_array(m_slaves, "parameters")) {
			assert(m_slaves["parameters"].IsArray());

			EcatHelper::ecat_size_io_al pr_size = m_slaves["parameters"].Size();
			EcatHelper::ecat_size_io_al i_parameters = 0;
			for (i_parameters = 0; i_parameters < pr_size; i_parameters++) {
				rapidjson::Value::Object m_parameters
					= m_slaves["parameters"][i_parameters].GetObject();

				assert(m_parameters.HasMember("index"));
				assert(m_parameters.HasMember("subindex"));
				assert(m_parameters.HasMember("size"));
				assert(m_parameters.HasMember("value"));

				EcatHelper::ecat_index_al p_index
					= to_uint32(m_parameters["index"]);
				EcatHelper::ecat_sub_al p_subindex
					= to_uint32(m_parameters["subindex"]);
				EcatHelper::ecat_size_al p_size
					= to_uint32(m_parameters["size"]);
				uint32_t pr_value = to_uint32(m_parameters["value"]);

				(*slave_parameters)
					.push_back({
						.size = p_size,
						.slavePosition
						= static_cast<EcatHelper::ecat_pos_al>(position),
						.index = p_index,
						.subindex = p_subindex,
						.value = { .u32 = pr_value },
					});

				(*parameters_length)++;
			}
		}
	}

#if VERBOSE > 0
	printf("slave_length = %d\n", *slave_length);
#endif

	return 0;
}

}
