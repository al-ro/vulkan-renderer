#include <string>

#include "vulkanUtils.h"

class Texture {
 public:
  uint32_t width;
  uint32_t height;
  uint32_t mipLevels;

  VkImage image;
  VkDeviceMemory memory;

  VkImageView imageView;
  VkSampler sampler;

  const VulkanContext& ctx;

  Texture() = delete;
  Texture(const VulkanContext& ctx, std::string sourcePath);
  Texture(const Texture& texture) = delete;
  ~Texture();

  void createTextureSampler();
};