#include "app.h"
#include "SDL_video.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <assert.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include "utils.h"

#define NUM_VALIDATION_LAYERS 1
#define NUM_DEVICE_EXTENSIONS 1
#define WIDTH 800
#define HEIGHT 600

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const char *validation_layers[NUM_VALIDATION_LAYERS] = {
	"VK_LAYER_KHRONOS_validation"
};

const char *device_extensions[NUM_DEVICE_EXTENSIONS] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT,
				   VK_DYNAMIC_STATE_SCISSOR };
typedef struct {
	uint32_t graphicsFamily;
	uint32_t presentFamily;
} QueueFamilyIndices;

static SDL_Window *window;
static VkInstance instance;
static VkPhysicalDevice physical_device;
static VkDevice device;
static VkQueue graphics_queue;
static VkSurfaceKHR surface;
static VkQueue present_queue;
static VkSwapchainKHR swap_chain;
static VkImage *swap_chain_images;
static uint32_t swap_chain_image_count;
static VkFramebuffer *swapChainFramebuffers;
static VkFormat swap_chain_image_format;
static VkExtent2D swap_chain_extent;
static VkImageView *swap_chain_image_views;
static VkPipelineLayout pipeline_layout;
static VkRenderPass render_pass;
static VkPipeline graphics_pipeline;
static VkCommandPool command_pool;
static VkCommandBuffer command_buffer;
static VkSemaphore image_available_semaphore;
static VkSemaphore render_finished_semaphore;
static VkFence in_flight_fence;

static void app_init_window();
static void app_init_vulkan();
static void app_main_loop();
static void app_clean_up();

static void vk_create_instance();
static bool vk_check_validation_layer();
static void vk_pick_physical_device();
static bool vk_is_device_suitable(VkPhysicalDevice device);
static QueueFamilyIndices vk_find_queue_families(VkPhysicalDevice device);
static void vk_create_logical_device();
static void vk_create_surface();
static void vk_create_swap_chain();
static void vk_create_image_views();
static void vk_create_graphics_pipeline();
static void vk_create_render_pass();
static void vk_create_framebuffers();
static void vk_create_command_pool();
static void vk_create_command_buffer();
static void vk_create_sync_objects();
void vk_record_command_buffer(VkCommandBuffer commandBuffer,
			      uint32_t imageIndex);
static void vk_draw_frame();

void app_run()
{
	app_init_window();
	app_init_vulkan();
	app_main_loop();
	app_clean_up();
}

void app_init_window()
{
	SDL_Init(SDL_INIT_EVERYTHING);
	window = SDL_CreateWindow("my test window", SDL_WINDOWPOS_CENTERED,
				  SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT,
				  SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
}

void app_init_vulkan()
{
	vk_create_instance();
	vk_create_surface();
	vk_pick_physical_device();
	vk_create_logical_device();
	vk_create_swap_chain();
	vk_create_image_views();
	vk_create_render_pass();
	vk_create_graphics_pipeline();
	vk_create_framebuffers();
	vk_create_command_pool();
	vk_create_command_buffer();
	vk_create_sync_objects();
}

void vk_create_sync_objects()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
			      &image_available_semaphore) != VK_SUCCESS ||
	    vkCreateSemaphore(device, &semaphoreInfo, nullptr,
			      &render_finished_semaphore) != VK_SUCCESS ||
	    vkCreateFence(device, &fenceInfo, nullptr, &in_flight_fence) !=
		    VK_SUCCESS) {
		fprintf(stderr, "Can't create sync objects");
	}
}

void vk_record_command_buffer(VkCommandBuffer commandBuffer,
			      uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional
	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		fprintf(stderr, "Failed to create command buffers");
	}
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = render_pass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = (VkOffset2D){ 0, 0 };
	renderPassInfo.renderArea.extent = swap_chain_extent;
	VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
			     VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			  graphics_pipeline);
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = swap_chain_extent.width;
	viewport.height = swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = (VkOffset2D){ 0, 0 };
	scissor.extent = swap_chain_extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);
	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		fprintf(stderr, "failed to record command buffer!");
	}
}

void vk_create_command_buffer()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = command_pool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(device, &allocInfo, &command_buffer) !=
	    VK_SUCCESS) {
		fprintf(stderr, "Failed to create Command buffers");
	}
}

void vk_create_command_pool()
{
	QueueFamilyIndices queueFamilyIndices =
		vk_find_queue_families(physical_device);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
	if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) !=
	    VK_SUCCESS) {
		fprintf(stderr, "Failed to create command pool");
	}
}

