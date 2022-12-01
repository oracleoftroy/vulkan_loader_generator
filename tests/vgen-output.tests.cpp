#include <catch2/catch_test_macros.hpp>
#include <vgen.hpp>

#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

TEST_CASE("write_guard", "[writer]")
{
	fmt::memory_buffer out;
	vgen::write_guard_start(out, "foo");
	vgen::write_guard_end(out, "foo");

	REQUIRE(to_string(out) == "#if defined(foo)\n#endif // defined(foo)\n");
}

TEST_CASE("write_command_definition", "[command][writer]")
{
	fmt::memory_buffer out;

	// clang-format off
	vgen::command_data void_command
	{
		.name = "test_void",
		.prototype = "void test_void",
		.params = "Foo foo, Bar bar",
		.param_names = "foo, bar",
		.comment = "// comment\n",
		.returns_void = true,
		.is_device_command = true,
	};

	vgen::command_data int_command
	{
		.name = "test_int",
		.prototype = "int test_int",
		.params = "Foo foo, Bar bar",
		.param_names = "foo, bar",
		.comment = "",
		.returns_void = false,
		.is_device_command = true,
	};
	// clang-format on

	write_command_definition(out, void_command);

	REQUIRE(to_string(out) == R"(
// comment
static PFN_test_void pfn_test_void;
VKAPI_ATTR void test_void(Foo foo, Bar bar)
{
	assert(pfn_test_void);
	pfn_test_void(foo, bar);
}
)");

	out.clear();
	write_command_definition(out, int_command);

	REQUIRE(to_string(out) == R"(
static PFN_test_int pfn_test_int;
VKAPI_ATTR int test_int(Foo foo, Bar bar)
{
	assert(pfn_test_int);
	return pfn_test_int(foo, bar);
}
)");
}

TEST_CASE("write_feature_definitions", "[feature][writer]")
{
	fmt::memory_buffer out;

	// clang-format off
	auto commands = std::unordered_map<std::string, vgen::command_data>
	{
		{
			"test_void"s, vgen::command_data
			{
				.name = "test_void",
				.prototype = "void test_void",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "// comment\n",
				.returns_void = true,
				.is_device_command = true,
			},
		},
		{
			"test_int"s, vgen::command_data
			{
				.name = "test_int",
				.prototype = "int test_int",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "",
				.returns_void = false,
				.is_device_command = true,
			},
		},
	};

	auto sections = std::vector
	{
		vgen::section_data
		{
			.comment = "// section comment\n",
			.commands = {"test_void"s, "test_int"s},
		},
	};

	auto feature = vgen::feature_data
	{
		.name = "test_feature",
		.comment = "// test feature comment\n",
		.sections = sections,
	};
	// clang-format on

	write_feature_definitions(out, feature, commands);
	REQUIRE(to_string(out) == R"(
// test feature comment
#if defined(test_feature)

// section comment

// comment
static PFN_test_void pfn_test_void;
VKAPI_ATTR void test_void(Foo foo, Bar bar)
{
	assert(pfn_test_void);
	pfn_test_void(foo, bar);
}

static PFN_test_int pfn_test_int;
VKAPI_ATTR int test_int(Foo foo, Bar bar)
{
	assert(pfn_test_int);
	return pfn_test_int(foo, bar);
}

#endif // defined(test_feature)
)");
}

