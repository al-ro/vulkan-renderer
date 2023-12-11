#include "vulkanUtils.h"

void createImage(const VulkanContext& ctx, uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples,
                 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                 VkImage& image, VkDeviceMemory& imageMemory);
VkImageView createImageView(const VkDevice& device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void generateMipmaps(const VulkanContext& ctx, VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
void copyBufferToImage(const VulkanContext& ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
void transitionImageLayout(const VulkanContext& ctx, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
