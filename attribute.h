#include "vulkanUtils.h"

template <typename DataFormat>
class Attribute {
 public:
  VkBuffer buffer;
  VkDeviceMemory memory;

  const VulkanContext& ctx;

  Attribute() = delete;
  Attribute(const VulkanContext& ctx, const std::vector<DataFormat>& data, VkBufferUsageFlagBits usage);
  Attribute(const Attribute& attribute) = delete;
  ~Attribute();
};

template <typename DataFormat>
Attribute<DataFormat>::Attribute(const VulkanContext& ctx, const std::vector<DataFormat>& cpuData, VkBufferUsageFlagBits usage) : ctx{ctx} {
  VkDeviceSize bufferSize = sizeof(DataFormat) * cpuData.size();

  // Create a buffer on the GPU which can have data copied into it from the CPU
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
               stagingBuffer, stagingBufferMemory);

  // Copy the data from the vertex structure to the mapped memory
  // Transfer to GPU happens in the background at an unspecified time but guaranteed to be before vkQueueSubmit()
  void* data;
  vkMapMemory(ctx.device, stagingBufferMemory, 0, bufferSize, 0, &data);
  memcpy(data, cpuData.data(), (size_t)bufferSize);
  vkUnmapMemory(ctx.device, stagingBufferMemory);

  // Create a buffer on the GPU which is optimal for rendering
  createBuffer(ctx, bufferSize,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
               buffer, memory);

  copyBuffer(ctx, stagingBuffer, buffer, bufferSize);

  // Staging buffer is not needed anymore
  vkDestroyBuffer(ctx.device, stagingBuffer, nullptr);
  vkFreeMemory(ctx.device, stagingBufferMemory, nullptr);
}

template <typename DataFormat>
Attribute<DataFormat>::~Attribute() {
  vkDestroyBuffer(ctx.device, buffer, nullptr);
  vkFreeMemory(ctx.device, memory, nullptr);
}