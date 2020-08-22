#include <fmt/format.h>
#include <pugixml.hpp>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>

namespace vgen
{
	struct command_data
	{
		std::string name;
		std::string prototype;
		std::string params;
		std::string param_names;
		std::string comment;
		bool returns_void;
		bool is_device_command;
	};

	struct section_data
	{
		std::string comment;
		std::vector<std::string> commands;
	};

	struct feature_data
	{
		std::string name;
		std::string comment;

		std::vector<section_data> sections;
	};

	using command_map = std::unordered_map<std::string, command_data>;
	using extension_map = std::multimap<std::set<std::string>, std::string>;

	command_map read_commands(const pugi::xml_document &doc);
	std::vector<feature_data> read_features(const pugi::xml_document &doc);

	// returns map of requirement sets to commands
	extension_map read_extensions(const pugi::xml_document &doc);

	std::string read_vulkan_header_version(const pugi::xml_document &doc);
	std::string read_full_text(const pugi::xml_node &node);
	std::string read_comment(const pugi::xml_node &node);
	bool is_device_command(const pugi::xml_node &command_node);

	command_data read_command(const pugi::xml_node &command_node);
	feature_data read_feature(const pugi::xml_node &feature_node);

	void write_guard_start(fmt::memory_buffer &out, const std::string &guard);
	void write_guard_end(fmt::memory_buffer &out, const std::string &guard);
	void write_command_definition(fmt::memory_buffer &out, const command_data &command);
	void write_feature_definitions(fmt::memory_buffer &out, const feature_data &feature, const command_map &commands);
	void write_extension_definitions(fmt::memory_buffer &out, const extension_map &extensions, const command_map &commands);

	void write_struct_command_field(fmt::memory_buffer &out, const command_data &command);
	void write_struct_section_fields(fmt::memory_buffer &out, const section_data &section, const command_map &commands);
	void write_struct_feature_fields(fmt::memory_buffer &out, const feature_data &feature, const command_map &commands);
	void write_struct_extension_fields(fmt::memory_buffer &out, const extension_map &extensions, const command_map &commands);

	void write_feature_instance_init(fmt::memory_buffer &out, const feature_data &feature);
	void write_feature_device_init(fmt::memory_buffer &out, const feature_data &feature);
	void write_extensions_instance_init(fmt::memory_buffer &out, const extension_map &extensions);
	void write_extensions_device_init(fmt::memory_buffer &out, const extension_map &extensions);

	void write_feature_instance_init_struct(fmt::memory_buffer &out, const feature_data &feature);
	void write_feature_device_init_struct(fmt::memory_buffer &out, const feature_data &feature);
	void write_extensions_instance_init_struct(fmt::memory_buffer &out, const extension_map &extensions);
	void write_extensions_device_init_struct(fmt::memory_buffer &out, const extension_map &extensions);

	void write_header(fmt::memory_buffer &out, const std::vector<feature_data> &features, const extension_map &extensions, const command_map &commands);
	void write_source(fmt::memory_buffer &out, const std::string_view vulkan_header_version, const std::vector<feature_data> &features, const extension_map &extensions, const command_map &commands);
}
