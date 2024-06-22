#include "app.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <assert.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define NUM_VALIDATION_LAYERS 1

typedef struct {
	uint32_t graphicsFamily;
} QueueFamilyIndices;

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

static SDL_Window *window;
static VkInstance instance;
static VkPhysicalDevice physical_device;
static VkDevice device;

const char *validation_layers[NUM_VALIDATION_LAYERS] = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

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
				  SDL_WINDOWPOS_CENTERED, 800, 600,
				  SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
}
void app_init_vulkan()
{
	vk_create_instance();
	vk_pick_physical_device();
	vk_create_logical_device();
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
		.pEnabledFeatures = &deviceFeatures
	};

	if (vkCreateDevice(physical_device, &createInfo, nullptr, &device) !=
	    VK_SUCCESS) {
		fprintf(stderr, "failed to create logical device");
		exit(1);
	}
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
	QueueFamilyIndices indices = { UINT32_MAX };
	uint32_t queueFamilyCount = {};
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
						 nullptr);
	VkQueueFamilyProperties queueFamilies[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
						 queueFamilies);

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
	}

	return indices;
}

bool vk_is_device_suitable(VkPhysicalDevice device)
{
	QueueFamilyIndices indices = vk_find_queue_families(device);
	return indices.graphicsFamily < UINT32_MAX;
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

	unsigned int enabled_extension_count = 0;
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
	unsigned int layerCount = {};
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	VkLayerProperties availableLayers[layerCount];

	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

	bool layer_found = { false };

	for (unsigned int i = 0; i < layerCount; i++) {
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
	}
}

void app_clean_up()
{
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
	SDL_DestroyWindow(window);
	SDL_Quit();
}
