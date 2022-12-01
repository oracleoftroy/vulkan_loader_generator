#include <catch2/catch_test_macros.hpp>
#include <vgen.hpp>

#include <algorithm>
#include <array>
#include <string_view>

using namespace std::string_literals;
using namespace std::string_view_literals;

pugi::xml_document load_fragment(const std::string_view &xml)
{
	pugi::xml_document doc;
	doc.load_string(xml.data(), pugi::parse_default | pugi::parse_trim_pcdata);

	return doc;
}

TEST_CASE("read vulkan header version", "[parser]")
{
	auto doc = load_fragment(R"xml(<?xml version="1.0" encoding="UTF-8"?>
<registry>
    <types comment="Vulkan type definitions">
        <type category="define">// Version of this file
#define <name>VK_HEADER_VERSION</name> 42</type>
    </types>
</registry>
)xml");

	REQUIRE(vgen::read_vulkan_header_version(doc) == "42");
}

TEST_CASE("command parsing", "[command][parser]")
{
	auto doc = load_fragment(
		R"(        <command queues="transfer,graphics,compute" renderpass="outside" cmdbufferlevel="primary,secondary" pipeline="transfer" comment="transfer support is only available when VK_KHR_maintenance1 is enabled, as documented in valid usage language in the specification">
            <proto><type>void</type> <name>vkCmdFillBuffer</name></proto>
            <param externsync="true"><type>VkCommandBuffer</type> <name>commandBuffer</name></param>
            <param><type>VkBuffer</type> <name>dstBuffer</name></param>
            <param><type>VkDeviceSize</type> <name>dstOffset</name></param>
            <param><type>VkDeviceSize</type> <name>size</name></param>
            <param><type>uint32_t</type> <name>data</name></param>
        </command>
)");
	const auto &command_node = doc.document_element();

	SECTION("Sanity check")
	{
		REQUIRE(command_node.name() == "command"sv);
	}

	SECTION("read_full_text")
	{
		REQUIRE(vgen::read_full_text(command_node.child("proto")) == "void vkCmdFillBuffer");
	}

	SECTION("read_comment")
	{
		REQUIRE(vgen::read_comment(command_node) == "// transfer support is only available when VK_KHR_maintenance1 is enabled, as documented in valid usage language in the specification\n");
	}

	SECTION("read_comment with no comment")
	{
		pugi::xml_document doc_nocomment = load_fragment("<command></command>");
		REQUIRE(vgen::read_comment(doc_nocomment.document_element()) == "");
	}

	SECTION("is_device_command")
	{
		REQUIRE(vgen::is_device_command(command_node) == true);
	}

	SECTION("read_command")
	{
		auto command = vgen::read_command(command_node);
		REQUIRE(command.comment == "// transfer support is only available when VK_KHR_maintenance1 is enabled, as documented in valid usage language in the specification\n");
		REQUIRE(command.is_device_command == true);
		REQUIRE(command.name == "vkCmdFillBuffer");
		REQUIRE(command.params == "VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data");
		REQUIRE(command.param_names == "commandBuffer, dstBuffer, dstOffset, size, data");
		REQUIRE(command.prototype == "void vkCmdFillBuffer");
		REQUIRE(command.returns_void == true);
	}

	SECTION("vkDestroyInstance is not device level")
	{
		auto vkDestroyInstanceCmd = load_fragment(R"(        <command>
            <proto><type>void</type> <name>vkDestroyInstance</name></proto>
            <param optional="true" externsync="true"><type>VkInstance</type> <name>instance</name></param>
            <param optional="true">const <type>VkAllocationCallbacks</type>* <name>pAllocator</name></param>
            <implicitexternsyncparams>
                <param>all sname:VkPhysicalDevice objects enumerated from pname:instance</param>
            </implicitexternsyncparams>
        </command>
)");
		REQUIRE(vgen::is_device_command(vkDestroyInstanceCmd.document_element()) == false);
	}
}

