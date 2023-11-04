#ifndef _ECAT_HELPER_CONFIG_PARSER_H_
#define _ECAT_HELPER_CONFIG_PARSER_H_

#include <etherlab-helper.h>

namespace ConfigParser {

int8_t get_file_contents(const std::string& filepath, std::string* contents);

int8_t parse(const char* json_string,
	std::vector<EcatHelper::ecat_slave_entry_al>* slave_entries,
	EcatHelper::ecat_size_slave_al* slave_length,
	std::vector<EcatHelper::ecat_startup_config_al>* slave_parameters,
	EcatHelper::ecat_size_param_al* parameters_length);

}

#endif
