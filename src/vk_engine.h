﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#pragma warning(disable : 26812) // The enum type * is unscoped. Prefer 'enum class' over 'enum'.

#include <vector>
#include <cstdint>
#include <deque>
#include <functional>
#include <unordered_map>
#include <string>
#include <set>

#include "vk_types.h"
#include "vk_mem_alloc.h"
#include "vk_mesh.h"
#include "glm/glm.hpp"

// number of frames to overlap when rendering
constexpr unsigned int FRAME_OVERLAP{ 2 };

struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistance; // x for min, y for max, zw unused
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

struct GPUCameraData {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 viewProj;
};

struct FrameData {
	VkSemaphore _presentSemaphore;
	VkSemaphore _renderSemaphore;
	VkFence _renderFence;

	// This belongs to a frame because it's fast to reset a whole
	// command pool, and we reset this for the whole frame
	// (aka lifetime of this command pool is lifetime of this frame)
	VkCommandPool _commandPool;
	// command buffer belongs to frame since we for the next frame
	// while the other frame's command buffer is submitted
	VkCommandBuffer _mainCommandBuffer;

	// buffer that holds a single GPUCameraData to use when rendering
	AllocatedBuffer cameraBuffer;
	// descriptor that has frame lifetime
	VkDescriptorSet globalDescriptor;
};

// note that we store the VkPipeline and layout by value, not pointer.
// They are 64 bit handles to internal driver structures anyway so storing
// a pointer to them isn't very useful
struct Material {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
};

struct RenderObject {
	Mesh* mesh;
	Material* material;
	glm::mat4 transformMatrix;

	bool operator<(const RenderObject& other) const;
};

struct MeshPushConstants {
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it{ deletors.rbegin() }; it != deletors.rend(); ++it) {
			(*it)(); // call functors
		}

		deletors.clear();
	}
};

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	VkExtent2D _windowExtent{ 1700 , 900 };
	struct SDL_Window* _window{ nullptr };

	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	VkSwapchainKHR _swapchain;
	// image format expected by the windowing system
	VkFormat _swapchainImageFormat;
	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VkRenderPass _renderPass;
	std::vector<VkFramebuffer> _framebuffers;

	DeletionQueue _mainDeletionQueue;

	VmaAllocator _allocator;

	VkImageView _depthImageView;
	AllocatedImage _depthImage;
	VkFormat _depthFormat;

	std::multiset<RenderObject> _renderables;
	std::unordered_map<std::string, Material> _materials;
	std::unordered_map<std::string, Mesh> _meshes;

	glm::vec3 _camPos;

	VkDescriptorSetLayout _globalSetLayout;
	VkDescriptorPool _descriptorPool;

	VkPhysicalDeviceProperties _gpuProperties;

	GPUSceneData _sceneParameters;
	AllocatedBuffer _sceneParameterBuffer;

	// frame storage
	FrameData _frames[FRAME_OVERLAP];

	// getter for the frame we are rendering to right now
	FrameData& get_current_frame();

	// for delta time
	double _lastTime{ 0.0 };

	// variables for keeping track of fps
	double _lastTimeFPS{ 0.0 };
	uint32_t _nbFrames{ 0 };

	// measure ms of each frame without vsync
	double _msDelta;

	// initializes everything in the engine
	void init();

	// shuts down the engine
	void cleanup();

	// draw loop
	void draw();

	// run main loop
	void run();

private:

	void init_vulkan();

	void init_swapchain();

	void init_commands();

	void init_default_renderpass();

	void init_framebuffers();

	void init_sync_structures();

	void init_pipeline(const std::string& name, const std::string& vertPath, const std::string& fragPath);

	bool load_shader_module(const std::string& filePath, VkShaderModule* outShaderModule);

	void load_meshes();

	void load_mesh(const std::string& name, const std::string& path);

	void load_materials();

	void upload_mesh(Mesh& mesh);

	void showFPS();

	void init_scene();

	bool process_input();

	// create material and add it to the map
	Material* create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

	// returns nullptr if it can't be found
	Material* get_material(const std::string& name);

	// returns nullptr if it can't be found
	Mesh* get_mesh(const std::string& name);

	void draw_objects(VkCommandBuffer cmd, const std::multiset<RenderObject>& renderables);

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

	void init_descriptors();

	size_t pad_uniform_buffer_size(size_t originalSize);
};

class PipelineBuilder {
public:

	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
	VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkViewport _viewport;
	VkRect2D _scissor;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;

	VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};
