﻿#include "vk_initializers.h"

VkCommandPoolCreateInfo vkinit::commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
	VkCommandPoolCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.queueFamilyIndex = queueFamilyIndex;
	info.flags = flags;

	return info;
}

VkCommandBufferAllocateInfo vkinit::commandBufferAllocateInfo(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
{
	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.pNext = nullptr;
	info.commandPool = pool;
	info.commandBufferCount = count;
	info.level = level;

	return info;
}

VkPipelineShaderStageCreateInfo vkinit::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
	VkPipelineShaderStageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.pNext = nullptr;
	// shader stage
	info.stage = stage;
	// module containing the code for this shader stage
	info.module = shaderModule;
	// entry point of the shader
	info.pName = "main";
	return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertexInputStateCreateInfo()
{
	VkPipelineVertexInputStateCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	info.pNext = nullptr;
	info.vertexBindingDescriptionCount = 0;
	info.vertexAttributeDescriptionCount = 0;
	return info;
}

VkPipelineInputAssemblyStateCreateInfo vkinit::inputAssemblyCreateInfo(VkPrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.pNext = nullptr;
	info.topology = topology;
	info.primitiveRestartEnable = VK_FALSE;
	return info;
}

VkPipelineRasterizationStateCreateInfo vkinit::rasterizationStateCreateInfo(VkPolygonMode polygonMode)
{
	VkPipelineRasterizationStateCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.pNext = nullptr;
	info.depthClampEnable = VK_FALSE;
	// this discards all primitives before the rasterization stage which we don't want
	info.rasterizerDiscardEnable = VK_FALSE;
	info.polygonMode = polygonMode;
	info.lineWidth = 1.0f;
	info.cullMode = VK_CULL_MODE_NONE;
	info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	// no depth bias
	info.depthBiasEnable = VK_FALSE;
	info.depthBiasConstantFactor = 0.0f;
	info.depthBiasClamp = 0.0f;
	info.depthBiasSlopeFactor = 0.0f;
	return info;
}

VkPipelineMultisampleStateCreateInfo vkinit::multisamplingStateCreateInfo(VkSampleCountFlagBits msaaSamples, float minSampleShading)
{
	VkPipelineMultisampleStateCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.pNext = nullptr;
	// Sample shading takes multiple samples from shaders
	info.sampleShadingEnable = minSampleShading == 0.0f ? VK_FALSE : VK_TRUE;
	// This only effects edges of geometry
	info.rasterizationSamples = msaaSamples;
	// Ratio of rasterizationSamples to take for shaders (usually less than 1.0)
	info.minSampleShading = minSampleShading;
	info.pSampleMask = nullptr;
	info.alphaToCoverageEnable = VK_FALSE;
	info.alphaToOneEnable = VK_FALSE;
	return info;
}

VkPipelineColorBlendAttachmentState vkinit::colorBlendAttachmentState()
{
	// learn more about this later! How does blending work???
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	return colorBlendAttachment;
}

VkPipelineLayoutCreateInfo vkinit::pipelineLayoutCreateInfo() {
	VkPipelineLayoutCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	info.pNext = nullptr;
	// empty defaults for now since we don't have descriptors or push constants yet
	info.flags = 0;
	info.setLayoutCount = 0;
	info.pSetLayouts = nullptr;
	info.pushConstantRangeCount = 0;
	info.pPushConstantRanges = nullptr;
	return info;
}

VkImageCreateInfo vkinit::imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent, uint32_t mipLevels, VkSampleCountFlagBits msaaSamples)
{
	VkImageCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	info.pNext = nullptr;
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = format;
	info.extent = extent;
	info.mipLevels = mipLevels;
	// we could set array layers to 6 for cubemaps! but here we only need 1
	info.arrayLayers = 1;
	info.samples = msaaSamples;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = usageFlags;
	return info;
}

VkImageViewCreateInfo vkinit::imageviewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
	VkImageViewCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	info.pNext = nullptr;
	info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	info.image = image;
	info.format = format;
	// using subresourceRange you can let the image view look at only particular layers of the image
	info.subresourceRange.baseMipLevel = 0;
	info.subresourceRange.levelCount = mipLevels;
	info.subresourceRange.baseArrayLayer = 0;
	info.subresourceRange.layerCount = 1;
	info.subresourceRange.aspectMask = aspectFlags;
	return info;
}

VkPipelineDepthStencilStateCreateInfo vkinit::depthStencilCreateInfo(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
{
	VkPipelineDepthStencilStateCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.pNext = nullptr;
	info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
	info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
	info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
	info.depthBoundsTestEnable = VK_FALSE;
	info.minDepthBounds = 0.0f;
	info.maxDepthBounds = 1.0f;
	info.stencilTestEnable = VK_FALSE;
	return info;
}

VkDescriptorSetLayoutBinding vkinit::descriptorsetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding)
{
	VkDescriptorSetLayoutBinding setBind{};
	setBind.binding = binding;
	setBind.descriptorCount = 1;
	setBind.descriptorType = type;
	setBind.pImmutableSamplers = nullptr;
	setBind.stageFlags = stageFlags;
	return setBind;
}

VkWriteDescriptorSet vkinit::writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
{
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstBinding = binding;
	write.dstSet = dstSet;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = bufferInfo;
	return write;
}

VkSamplerCreateInfo vkinit::samplerCreateInfo(VkFilter filters, uint32_t mipLevels, VkSamplerAddressMode samplerAddressMode)
{
	VkSamplerCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	info.pNext = nullptr;
	info.magFilter = filters;
	info.minFilter = filters;
	info.addressModeU = samplerAddressMode;
	info.addressModeV = samplerAddressMode;
	info.addressModeW = samplerAddressMode;

	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	info.minLod = 0.0f;
	info.maxLod = static_cast<float>(mipLevels);
	info.mipLodBias = 0.0f;

	return info;
}

VkWriteDescriptorSet vkinit::writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfo, uint32_t binding)
{
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstBinding = binding;
	write.dstSet = dstSet;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = imageInfo;

	return write;
}
