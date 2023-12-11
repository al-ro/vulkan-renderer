#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

#pragma once

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// ----- Helper structs -----

struct QueueFamilyIndices {
  // Index of queue family which supports graphics commands
  std::optional<uint32_t> graphicsFamily;
  // Index of queue family which supports presentation
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

/**
 * Wrapper class to manage and pass logical and physical device handles
 * together with application independent functionality
 */
class VulkanContext {
 public:
  VkInstance instance;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device;

  VkCommandPool commandPool;

  VkQueue graphicsQueue;
  VkQueue presentQueue;

  GLFWwindow* window;
  VkSurfaceKHR surface;

  VkDebugUtilsMessengerEXT debugMessenger;

  VkSampleCountFlagBits maxMSAASamples;

  VulkanContext() = default;
  ~VulkanContext();

  void initContext(GLFWwindow* window);
  void createInstance();
  void createSurface();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createCommandPool();
  void setupDebugMessenger();
};

SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface);
VkShaderModule createShaderModule(const VulkanContext& ctx, const std::vector<char>& code);

void findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

uint32_t findMemoryType(const VkPhysicalDevice& physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

void createBuffer(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer& buffer, VkDeviceMemory& bufferMemory);
void copyBuffer(const VulkanContext& ctx, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void beginCommand(const VulkanContext& ctx, VkCommandBuffer& commanBuffer);
void submitCommand(const VulkanContext& ctx, VkCommandBuffer& commandBuffer, const VkQueue& queue);
QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface);

VkSampleCountFlagBits getMaxUsableSampleCount(const VkPhysicalDevice& physicalDevice);
