#include "application.hpp"

#include <imgui.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace application {

// ===== ШАГ 0 =====

struct Vertex {
	float position[3];
	float color[3];
};

struct GlobalUniforms {
	float matrix[4][4];
};

VkBuffer vk_vertex_buffer;
VmaAllocation vk_vertex_buffer_allocation;
void* vk_vertex_buffer_memory;

VkBuffer vk_index_buffer;
VmaAllocation vk_index_buffer_allocation;
void* vk_index_buffer_memory;

VkBuffer vk_uniform_buffer_global;
VmaAllocation vk_uniform_buffer_global_allocation;
GlobalUniforms* vk_uniform_buffer_global_memory;

VkDescriptorSetLayout vk_descriptor_set_layout;
VkDescriptorPool vk_descriptor_pool;
VkDescriptorSet vk_descriptor_set;

VkPipelineLayout vk_pipeline_layout;
VkPipeline vk_pipeline;

VkShaderModule loadShaderModule(const char path[]) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	const size_t size = file.tellg();
	std::vector<uint32_t> buffer(size / sizeof(uint32_t));

	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), size);
	file.close();

	VkShaderModuleCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = buffer.data(),
	};

	VkShaderModule result;
	if (vkCreateShaderModule(graphics::internal::context.device,
	                         &info, nullptr, &result) != VK_SUCCESS) {
		return nullptr;
	}

	return result;
}

