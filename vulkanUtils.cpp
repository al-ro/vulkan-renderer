#include "vulkanUtils.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>

/*----- Debgug Messenger -----*/

// These functions are linked dynamically and must be checked for during runtime
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
  std::cerr << "[DEBUG] Validation layer: " << pCallbackData->pMessage << std::endl;
  return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

/*----- Extension and validation layer tests -----*/

/* Tests whether required extensions are available */
void checkExtensionSupport(std::vector<const char*> requiredExtensionNames) {
  uint32_t count{};
  vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> availableExtensions(count);
  vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data());

  // Throw an exception if a required extension is not available
  for (auto const& n : requiredExtensionNames) {
    auto position = std::find_if(availableExtensions.begin(), availableExtensions.end(),
                                 [&](auto const& p) { return p.extensionName == std::string(n); });
    if (position == availableExtensions.end()) {
      throw std::runtime_error("Required extension " + std::string(n) + " is not supported.");
    } else {
      std::cout << "Required extension " << n << " is supported" << std::endl;
    }
  }
}

void checkValidationLayerSupport() {
  uint32_t layerCount{};
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  // Throw an exception if a required validation layer is not available
  for (auto const& l : validationLayers) {
    auto position = std::find_if(availableLayers.begin(), availableLayers.end(),
                                 [&](auto const& p) { return p.layerName == std::string(l); });
    if (position == availableLayers.end()) {
      throw std::runtime_error("Required layer " + std::string(l) + " is not supported.");
    } else {
      std::cout << "Required layer " << l << " is supported" << std::endl;
    }
  }
}

void VulkanContext::initContext(GLFWwindow* window) {
  this->window = window;
  createInstance();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  createCommandPool();
  setupDebugMessenger();
}

/*----- Surface -----*/

void VulkanContext::createSurface() {
  if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface");
  }
}

void VulkanContext::createInstance() {
  // Optional information to driver for possible optimisation
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Vulkan Renderer";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  uint32_t glfwExtensionCount{};
  const char** glfwExtensionNames;

  // Get array and count of extensions required by GLFW
  glfwExtensionNames = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  std::vector<const char*> requiredExtension(glfwExtensionNames, glfwExtensionNames + glfwExtensionCount);

  if (enableValidationLayers) {
    requiredExtension.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  checkExtensionSupport(requiredExtension);

  createInfo.enabledExtensionCount = requiredExtension.size();
  createInfo.ppEnabledExtensionNames = requiredExtension.data();

  // Outside the if scope to ensure it is not destroyed before instance creation
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  if (enableValidationLayers) {
    checkValidationLayerSupport();
    createInfo.enabledLayerCount = validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.data();

    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan instance");
  }
}

void VulkanContext::pickPhysicalDevice() {
  uint32_t deviceCount{};
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount < 1) {
    throw std::runtime_error("Failed to find GPUs with Vulkan support");
  }
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  auto position = std::find_if(devices.begin(), devices.end(),
                               [&](auto const& d) { return isDeviceSuitable(d, surface); });

  if (position == devices.end()) {
    throw std::runtime_error("Failed to find a suitable GPU");
  }

  physicalDevice = *position;
  maxMSAASamples = getMaxUsableSampleCount(physicalDevice);
}

void VulkanContext::createLogicalDevice() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};
  deviceFeatures.samplerAnisotropy = VK_TRUE;
  // MSAA for shader (e.g. texture aliasing)
  deviceFeatures.sampleRateShading = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.queueCreateInfoCount = queueCreateInfos.size();
  createInfo.pQueueCreateInfos = queueCreateInfos.data();

  createInfo.enabledExtensionCount = deviceExtensions.size();
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  createInfo.pEnabledFeatures = &deviceFeatures;

  // There is no longer any difference between device and instance validation layers
  // On latest Vulkan implementations, these values are ignored
  // Set them to be compatible with older versions
  if (enableValidationLayers) {
    createInfo.enabledLayerCount = validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical device");
  }

  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanContext::createCommandPool() {
  QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice, surface);

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  // Allow command buffers to be recorded individually
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

  if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create command pool");
  }
}

void VulkanContext::setupDebugMessenger() {
  if (!enableValidationLayers) {
    return;
  }

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  populateDebugMessengerCreateInfo(createInfo);

  if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
    throw std::runtime_error("Failed to set up debug messenger");
  }
}

