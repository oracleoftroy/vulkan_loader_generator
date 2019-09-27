#include <pugixml.hpp>
#include <cxxopts.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std::literals;
namespace fs = std::filesystem;

void generate_content(const pugi::xml_document &doc, fmt::memory_buffer &header, fmt::memory_buffer &impl);

int main(int argc, char *argv[])
{
	try
	{
		cxxopts::Options options("vgen", "Vulkan loader library generator");
		options.positional_help("(path to vk.xml) [output dir]");

		options.add_options()
			("h,help", "Show this help")
			("i,in", "path to Vulkan API Registry file (vk.xml)", cxxopts::value<std::string>())
			("o,out", "output directory", cxxopts::value<std::string>())
			;

		options.parse_positional({ "in"s, "out"s });

		auto parsed_options = options.parse(argc, argv);
		if (parsed_options.count("help"))
		{
			std::cout << options.help();
			exit(0);
		}

		if (!parsed_options.count("in"))
		{
			std::cerr << "ERROR: No input file specified\n";
			std::cerr << options.help();
			exit(1);
		}

		auto in_file = fs::u8path(parsed_options["in"].as<std::string>());
		auto output_dir = parsed_options.count("out") ? fs::u8path(parsed_options["out"].as<std::string>()) : fs::current_path();

		pugi::xml_document doc;

		auto result = doc.load_file(in_file.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
		if (!result)
		{
			std::cerr << result.description();
			exit(1);
		}

		fmt::memory_buffer header, impl;
		std::cout << "Generating loader\n";
		generate_content(doc, header, impl);

		auto header_path = output_dir / fs::u8path("vulkan_loader.h");
		auto impl_path = output_dir / fs::u8path("vulkan_loader.c");
		std::ofstream header_file(header_path);
		std::ofstream impl_file(impl_path);

		std::cout << "Writing " << header_path.u8string() << '\n';
		header_file << to_string(header);

		std::cout << "Writing " << impl_path.u8string() << '\n';
		impl_file << to_string(impl);

		std::cout << "Done\n";
	}
	catch (std::exception &e)
	{
		std::cerr << e.what();
		throw;
	}
}

std::string get_full_text(const pugi::xml_node &node)
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

void add_comment(const pugi::xml_node &node, fmt::memory_buffer &out)
{
	auto comment = node.attribute("comment");
	if (comment)
		format_to(out, "// {0}\n", comment.value());
};

void generate_impl(const pugi::xml_document &doc, fmt::memory_buffer &impl)
{
	const std::unordered_map<std::string, pugi::xml_node> commands = [](const auto &doc)
	{
		std::unordered_map<std::string, std::string> aliases;
		std::unordered_map<std::string, pugi::xml_node> result;
		auto command_elements = doc.select_nodes("/registry/commands/command");
		for (auto &command : command_elements)
		{
			const auto &node = command.node();

			if (node.attribute("alias"))
				aliases.insert({node.attribute("name").value(), node.attribute("alias").value()});
			else
				result.insert({node.child("proto").child_value("name"), node});
		}

		for (auto &alias : aliases)
		{
			if (auto iter = result.find(alias.second);
				iter != end(result))
			{
				// Copy the node and set the name to the alias before storing. This seems
				// a bit hacky, but pugixml doesn't support copying the node itself, just
				// in relationship to a document. So we make a new sibling and update the name.
				auto node = iter->second.parent().append_copy(iter->second);
				node.child("proto").child("name").text().set(alias.first.c_str());
				result.insert({alias.first, std::move(node)});
			}
		}

		return result;
	}(doc);

	auto define_command = [&](const auto &name, auto &out)
	{
		auto iter = commands.find(name);
		if (iter == end(commands))
			throw std::runtime_error(fmt::format("Command '{0}' not found in map", name));

		auto command = iter->second;
		add_comment(command, out);

		// Declare static pointer to function
		format_to(out, "static PFN_{0} pfn_{0};\n", name);

		// Declare wrapper function
		std::vector<std::string> proto;
		std::vector<std::string> params;
		std::vector<std::string> param_names;

		for (auto &node : command.children("proto"))
			proto.emplace_back(get_full_text(node));
		for (auto &node : command.children("param"))
			params.emplace_back(get_full_text(node));
		for (auto &node : command.select_nodes("param/name/text()"))
			param_names.emplace_back(node.node().value());

		format_to(out, "VKAPI_ATTR {0}({1})\n",
			fmt::join(begin(proto), end(proto), " "),
			fmt::join(begin(params), end(params), ", "));
		format_to(out, "{{\n\tassert(pfn_{0});\n\t", name);

		bool returns_void = command.select_node("proto/type").node().text().as_string() == "void"sv;
		if (!returns_void)
			format_to(out, "return ");

		format_to(out, "pfn_{0}({1});\n}}\n\n", name, fmt::join(begin(param_names), end(param_names), ", "));
	};

	format_to(impl, "{}",
R"(#include <vulkan/vulkan.h>
#include <assert.h>

#if defined(_MSC_VER)
// disable warning: 'void' function returning a value
// Wrapper functions return whatever their function pointer returns, and the types should always match.
// MSC warns on void f() { return p_f(); } where p_f() is also returning void
#pragma warning(disable: 4098)
#endif

)");

	// sanity check, make sure this file matches the header version of vulkan.h
	auto version = doc.select_node("/registry/types/type[@category='define' and ./name/text() = 'VK_HEADER_VERSION']/text()[last()]");
	format_to(impl, "#if VK_HEADER_VERSION != {0}\n\t#error \"Vulkan header version does not match\"\n#endif\n\n", version.node().value());

	fmt::memory_buffer instance_body;
	fmt::memory_buffer device_body;

	// iterate all core features and create prototypes and definitions
	for (auto &feature_item : doc.select_nodes("/registry/feature"))
	{
		const auto &feature = feature_item.node();

		// First, define functions for the core vulkan api
		format_to(impl, "#if defined({0})\n\n", feature.attribute("name").value());
		add_comment(feature, impl);
		format_to(impl, "\n");

		// generate loaders for instance and device functions
		std::array<std::string, 3> global_functions = { "vkCreateInstance"s, "vkEnumerateInstanceExtensionProperties"s, "vkEnumerateInstanceLayerProperties"s };

		auto required_commands = feature.select_nodes("require[command]");
		for (auto &item : required_commands)
		{
			add_comment(item.node(), impl);
			format_to(impl, "\n");

			for (auto &cmd : item.node().children("command"))
			{
				std::string name = cmd.attribute("name").value();
				define_command(name, impl);

				// if this command is one of the three global functions, filter it out, it will be instantiated elsewhere
				if (std::find(begin(global_functions), end(global_functions), name) == end(global_functions))
					format_to(instance_body, "\tpfn_{0} = (PFN_{0})vkGetInstanceProcAddr(vulkan, \"{0}\");\n", name);
			}
		}

		format_to(impl, "#endif // defined({0})\n\n", feature.attribute("name").value());
	}
	// load the vulkan extensions (registry -> extensions)
	// This describes every feature and whether it is a core feature or requires a particular extension to be enabled
	// structure
	// extension: name (required #define), type (instance or device), supported (vulkan or disabled, ignore disabled?)
	//   require: extension (optional, additional required #define)
	//     command: name (function name)
	auto extensions = doc.select_node("/registry/extensions").node();

	for (auto &extension : extensions.select_nodes("extension[require/command]"))
	{
		format_to(impl, "#if defined({0})\n\n", extension.node().attribute("name").value());
		format_to(instance_body, "\n#if defined({0})\n", extension.node().attribute("name").value());

		auto is_device = extension.node().attribute("type").value() == "device"s;
		if (is_device)
			format_to(device_body, "\n#if defined({0})\n", extension.node().attribute("name").value());

		for (auto &require_nodes : extension.node().select_nodes("require[command]"))
		{
			auto require = require_nodes.node();

			// extensions marked with a feature attribute contain duplicate definitons of commands, so skip those
			if (require.attribute("feature"))
				continue;

			if (require.attribute("extension"))
			{
				format_to(impl, "#if defined({0})\n\n", require.attribute("extension").value());
				format_to(instance_body, "\n#if defined({0})\n", require.attribute("extension").value());
				if (is_device)
				format_to(device_body, "\n#if defined({0})\n", require.attribute("extension").value());
			}

			for (auto &command : require.children("command"))
			{
				define_command(command.attribute("name").value(), impl);
				format_to(instance_body, "\tpfn_{0} = (PFN_{0})vkGetInstanceProcAddr(vulkan, \"{0}\");\n", command.attribute("name").value());

				if (is_device)
					format_to(device_body, "\tpfn_{0} = (PFN_{0})vkGetDeviceProcAddr(device, \"{0}\");\n", command.attribute("name").value());
			}

			if (require.attribute("extension"))
			{
				format_to(impl, "#endif // defined({0})\n\n", require.attribute("extension").value());
				format_to(instance_body, "#endif // defined({0})\n", require.attribute("extension").value());
				if (is_device)
					format_to(device_body, "#endif // defined({0})\n", require.attribute("extension").value());
			}
		}

		format_to(impl, "#endif // defined({0})\n\n", extension.node().attribute("name").value());
		format_to(instance_body, "#endif // defined({0})\n", extension.node().attribute("name").value());
		if (is_device)
			format_to(device_body, "#endif // defined({0})\n", extension.node().attribute("name").value());
	}

	// create loader for global functions
	format_to(impl, "{}",
R"(void vulkan_loader_init(PFN_vkGetInstanceProcAddr get_address)
{
	pfn_vkGetInstanceProcAddr = get_address;
	pfn_vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(0, "vkCreateInstance");
	pfn_vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties)vkGetInstanceProcAddr(0, "vkEnumerateInstanceExtensionProperties");
	pfn_vkEnumerateInstanceLayerProperties = (PFN_vkEnumerateInstanceLayerProperties)vkGetInstanceProcAddr(0, "vkEnumerateInstanceLayerProperties");
}

)");

	// create loader for instance and device functions
	format_to(impl,
R"(void vulkan_load_instance_procs(VkInstance vulkan)
{{
{0}}}

void vulkan_load_device_procs(VkDevice device)
{{
{1}}}

)", to_string(instance_body), to_string(device_body));
}

void generate_header(fmt::memory_buffer &header)
{
	// write the header
	format_to(header, "{}",
R"(#pragma once

#if !defined(VULKAN_H_)
#error "Please include vulkan/vulkan.h before including this file"
#endif

#if defined(__cplusplus)
extern "C" {
#endif // defined(__cplusplus)

// Initialize the vulkan loader
// Requires loading vkGetInstnaceProcAddr via GetProcAddr(), dlsym(), SDL_Vulkan_GetVkGetInstanceProcAddr(), etc
// On completion, the following functions will be available: vkGetInstanceProcAddr, vkCreateInstance, vkEnumerateInstanceExtensionProperties, vkEnumerateInstanceLayerProperties
void vulkan_loader_init(PFN_vkGetInstanceProcAddr get_address);

// Load vulkan instance functions
// On completion, all vulkan functions will be available
void vulkan_load_instance_procs(VkInstance vulkan);

// Load vulkan device functions
// Optional: avoids indirection by loading functions for a specific device
void vulkan_load_device_procs(VkDevice device);

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)
)");
}

void generate_content(const pugi::xml_document &doc, fmt::memory_buffer &header, fmt::memory_buffer &impl)
{
	generate_impl(doc, impl);
	generate_header(header);
}