bool initialize() {
	auto& context = graphics::internal::context;

	// ===== ШАГ 1.1 =====
	const Vertex vertices[] = {
		{ { -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f, 0.0f } }, // 0
		{ { +0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f, 0.0f } }, // 1
		{ { +0.5f, +0.5f, -0.5f }, { 1.0f, 1.0f, 0.0f } }, // 2
		{ { -0.5f, +0.5f, -0.5f }, { 0.0f, 1.0f, 0.0f } }, // 3
		{ { -0.5f, -0.5f, +0.5f }, { 0.0f, 0.0f, 1.0f } }, // 4
		{ { +0.5f, -0.5f, +0.5f }, { 1.0f, 0.0f, 1.0f } }, // 5
		{ { +0.5f, +0.5f, +0.5f }, { 1.0f, 1.0f, 1.0f } }, // 6
		{ { -0.5f, +0.5f, +0.5f }, { 0.0f, 1.0f, 1.0f } }, // 7
	};

	const VkBufferCreateInfo vertex_buffer = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(vertices),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	// ===== ШАГ 1.2 =====
	const VmaAllocationCreateInfo vertex_buffer_allocation = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &vertex_buffer, &vertex_buffer_allocation,
	                    &vk_vertex_buffer, &vk_vertex_buffer_allocation,
	                    nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create and allocate vertex buffer\n";
		return false;
	}

	// ===== ШАГ 1.3 =====
	if (vmaMapMemory(context.allocator, vk_vertex_buffer_allocation,
	                 (void **)&vk_vertex_buffer_memory) != VK_SUCCESS) {
		std::cerr << "Failed to map vertex buffer memory\n";
		return false;
	}

	memcpy(vk_vertex_buffer_memory, vertices, sizeof(vertices));

	// ===== ШАГ 1.4 =====
	const uint32_t indices[] = {
		0, 1, 2,   0, 2, 3, // передняя
		5, 4, 7,   5, 7, 6, // задняя
		4, 0, 3,   4, 3, 7, // левая
		1, 5, 6,   1, 6, 2, // правая
		1, 0, 4,   1, 4, 5, // верхняя
		3, 2, 6,   3, 6, 7, // нижняя
	};

	const VkBufferCreateInfo index_buffer = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(indices),
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo index_buffer_allocation = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &index_buffer, &index_buffer_allocation,
	                    &vk_index_buffer, &vk_index_buffer_allocation,
	                    nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create and allocate index buffer\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, vk_index_buffer_allocation,
	                 (void **)&vk_index_buffer_memory) != VK_SUCCESS) {
		std::cerr << "Failed to map index buffer memory\n";
		return false;
	}

	memcpy(vk_index_buffer_memory, indices, sizeof(indices));

	// ===== ШАГ 1.5 =====
	const VkBufferCreateInfo global_uniform_buffer = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = (sizeof(GlobalUniforms) + 0xf) & ~0xf,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo global_uniform_buffer_allocation = {
		.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
		         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO,
	};

	if (vmaCreateBuffer(context.allocator, &global_uniform_buffer,
	                    &global_uniform_buffer_allocation,
	                    &vk_uniform_buffer_global, &vk_uniform_buffer_global_allocation,
	                    nullptr) != VK_SUCCESS) {
		std::cerr << "Failed to create and allocate uniform buffer\n";
		return false;
	}

	if (vmaMapMemory(context.allocator, vk_uniform_buffer_global_allocation,
	                 (void **)&vk_uniform_buffer_global_memory) != VK_SUCCESS) {
		std::cerr << "Failed to map uniform buffer memory\n";
		return false;
	}

	// ===== ШАГ 2.1 =====
	const VkDescriptorSetLayoutBinding descriptor_set_bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	};

	// ===== ШАГ 2.2 =====
	const VkDescriptorSetLayoutCreateInfo descriptor_set_layout = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = sizeof(descriptor_set_bindings) / sizeof(descriptor_set_bindings[0]),
		.pBindings = descriptor_set_bindings,
	};

	if (vkCreateDescriptorSetLayout(context.device, &descriptor_set_layout, nullptr,
	                                &vk_descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	// ===== ШАГ 2.3 =====
	const VkPipelineLayoutCreateInfo pipeline_layout = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout,
	};

	if (vkCreatePipelineLayout(context.device, &pipeline_layout, nullptr,
	                           &vk_pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline layout\n";
		return false;
	}

	// ===== ШАГ 2.4 =====
	const VkDescriptorPoolSize descriptor_pool_sizes[] = {
		{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 4,
		},
	};

	// ===== ШАГ 2.5 =====
	const VkDescriptorPoolCreateInfo descriptor_pool = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = sizeof(descriptor_pool_sizes) / sizeof(descriptor_pool_sizes[0]),
		.pPoolSizes = descriptor_pool_sizes,
	};

	if (vkCreateDescriptorPool(context.device, &descriptor_pool, nullptr,
	                           &vk_descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	// ===== ШАГ 2.6 =====
	const VkDescriptorSetAllocateInfo descriptor_set = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout,
	};

	if (vkAllocateDescriptorSets(context.device, &descriptor_set,
	                             &vk_descriptor_set) != VK_SUCCESS) {
		std::cerr << "Failed to allocate descriptor set\n";
		return false;
	}

	// ===== ШАГ 2.7 =====
	const VkDescriptorBufferInfo global_uniform_buffer_descriptor = {
		.buffer = vk_uniform_buffer_global,
		.offset = 0,
		.range = sizeof(GlobalUniforms),
	};

	const VkWriteDescriptorSet descriptor_writes[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = vk_descriptor_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &global_uniform_buffer_descriptor,
		},
	};

	vkUpdateDescriptorSets(context.device, sizeof(descriptor_writes) / sizeof(descriptor_writes[0]),
	                       descriptor_writes, 0, nullptr);

	// ===== ШАГ 3.1 =====
	VkShaderModule vk_vertex_shader = loadShaderModule("shaders/cube.vert.spv");
	VkShaderModule vk_fragment_shader = loadShaderModule("shaders/cube.frag.spv");

	// ===== ШАГ 3.2 =====
	VkPipelineShaderStageCreateInfo stage_infos[2];
	stage_infos[0] = VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vk_vertex_shader,
		.pName = "main",
	};
	stage_infos[1] = VkPipelineShaderStageCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = vk_fragment_shader,
		.pName = "main",
	};

	// ===== ШАГ 3.3 =====
	const VkVertexInputBindingDescription vertex_bindings[] = {
		{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		},
	};

	// ===== ШАГ 3.4 =====
	const VkVertexInputAttributeDescription vertex_attributes[] = {
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, position),
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, color),
		},
	};

	// ===== ШАГ 3.5 =====
	VkPipelineVertexInputStateCreateInfo input_state_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = vertex_bindings,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	// ===== ШАГ 3.6 =====
	VkPipelineInputAssemblyStateCreateInfo assembly_state_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	// ===== ШАГ 3.7 =====
	VkPipelineViewportStateCreateInfo viewport_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	// ===== ШАГ 3.8 =====
	VkPipelineRasterizationStateCreateInfo raster_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_BACK_BIT,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	VkPipelineMultisampleStateCreateInfo sample_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	// ===== ШАГ 3.9 =====
	VkPipelineDepthStencilStateCreateInfo depth_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = true,
		.depthWriteEnable = true,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
	};

	// ===== ШАГ 3.10 =====
	VkPipelineColorBlendAttachmentState attachment_info{
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                  VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT,
	};

	VkPipelineColorBlendStateCreateInfo blend_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &attachment_info
	};

	// ===== ШАГ 3.11 =====
	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]),
		.pDynamicStates = dynamic_states,
	};

	// ===== ШАГ 3.12 =====
	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = stage_infos,
		.pVertexInputState = &input_state_info,
		.pInputAssemblyState = &assembly_state_info,
		.pViewportState = &viewport_info,
		.pRasterizationState = &raster_info,
		.pMultisampleState = &sample_info,
		.pDepthStencilState = &depth_info,
		.pColorBlendState = &blend_info,
		.pDynamicState = &dynamic_state,
		.layout = vk_pipeline_layout,
		.renderPass = context.render_pass,
	};

	// ===== ШАГ 3.13 =====
	if (vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info,
	                              nullptr, &vk_pipeline) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline\n";
		return false;
	}

	vkDestroyShaderModule(context.device, vk_vertex_shader, nullptr);
	vkDestroyShaderModule(context.device, vk_fragment_shader, nullptr);

	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	// ===== ШАГ 6 =====
	vkDestroyPipeline(context.device, vk_pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, vk_pipeline_layout, nullptr);
	vkDestroyDescriptorPool(context.device, vk_descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, vk_descriptor_set_layout, nullptr);

	vmaUnmapMemory(context.allocator, vk_uniform_buffer_global_allocation);
	vmaDestroyBuffer(context.allocator, vk_uniform_buffer_global, vk_uniform_buffer_global_allocation);

	vmaUnmapMemory(context.allocator, vk_index_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_index_buffer, vk_index_buffer_allocation);

	vmaUnmapMemory(context.allocator, vk_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_vertex_buffer, vk_vertex_buffer_allocation);
}

