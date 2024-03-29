#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "attribute.h"
#include "camera.h"
#include "image.h"
#include "texture.h"
#include "vulkanUtils.h"

const uint32_t WIDTH = 2000;
const uint32_t HEIGHT = 1200;

const std::string MODEL_PATH = "obj/viking-room/viking_room.obj";
const std::string TEXTURE_PATH = "obj/viking-room/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;
uint32_t currentFrame = 0;

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};

    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    // layout(location = 0) directive in shader
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    return attributeDescriptions;
  }
  bool operator==(const Vertex& other) const {
    return pos == other.pos && color == other.color && texCoord == other.texCoord;
  }
};

namespace std {
template <>
struct hash<Vertex> {
  size_t operator()(Vertex const& vertex) const {
    return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
  }
};
}  // namespace std

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

static std::vector<char> readFile(const std::string& filename) {
  // Start at end, treat as binary
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  // Starting the end and getting the current position with tellg() gives the size
  size_t fileSize = (size_t)file.tellg();
  // Create a buffer to store all binary data
  std::vector<char> buffer(fileSize);

  // Move to the start and read all data
  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

class Renderer {
 public:
  Renderer() {
    initWindow();
    ctx.initContext(window);
    msaaSamples = std::min(VK_SAMPLE_COUNT_8_BIT, ctx.maxMSAASamples);
  }
  void run() {
    initVulkan();
    mainLoop();
    cleanup();
  }

 private:
  VulkanContext ctx;

  GLFWwindow* window;

  VkSwapchainKHR swapChain;
  // Contained images are created and destroyed automatically
  std::vector<VkImage> swapChainImages;
  std::vector<VkImageView> swapChainImageViews;

  VkImage depthImage;
  VkDeviceMemory depthImageMemory;
  VkImageView depthImageView;

  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;

  // Uniforms and push values
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorPool descriptorPool;
  // Freed automatically with pool
  std::vector<VkDescriptorSet> descriptorSets;

  VkPipelineLayout pipelineLayout;

  VkRenderPass renderPass;
  VkPipeline graphicsPipeline;

  std::vector<VkFramebuffer> swapChainFramebuffers;

  std::vector<VkCommandBuffer> drawCommandBuffers;

  std::vector<VkSemaphore> imageAvailableSemaphores;
  std::vector<VkSemaphore> renderFinishedSemaphores;
  std::vector<VkFence> inFlightFences;

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;

  std::vector<std::shared_ptr<Attribute<Vertex>>> vertexAttributes;
  std::vector<std::shared_ptr<Attribute<uint32_t>>> indexAttributes;

  std::vector<VkBuffer> uniformBuffers;
  std::vector<VkDeviceMemory> uniformBuffersMemory;
  std::vector<void*> uniformBuffersMapped;

  std::vector<std::shared_ptr<Texture>> textures;

  VkImage colorImage;
  VkDeviceMemory colorImageMemory;
  VkImageView colorImageView;

  VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

  bool framebufferResized = false;

  Camera camera{
      1.0f, 0.5f, 2.0f, glm::vec3{0.0f, 0.0f, 1.0f},
      glm::radians(45.0f), 1.0,
      0.01f, 10.0f};

  bool mouseDown = false;

  /*----- GLFW  -----*/

  void initWindow() {
    glfwInit();
    // Stop GLFW from creating an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Renderer", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorPosCallback(window, mouseMoveCallback);
  }

  static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->framebufferResized = true;
  }

  static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
    auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    double oldXPos;
    double oldYPos;
    glfwGetCursorPos(window, &oldXPos, &oldYPos);
    if (renderer->mouseDown) {
      renderer->camera.updateCoordinates(glm::vec2{oldXPos - xpos, ypos - oldYPos});
    }
  }

  static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
      auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
      if (action == GLFW_PRESS) {
        renderer->mouseDown = true;
      } else if (action == GLFW_RELEASE) {
        renderer->mouseDown = false;
      }
    }
  }

  static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->camera.distance = std::max(0.0, renderer->camera.distance - 0.01 * yoffset);
  }

  /*----- Model Loader -----*/

  void loadModel() {
    tinyobj::attrib_t attrib{};
    std::vector<tinyobj::shape_t> shapes{};
    std::vector<tinyobj::material_t> materials{};
    std::string warn{};
    std::string err{};

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str())) {
      throw std::runtime_error("Failed to load model: " + warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
      for (const auto& index : shape.mesh.indices) {
        Vertex vertex{};

        vertex.pos = {
            attrib.vertices[3 * index.vertex_index + 0],
            attrib.vertices[3 * index.vertex_index + 1],
            attrib.vertices[3 * index.vertex_index + 2]};

        vertex.texCoord = {
            attrib.texcoords[2 * index.texcoord_index + 0],
            1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};

        vertex.color = {1.0f, 1.0f, 1.0f};

        if (uniqueVertices.count(vertex) == 0) {
          uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
          vertices.push_back(vertex);
        }

        indices.push_back(uniqueVertices[vertex]);
      }
    }
  }

  /*----- Pipeline -----*/

  void createGraphicsPipeline() {
    // ----- Create shader modules -----
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(ctx, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(ctx, fragShaderCode);

    // TODO: pSpecializationInfo

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

    vertShaderStageInfo.module = vertShaderModule;
    // Specify the entrypoint
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    // Specify the entrypoint
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // ----- Specify which stages will be provided during draw -----
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStates.size();
    dynamicState.pDynamicStates = dynamicStates.data();

    // ----- Specify vertex format -----
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // ----- Input assembly -----
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // ----- Rasterizer -----
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // Discard fragments beyond the near and far planes instead of clamping them
    rasterizer.depthClampEnable = VK_FALSE;
    // Do not disable rendering ???
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    // Fill, wireframe or point
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // Is this needed when rendering triangles?
    rasterizer.lineWidth = 1.0f;

    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    rasterizer.depthBiasEnable = VK_FALSE;

    // ----- Multisampling -----
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = msaaSamples;
    multisampling.sampleShadingEnable = VK_TRUE;  // enable sample shading in the pipeline
    multisampling.minSampleShading = 0.2f;        // min fraction for sample shading; closer to one is smoother

    // ----- Blending -----
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable = VK_FALSE;

    // TODO investigate more
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // ----- Layout -----
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    if (vkCreatePipelineLayout(ctx.device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create pipeline layout");
    }

    // ----- Depth -----

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;  // Optional
    depthStencil.maxDepthBounds = 1.0f;  // Optional
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {};  // Optional
    depthStencil.back = {};   // Optional

    // ----- Create pipeline ----
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;  // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = pipelineLayout;

    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    pipelineInfo.pDepthStencilState = &depthStencil;

    if (vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create graphics pipeline");
    }

    // ----- Cleanup -----

    // Can be destroyed after pipeline creation
    vkDestroyShaderModule(ctx.device, fragShaderModule, nullptr);
    vkDestroyShaderModule(ctx.device, vertShaderModule, nullptr);
  }

  /*----- Render Pass -----*/

  void createRenderPass() {
    // --- Color attachement
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    // For multisampling
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // 10/10 enum naming
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Data before rendering is not preserved but we clear anyway
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // After rendering, data should be suitable for swapchain presentation
    // Multisampled images cannot be presented directly
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    // layout(location = 0) out vec4 outColor
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // --- Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // --- Resolve attachment (MSAA to presentable)
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // ---

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(ctx.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create render pass");
    }
  }

  /*----- Framebuffer -----*/

  void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
      std::array<VkImageView, 3> attachments = {
          colorImageView,
          depthImageView,
          swapChainImageViews[i]};

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = swapChainExtent.width;
      framebufferInfo.height = swapChainExtent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(ctx.device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create framebuffer");
      }
    }
  }

  /*----- Depth Attachment -----*/

  VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
      VkFormatProperties props;
      vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice, format, &props);

      if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
        return format;
      } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
        return format;
      }
    }

    throw std::runtime_error("Failed to find supported format");
  }

  bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
  }

  VkFormat findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  }

  void createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(ctx, swapChainExtent.width, swapChainExtent.height, 1, msaaSamples,
                depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);
    depthImageView = createImageView(ctx.device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
  }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }

    return availableFormats[0];
  }

  VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width{};
      int height{};
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

      actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  void createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(ctx.physicalDevice, ctx.surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // In order not to have to wait, it is recommended to create one more than the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    // Make sure this does not exceed maximum
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = ctx.surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // VK_IMAGE_USAGE_TRANSFER_DST_BIT to copy data for postprocessing instead

    QueueFamilyIndices indices = findQueueFamilies(ctx.physicalDevice, ctx.surface);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;      // Optional
      createInfo.pQueueFamilyIndices = nullptr;  // Optional
    }

    // No rotation or scaling etc.
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    // Do not belnd with other windows
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    // No interest in pixels obscured by another window
    createInfo.clipped = VK_TRUE;
    // Required when swap chain is invalidated e.g. by resizing
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(ctx.device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create swap chain");
    }

    // Note that imageCount is minimum and more images could have been created
    vkGetSwapchainImagesKHR(ctx.device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(ctx.device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    camera.aspect = (float)extent.width / (float)extent.height;
  }

  void recreateSwapChain() {
    int width{0};
    int height{0};

    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window, &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(ctx.device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createColorResources();
    createDepthResources();
    createFramebuffers();
  }

  void cleanupSwapChain() {
    vkDestroyImageView(ctx.device, colorImageView, nullptr);
    vkDestroyImage(ctx.device, colorImage, nullptr);
    vkFreeMemory(ctx.device, colorImageMemory, nullptr);

    vkDestroyImageView(ctx.device, depthImageView, nullptr);
    vkDestroyImage(ctx.device, depthImage, nullptr);
    vkFreeMemory(ctx.device, depthImageMemory, nullptr);

    for (auto imageView : swapChainImageViews) {
      vkDestroyImageView(ctx.device, imageView, nullptr);
    }
    for (auto framebuffer : swapChainFramebuffers) {
      vkDestroyFramebuffer(ctx.device, framebuffer, nullptr);
    }
    vkDestroySwapchainKHR(ctx.device, swapChain, nullptr);
  }

  void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
      swapChainImageViews[i] = createImageView(ctx.device, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
  }

  void createColorResources() {
    VkFormat colorFormat = swapChainImageFormat;

    createImage(ctx, swapChainExtent.width, swapChainExtent.height, 1, msaaSamples,
                colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                colorImage, colorImageMemory);
    colorImageView = createImageView(ctx.device, colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
  }

  /*----- Resource descriptors -----*/

  void createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    // layout(binding = 0) in shader
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // Can pass arrays in which case the count is the number of elements
    uboLayoutBinding.descriptorCount = 1;
    // Or VK_SHADER_STAGE_FRAGMENT_BIT
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;  // Optional, for images

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(ctx.device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create descriptor set layout");
    }
  }

  void createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(ctx.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create descriptor pool");
    }
  }

  void createDescriptorSets() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(ctx.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      VkDescriptorBufferInfo bufferInfo{};
      bufferInfo.buffer = uniformBuffers[i];
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(UniformBufferObject);

      VkDescriptorImageInfo imageInfo{};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = textures[0]->imageView;
      imageInfo.sampler = textures[0]->sampler;

      std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet = descriptorSets[i];
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].pBufferInfo = &bufferInfo;

      descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet = descriptorSets[i];
      descriptorWrites[1].dstBinding = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[1].descriptorCount = 1;
      descriptorWrites[1].pImageInfo = &imageInfo;

      vkUpdateDescriptorSets(ctx.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
  }

  /*----- Buffers -----*/

  // As uniform buffers are updated often, do not use a staging buffer
  void createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      createBuffer(ctx, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   uniformBuffers[i], uniformBuffersMemory[i]);

      vkMapMemory(ctx.device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
  }

  void updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::mat4(1.0f);
    ubo.view = camera.viewMatrix;
    ubo.proj = camera.projectionMatrix;
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }

  /*----- Commands -----*/

  void createDrawCommandBuffers() {
    drawCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = ctx.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)drawCommandBuffers.size();

    if (vkAllocateCommandBuffers(ctx.device, &allocInfo, drawCommandBuffers.data()) != VK_SUCCESS) {
      throw std::runtime_error("Failed to allocate draw command buffers");
    }
  }

  void recordDrawCommandBuffer(VkCommandBuffer drawCommandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;                   // Optional
    beginInfo.pInheritanceInfo = nullptr;  // Optional

    if (vkBeginCommandBuffer(drawCommandBuffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(drawCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(drawCommandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {vertexAttributes[0]->buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(drawCommandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(drawCommandBuffer, indexAttributes[0]->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    // Vertex count, instance count, first vertex (and minimum gl_VertexIndex), first instance (gl_InstanceIndex)
    // vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    vkCmdDrawIndexed(drawCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(drawCommandBuffer);

    if (vkEndCommandBuffer(drawCommandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("Failed to record command buffer");
    }
  }

  /*----- Synchronization -----*/

  void createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Create in a signaled state to draw first frame immediately
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      if (vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
          vkCreateSemaphore(ctx.device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
          vkCreateFence(ctx.device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects");
      }
    }
  }

  /*----- Controls -----*/

  void updateCamera() {
    camera.updatePosition();
    camera.updateMatrices();
  }

  /*----- Draw -----*/

  void drawFrame() {
    // Wait for previous frame to finish. Wait for all fences, infinite timeout
    vkWaitForFences(ctx.device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // The index of the swap chain image that has become available
    uint32_t imageIndex;

    VkResult result = vkAcquireNextImageKHR(ctx.device, swapChain, UINT64_MAX,
                                            imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapChain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("Failed to acquire swap chain image");
    }

    vkResetFences(ctx.device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(drawCommandBuffers[currentFrame], 0);
    recordDrawCommandBuffer(drawCommandBuffers[currentFrame], imageIndex);

    updateCamera();

    updateUniformBuffer(currentFrame);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // The indices of these two arrays are linked
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCommandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(ctx.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
      throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    // Check swapchain results against success value
    presentInfo.pResults = nullptr;  // Optional

    result = vkQueuePresentKHR(ctx.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
      framebufferResized = false;
      recreateSwapChain();
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("Failed to present swap chain image");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  }

  /*----- Initialization -----*/

  void initVulkan() {
    // Must be called after logical device creation
    createSwapChain();
    createImageViews();
    createRenderPass();

    createDescriptorSetLayout();
    createGraphicsPipeline();
    createColorResources();
    createDepthResources();

    createFramebuffers();

    textures.push_back(std::make_shared<Texture>(ctx, TEXTURE_PATH));

    loadModel();

    vertexAttributes.push_back(std::make_shared<Attribute<Vertex>>(ctx, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
    indexAttributes.push_back(std::make_shared<Attribute<uint32_t>>(ctx, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createDrawCommandBuffers();
    createSyncObjects();
  }

  /*----- Main loop -----*/

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }
    // Wait for all work to finish before quitting
    vkDeviceWaitIdle(ctx.device);
  }

  /*----- Cleanup -----*/

  void cleanup() noexcept {
    vkDestroyPipelineLayout(ctx.device, pipelineLayout, nullptr);
    vkDestroyPipeline(ctx.device, graphicsPipeline, nullptr);
    vkDestroyRenderPass(ctx.device, renderPass, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroySemaphore(ctx.device, renderFinishedSemaphores[i], nullptr);
      vkDestroySemaphore(ctx.device, imageAvailableSemaphores[i], nullptr);
      vkDestroyFence(ctx.device, inFlightFences[i], nullptr);
    }

    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      vkDestroyBuffer(ctx.device, uniformBuffers[i], nullptr);
      vkFreeMemory(ctx.device, uniformBuffersMemory[i], nullptr);
    }

    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, descriptorSetLayout, nullptr);

    vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

int main() {
  Renderer renderer;

  try {
    renderer.run();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}