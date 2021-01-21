#include "render_to_texture.h"

#include <iostream>
#include <array>

#include "vk_initializers.h"
#include "vk_textures.h"
#include "glm/glm.hpp"
#include "glm/gtx/transform.hpp"

// 6 faces per cube, 2 traingles per face, 3 vertices per triangle
constexpr uint32_t NUM_VERTICES_PLANE{ 2 * 3 };
constexpr uint32_t NUM_VERTICES_CUBE{ 6 * NUM_VERTICES_PLANE };

glm::vec3 vertexDataCube[NUM_VERTICES_CUBE]{
	// front
	{ 1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0}, {-1.0, -1.0,  1.0},
	{ 1.0, -1.0,  1.0}, { 1.0,  1.0,  1.0}, {-1.0,  1.0,  1.0},

	// back
	{ 1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0,  1.0, -1.0},
	{ 1.0,  1.0, -1.0}, { 1.0, -1.0, -1.0}, {-1.0, -1.0, -1.0},

	// up
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0}, { 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0}, {-1.0,  1.0, -1.0}, {-1.0,  1.0,  1.0},

	// down
	{-1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0}, {-1.0, -1.0,  1.0},
	{-1.0, -1.0, -1.0}, { 1.0, -1.0, -1.0}, { 1.0, -1.0,  1.0},

	// right
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0,  1.0}, { 1.0, -1.0,  1.0},
	{ 1.0, -1.0, -1.0}, { 1.0,  1.0, -1.0}, { 1.0,  1.0,  1.0},

	// left
	{-1.0,  1.0, -1.0}, {-1.0, -1.0,  1.0}, {-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0}, {-1.0, -1.0, -1.0}, {-1.0, -1.0,  1.0},
};

glm::vec3 vertexDataPlane[NUM_VERTICES_PLANE]{
	{ 1.0, -1.0,  0.5}, {-1.0,  1.0,  0.5}, {-1.0, -1.0,  0.5},
	{ 1.0, -1.0,  0.5}, { 1.0,  1.0,  0.5}, {-1.0,  1.0,  0.5},
};

struct PushConstantData
{
	glm::mat4 rotMat;
	float roughness;
};

// Since Vulkan's coordinate system has y going down, we rotate the cube by 180 degrees in each case except up and down,
// in which case idk why they vulkan uses these orientations, I just matched them...
const glm::mat4 rotate180{ glm::rotate(glm::radians(180.0f), glm::vec3{0.0, 0.0, 1.0}) };
const glm::mat4 rotate90{ glm::rotate(glm::radians(90.0f), glm::vec3{0.0, 0.0, 1.0}) };
const glm::mat4 rotate270{ glm::rotate(glm::radians(270.0f), glm::vec3{0.0, 0.0, 1.0}) };

glm::mat4 rotationMatrices[6]{
	rotate180 * glm::mat4(1.0),													// front
	rotate180 * glm::rotate(glm::radians(180.0f), glm::vec3{0.0, 1.0, 0.0}),	// back
	rotate90 * glm::rotate(glm::radians(90.0f), glm::vec3{1.0, 0.0, 0.0}),		// up
	rotate270 * glm::rotate(glm::radians(-90.0f), glm::vec3{1.0, 0.0, 0.0}),	// down
	rotate180 * glm::rotate(glm::radians(-90.0f), glm::vec3{0.0, 1.0, 0.0}),	// right
	rotate180 * glm::rotate(glm::radians(90.0f), glm::vec3{0.0, 1.0, 0.0}),		// left
};

void create_rt_framebuffer(VulkanEngine& engine, VkRenderPass renderpass, VkExtent2D extent, VkImageView attachment, VkFramebuffer* outFramebuffer)
{
	VkFramebufferCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	info.pNext = nullptr;
	info.renderPass = renderpass;
	info.width = extent.width;
	info.height = extent.height;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.layers = 1;

	vkCreateFramebuffer(engine._device, &info, nullptr, outFramebuffer);
}

