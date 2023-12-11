#include "texture.h"

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "image.h"

Texture::Texture(const VulkanContext& ctx, std::string sourcePath) : ctx{ctx} {
  int texWidth;
  int texHeight;
  int texChannels;
  stbi_uc* pixels = stbi_load(sourcePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

  if (!pixels) {
    throw std::runtime_error("Failed to load texture image: " + sourcePath);
  }

  mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

  VkDeviceSize imageSize = texWidth * texHeight * 4;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  // Create buffer that can have data passed in from the CPU and copied to a better location on the GPU
  createBuffer(ctx, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(ctx.device, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(ctx.device, stagingBufferMemory);

  stbi_image_free(pixels);

  createImage(ctx, texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory);

  transitionImageLayout(ctx, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
  copyBufferToImage(ctx, stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
  generateMipmaps(ctx, image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

  vkDestroyBuffer(ctx.device, stagingBuffer, nullptr);
  vkFreeMemory(ctx.device, stagingBufferMemory, nullptr);

  imageView = createImageView(ctx.device, image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
  createTextureSampler();
}

void Texture::createTextureSampler() {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  samplerInfo.anisotropyEnable = VK_TRUE;
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(ctx.physicalDevice, &properties);
  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.maxLod = static_cast<float>(mipLevels);

  if (vkCreateSampler(ctx.device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create texture sampler");
  }
}

Texture::~Texture() {
  vkDestroySampler(ctx.device, sampler, nullptr);
  vkDestroyImageView(ctx.device, imageView, nullptr);
  vkDestroyImage(ctx.device, image, nullptr);
  vkFreeMemory(ctx.device, memory, nullptr);
}