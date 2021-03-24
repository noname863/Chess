#pragma once
#define VKFW_ASSERT_ON_RESULT(expr)
#define VULKAN_HPP_ASSERT(expr)
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan.hpp>
#include <renderer/family_indeces.hpp>
#include <utils/assert.hpp>
#include <vector>

inline void criticalVulkanAssert(vk::Result received, std::string message)
{
    criticalAssertEqual(received, vk::Result::eSuccess, std::move(message));
}

class RenderSystem
{
private:
    struct RenderParametersCache
    {
        RenderParametersCache(RenderSystem& owner);

        void updateWindowDependentProperties(const vkfw::Window& window);
        void updateSurfaceDependentProperties();
        void updatePhysicalDeviceDependentProperties();
        void updateQueueDependentProperties();
        void updateDeviceDependentProperties();
        void updateSwapchainDependentProperties();
        void updateImageViewsDependentProperties();
        void updateRenderPassDependentProperties();
        void updateShadersDependentProperties();
        void updateFramebufferDependentProperties();

        vk::ApplicationInfo appInfo;
        std::vector<const char*> instanceExtentions;
        std::vector<const char*> enabledLayers;
        vk::InstanceCreateInfo instanceCreateInfo;
        std::vector<const char*> deviceExtentions;
        std::vector<vk::DeviceQueueCreateInfo> queueInfos;
        vk::PhysicalDeviceFeatures deviceFeatures;
        vk::DeviceCreateInfo deviceCreateInfo;
        vk::Extent2D windowExtent;
        vk::SwapchainCreateInfoKHR swapchainCreateInfo;
        vk::ImageSubresourceRange imageSubresourceRange;
        std::vector<vk::ImageViewCreateInfo> imageCreateInfos;
        vk::AttachmentDescription colorAttachment;
        vk::AttachmentReference colorAttachmentRef;
        vk::SubpassDescription subpassDescription;
        vk::SubpassDependency dependency;
        vk::RenderPassCreateInfo renderPassCreateInfo;
        vk::ShaderModuleCreateInfo shaderCreateInfos[2];
        vk::PipelineShaderStageCreateInfo shaderStageInfos[2];
        vk::PipelineVertexInputStateCreateInfo vertexInputStageInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssemplyStateInfo;
        vk::Viewport viewport;
        vk::Rect2D scissor;
        vk::PipelineViewportStateCreateInfo viewportStageInfo;
        vk::PipelineRasterizationStateCreateInfo rasterizerInfo;
        vk::PipelineMultisampleStateCreateInfo multisamplingInfo;
        vk::PipelineColorBlendAttachmentState colorBlendAttachment;
        vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
        vk::DynamicState dynamicStates[1] = { vk::DynamicState::eLineWidth};
        vk::PipelineDynamicStateCreateInfo dynamicStateInfo;
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
        vk::GraphicsPipelineCreateInfo pipelineInfo;
        std::vector<vk::FramebufferCreateInfo> framebufferCreateInfos;
        vk::CommandPoolCreateInfo poolCreateInfo;
        vk::CommandBufferAllocateInfo allocateInfo;
        vk::ClearValue clearColor;
        std::vector<vk::RenderPassBeginInfo> renderPassInfos;
        RenderSystem& owner;
    };
public:
    ~RenderSystem();
    static void init(const vkfw::Window&);
    static RenderSystem& instance();
    void update(float dt);
private:
    RenderSystem(const vkfw::Window& window);

    void createInstance();
    void pickPhysicalDeviceAndQueueFamily();
    void createDevice();
    void createCommandPool();
    void createShaders();
    void createSyncObjects();
    void createSwapchain();
    void createRenderPass();
    void createPipeline();
    void createImageViews();
    void createFramebuffers();
    void createCommandBuffers();

    void recreateSwapchain();

    RenderParametersCache m_paramCache;

    vk::UniqueInstance m_instance;
    vk::PhysicalDevice m_physicalDevice;
    vk::UniqueSurfaceKHR m_surface;
    FamilyIndeces m_familyIndeces;
    vk::UniqueDevice m_device;
    vk::Queue m_graphicQueue;
    vk::UniqueSwapchainKHR m_swapchain;
    std::vector<vk::UniqueImageView> m_swapchainImages;
    vk::UniqueRenderPass m_renderPass;
    vk::UniqueShaderModule m_vertexShader;
    vk::UniqueShaderModule m_fragmentShader;
    vk::UniquePipelineLayout m_pipelineLayout;
    vk::UniquePipeline m_pipeline;
    vk::UniqueBuffer m_vertexBuffer;
    std::vector<vk::UniqueFramebuffer> m_framebuffers;
    vk::UniqueCommandPool m_commandPool;
    std::vector<vk::UniqueCommandBuffer> m_commandBuffers;
    std::vector<vk::UniqueSemaphore> m_imageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_renderFinishedSemaphores;
    std::vector<vk::UniqueFence> m_commandBufferFences;

    // update data
    std::vector<vk::Fence> imageFences;
    size_t frameIndex = 0;
};