// Queues are destroyed with device. Physical device is destroyed with instance. Command buffers are destroyed with command pool
VulkanContext::~VulkanContext() {
  if (enableValidationLayers) {
    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
  }
  vkDestroyCommandPool(device, commandPool, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(instance, nullptr);
}

/*----- Device -----*/

bool checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

  // Ensure that all required extensions exist in the available ones
  return std::all_of(deviceExtensions.begin(), deviceExtensions.end(),
                     [&](auto& e) {
                       return std::find_if(availableExtensions.begin(), availableExtensions.end(),
                                           [&](auto& a) { return std::string(e) == a.extensionName; }) != availableExtensions.end();
                     });
}

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) {
  SwapChainSupportDetails details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

bool checkSwapChainSupport(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) {
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
  return !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
}

bool isDeviceSuitable(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  VkPhysicalDeviceFeatures deviceFeatures{};

  vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
  return properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
         deviceFeatures.geometryShader &&
         findQueueFamilies(physicalDevice, surface).isComplete() &&
         checkDeviceExtensionSupport(physicalDevice) &&
         // Must be called after extensions have been checked
         checkSwapChainSupport(physicalDevice, surface) &&
         deviceFeatures.samplerAnisotropy;
}

/*----- Shader module -----*/

VkShaderModule createShaderModule(const VulkanContext& ctx, const std::vector<char>& code) {
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  // Interpret char* as uint32_t*. Vector ensures alignment
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(ctx.device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create shader module");
  }

  return shaderModule;
}

/*----- Queue -----*/

QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device, const VkSurfaceKHR& surface) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount{};
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  // Find index of first queue family which supports graphics commands
  auto graphicsPosition = std::find_if(queueFamilies.begin(), queueFamilies.end(),
                                       [](auto const& p) { return p.queueFlags & VK_QUEUE_GRAPHICS_BIT; });

  VkBool32 presentSupport = false;
  uint32_t presentIndex{};
  // Find index of first queue family which supports presenting to our surface. The function takes an index rather than an element
  while (presentIndex < queueFamilies.size() && !presentSupport) {
    vkGetPhysicalDeviceSurfaceSupportKHR(device, presentIndex, surface, &presentSupport);
    if (!presentSupport) {
      presentIndex++;
    }
  }

  if (graphicsPosition != queueFamilies.end()) {
    indices.graphicsFamily = graphicsPosition - queueFamilies.begin();
  }

  if (presentSupport) {
    indices.presentFamily = presentIndex;
  }

  return indices;
}

/*----- MSAA-----*/

VkSampleCountFlagBits getMaxUsableSampleCount(const VkPhysicalDevice& physicalDevice) {
  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

  VkSampleCountFlags counts =
      physicalDeviceProperties.limits.framebufferColorSampleCounts &
      physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  if (counts & VK_SAMPLE_COUNT_64_BIT) {
    return VK_SAMPLE_COUNT_64_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_32_BIT) {
    return VK_SAMPLE_COUNT_32_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_16_BIT) {
    return VK_SAMPLE_COUNT_16_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_8_BIT) {
    return VK_SAMPLE_COUNT_8_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_4_BIT) {
    return VK_SAMPLE_COUNT_4_BIT;
  }
  if (counts & VK_SAMPLE_COUNT_2_BIT) {
    return VK_SAMPLE_COUNT_2_BIT;
  }

  return VK_SAMPLE_COUNT_1_BIT;
}

/*----- Buffers -----*/

uint32_t findMemoryType(const VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  // Get available types of memory
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  throw std::runtime_error("Failed to find suitable memory type");
}

// Copy from one GPU buffer to another
// TODO: separate command pool for transfer commands
void copyBuffer(const VulkanContext& ctx, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
  VkCommandBuffer commandBuffer;
  beginCommand(ctx, commandBuffer);

  VkBufferCopy copyRegion{};
  copyRegion.size = size;
  vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  submitCommand(ctx, commandBuffer, ctx.graphicsQueue);
}

// Create buffer on the GPU
void createBuffer(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = size;
  bufferInfo.usage = usage;
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(ctx.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer");
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(ctx.device, buffer, &memRequirements);

  VkMemoryAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(ctx.physicalDevice, memRequirements.memoryTypeBits, properties);

  if (vkAllocateMemory(ctx.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate buffer memory");
  }

  vkBindBufferMemory(ctx.device, buffer, bufferMemory, 0);
}

/*----- Commands -----*/

void beginCommand(const VulkanContext& ctx, VkCommandBuffer& commandBuffer) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = ctx.commandPool;
  allocInfo.commandBufferCount = 1;

  vkAllocateCommandBuffers(ctx.device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);
}

/**
 * End commandbuffer, submit it and free it back into the pool
 */
void submitCommand(const VulkanContext& ctx, VkCommandBuffer& commandBuffer, const VkQueue& queue) {
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(ctx.device, ctx.commandPool, 1, &commandBuffer);
}