TEST_CASE("write_extension_definitions", "[extension][writer]")
{
	// clang-format off
	const auto commands = std::unordered_map<std::string, vgen::command_data>
	{
		{
			"test_void"s, vgen::command_data
			{
				.name = "test_void",
				.prototype = "void test_void",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "// comment\n",
				.returns_void = true,
				.is_device_command = true,
			},
		},
		{
			"test_int"s, vgen::command_data
			{
				.name = "test_int",
				.prototype = "int test_int",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "",
				.returns_void = false,
				.is_device_command = true,
			},
		},
	};
	// clang-format on

	SECTION("one feature, multiple commands")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s}, "test_void" },
			{ {"defined(feature_foo)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_extension_definitions(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_foo)

// comment
static PFN_test_void pfn_test_void;
VKAPI_ATTR void test_void(Foo foo, Bar bar)
{
	assert(pfn_test_void);
	pfn_test_void(foo, bar);
}

static PFN_test_int pfn_test_int;
VKAPI_ATTR int test_int(Foo foo, Bar bar)
{
	assert(pfn_test_int);
	return pfn_test_int(foo, bar);
}
#endif // defined(feature_foo)
)");
	}

	SECTION("one feature, multiple requirements")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
			{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_extension_definitions(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)

// comment
static PFN_test_void pfn_test_void;
VKAPI_ATTR void test_void(Foo foo, Bar bar)
{
	assert(pfn_test_void);
	pfn_test_void(foo, bar);
}

static PFN_test_int pfn_test_int;
VKAPI_ATTR int test_int(Foo foo, Bar bar)
{
	assert(pfn_test_int);
	return pfn_test_int(foo, bar);
}
#endif // defined(feature_bar) || defined(feature_foo)
)");
	}

	SECTION("two features")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s}, "test_void" },
			{ {"defined(feature_bar)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_extension_definitions(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_bar)

static PFN_test_int pfn_test_int;
VKAPI_ATTR int test_int(Foo foo, Bar bar)
{
	assert(pfn_test_int);
	return pfn_test_int(foo, bar);
}
#endif // defined(feature_bar)
#if defined(feature_foo)

// comment
static PFN_test_void pfn_test_void;
VKAPI_ATTR void test_void(Foo foo, Bar bar)
{
	assert(pfn_test_void);
	pfn_test_void(foo, bar);
}
#endif // defined(feature_foo)
)");
	}
}

TEST_CASE("write_struct_command_field", "[struct][writer]")
{
	auto command = vgen::command_data{
		.name = "test_fn",
		.prototype = "VkResult test_fn",
		.params = "int foo, char bar",
		.param_names = "foo, bar",
		.comment = "// a comment\n",
		.returns_void = false,
		.is_device_command = false,
	};

	fmt::memory_buffer out;
	vgen::write_struct_command_field(out, command);

	REQUIRE(to_string(out) == R"(	// a comment
	PFN_test_fn test_fn;
)");
}

TEST_CASE("write_struct_section_fields", "[struct][writer]")
{
	auto section = vgen::section_data{
		.comment = "// section comment\n",
		.commands = {"fn_one"s, "fn_two"s},
	};

	auto commands = std::unordered_map<std::string, vgen::command_data>{
		{"fn_one"s,
			vgen::command_data{
				.name = "fn_one",
				.prototype = "VkResult fn_one",
				.params = "int foo, char bar",
				.param_names = "foo, bar",
				.comment = "// a comment #1\n",
				.returns_void = false,
				.is_device_command = false,
			}},
		{"fn_two"s,
			vgen::command_data{
				.name = "fn_two",
				.prototype = "VkResult fn_two",
				.params = "int foo, char bar",
				.param_names = "foo, bar",
				.comment = "// a comment #2\n",
				.returns_void = false,
				.is_device_command = false,
			}},
	};

	fmt::memory_buffer out;
	write_struct_section_fields(out, section, commands);

	REQUIRE(to_string(out) == R"(
	// section comment

	// a comment #1
	PFN_fn_one fn_one;
	// a comment #2
	PFN_fn_two fn_two;
)");
}

TEST_CASE("write_struct_feature_fields", "[struct][writer]")
{
	auto section = vgen::section_data{
		.comment = "// section comment\n",
		.commands = {"fn_one"s, "fn_two"s},
	};

	auto feature = vgen::feature_data{
		.name = "test_feature",
		.comment = "// test feature comment\n",
		.sections = {section},
	};

	auto commands = std::unordered_map<std::string, vgen::command_data>{
		{"fn_one"s,
			vgen::command_data{
				.name = "fn_one",
				.prototype = "VkResult fn_one",
				.params = "int foo, char bar",
				.param_names = "foo, bar",
				.comment = "// a comment #1\n",
				.returns_void = false,
				.is_device_command = false,
			}},
		{"fn_two"s,
			vgen::command_data{
				.name = "fn_two",
				.prototype = "VkResult fn_two",
				.params = "int foo, char bar",
				.param_names = "foo, bar",
				.comment = "// a comment #2\n",
				.returns_void = false,
				.is_device_command = false,
			}},
	};

	fmt::memory_buffer out;
	write_struct_feature_fields(out, feature, commands);

	REQUIRE(to_string(out) == R"(
// test feature comment
#if defined(test_feature)

	// section comment

	// a comment #1
	PFN_fn_one fn_one;
	// a comment #2
	PFN_fn_two fn_two;

#endif // defined(test_feature)
)");
}