TEST_CASE("feature parsing", "[feature][parser]")
{
	auto doc = load_fragment(
		R"xml(    <feature api="vulkan" name="VK_VERSION_1_2" number="1.2" comment="Vulkan 1.2 core API interface definitions.">
        <require>
            <type name="VK_API_VERSION_1_2"/>
        </require>
        <require>
            <enum extends="VkStructureType" value="49" name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES"/>
            <enum extends="VkStructureType" value="50" name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES"/>
            <enum extends="VkStructureType" value="51" name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES"/>
            <enum extends="VkStructureType" value="52" name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES"/>
            <type name="VkPhysicalDeviceVulkan11Features"/>
            <type name="VkPhysicalDeviceVulkan11Properties"/>
            <type name="VkPhysicalDeviceVulkan12Features"/>
            <type name="VkPhysicalDeviceVulkan12Properties"/>
        </require>
        <require comment="Promoted from VK_KHR_draw_indirect_count (extension 170)">
            <command name="vkCmdDrawIndirectCount"/>
            <command name="vkCmdDrawIndexedIndirectCount"/>
        </require>
        <require comment="Promoted from VK_KHR_create_renderpass2 (extension 110)">
            <enum offset="0" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2"/>
            <enum offset="1" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2"/>
            <enum offset="2" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2"/>
            <enum offset="3" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2"/>
            <enum offset="4" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2"/>
            <enum offset="5" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO"/>
            <enum offset="6" extends="VkStructureType" extnumber="110"          name="VK_STRUCTURE_TYPE_SUBPASS_END_INFO"/>
            <command name="vkCreateRenderPass2"/>
            <command name="vkCmdBeginRenderPass2"/>
            <command name="vkCmdNextSubpass2"/>
            <command name="vkCmdEndRenderPass2"/>
            <type name="VkRenderPassCreateInfo2"/>
            <type name="VkAttachmentDescription2"/>
            <type name="VkAttachmentReference2"/>
            <type name="VkSubpassDescription2"/>
            <type name="VkSubpassDependency2"/>
            <type name="VkSubpassBeginInfo"/>
            <type name="VkSubpassEndInfo"/>
        </require>
    </feature>
)xml");

	auto feature = vgen::read_feature(doc.document_element());

	SECTION("read_feature")
	{
		REQUIRE(feature.comment == "// Vulkan 1.2 core API interface definitions.\n");
		REQUIRE(feature.name == "VK_VERSION_1_2");

		// though there are four <require> sections in the sample document, only two of them have commands, and we only care about commands
		REQUIRE(feature.sections.size() == 2);
	}

	SECTION("read_feature sections")
	{
		REQUIRE(feature.sections[0].comment == "// Promoted from VK_KHR_draw_indirect_count (extension 170)\n");
		REQUIRE(feature.sections[1].comment == "// Promoted from VK_KHR_create_renderpass2 (extension 110)\n");

		REQUIRE(feature.sections[0].commands.size() == 2);
		REQUIRE(feature.sections[1].commands.size() == 4);

		REQUIRE(feature.sections[0].commands == std::vector{"vkCmdDrawIndirectCount"s, "vkCmdDrawIndexedIndirectCount"s});
		REQUIRE(feature.sections[1].commands == std::vector{"vkCreateRenderPass2"s, "vkCmdBeginRenderPass2"s, "vkCmdNextSubpass2"s, "vkCmdEndRenderPass2"s});
	}
}

TEST_CASE("extension parsing", "[extension][parser]")
{
	// Using problematic extensions, command vkCmdPushDescriptorSetWithTemplateKHR is declared multiple times
	auto doc = load_fragment(
		R"xml(<?xml version="1.0" encoding="UTF-8"?>
<registry>
    <extensions comment="Vulkan extension interface definitions">
        <extension name="VK_KHR_push_descriptor" number="81" type="device" author="KHR" requires="VK_KHR_get_physical_device_properties2" contact="Jeff Bolz @jeffbolznv" supported="vulkan">
            <require>
                <enum value="2"                                             name="VK_KHR_PUSH_DESCRIPTOR_SPEC_VERSION"/>
                <enum value="&quot;VK_KHR_push_descriptor&quot;"            name="VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME"/>
                <enum offset="0" extends="VkStructureType"                  name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR"/>
                <enum bitpos="0" extends="VkDescriptorSetLayoutCreateFlagBits"   name="VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR"  comment="Descriptors are pushed via flink:vkCmdPushDescriptorSetKHR"/>
                <command name="vkCmdPushDescriptorSetKHR"/>
                <type name="VkPhysicalDevicePushDescriptorPropertiesKHR"/>
            </require>
            <require feature="VK_VERSION_1_1">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
            <require extension="VK_KHR_descriptor_update_template">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
        </extension>
        <extension name="VK_KHR_descriptor_update_template" number="86" type="device" author="KHR" contact="Markus Tavenrath @mtavenrath" supported="vulkan" promotedto="VK_VERSION_1_1">
            <require>
                <enum value="1"                                             name="VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_SPEC_VERSION"/>
                <enum value="&quot;VK_KHR_descriptor_update_template&quot;" name="VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME"/>
                <enum extends="VkStructureType"                             name="VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR" alias="VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO"/>
                <enum extends="VkObjectType"                                name="VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR" alias="VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE"/>
                <command name="vkCreateDescriptorUpdateTemplateKHR"/>
                <command name="vkDestroyDescriptorUpdateTemplateKHR"/>
                <command name="vkUpdateDescriptorSetWithTemplateKHR"/>
                <type name="VkDescriptorUpdateTemplateKHR"/>
                <type name="VkDescriptorUpdateTemplateCreateFlagsKHR"/>
                <type name="VkDescriptorUpdateTemplateTypeKHR"/>
                <type name="VkDescriptorUpdateTemplateEntryKHR"/>
                <type name="VkDescriptorUpdateTemplateCreateInfoKHR"/>
                <enum extends="VkDescriptorUpdateTemplateType"              name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR" alias="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET"/>
            </require>
            <require extension="VK_KHR_push_descriptor">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
            <require extension="VK_EXT_debug_report">
                <enum extends="VkDebugReportObjectTypeEXT"                  name="VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR_EXT" alias="VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT"/>
            </require>
        </extension>
    </extensions>
</registry>
)xml");

	auto extensions = vgen::read_extensions(doc);

	SECTION("read_extensions")
	{
		auto key1 = std::set{"defined(VK_KHR_push_descriptor)"s};
		// clang-format off
        auto key2 = std::set
        {
			"defined(VK_KHR_push_descriptor) && defined(VK_VERSION_1_1)"s,
            "defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)"s,
            "defined(VK_KHR_descriptor_update_template) && defined(VK_KHR_push_descriptor)"s
        };
		// clang-format on
		auto key3 = std::set{"defined(VK_KHR_descriptor_update_template)"s};

		REQUIRE(extensions.size() == 5);
		REQUIRE(extensions.count(key1) == 1);
		REQUIRE(extensions.count(key2) == 1);
		REQUIRE(extensions.count(key3) == 3);

		REQUIRE(extensions.find(key1)->second == "vkCmdPushDescriptorSetKHR");
		REQUIRE(extensions.find(key2)->second == "vkCmdPushDescriptorSetWithTemplateKHR");

		auto range = extensions.equal_range(key3);
		std::array expected{"vkCreateDescriptorUpdateTemplateKHR"s, "vkDestroyDescriptorUpdateTemplateKHR"s, "vkUpdateDescriptorSetWithTemplateKHR"s};
		REQUIRE(std::equal(range.first, range.second, begin(expected), end(expected), [](const auto &x, const auto &y) { return x.second == y; }));
	}
}