void vk_create_framebuffers()
{
	swapChainFramebuffers =
		calloc(swap_chain_image_count, sizeof(VkFramebuffer));

	for (size_t i = 0; i < swap_chain_image_count; i++) {
		VkImageView attachments[] = { swap_chain_image_views[i] };

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType =
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = render_pass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = swap_chain_extent.width;
		framebufferInfo.height = swap_chain_extent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
					&swapChainFramebuffers[i]) !=
		    VK_SUCCESS) {
			fprintf(stderr, "Unable to create framebuffers");
			exit(1);
		}
	}
}

void vk_create_render_pass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swap_chain_image_format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr,
			       &render_pass) != VK_SUCCESS) {
		fprintf(stderr, "Cannot create Render pass");
	}
}

VkShaderModule createShaderModule(unsigned char *code, size_t size)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = size;
	createInfo.pCode = (uint32_t *)code;
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
	    VK_SUCCESS) {
		fprintf(stderr, "Failed to create shader");
		exit(1);
	}
	return shaderModule;
}

void vk_create_graphics_pipeline()
{
	size_t frag_buf_size = 0;
	size_t vert_buf_size = 0;

	auto frag_buf = read_binary_file("frag.spv", &frag_buf_size);
	auto vert_buf = read_binary_file("vert.spv", &vert_buf_size);
	auto frag_module = createShaderModule(frag_buf, frag_buf_size);
	auto vert_module = createShaderModule(vert_buf, vert_buf_size);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vert_module,
		.pName = "main"
	};
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = frag_module,
		.pName = "main"
	};

	VkPipelineShaderStageCreateInfo shaderStages[] = {
		vertShaderStageInfo, fragShaderStageInfo
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType =
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swap_chain_extent.width;
	viewport.height = (float)swap_chain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = (VkOffset2D){ 0, 0 };
	scissor.extent = swap_chain_extent;

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType =
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType =
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType =
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType =
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType =
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
				   &pipeline_layout) != VK_SUCCESS) {
		fprintf(stderr, "Cannot create vk pipeline layout");
		exit(1);
	}

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipeline_layout;
	pipelineInfo.renderPass = render_pass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
				      nullptr,
				      &graphics_pipeline) != VK_SUCCESS) {
		fprintf(stderr, "Faile to create pipeline");
	}
	vkDestroyShaderModule(device, frag_module, nullptr);
	vkDestroyShaderModule(device, vert_module, nullptr);
}

void vk_create_image_views()
{
	swap_chain_image_views =
		calloc(swap_chain_image_count, sizeof(VkImageView));

	for (uint32_t i = 0; i < swap_chain_image_count; i++) {
		VkImageViewCreateInfo createInfo = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swap_chain_images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swap_chain_image_format,
			.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
			.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
			.subresourceRange.aspectMask =
				VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel = 0,
			.subresourceRange.levelCount = 1,
			.subresourceRange.baseArrayLayer = 0,
			.subresourceRange.layerCount = 1
		};
		if (vkCreateImageView(device, &createInfo, nullptr,
				      &swap_chain_image_views[i]) !=
		    VK_SUCCESS) {
			fprintf(stderr, "failed to create image views");
			exit(1);
		}
	}
}

void vk_create_swap_chain()
{
	VkSurfaceFormatKHR surfaceFormat = {
		.format = VK_FORMAT_B8G8R8A8_SRGB,
		.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
	};
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkExtent2D actualExtent = { WIDTH, HEIGHT };
	VkSwapchainCreateInfoKHR createInfo = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = 3,
		.imageFormat = surfaceFormat.format,
		.imageColorSpace = surfaceFormat.colorSpace,
		.imageExtent = actualExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
	};
	QueueFamilyIndices indices = vk_find_queue_families(physical_device);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily,
					  indices.presentFamily };
	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	} else {
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swap_chain) !=
	    VK_SUCCESS) {
		fprintf(stderr, "Faild to create Swap Chain");
		exit(1);
	}
	swap_chain_image_count = 0;
	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain_image_count,
				nullptr);
	swap_chain_images = calloc(swap_chain_image_count, sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swap_chain, &swap_chain_image_count,
				swap_chain_images);
	swap_chain_image_format = surfaceFormat.format;
	swap_chain_extent = actualExtent;
}

void vk_create_surface()
{
	if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
		fprintf(stderr, "Cannot create surface");
		exit(1);
	}
}

void vk_create_logical_device()
{
	QueueFamilyIndices indices = vk_find_queue_families(physical_device);

	float queue_priority = { 1.0f };
	VkDeviceQueueCreateInfo queueCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = indices.graphicsFamily,
		.queueCount = 1,
		.pQueuePriorities = &queue_priority
	};

	VkPhysicalDeviceFeatures deviceFeatures = {};
	VkDeviceCreateInfo createInfo = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pQueueCreateInfos = &queueCreateInfo,
		.queueCreateInfoCount = 1,
		.pEnabledFeatures = &deviceFeatures,
		.enabledExtensionCount = NUM_DEVICE_EXTENSIONS,
		.ppEnabledExtensionNames = device_extensions
	};

	if (vkCreateDevice(physical_device, &createInfo, nullptr, &device) !=
	    VK_SUCCESS) {
		fprintf(stderr, "failed to create logical device");
		exit(1);
	}
	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphics_queue);
	printf("%d", indices.graphicsFamily);
	vkGetDeviceQueue(device, indices.presentFamily, 0, &present_queue);
}