TEST_CASE("write_struct_extension_fields", "[struct][writer]")
{
	// clang-format off
	const auto commands = std::unordered_map<std::string, vgen::command_data>
	{
		{
			"test_void"s, vgen::command_data
			{
				.name = "test_void",
				.prototype = "void test_void",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "// comment\n",
				.returns_void = true,
				.is_device_command = true,
			},
		},
		{
			"test_int"s, vgen::command_data
			{
				.name = "test_int",
				.prototype = "int test_int",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "",
				.returns_void = false,
				.is_device_command = true,
			},
		},
	};
	// clang-format on

	SECTION("one feature, multiple commands")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s}, "test_void" },
			{ {"defined(feature_foo)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_struct_extension_fields(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_foo)
	// comment
	PFN_test_void test_void;
	PFN_test_int test_int;
#endif // defined(feature_foo)
)");
	}

	SECTION("one feature, multiple requirements")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
			{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_struct_extension_fields(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)
	// comment
	PFN_test_void test_void;
	PFN_test_int test_int;
#endif // defined(feature_bar) || defined(feature_foo)
)");
	}

	SECTION("two features")
	{
		// clang-format off
		auto defs = vgen::extension_map
		{
			{ {"defined(feature_foo)"s}, "test_void" },
			{ {"defined(feature_bar)"s}, "test_int" },
		};
		// clang-format on

		fmt::memory_buffer out;
		vgen::write_struct_extension_fields(out, defs, commands);

		REQUIRE(to_string(out) == R"(#if defined(feature_bar)
	PFN_test_int test_int;
#endif // defined(feature_bar)
#if defined(feature_foo)
	// comment
	PFN_test_void test_void;
#endif // defined(feature_foo)
)");
	}
}

TEST_CASE("write feature pointer init impl", "[writer]")
{
	auto section = vgen::section_data{
		.comment = "// section comment\n",
		.commands = {"fn_one"s, "fn_two"s},
	};

	auto feature = vgen::feature_data{
		.name = "test_feature",
		.comment = "// test feature comment\n",
		.sections = {section},
	};

	SECTION("instance")
	{
		fmt::memory_buffer out;
		write_feature_instance_init(out, feature);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	pfn_fn_one = (PFN_fn_one)vkGetInstanceProcAddr(instance, "fn_one");
	pfn_fn_two = (PFN_fn_two)vkGetInstanceProcAddr(instance, "fn_two");

#endif // defined(test_feature)
)");
	}

	SECTION("skip non-instance functions")
	{
		auto section2 = vgen::section_data{
			.comment = "// section comment\n",
			.commands = {"vkCreateInstance"s, "vkEnumerateInstanceExtensionProperties"s, "vkEnumerateInstanceLayerProperties"s, "fn_one"},
		};

		auto feature2 = vgen::feature_data{
			.name = "test_feature",
			.comment = "// test feature comment\n",
			.sections = {section2},
		};

		fmt::memory_buffer out;
		write_feature_instance_init(out, feature2);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	pfn_fn_one = (PFN_fn_one)vkGetInstanceProcAddr(instance, "fn_one");

#endif // defined(test_feature)
)");
	}

	SECTION("device")
	{
		fmt::memory_buffer out;
		write_feature_device_init(out, feature);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	pfn_fn_one = (PFN_fn_one)vkGetDeviceProcAddr(device, "fn_one");
	pfn_fn_two = (PFN_fn_two)vkGetDeviceProcAddr(device, "fn_two");

#endif // defined(test_feature)
)");
	}
}