void update([[maybe_unused]] double time) {
	ImGui::ShowDemoWindow();

	// ===== ШАГ 5 =====
	// Сдвиг на 0.5 по Z: без него передняя грань (z = -0.5) выходит за диапазон глубины [0, 1].
	// GLSL хранит матрицу по столбцам, поэтому последняя строка массива — это столбец сдвига.
	const float identity[4][4] = {
		{ 1, 0, 0, 0 },
		{ 0, 1, 0, 0 },
		{ 0, 0, 1, 0 },
		{ 0, 0, 0.5f, 1 },
	};

	memcpy(vk_uniform_buffer_global_memory->matrix, identity, sizeof(identity));
}

void render(const graphics::internal::FrameData& fd) {
	auto& context = graphics::internal::context;

	vkResetCommandBuffer(fd.command_buffer, 0);

	const VkCommandBufferBeginInfo command_buffer_begin = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};

	vkBeginCommandBuffer(fd.command_buffer, &command_buffer_begin);

	const VkClearValue clear_values[] = {
		{ .color = { .float32 = { 0.1f, 0.1f, 0.12f, 1.0f } } },
		{ .depthStencil = { .depth = 1.0f, .stencil = 0 } },
	};

	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = context.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = context.swapchain_extent },
		.clearValueCount = sizeof(clear_values) / sizeof(clear_values[0]),
		.pClearValues = clear_values,
	};

	vkCmdBeginRenderPass(fd.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

	// ===== ШАГ 4.1 =====
	const VkViewport viewport = {
		.x = 0, .y = 0,
		.width = float(context.swapchain_extent.width),
		.height = float(context.swapchain_extent.height),
		.minDepth = 0, .maxDepth = 1,
	};

	const VkRect2D scissor = { .extent = context.swapchain_extent };

	vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
	vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

	// ===== ШАГ 4.2 – 4.5 =====
	const VkDeviceSize vertex_buffer_offset = 0;
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, &vertex_buffer_offset);
	vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

	vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
	                        vk_pipeline_layout, 0, 1, &vk_descriptor_set,
	                        0, nullptr);

	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

	vkCmdDrawIndexed(fd.command_buffer, 36, 1, 0, 0, 0);

	vkCmdEndRenderPass(fd.command_buffer);

	vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application