void vk_pick_physical_device()
{
	uint32_t device_count = {};
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(instance, &device_count, devices);
	if (!vk_is_device_suitable(devices[0])) {
		fprintf(stderr, "First device not suitable\n");
		exit(1);
	}
	physical_device = devices[0];
}

QueueFamilyIndices vk_find_queue_families(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = { UINT32_MAX, UINT32_MAX };
	uint32_t queueFamilyCount = {};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
						 nullptr);
	VkQueueFamilyProperties queueFamilies[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
						 queueFamilies);

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
							     &presentSupport);
			if (presentSupport) {
				indices.presentFamily = i;
			}
		}
	}

	return indices;
}

bool vk_is_device_suitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = vk_find_queue_families(device);
	return indices.graphicsFamily < UINT32_MAX &&
	       indices.presentFamily < UINT32_MAX;
}

void vk_create_instance()
{
	if (enableValidationLayers && !vk_check_validation_layer()) {
		fprintf(stderr, "No validation layer support");
		exit(1);
	}

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Hello Triangle",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName = "No Engine",
		.engineVersion = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion = VK_API_VERSION_1_0
	};

	uint32_t enabled_extension_count = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(window, &enabled_extension_count,
					      nullptr)) {
		fprintf(stderr, "SDL Vulkan Extensions Failed");
	}

	const char *extension_names[enabled_extension_count];

	if (!SDL_Vulkan_GetInstanceExtensions(window, &enabled_extension_count,
					      extension_names)) {
		fprintf(stderr, "SDL Vulkan Extensions Names Failed");
	}

	VkInstanceCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pNext = nullptr,
		.pApplicationInfo = &app_info,
		.enabledLayerCount =
			((enableValidationLayers) ? NUM_VALIDATION_LAYERS : 0),
		.ppEnabledLayerNames = ((enableValidationLayers) ?
						validation_layers :
						nullptr),
		.enabledExtensionCount = enabled_extension_count,
		.ppEnabledExtensionNames = (const char *const *)extension_names,
	};

	if (vkCreateInstance(&create_info, nullptr, &instance) != VK_SUCCESS) {
		fprintf(stderr, "Instantiation Failed");
	}
}

bool vk_check_validation_layer()
{
	uint32_t layerCount = {};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	VkLayerProperties availableLayers[layerCount];

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	bool layer_found = { false };

	for (uint32_t i = 0; i < layerCount; i++) {
		if (strcmp(availableLayers[i].layerName,
			   validation_layers[0]) == 0) {
			layer_found = true;
			break;
		}
	}

	if (!layer_found) {
		return false;
	}
	return true;
}

void vk_draw_frame()
{
	vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &in_flight_fence);
	uint32_t imageIndex;
	vkAcquireNextImageKHR(device, swap_chain, UINT64_MAX,
			      image_available_semaphore, VK_NULL_HANDLE,
			      &imageIndex);
	vkResetCommandBuffer(command_buffer, 0);
	vk_record_command_buffer(command_buffer, imageIndex);
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { image_available_semaphore };
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command_buffer;
	VkSemaphore signalSemaphores[] = { render_finished_semaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	if (vkQueueSubmit(graphics_queue, 1, &submitInfo, in_flight_fence) !=
	    VK_SUCCESS) {
		fprintf(stderr, "Failed to submit command in queue");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;
	VkSwapchainKHR swapChains[] = { swap_chain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;
	vkQueuePresentKHR(present_queue, &presentInfo);
}

void app_main_loop()
{
	SDL_Event event;
	bool running = true;
	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				running = false;
			}
		}
		vk_draw_frame();
		vkDeviceWaitIdle(device);
	}
}

void app_clean_up()
{
	vkDestroySemaphore(device, image_available_semaphore, nullptr);
	vkDestroySemaphore(device, render_finished_semaphore, nullptr);
	vkDestroyFence(device, in_flight_fence, nullptr);
	vkDestroyCommandPool(device, command_pool, nullptr);
	vkDestroyRenderPass(device, render_pass, nullptr);
	vkDestroyPipeline(device, graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	for (uint32_t i = 0; i < swap_chain_image_count; i++) {
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
		vkDestroyImageView(device, swap_chain_image_views[i], nullptr);
	}
	vkDestroySwapchainKHR(device, swap_chain, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
	SDL_DestroyWindowSurface(window);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