TEST_CASE("write extension pointer init impl", "[writer]")
{
	SECTION("Instance init")
	{
		SECTION("one feature, multiple commands")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_foo)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetInstanceProcAddr(instance, "test_void");
	pfn_test_int = (PFN_test_int)vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_foo)
)");
		}

		SECTION("one feature, multiple requirements")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetInstanceProcAddr(instance, "test_void");
	pfn_test_int = (PFN_test_int)vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_bar) || defined(feature_foo)
)");
		}

		SECTION("two features")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar)
	pfn_test_int = (PFN_test_int)vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_bar)
#if defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetInstanceProcAddr(instance, "test_void");
#endif // defined(feature_foo)
)");
		}
	}

	SECTION("Device init")
	{
		SECTION("one feature, multiple commands")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_foo)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetDeviceProcAddr(device, "test_void");
	pfn_test_int = (PFN_test_int)vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_foo)
)");
		}

		SECTION("one feature, multiple requirements")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetDeviceProcAddr(device, "test_void");
	pfn_test_int = (PFN_test_int)vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_bar) || defined(feature_foo)
)");
		}

		SECTION("two features")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar)
	pfn_test_int = (PFN_test_int)vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_bar)
#if defined(feature_foo)
	pfn_test_void = (PFN_test_void)vkGetDeviceProcAddr(device, "test_void");
#endif // defined(feature_foo)
)");
		}
	}
}

TEST_CASE("write feature pointer init struct impl", "[writer]")
{
	auto section = vgen::section_data{
		.comment = "// section comment\n",
		.commands = {"fn_one"s, "fn_two"s},
	};

	auto feature = vgen::feature_data{
		.name = "test_feature",
		.comment = "// test feature comment\n",
		.sections = {section},
	};

	SECTION("instance")
	{
		fmt::memory_buffer out;
		write_feature_instance_init_struct(out, feature);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	vk->fn_one = (PFN_fn_one)vk->vkGetInstanceProcAddr(instance, "fn_one");
	vk->fn_two = (PFN_fn_two)vk->vkGetInstanceProcAddr(instance, "fn_two");

#endif // defined(test_feature)
)");
	}

	SECTION("skip non-instance functions")
	{
		auto section2 = vgen::section_data{
			.comment = "// section comment\n",
			.commands = {"vkCreateInstance"s, "vkEnumerateInstanceExtensionProperties"s, "vkEnumerateInstanceLayerProperties"s, "fn_one"},
		};

		auto feature2 = vgen::feature_data{
			.name = "test_feature",
			.comment = "// test feature comment\n",
			.sections = {section2},
		};

		fmt::memory_buffer out;
		write_feature_instance_init_struct(out, feature2);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	vk->fn_one = (PFN_fn_one)vk->vkGetInstanceProcAddr(instance, "fn_one");

#endif // defined(test_feature)
)");
	}

	SECTION("device")
	{
		fmt::memory_buffer out;
		write_feature_device_init_struct(out, feature);

		REQUIRE(to_string(out) == R"(
#if defined(test_feature)

	vk->fn_one = (PFN_fn_one)vk->vkGetDeviceProcAddr(device, "fn_one");
	vk->fn_two = (PFN_fn_two)vk->vkGetDeviceProcAddr(device, "fn_two");

#endif // defined(test_feature)
)");
	}
}

TEST_CASE("write extension pointer init struct impl", "[writer]")
{
	SECTION("Instance init")
	{
		SECTION("one feature, multiple commands")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_foo)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetInstanceProcAddr(instance, "test_void");
	vk->test_int = (PFN_test_int)vk->vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_foo)
)");
		}

		SECTION("one feature, multiple requirements")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetInstanceProcAddr(instance, "test_void");
	vk->test_int = (PFN_test_int)vk->vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_bar) || defined(feature_foo)
)");
		}

		SECTION("two features")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_instance_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar)
	vk->test_int = (PFN_test_int)vk->vkGetInstanceProcAddr(instance, "test_int");