TEST_CASE("skip disabled extensions", "[extension][parser]")
{
	auto doc = load_fragment(
		R"xml(<?xml version="1.0" encoding="UTF-8"?>
<registry>
    <extensions comment="Vulkan extension interface definitions">
        <extension name="VK_KHR_push_descriptor" number="81" type="device" author="KHR" requires="VK_KHR_get_physical_device_properties2" contact="Jeff Bolz @jeffbolznv" supported="disabled">
            <require>
                <enum value="2"                                             name="VK_KHR_PUSH_DESCRIPTOR_SPEC_VERSION"/>
                <enum value="&quot;VK_KHR_push_descriptor&quot;"            name="VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME"/>
                <enum offset="0" extends="VkStructureType"                  name="VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PUSH_DESCRIPTOR_PROPERTIES_KHR"/>
                <enum bitpos="0" extends="VkDescriptorSetLayoutCreateFlagBits"   name="VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR"  comment="Descriptors are pushed via flink:vkCmdPushDescriptorSetKHR"/>
                <command name="vkCmdPushDescriptorSetKHR"/>
                <type name="VkPhysicalDevicePushDescriptorPropertiesKHR"/>
            </require>
            <require feature="VK_VERSION_1_1">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
            <require extension="VK_KHR_descriptor_update_template">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
        </extension>
        <extension name="VK_KHR_descriptor_update_template" number="86" type="device" author="KHR" contact="Markus Tavenrath @mtavenrath" supported="disabled" promotedto="VK_VERSION_1_1">
            <require>
                <enum value="1"                                             name="VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_SPEC_VERSION"/>
                <enum value="&quot;VK_KHR_descriptor_update_template&quot;" name="VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME"/>
                <enum extends="VkStructureType"                             name="VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR" alias="VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO"/>
                <enum extends="VkObjectType"                                name="VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR" alias="VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE"/>
                <command name="vkCreateDescriptorUpdateTemplateKHR"/>
                <command name="vkDestroyDescriptorUpdateTemplateKHR"/>
                <command name="vkUpdateDescriptorSetWithTemplateKHR"/>
                <type name="VkDescriptorUpdateTemplateKHR"/>
                <type name="VkDescriptorUpdateTemplateCreateFlagsKHR"/>
                <type name="VkDescriptorUpdateTemplateTypeKHR"/>
                <type name="VkDescriptorUpdateTemplateEntryKHR"/>
                <type name="VkDescriptorUpdateTemplateCreateInfoKHR"/>
                <enum extends="VkDescriptorUpdateTemplateType"              name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR" alias="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET"/>
            </require>
            <require extension="VK_KHR_push_descriptor">
                <command name="vkCmdPushDescriptorSetWithTemplateKHR"/>
                <enum value="1" extends="VkDescriptorUpdateTemplateType"    name="VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_PUSH_DESCRIPTORS_KHR" comment="Create descriptor update template for pushed descriptor updates"/>
            </require>
            <require extension="VK_EXT_debug_report">
                <enum extends="VkDebugReportObjectTypeEXT"                  name="VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR_EXT" alias="VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_EXT"/>
            </require>
        </extension>
    </extensions>
</registry>
)xml");

	auto extensions = vgen::read_extensions(doc);
	REQUIRE(extensions.size() == 0);
}
