#include "vgen.hpp"

#include <fmt/chrono.h>

#include <array>
#include <chrono>
#include <stdexcept>
#include <utility>

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace vgen
{
	constexpr auto global_functions = std::array{"vkCreateInstance"sv, "vkEnumerateInstanceExtensionProperties"sv, "vkEnumerateInstanceLayerProperties"sv};

	const command_data &find_command(const std::string &command, const command_map &commands)
	{
		if (auto it = commands.find(command); it != commands.end())
			return it->second;
		else
			throw std::runtime_error(fmt::format("Command {0} not found in command map", command));
	}

	std::string read_full_text(const pugi::xml_node &node)
	{
		std::string result;
		std::string sep = "";
		for (auto &t : node.select_nodes("descendant-or-self::text()"))
		{
			result += sep + t.node().text().as_string(" ");
			sep = " ";
		}
		return result;
	}

	std::string read_comment(const pugi::xml_node &node)
	{
		auto comment = node.attribute("comment");
		if (comment)
			return fmt::format("// {0}\n", comment.value());

		return ""s;
	}

	bool is_device_command(const pugi::xml_node &command_node)
	{
		// Vulkan-Hpp checks the first parameter type, and if it exists and is not 'VkInstance' or 'VkPhysicalDevice', it is device level
		auto param_type = command_node.select_node("param[1]/type/text()");
		return param_type && param_type.node().value() != "VkInstance"sv && param_type.node().value() != "VkPhysicalDevice"sv;
	}

	command_data read_command(const pugi::xml_node &command_node)
	{
		std::vector<std::string> proto;
		for (auto &node : command_node.children("proto"))
			proto.emplace_back(read_full_text(node));

		std::vector<std::string> params;
		for (auto &node : command_node.children("param"))
			params.emplace_back(read_full_text(node));

		std::vector<std::string> param_names;
		for (auto &node : command_node.select_nodes("param/name/text()"))
			param_names.emplace_back(node.node().value());

		bool returns_void = command_node.select_node("proto/type").node().text().as_string() == "void"sv;

		// clang-format off
		return command_data
		{
			.name = command_node.child("proto").child_value("name"),
			.prototype = to_string(fmt::join(begin(proto), end(proto), " ")),
			.params = to_string(fmt::join(begin(params), end(params), ", ")),
			.param_names = to_string(fmt::join(begin(param_names), end(param_names), ", ")),
			.comment = read_comment(command_node),
			.returns_void = returns_void,
			.is_device_command = is_device_command(command_node),
		};
		// clang-format on
	}

	command_map read_commands(const pugi::xml_document &doc)
	{
		std::unordered_map<std::string, std::string> aliases;
		command_map command_map;

		auto command_elements = doc.select_nodes("/registry/commands/command");
		for (auto &c : command_elements)
		{
			const auto &command_node = c.node();

			if (command_node.attribute("alias"))
				aliases.emplace(command_node.attribute("name").value(), command_node.attribute("alias").value());
			else
			{
				auto command = read_command(command_node);
				command_map.emplace(command.name, std::move(command));
			}
		}

		for (const auto &[alias, potential_command] : aliases)
		{
			std::string command_name = potential_command;
			auto iter = command_map.find(command_name);

			// The alias can refer to another alias, so keep looking up aliases until we find something in the command map
			while (iter == end(command_map))
			{
				if (auto alias_iter = aliases.find(command_name); alias_iter == end(aliases))
					break;
				else
				{
					command_name = alias_iter->second;
					iter = command_map.find(command_name);
				}
			}

			if (iter == end(command_map))
				throw std::runtime_error(fmt::format("Alias '{0}' not found in map", potential_command));

			// create a command for the alias based on the existing command
			const auto &existing_command = iter->second;
			command_data cmd = existing_command;
			cmd.name = alias;

			auto pos = cmd.prototype.find(existing_command.name);
			cmd.prototype.replace(pos, existing_command.name.size(), alias);

			command_map.emplace(alias, std::move(cmd));
		}

		return command_map;
	}

	feature_data read_feature(const pugi::xml_node &feature_node)
	{
		feature_data feature{
			.name = feature_node.attribute("name").value(),
			.comment = read_comment(feature_node),
		};

		auto required_commands = feature_node.select_nodes("require[command]");
		for (auto &item : required_commands)
		{
			section_data section{
				.comment = read_comment(item.node()),
			};

			for (auto &cmd : item.node().children("command"))
				section.commands.emplace_back(cmd.attribute("name").value());

			feature.sections.emplace_back(std::move(section));
		}

		return feature;
	}

	std::vector<feature_data> read_features(const pugi::xml_document &doc)
	{
		std::vector<feature_data> features;

		for (auto &feature_item : doc.select_nodes("/registry/feature"))
			features.emplace_back(read_feature(feature_item.node()));

		return features;
	}

	extension_map read_extensions(const pugi::xml_document &doc)
	{
		// We will select all the commands, then walk up the tree to figure out the required extensions
		// commands can appear multiple times
		std::unordered_map<std::string, std::set<std::string>> extensions;

		// clang-format off
		auto defined = [](const auto &ext)
		{
			return fmt::format("defined({0})", ext);
		};
		// clang-format on

		for (const auto &command : doc.select_nodes("/registry/extensions/extension/require/command"))
		{
			const auto &command_node = command.node();

			std::string name = command_node.attribute("name").as_string();
			std::set<std::string> reqs;

			// <command> parent is <require>, save any 'feature' and 'extension' attributes (might not have any)
			const auto &require_node = command_node.parent();
			if (require_node.attribute("extension"))
				reqs.emplace(defined(require_node.attribute("extension").as_string()));
			if (require_node.attribute("feature"))
				reqs.emplace(defined(require_node.attribute("feature").as_string()));

			// <require> parent is <extension>, save the 'name' attribute
			const auto &extension_node = require_node.parent();
			if (extension_node.attribute("name"))
				reqs.emplace(defined(extension_node.attribute("name").as_string()));

			std::string req_string = format("{0}", fmt::join(reqs, " && "));

			if (const auto result = extensions.find(name); result != extensions.end())
			{
				// item exists, update the existing item
				result->second.emplace(std::move(req_string));
			}
			else
			{
				// insert new item
				extensions.emplace(std::move(name), std::set{std::move(req_string)});
			}
		}

		// flip the key and value of our extensions so that we group extensions with the exact same requirements
		extension_map result;
		for (auto &[command, reqs] : extensions)
			result.emplace(std::move(reqs), command);

		return result;
	}

	enum class option_comments
	{
		no_comments,
		write_comments,
	};

	// clang-format off
	template <typename Fn>
	requires std::is_invocable_v<Fn, const std::string &>
	void write_feature_commands(fmt::memory_buffer &out, const feature_data &feature, Fn func, option_comments comments = option_comments::write_comments)
	// clang-format on
	{
		format_to(out, "\n");
		if (comments == option_comments::write_comments)
			format_to(out, "{0}", feature.comment);

		write_guard_start(out, feature.name);

		for (const auto &section : feature.sections)
		{
			format_to(out, "\n");
			if (comments == option_comments::write_comments)
				format_to(out, "{0}", section.comment);

			for (const auto &command : section.commands)
				func(command);
		}

		format_to(out, "\n");
		write_guard_end(out, feature.name);
	}

	// clang-format off
	template <typename Fn>
	requires std::is_invocable_v<Fn, const std::string &>
	void write_extension_commands(fmt::memory_buffer &out, const extension_map &extensions, Fn func)
	// clang-format on
	{
		const std::set<std::string> *current = nullptr;

		for (const auto &[reqs, command] : extensions)
		{
			// We started a new group, close the old group (if applicable) and start another
			if (!current || *current != reqs)
			{
				if (current)
					format_to(out, "#endif // {0}\n", fmt::join(*current, " || "));

				current = &reqs;
				format_to(out, "#if {0}\n", fmt::join(*current, " || "));
			}

			func(command);
		}

		if (current)
			format_to(out, "#endif // {0}\n", fmt::join(*current, " || "));
	}

	std::string read_vulkan_header_version(const pugi::xml_document &doc)
	{
		auto version = doc.select_node("/registry/types/type[@category='define' and ./name/text() = 'VK_HEADER_VERSION']/text()[last()]");
		return version.node().value();
	}

	void write_guard_start(fmt::memory_buffer &out, const std::string &guard)
	{
		fmt::format_to(out, "#if defined({0})\n", guard);
	}

	void write_guard_end(fmt::memory_buffer &out, const std::string &guard)
	{
		fmt::format_to(out, "#endif // defined({0})\n", guard);
	}

	void write_command_definition(fmt::memory_buffer &out, const command_data &command)
	{
		format_to(out,
			R"(
{5}static PFN_{0} pfn_{0};
VKAPI_ATTR {1}({2})
{{
	assert(pfn_{0});
	{4}pfn_{0}({3});
}}
)",
			command.name, command.prototype, command.params, command.param_names, command.returns_void ? "" : "return ", command.comment);
	}

	void write_feature_definitions(fmt::memory_buffer &out, const feature_data &feature, const command_map &commands)
	{
		// clang-format off
		write_feature_commands(out, feature,
			[&](const std::string &command)
			{
				write_command_definition(out, find_command(command, commands));
			}
		);
		// clang-format on
	}

	void write_extension_definitions(fmt::memory_buffer &out, const extension_map &extensions, const command_map &commands)
	{
		write_extension_commands(out, extensions, [&](const std::string &command) { write_command_definition(out, find_command(command, commands)); });
	}

	void write_struct_command_field(fmt::memory_buffer &out, const command_data &command)
	{
		const std::string_view tab = command.comment.empty() ? "" : "\t";
		format_to(out, "\t{1}{2}PFN_{0} {0};\n", command.name, command.comment, tab);
	}

	void write_struct_section_fields(fmt::memory_buffer &out, const section_data &section, const command_map &commands)
	{
		const std::string_view tab = section.comment.empty() ? "" : "\t";
		format_to(out, "\n{1}{0}\n", section.comment, tab);

		for (const auto &command : section.commands)
			write_struct_command_field(out, find_command(command, commands));
	}

	void write_struct_feature_fields(fmt::memory_buffer &out, const feature_data &feature, const command_map &commands)
	{
		format_to(out, "\n{0}", feature.comment);
		write_guard_start(out, feature.name);

		for (const auto &section : feature.sections)
			write_struct_section_fields(out, section, commands);

		format_to(out, "\n");
		write_guard_end(out, feature.name);
	}

	void write_struct_extension_fields(fmt::memory_buffer &out, const extension_map &extensions, const command_map &commands)
	{
		write_extension_commands(out, extensions, [&](const std::string &command) { write_struct_command_field(out, find_command(command, commands)); });
	}

	void write_feature_instance_init(fmt::memory_buffer &out, const feature_data &feature)
	{
		// clang-format off
		write_feature_commands(out, feature,
			[&](const std::string &command)
			{
				// filter out functions that are defined in the spec, but are initialized elsewhere by the loader
				if (std::find(begin(global_functions), end(global_functions), command) != end(global_functions))
					return;

				format_to(out, "\tpfn_{0} = (PFN_{0})vkGetInstanceProcAddr(instance, \"{0}\");\n", command);
			}, option_comments::no_comments
		);
		// clang-format on
	}

	void write_feature_device_init(fmt::memory_buffer &out, const feature_data &feature)
	{
		// clang-format off
		write_feature_commands(out, feature, [&](const std::string &command)
		{
			format_to(out, "\tpfn_{0} = (PFN_{0})vkGetDeviceProcAddr(device, \"{0}\");\n", command);
		}, option_comments::no_comments);
		// clang-format on
	}

	void write_extensions_instance_init(fmt::memory_buffer &out, const extension_map &extensions)
	{
		// clang-format off
		write_extension_commands(out, extensions,
			[&](const std::string &command)
			{
				format_to(out, "\tpfn_{0} = (PFN_{0})vkGetInstanceProcAddr(instance, \"{0}\");\n",  command);
			}
		);
		// clang-format on
	}

	void write_extensions_device_init(fmt::memory_buffer &out, const extension_map &extensions)
	{
		// clang-format off
		write_extension_commands(out, extensions,
			[&](const std::string &command)
			{
				format_to(out, "\tpfn_{0} = (PFN_{0})vkGetDeviceProcAddr(device, \"{0}\");\n",  command);
			}
		);
		// clang-format on
	}

	void write_feature_instance_init_struct(fmt::memory_buffer &out, const feature_data &feature)
	{
		// clang-format off
		write_feature_commands(out, feature,
			[&](const std::string &command)
			{
				// filter out functions that are defined in the spec, but are initialized elsewhere by the loader
				if (std::find(begin(global_functions), end(global_functions), command) != end(global_functions))
					return;

				format_to(out, "\tvk->{0} = (PFN_{0})vk->vkGetInstanceProcAddr(instance, \"{0}\");\n", command);
			}, option_comments::no_comments
		);
		// clang-format on
	}

	void write_feature_device_init_struct(fmt::memory_buffer &out, const feature_data &feature)
	{
		// clang-format off
		write_feature_commands(out, feature, [&](const std::string &command)
		{
			format_to(out, "\tvk->{0} = (PFN_{0})vk->vkGetDeviceProcAddr(device, \"{0}\");\n", command);
		}, option_comments::no_comments);
		// clang-format on
	}

	void write_extensions_instance_init_struct(fmt::memory_buffer &out, const extension_map &extensions)
	{
		// clang-format off
		write_extension_commands(out, extensions,
			[&](const std::string &command)
			{
				format_to(out, "\tvk->{0} = (PFN_{0})vk->vkGetInstanceProcAddr(instance, \"{0}\");\n", command);
			}
		);
		// clang-format on
	}

	void write_extensions_device_init_struct(fmt::memory_buffer &out, const extension_map &extensions)
	{
		// clang-format off
		write_extension_commands(out, extensions,
			[&](const std::string &command)
			{
				format_to(out, "\tvk->{0} = (PFN_{0})vk->vkGetDeviceProcAddr(device, \"{0}\");\n", command);
			}
		);
		// clang-format on
	}

	void write_header(fmt::memory_buffer &out, const std::vector<feature_data> &features, const extension_map &extensions, const command_map &commands)
	{
		auto now = [] {
			using namespace std::chrono;
			return fmt::gmtime(system_clock::to_time_t(system_clock::now()));
		};

		// header guard, preamble, and sanity checks

		format_to(out, R"header(#if !defined(VGEN_VULKAN_LOADER_HEADER)
#define VGEN_VULKAN_LOADER_HEADER

/*******************************************************************************
This file was generated by vulkan_loader_generator on {0:%c} UTC
For more information, see: https://github.com/oracleoftroy/vulkan_loader_generator

INSTRUCTIONS:

The loader comes in two variants.

When VK_NO_PROTOTYPES is not defined, it
provides implementations of the prototypes found in vulkan.h, and once loaded,
you can use the normal C vulkan api.

When VK_NO_PROTOTYPES is defined, the loader provides a struct containing function pointers for the vulkan API.

The loader provides three functions:
	vgen_init_vulkan_loader
	vgen_load_instance_procs
	vgen_load_device_procs

vgen_init_vulkan_loader is required to initialize the loader and requires the caller to provide
vkGetInstnaceProcAddr, obtainable via GetProcAddr(), dlsym(), SDL_Vulkan_GetVkGetInstanceProcAddr(), etc.

On completion, the following functions will be available:
	vkGetInstanceProcAddr
	vkCreateInstance
	vkEnumerateInstanceExtensionProperties
	vkEnumerateInstanceLayerProperties

Once a vulkan instance is created, call vgen_load_instance_procs to load the rest of the vulkan api.

After creating a device, you may load device specific instances via vgen_load_device_procs. See the
Vulkan API docs for vkGetDeviceProcAddr for more information.

---

This file is distributed under the terms of the MIT License

Copyright {0:%Y} Marc Gallagher

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#include <vulkan/vulkan.h>

#if defined(__cplusplus)
extern "C" {{
#endif
)header",
			now());

		// structs for dynamic loading (always available / available by default)

		// start of struct
		fmt::format_to(out, R"(
#if !defined(VK_NO_PROTOTYPES)

void vgen_init_vulkan_loader(PFN_vkGetInstanceProcAddr get_address);
void vgen_load_instance_procs(VkInstance instance);
void vgen_load_device_procs(VkDevice device);

#else // !defined(VK_NO_PROTOTYPES)

struct vgen_vulkan_api
{{)");

		for (const auto &feature : features)
			write_struct_feature_fields(out, feature, commands);

		write_struct_extension_fields(out, extensions, commands);

		// end of struct
		fmt::format_to(out, R"(}};

void vgen_init_vulkan_loader(PFN_vkGetInstanceProcAddr get_address, struct vgen_vulkan_api *vk);
void vgen_load_instance_procs(VkInstance instance, struct vgen_vulkan_api *vk);
void vgen_load_device_procs(VkDevice device, struct vgen_vulkan_api *vk);

#endif // !defined(VK_NO_PROTOTYPES)

#if defined(__cplusplus)
}} // extern "C"
#endif

#endif // !defined(VGEN_VULKAN_LOADER_HEADER)
)");
	}

	void write_source(fmt::memory_buffer &out, const std::string_view vulkan_header_version, const std::vector<feature_data> &features, const extension_map &extensions, const command_map &commands)
	{
		fmt::format_to(out, R"(#include <vulkan_loader.h>

#if !defined(VKLG_ASSERT_MACRO)
	#include <assert.h>
	#define VKLG_ASSERT_MACRO assert;
#endif

#if VK_HEADER_VERSION > {0} && !defined(VK_NO_PROTOTYPES) && !defined(VGEN_VULKAN_LOADER_DISABLE_HEADER_CHECK)
	#error "Generating prototypes for a version of vulkan than the loader expects, you may experience linking errors. " \
		"Please check for a newer version of vulkan_loader at https://github.com/oracleoftroy/vulkan_loader " \
		"or define VK_NO_PROTOTYPES for a purely dynamic interface. " \ 
		"You may also define VGEN_VULKAN_LOADER_DISABLE_HEADER_CHECK to skip this check."
#endif

#if defined(VK_NO_PROTOTYPES)

void vgen_init_vulkan_loader(PFN_vkGetInstanceProcAddr get_address, struct vgen_vulkan_api *vk)
{{
	vk->vkGetInstanceProcAddr = get_address;
	vk->vkCreateInstance = (PFN_vkCreateInstance)vk->vkGetInstanceProcAddr(0, "vkCreateInstance");
	vk->vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vk->vkGetInstanceProcAddr(0, "vkEnumerateInstanceExtensionProperties");
	vk->vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vk->vkGetInstanceProcAddr(0, "vkEnumerateInstanceLayerProperties");
}}

void vgen_load_instance_procs(VkInstance instance, struct vgen_vulkan_api *vk)
{{
)",
			vulkan_header_version);

		for (const auto &feature : features)
			write_feature_instance_init_struct(out, feature);

		write_extensions_instance_init_struct(out, extensions);

		fmt::format_to(out, R"(}}

void vgen_load_device_procs(VkDevice device, struct vgen_vulkan_api *vk)
{{
)");

		for (const auto &feature : features)
			write_feature_device_init_struct(out, feature);

		write_extensions_device_init_struct(out, extensions);

		fmt::format_to(out, R"(}}

#else // defined(VK_NO_PROTOTYPES)
)");

		for (const auto &feature : features)
			write_feature_definitions(out, feature, commands);

		write_extension_definitions(out, extensions, commands);

		fmt::format_to(out, R"(
void vgen_init_vulkan_loader(PFN_vkGetInstanceProcAddr get_address)
{{
	pfn_vkGetInstanceProcAddr = get_address;
	pfn_vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(0, "vkCreateInstance");
	pfn_vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(0, "vkEnumerateInstanceExtensionProperties");
	pfn_vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkGetInstanceProcAddr(0, "vkEnumerateInstanceLayerProperties");
}}

void vgen_load_instance_procs(VkInstance instance)
{{
)");

		for (const auto &feature : features)
			write_feature_instance_init(out, feature);

		write_extensions_instance_init(out, extensions);

		fmt::format_to(out, R"(}}

void vgen_load_device_procs(VkDevice device)
{{
)");

		for (const auto &feature : features)
			write_feature_device_init(out, feature);

		write_extensions_device_init(out, extensions);

		fmt::format_to(out, R"(}}

#endif // defined(VK_NO_PROTOTYPES)
)");
	}
}
