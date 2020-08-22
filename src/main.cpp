#include <vgen.hpp>

#include <cxxopts.hpp>
#include <fmt/color.h>
#include <fmt/format.h>
#include <pugixml.hpp>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std::literals;
namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
	constexpr auto major_style = fg(fmt::color::white) | fmt::emphasis::bold;
	constexpr auto minor_style = fg(fmt::color::dim_gray);
	constexpr auto error_style = fg(fmt::color::red) | fmt::emphasis::bold;

	try
	{
		cxxopts::Options options("vgen", "Vulkan loader library generator");
		options.positional_help("(path to vk.xml) [output dir]");

		// clang-format off
		options.add_options()
			("h,help", "Show this help")
			("i,in", "path to Vulkan API Registry file (vk.xml)", cxxopts::value<std::string>())
			("o,out", "output directory", cxxopts::value<std::string>());
		// clang-format on

		options.parse_positional({"in"s, "out"s});

		auto parsed_options = options.parse(argc, argv);
		if (parsed_options.count("help"))
		{
			fmt::print("{0}", options.help());
			exit(0);
		}

		if (!parsed_options.count("in"))
		{
			fmt::print(stderr, error_style, "ERROR: No input file specified\n");
			fmt::print(stderr, "{0}", options.help());
			exit(1);
		}

		auto in_file = fs::path(parsed_options["in"].as<std::string>());
		auto output_dir = parsed_options.count("out") ? fs::path(parsed_options["out"].as<std::string>()) : fs::current_path();

		pugi::xml_document doc;

		fmt::print(major_style, "Loading {0}\n", in_file.string());
		auto result = doc.load_file(in_file.c_str(), pugi::parse_default | pugi::parse_trim_pcdata);
		if (!result)
		{
			fmt::print(stderr, error_style, "{0}", result.description());
			exit(1);
		}

		fmt::print(minor_style, "Reading header version.... ");
		auto version = vgen::read_vulkan_header_version(doc);
		fmt::print(minor_style, "{0}\n", version);

		fmt::print(minor_style, "Reading commands\n");
		auto commands = vgen::read_commands(doc);

		fmt::print(minor_style, "Reading features\n");
		auto features = vgen::read_features(doc);

		fmt::print(minor_style, "Reading extensions\n");
		auto extensions = vgen::read_extensions(doc);

		fmt::print(major_style, "Generating loader\n");

		{
			fmt::memory_buffer header;
			write_header(header, features, extensions, commands);
			auto header_path = output_dir / fs::path("vulkan_loader.h");
			fmt::print(minor_style, "Writing {0}\n", header_path.string());
			std::ofstream header_file(header_path);
			header_file << to_string(header);
		}
		{
			fmt::memory_buffer source;
			write_source(source, version, features, extensions, commands);
			auto source_path = output_dir / fs::path("vulkan_loader.c");
			fmt::print(minor_style, "Writing {0}\n", source_path.string());
			std::ofstream header_file(source_path);
			header_file << to_string(source);
		}

		fmt::print(major_style, "Done!\n");
	}
	catch (std::exception &e)
	{
		fmt::print(stderr, error_style, "{0}\n", e.what());
		throw;
	}
}