void create_rt_renderpass(VulkanEngine& engine, VkFormat format, VkRenderPass* outRenderpass)
{
	VkAttachmentDescription attachment{};
	attachment.format = format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	// don't care what previous layout was, but undefined means the content isn't
	// guaranteed to be preserved.
	// since we are clearing it then writing into it, it doesn't matter the previous layout
	// since the implementation will know the layout it writes to it
	attachment.initialLayout - VK_IMAGE_LAYOUT_UNDEFINED;
	// final layout is automatically transitioned to at end of renderpass
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentReference attachmentRef{};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &attachmentRef;
	subpass.pDepthStencilAttachment = nullptr;

	VkRenderPassCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.pNext = nullptr;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 0;
	info.pDependencies = nullptr;

	vkCreateRenderPass(engine._device, &info, nullptr, outRenderpass);
}

void create_rt_pipeline_layout(VulkanEngine& engine, VkDescriptorSetLayout layout, VkPipelineLayout* outLayout)
{
	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(PushConstantData);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	info.pushConstantRangeCount = 1;
	info.pPushConstantRanges = &pushConstant;
	info.setLayoutCount = 1;
	info.pSetLayouts = &layout;

	vkCreatePipelineLayout(engine._device, &info, nullptr, outLayout);
}


// todo: cleanup this pipeline creation, then test that everything is working for 1 face of the cube, then put in loop for all 6 faces!!!!!!!!!!!!!

void create_rt_pipeline(VulkanEngine& engine, VkRenderPass renderpass, VkExtent2D extent, VkPipelineLayout layout, VkPipeline* outPipeline, const std::string& vertPath, const std::string& fragPath)
{
	std::string prefix{ "../../shaders/" };

	VkShaderModule vertShader;
	if (!engine.load_shader_module(prefix + vertPath, &vertShader)) {
		std::cout << "Error when building vertex shader module: " << (prefix + vertPath) << "\n";
	}

	VkShaderModule fragShader;
	if (!engine.load_shader_module(prefix + fragPath, &fragShader)) {
		std::cout << "Error when building fragment shader module: " << (prefix + fragPath) << "\n";
	}

	PipelineBuilder pipelineBuilder;
	// vertex input controls how to read vertices from vertex buffers
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();
	// input assembly is the configuration for drawing triangle lists, strips, or individual points
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	// build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float)extent.width;
	pipelineBuilder._viewport.height = (float)extent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;
	pipelineBuilder._scissor.offset = { 0, 0 };
	pipelineBuilder._scissor.extent = extent;
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();
	pipelineBuilder._depthStencil = vkinit::depth_stencil_create_info(false, false, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkVertexInputAttributeDescription attribute{};
	attribute.binding = 0;
	attribute.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	attribute.location = 0;
	attribute.offset = 0;

	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDescription.stride = sizeof(glm::vec3);

	pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = 1;
	pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = &attribute;
	pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = 1;
	pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	pipelineBuilder._pipelineLayout = layout;

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertShader)
	);

	pipelineBuilder._shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragShader)
	);

	*outPipeline = pipelineBuilder.build_pipeline(engine._device, renderpass);

	vkDestroyShaderModule(engine._device, vertShader, nullptr);
	vkDestroyShaderModule(engine._device, fragShader, nullptr);
}

AllocatedBuffer create_rt_vertex_buffer(VulkanEngine& engine, size_t bufferSize, void* cpuArray)
{
	AllocatedBuffer vertexBuffer{ engine.create_buffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU) };

	void* data;
	vmaMapMemory(engine._allocator, vertexBuffer._allocation, &data);
	std::memcpy(data, cpuArray, bufferSize);
	vmaUnmapMemory(engine._allocator, vertexBuffer._allocation);

	return vertexBuffer;
}