#endif // defined(feature_bar)
#if defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetInstanceProcAddr(instance, "test_void");
#endif // defined(feature_foo)
)");
		}
	}

	SECTION("Device init")
	{
		SECTION("one feature, multiple commands")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_foo)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetDeviceProcAddr(device, "test_void");
	vk->test_int = (PFN_test_int)vk->vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_foo)
)");
		}

		SECTION("one feature, multiple requirements")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_void" },
				{ {"defined(feature_foo)"s, "defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar) || defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetDeviceProcAddr(device, "test_void");
	vk->test_int = (PFN_test_int)vk->vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_bar) || defined(feature_foo)
)");
		}

		SECTION("two features")
		{
			// clang-format off
			auto defs = vgen::extension_map
			{
				{ {"defined(feature_foo)"s}, "test_void" },
				{ {"defined(feature_bar)"s}, "test_int" },
			};
			// clang-format on

			fmt::memory_buffer out;
			vgen::write_extensions_device_init_struct(out, defs);

			REQUIRE(to_string(out) == R"(#if defined(feature_bar)
	vk->test_int = (PFN_test_int)vk->vkGetDeviceProcAddr(device, "test_int");
#endif // defined(feature_bar)
#if defined(feature_foo)
	vk->test_void = (PFN_test_void)vk->vkGetDeviceProcAddr(device, "test_void");
#endif // defined(feature_foo)
)");
		}
	}
}

TEST_CASE("Filter non-device commands")
{
	// clang-format off
	const auto commands = std::unordered_map<std::string, vgen::command_data>
	{
		{
			"test_void"s, vgen::command_data
			{
				.name = "test_void",
				.prototype = "void test_void",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "// comment\n",
				.returns_void = true,
				.is_device_command = false,
			},
		},
		{
			"test_int"s, vgen::command_data
			{
				.name = "test_int",
				.prototype = "int test_int",
				.params = "Foo foo, Bar bar",
				.param_names = "foo, bar",
				.comment = "",
				.returns_void = false,
				.is_device_command = true,
			},
		},
	};
	// clang-format on

	SECTION("get_device_features")
	{
		auto sections = std::vector{
			vgen::section_data{
				.comment = "// section comment\n",
				.commands = {"test_void"s, "test_int"s},
			},
		};

		auto features = std::vector{vgen::feature_data{
			.name = "test_feature",
			.comment = "// test feature comment\n",
			.sections = sections,
		}};

		auto device_features = get_device_features(features, commands);
		REQUIRE(device_features.at(0).sections.at(0).commands.size() == 1);
		REQUIRE(device_features.at(0).sections.at(0).commands.at(0) == "test_int");
	}

	SECTION("get_device_features filters empty sections")
	{
		auto sections = std::vector{
			vgen::section_data{
				.comment = "// section comment\n",
				.commands = {"test_int"s},
			},
			vgen::section_data{
				.comment = "// section comment\n",
				.commands = {"test_void"s},
			},
		};

		auto features = std::vector{vgen::feature_data{
			.name = "test_feature",
			.comment = "// test feature comment\n",
			.sections = sections,
		}};

		auto device_features = get_device_features(features, commands);
		REQUIRE(device_features.at(0).sections.size() == 1);
		REQUIRE(device_features.at(0).sections.at(0).commands.size() == 1);
		REQUIRE(device_features.at(0).sections.at(0).commands.at(0) == "test_int");
	}

	SECTION("get_device_features filters empty features")
	{
		auto sections = std::vector{
			vgen::section_data{
				.comment = "// section comment\n",
				.commands = {"test_void"s},
			},
		};

		auto features = std::vector{vgen::feature_data{
			.name = "test_feature",
			.comment = "// test feature comment\n",
			.sections = sections,
		}};

		auto device_features = get_device_features(features, commands);
		REQUIRE(device_features.size() == 0);
	}

	SECTION("get_device_extensions")
	{
		auto extensions = vgen::extension_map{
			{std::set{"extension1"s}, "test_void"},
			{std::set{"extension1"s}, "test_int"},
		};

		auto device_extensions = get_device_extensions(extensions, commands);

		REQUIRE(device_extensions.count(std::set{"extension1"s}) == 1);
		REQUIRE(device_extensions.lower_bound(std::set{"extension1"s})->second == "test_int");
	}

	SECTION("get_device_extensions 2")
	{
		auto extensions = vgen::extension_map{
			{std::set{"extension1"s}, "test_void"},
		};

		auto device_extensions = get_device_extensions(extensions, commands);

		REQUIRE(device_extensions.empty());
		REQUIRE(device_extensions.count(std::set{"extension1"s}) == 0);
	}
}