void create_rt_image(VulkanEngine& engine, VkExtent2D extent, VkFormat format, uint32_t mipLevels, bool isCubemap, AllocatedImage* outCubemap)
{
	VkExtent3D extent3D{};
	extent3D.depth = 1;
	extent3D.height = extent.height;
	extent3D.width = extent.width;

	VkImageCreateInfo cubemapInfo{};
	cubemapInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	cubemapInfo.pNext = nullptr;
	cubemapInfo.extent = extent3D;
	cubemapInfo.format = format;
	cubemapInfo.imageType = VK_IMAGE_TYPE_2D;
	cubemapInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	cubemapInfo.mipLevels = mipLevels;
	cubemapInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	cubemapInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	cubemapInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (isCubemap) {
		cubemapInfo.arrayLayers = 6;
		cubemapInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	} else {
		cubemapInfo.arrayLayers = 1;
	}

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	vmaCreateImage(engine._allocator, &cubemapInfo, &allocInfo, &outCubemap->_image, &outCubemap->_allocation, nullptr);
}

Texture render_to_texture(VulkanEngine& engine, VkDescriptorSet equirectangularSet, VkExtent2D extent, bool useMipmap, bool isCubemap, const std::string& vertPath, const std::string& fragPath)
{
	VkFormat hdriFormat{ VK_FORMAT_R32G32B32A32_SFLOAT };

	uint32_t mipLevels{ useMipmap ? vkutil::get_mip_levels(extent.width, extent.height) : 1 };

	VkDescriptorSetLayoutBinding binding{};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = 1;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.pNext = nullptr;
	descriptorSetLayoutInfo.bindingCount = 1;
	descriptorSetLayoutInfo.pBindings = &binding;

	VkDescriptorSetLayout setLayout;
	VK_CHECK(vkCreateDescriptorSetLayout(engine._device, &descriptorSetLayoutInfo, nullptr, &setLayout));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyDescriptorSetLayout(engine._device, setLayout, nullptr);
	});

	VkPipelineLayout pipelineLayout;
	create_rt_pipeline_layout(engine, setLayout, &pipelineLayout);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyPipelineLayout(engine._device, pipelineLayout, nullptr);
	});

	VkRenderPass renderpass;
	create_rt_renderpass(engine, hdriFormat, &renderpass);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyRenderPass(engine._device, renderpass, nullptr);
	});

	AllocatedImage allocatedImage;
	create_rt_image(engine, extent, hdriFormat, mipLevels, isCubemap, &allocatedImage);
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vmaDestroyImage(engine._allocator, allocatedImage._image, allocatedImage._allocation);
	});

	uint32_t numVertices{ isCubemap ? NUM_VERTICES_CUBE : NUM_VERTICES_PLANE };
	glm::vec3* vertexData{ isCubemap ? vertexDataCube : vertexDataPlane };
	AllocatedBuffer vertexBuffer{ create_rt_vertex_buffer(engine, numVertices * sizeof(glm::vec3), vertexData) };
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vmaDestroyBuffer(engine._allocator, vertexBuffer._buffer, vertexBuffer._allocation);
	});

	for (auto mipLevel{ 0 }; mipLevel < mipLevels; ++mipLevel) {
		VkPipeline pipeline;
		create_rt_pipeline(engine, renderpass, extent, pipelineLayout, &pipeline, vertPath, fragPath);
		engine._mainDeletionQueue.push_function([=, &engine]() {
			vkDestroyPipeline(engine._device, pipeline, nullptr);
		});

		uint32_t numLayers{ isCubemap ? 6u : 1u };
		for (auto layer{ 0 }; layer < numLayers; ++layer) {

			VkImageViewUsageCreateInfo viewUsage{};
			viewUsage.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
			viewUsage.pNext = nullptr;
			viewUsage.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

			// This is a temporary image view only used as an attachment to the framebuffer
			VkImageViewCreateInfo attachmentViewInfo{};
			attachmentViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			attachmentViewInfo.pNext = &viewUsage;
			attachmentViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			attachmentViewInfo.image = allocatedImage._image;
			attachmentViewInfo.format = hdriFormat;
			// using subresourceRange you can let the image view look at only particular layers of the image
			attachmentViewInfo.subresourceRange.baseMipLevel = mipLevel;
			attachmentViewInfo.subresourceRange.levelCount = 1;
			attachmentViewInfo.subresourceRange.baseArrayLayer = layer;
			attachmentViewInfo.subresourceRange.layerCount = 1;
			attachmentViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			VkImageView attachmentView;
			VK_CHECK(vkCreateImageView(engine._device, &attachmentViewInfo, nullptr, &attachmentView));
			engine._mainDeletionQueue.push_function([=, &engine]() {
				vkDestroyImageView(engine._device, attachmentView, nullptr);
			});

			VkFramebuffer framebuffer;
			create_rt_framebuffer(engine, renderpass, extent, attachmentView, &framebuffer);
			engine._mainDeletionQueue.push_function([=, &engine]() {
				vkDestroyFramebuffer(engine._device, framebuffer, nullptr);
			});

			engine.immediate_submit([=](VkCommandBuffer cmd) {
				VkClearValue clearValue{};
				clearValue.color = { {0.01, 0.01, 0.02, 1.0} };
				VkClearValue depthClear{};
				depthClear.depthStencil.depth = 1.0f;

				std::array<VkClearValue, 2> clearValues{ clearValue, depthClear };

				// start the main renderpass. we will use the clear color from above,
				// and the framebuffer corresponding to the index the swapchain gave us
				VkRenderPassBeginInfo rpInfo{};
				rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				rpInfo.pNext = nullptr;
				rpInfo.renderPass = renderpass;
				rpInfo.renderArea.offset.x = 0;
				rpInfo.renderArea.offset.y = 0;
				rpInfo.renderArea.extent = extent;
				rpInfo.framebuffer = framebuffer;
				rpInfo.clearValueCount = clearValues.size();
				rpInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

				// if this material has the same descriptor set layout then the pipelines might be the same and we don't have to rebind??
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &equirectangularSet, 0, nullptr);

				PushConstantData pushConstant{};
				// used in equirect_to_cubemap.vert
				pushConstant.rotMat = rotationMatrices[layer];
				// used in prefilter_environment.frag
				pushConstant.roughness = mipLevel / (mipLevels - 1.0f);

				vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &pushConstant);

				VkDeviceSize offset{ 0 };
				vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer._buffer, &offset);

				vkCmdDraw(cmd, numVertices, 1, 0, 0);

				vkCmdEndRenderPass(cmd);
			});
		}

		extent = vkutil::next_mip_level_extent(extent);
	}

	// This is the final image view, viewing all 6 layers of the image as a cube
	VkImageViewCreateInfo textureViewInfo{};
	textureViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureViewInfo.pNext = nullptr;
	textureViewInfo.image = allocatedImage._image;
	textureViewInfo.format = hdriFormat;
	textureViewInfo.subresourceRange.baseMipLevel = 0;
	textureViewInfo.subresourceRange.levelCount = mipLevels;
	textureViewInfo.subresourceRange.baseArrayLayer = 0;
	textureViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	if (isCubemap) {
		textureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		textureViewInfo.subresourceRange.layerCount = 6;
	} else {
		textureViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureViewInfo.subresourceRange.layerCount = 1;
	}

	VkImageView textureView;
	VK_CHECK(vkCreateImageView(engine._device, &textureViewInfo, nullptr, &textureView));
	engine._mainDeletionQueue.push_function([=, &engine]() {
		vkDestroyImageView(engine._device, textureView, nullptr);
	});

	Texture texture{};
	texture.image = allocatedImage;
	texture.imageView = textureView;
	texture.mipLevels = mipLevels;

	return texture;
}