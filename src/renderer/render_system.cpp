#include <renderer/render_system.hpp>
#include <utils/executable_folder.hpp>
#include <optional>
#include <filesystem>
#include <array>
#include <tuple>
#include <fstream>
#include <iostream>

namespace
{
constexpr bool enableVulkanDebug = true;
constexpr size_t parallelFrames = 2;

std::unique_ptr<RenderSystem> s_instance;

template <typename T, typename Result, template<typename> typename ResultValue>
void extractResult(std::tuple<Result&, T&>&& tuple, ResultValue<T> result)
{
    std::get<0>(tuple) = result.result;
    std::get<1>(tuple) = std::move(result.value);
}

bool isRequiredDeviceExtentionsSupported(vk::PhysicalDevice physicalDevice)
{
    auto[getExtentionsResult, extentions] = physicalDevice.enumerateDeviceExtensionProperties();
    criticalVulkanAssert(getExtentionsResult, "error enumerating device extentions");
    return std::find_if(extentions.begin(), extentions.end(), [](const vk::ExtensionProperties& prop)
            {
                return std::strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
            }) != extentions.end();
}

std::optional<FamilyIndeces> getFamilyIndeces(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface)
{
    auto queueFamiliesProperties = physicalDevice.getQueueFamilyProperties();
    // std::numeric_limits wrapped in parenthesis, because it collides
    // with macro max(a, b) in windows.h see
    // https://github.com/microsoft/cppwinrt/issues/479
    uint32_t indexSurfaceSupported = (std::numeric_limits<uint32_t>::max)();
    uint32_t indexGraphicsSupported = (std::numeric_limits<uint32_t>::max)();
    for (uint32_t i = 0; i < queueFamiliesProperties.size(); ++i)
    {
        auto[result, value] = physicalDevice.getSurfaceSupportKHR(i, surface);
        criticalVulkanAssert(result, "error in checking support of surface in physical device");
        const bool surfaceSupported = value;
        const bool graphicsSupported = 
            (queueFamiliesProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) ==
            vk::QueueFlagBits::eGraphics; 
        if (surfaceSupported && graphicsSupported)
        {
            return FamilyIndeces{i, i};
        }
        else
        {
            if (surfaceSupported)
            {
                indexSurfaceSupported = i;
            }
            if (graphicsSupported)
            {
                indexGraphicsSupported = i;
            }
        }
    }
    if (indexGraphicsSupported < queueFamiliesProperties.size() &&
        indexSurfaceSupported < queueFamiliesProperties.size())
    {
        return FamilyIndeces{indexGraphicsSupported, indexSurfaceSupported};
    }
    else
    {
        return std::nullopt;
    }
}

bool surfaceAndSwapChainCompatible(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR& surface)
{
    auto[getFormatsResult, surfaceFormats] = physicalDevice.getSurfaceFormatsKHR(surface);
    criticalVulkanAssert(getFormatsResult, "error getting surface formats");
    auto[getPresentModesResult, presentModes] = physicalDevice.getSurfacePresentModesKHR(surface);
    criticalVulkanAssert(getPresentModesResult, "error getting present modes");
    return !surfaceFormats.empty() && !presentModes.empty();
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats)
{
    for (auto& format : formats)
    {
        if (format.format == vk::Format::eB8G8R8A8Srgb && 
                format.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
        {
            return format;
        }
    }
    return formats.front();
}

vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& presentModes)
{
    if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eMailbox) != presentModes.end())
    {
        return vk::PresentModeKHR::eMailbox;
    }
    if (std::find(presentModes.begin(), presentModes.end(), vk::PresentModeKHR::eFifoRelaxed) != presentModes.end())
    {
        return vk::PresentModeKHR::eFifoRelaxed;
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D createNewExtent2D(const vk::Extent2D& windowExtent, const vk::SurfaceCapabilitiesKHR& capabilities)
{
        vk::Extent2D resultExtent;
        resultExtent.width = std::clamp(static_cast<uint32_t>(windowExtent.width), 
                capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        resultExtent.height = std::clamp(static_cast<uint32_t>(windowExtent.height), 
                capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return resultExtent;
}

vk::Extent2D getSwapExtent2D(const vk::Extent2D& windowExtent, const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
    {
        return capabilities.currentExtent;
    }
    else
    {
        return createNewExtent2D(windowExtent, capabilities);
    }
}

std::vector<char> readFile(std::filesystem::path filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    fassert(file.is_open(), "failed to open file!");
    std::vector<char> buffer(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(buffer.data(), buffer.size());
    
    file.close();
    return buffer;
}


}

// if it is possible to fill parameters without creating anything, we should do this
RenderSystem::RenderParametersCache::RenderParametersCache(RenderSystem& owner) :
    appInfo("Chess", VK_MAKE_VERSION(1, 0, 0), "None", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_1),
    deviceExtentions({VK_KHR_SWAPCHAIN_EXTENSION_NAME}),
    imageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
    colorAttachment({}, {}, vk::SampleCountFlagBits::e1, 
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR),
    colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal),
    subpassDescription({}, vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorAttachmentRef),
    dependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
            vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, vk::AccessFlagBits::eColorAttachmentWrite),
    renderPassCreateInfo({}, 1, &colorAttachment, 1, &subpassDescription, 1, &dependency),
    vertexInputStageInfo({}, 0, nullptr, 0, nullptr),
    inputAssemplyStateInfo({}, vk::PrimitiveTopology::eTriangleList, false),
    viewportStageInfo({}, 1, &viewport, 1, &scissor),
    rasterizerInfo({},
            false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f),
    multisamplingInfo({},
            vk::SampleCountFlagBits::e1, false),
    colorBlendAttachment(false,
            vk::BlendFactor::eOne, vk::BlendFactor::eZero, 
            vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero,
            vk::BlendOp::eAdd, 
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA),
    colorBlendStateCreateInfo({}, 
            false, vk::LogicOp::eCopy, 1, &colorBlendAttachment,
            {0.0f, 0.0f, 0.0f, 0.0f}),
    dynamicStateInfo({}, 1, dynamicStates),
    pipelineInfo({}, 2, 
            shaderStageInfos, &vertexInputStageInfo, &inputAssemplyStateInfo,
            nullptr, &viewportStageInfo, &rasterizerInfo, &multisamplingInfo, 
            nullptr, &colorBlendStateCreateInfo, &dynamicStateInfo),
    clearColor(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
    owner(owner)
{
    uint32_t glfwExtentionCount = 0;
    const char * const* glfwExtentions = vkfw::getRequiredInstanceExtensions(&glfwExtentionCount);
    instanceExtentions.assign(glfwExtentions, glfwExtentions + glfwExtentionCount);
    // TODO: Check if layers exsists
    enabledLayers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_standard_validation",
        // "VK_LAYER_LUNARG_api_dump"
    };
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtentions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtentions.data();
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
    instanceCreateInfo.ppEnabledLayerNames = enabledLayers.data();

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtentions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtentions.data();
    if (enableVulkanDebug)
    {
        deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(enabledLayers.size());
        deviceCreateInfo.ppEnabledLayerNames = enabledLayers.data();
    }
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapchainCreateInfo.imageSharingMode = vk::SharingMode::eExclusive;
    swapchainCreateInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapchainCreateInfo.clipped = true;

    shaderStageInfos[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStageInfos[0].pName = "main";
    shaderStageInfos[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStageInfos[1].pName = "main";

    allocateInfo.level = vk::CommandBufferLevel::ePrimary;

    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = vk::Pipeline{};
    pipelineInfo.basePipelineIndex = -1;
}

void RenderSystem::RenderParametersCache::updateWindowDependentProperties(const vkfw::Window& window)
{
    auto [result, sizeTuple] = window.getFramebufferSize();
    std::tie(windowExtent.width, windowExtent.height) = sizeTuple;
    swapchainCreateInfo.imageExtent = windowExtent;
}

void RenderSystem::RenderParametersCache::updateSurfaceDependentProperties()
{
    swapchainCreateInfo.surface = owner.m_surface.get();
}

void RenderSystem::RenderParametersCache::updatePhysicalDeviceDependentProperties()
{
    auto[getCapabilitiesResult, capabilities] =
        owner.m_physicalDevice.getSurfaceCapabilitiesKHR(owner.m_surface.get());
    criticalVulkanAssert(getCapabilitiesResult, "error receiving capabilities");
    uint32_t minImageCount = capabilities.minImageCount;
    if (capabilities.maxImageCount == 0 ||
        capabilities.minImageCount != capabilities.maxImageCount)
    {
        ++minImageCount;
    }
    auto [getSurfaceFormatsResult, surfaceFormats] = 
        owner.m_physicalDevice.getSurfaceFormatsKHR(owner.m_surface.get());
    criticalVulkanAssert(getSurfaceFormatsResult, "error getting surface formats");
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
    auto [getPresentModesResult, presentModes] = 
        owner.m_physicalDevice.getSurfacePresentModesKHR(owner.m_surface.get());
    criticalVulkanAssert(getPresentModesResult, "error getting present modes");
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    windowExtent = getSwapExtent2D(windowExtent, capabilities);

    swapchainCreateInfo.minImageCount = minImageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.preTransform = capabilities.currentTransform;
    swapchainCreateInfo.presentMode = presentMode;
}

void RenderSystem::RenderParametersCache::updateQueueDependentProperties()
{
    float queuePriority = 1.0f;
    queueInfos.clear();
    queueInfos.reserve(owner.m_familyIndeces.indexes.size());
    for (uint32_t index : owner.m_familyIndeces.indexes)
    {
        queueInfos.push_back(vk::DeviceQueueCreateInfo({}, index, 1, &queuePriority));
    }

    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueInfos.data();

    swapchainCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(owner.m_familyIndeces.indexes.size());
    swapchainCreateInfo.pQueueFamilyIndices = owner.m_familyIndeces.indexes.data();

    poolCreateInfo.queueFamilyIndex = owner.m_familyIndeces.graphicsFamily;
}

void RenderSystem::RenderParametersCache::updateDeviceDependentProperties()
{
    vk::Result result;
    extractResult(std::tie(result, owner.m_pipelineLayout),
            owner.m_device->createPipelineLayoutUnique(pipelineLayoutInfo));

    criticalVulkanAssert(result, "error creating pipeline layout");

    pipelineInfo.layout = owner.m_pipelineLayout.get();
}

void RenderSystem::RenderParametersCache::updateSwapchainDependentProperties()
{
    swapchainCreateInfo.oldSwapchain = owner.m_swapchain.get();
    auto[getSwapChainResult, swapchainImages] = owner.m_device->getSwapchainImagesKHR(owner.m_swapchain.get());
    imageCreateInfos.clear();
    imageCreateInfos.reserve(swapchainImages.size());
    framebufferCreateInfos.clear();
    framebufferCreateInfos.reserve(swapchainImages.size());
    for (auto& image : swapchainImages)
    {
        vk::ImageViewCreateInfo imageViewCreateInfo({}, image, vk::ImageViewType::e2D,
                swapchainCreateInfo.imageFormat, {}, imageSubresourceRange); 
        imageCreateInfos.push_back(imageViewCreateInfo);

        vk::FramebufferCreateInfo framebufferCreateInfo({}, {}, 1, {},
                windowExtent.width, windowExtent.height, 1);
        framebufferCreateInfos.push_back(framebufferCreateInfo);
    }

    colorAttachment.format = swapchainCreateInfo.imageFormat;

    if (windowExtent.width < windowExtent.height)
    {
        viewport.width = viewport.height = static_cast<float>(windowExtent.width);
        viewport.x = 0.0f;
        viewport.y = (static_cast<float>(windowExtent.height) - viewport.width) / 2;
    }
    else
    {
        viewport.width = viewport.height = static_cast<float>(windowExtent.height);
        viewport.x = (static_cast<float>(windowExtent.width) - viewport.width) / 2;
        viewport.y = 0.0f;
    }
    scissor.extent = windowExtent;

    allocateInfo.commandBufferCount = swapchainImages.size();
}

void RenderSystem::RenderParametersCache::updateImageViewsDependentProperties()
{
    for (size_t i = 0; i < owner.m_swapchainImages.size(); ++i)
    {
        framebufferCreateInfos[i].pAttachments = &owner.m_swapchainImages[i].get();
    }
}

void RenderSystem::RenderParametersCache::updateRenderPassDependentProperties()
{
    pipelineInfo.renderPass = owner.m_renderPass.get();
    for (auto& framebufferCreateInfo : framebufferCreateInfos)
    {
        framebufferCreateInfo.renderPass = owner.m_renderPass.get();
    }
}

void RenderSystem::RenderParametersCache::updateShadersDependentProperties()
{
    shaderStageInfos[0].module = owner.m_vertexShader.get();
    shaderStageInfos[1].module = owner.m_fragmentShader.get();
}

void RenderSystem::RenderParametersCache::updateFramebufferDependentProperties()
{
    renderPassInfos.clear();
    renderPassInfos.reserve(owner.m_framebuffers.size());
    for (auto& framebuffer : owner.m_framebuffers)
    {
        vk::RenderPassBeginInfo renderPassInfo(owner.m_renderPass.get(), 
                framebuffer.get(), {{0, 0}, windowExtent}, 1, &clearColor);
        renderPassInfos.push_back(renderPassInfo);
    }
}

void RenderSystem::createInstance()
{
    vk::Result result;
    extractResult(std::tie(result, m_instance), 
            vk::createInstanceUnique(m_paramCache.instanceCreateInfo));
    criticalVulkanAssert(result, "error creating vulkan instance");
}

void RenderSystem::pickPhysicalDeviceAndQueueFamily()
{
    auto[result, physicalDevices] = m_instance->enumeratePhysicalDevices();
    criticalVulkanAssert(result, "error enumerating phisical devices");
    fassert(physicalDevices.size() != 0, "no physical devices found");
    for (auto& physicalDevice : physicalDevices)
    {
        if (!isRequiredDeviceExtentionsSupported(physicalDevice))
        {
            continue;
        }
        else
        {
            if (!surfaceAndSwapChainCompatible(physicalDevice, m_surface.get()))
            {
                continue;
            }
        }
        auto optionalFamilyIndeces = getFamilyIndeces(physicalDevice, m_surface.get());
        if (optionalFamilyIndeces.has_value())
        {
            m_physicalDevice = physicalDevice;
            m_familyIndeces = optionalFamilyIndeces.value();
            m_paramCache.updateQueueDependentProperties();
            m_paramCache.updatePhysicalDeviceDependentProperties();
            return;
        }
    }
    fassert(false, "no suitable device found");
}

void RenderSystem::createDevice()
{
    // default zero
    vk::Result result;
    extractResult(std::tie(result, m_device), m_physicalDevice.createDeviceUnique(m_paramCache.deviceCreateInfo));
    criticalVulkanAssert(result, "failed to create logical device");
    m_paramCache.updateDeviceDependentProperties();
}

void RenderSystem::createSwapchain()
{
    vk::Result result;
    extractResult(std::tie(result, m_swapchain), m_device->createSwapchainKHRUnique(m_paramCache.swapchainCreateInfo));
    criticalVulkanAssert(result, "failed to create swapchain");
    m_paramCache.updateSwapchainDependentProperties();
}

void RenderSystem::createImageViews()
{
    m_swapchainImages.clear();
    m_swapchainImages.reserve(m_paramCache.imageCreateInfos.size());
    for (auto& imageInfo : m_paramCache.imageCreateInfos)
    {
        auto [createViewResult, imageView] = m_device->createImageViewUnique(imageInfo);
        criticalVulkanAssert(createViewResult, "failed to create imageView for swapchain");
        m_swapchainImages.push_back(std::move(imageView));
    }
    m_paramCache.updateImageViewsDependentProperties();
}

void RenderSystem::createRenderPass()
{
    vk::Result result;
    extractResult(std::tie(result, m_renderPass), m_device->createRenderPassUnique(m_paramCache.renderPassCreateInfo));
    criticalVulkanAssert(result, "failed to create renderPass");
    m_paramCache.updateRenderPassDependentProperties();
}

void RenderSystem::createShaders()
{
    std::filesystem::path shadersFolder = getExecutableFolder() / "assets" / "shaders";
    std::vector<char> vertShaderCode = readFile(shadersFolder / "shader.vert.spv");
    std::vector<char> fragShaderCode = readFile(shadersFolder / "shader.frag.spv");
    
    vk::Result result;
    // shader module create info is not stored in m_paramCache to not save shader code in memory
    vk::ShaderModuleCreateInfo createInfo({}, vertShaderCode.size(), reinterpret_cast<uint32_t*>(vertShaderCode.data()));
    extractResult(std::tie(result, m_vertexShader), m_device->createShaderModuleUnique(createInfo));
    criticalVulkanAssert(result, "failed to create vertex shader");
    createInfo.codeSize = fragShaderCode.size();
    createInfo.pCode = reinterpret_cast<uint32_t*>(fragShaderCode.data());
    extractResult(std::tie(result, m_fragmentShader), m_device->createShaderModuleUnique(createInfo));
    criticalVulkanAssert(result, "failed to create fragment shader");
    m_paramCache.updateShadersDependentProperties();
}

void RenderSystem::createPipeline()
{
    vk::Result result;
    extractResult(std::tie(result, m_pipeline), m_device->createGraphicsPipelineUnique({}, m_paramCache.pipelineInfo));
    criticalVulkanAssert(result, "failed to create pipeline");
}

void RenderSystem::createSyncObjects()
{
    m_imageAvailableSemaphores.clear();
    m_imageAvailableSemaphores.reserve(parallelFrames);
    m_renderFinishedSemaphores.clear();
    m_renderFinishedSemaphores.reserve(parallelFrames);
    m_commandBufferFences.clear();
    m_commandBufferFences.reserve(parallelFrames);
    for (size_t i = 0; i < parallelFrames; ++i)
    {
        auto[result1, imageAvailableSemaphore] = m_device->createSemaphoreUnique({});
        criticalVulkanAssert(result1, "failed to create imageAvailableSemaphore");
        m_imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
        auto[result2, renderFinishedSemaphore] = m_device->createSemaphoreUnique({});
        criticalVulkanAssert(result2, "failed to create renderFinishedSemaphore");
        m_renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
        auto[result3, commandBufferFence] = m_device->createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
        criticalVulkanAssert(result3, "failed to create commandBufferFence");
        m_commandBufferFences.push_back(std::move(commandBufferFence));
    }
}

void RenderSystem::createCommandPool()
{
    vk::Result result;
    extractResult(std::tie(result, m_commandPool), m_device->createCommandPoolUnique(m_paramCache.poolCreateInfo));
    criticalVulkanAssert(result, "failed to create command pool");
    m_paramCache.allocateInfo.commandPool = m_commandPool.get();
}

void RenderSystem::createFramebuffers()
{
    m_framebuffers.clear();
    m_framebuffers.reserve(m_paramCache.framebufferCreateInfos.size());
    for (const auto& framebufferInfo : m_paramCache.framebufferCreateInfos)
    {
        auto [createFramebufferResult, framebuffer] =
            m_device->createFramebufferUnique(framebufferInfo);
        criticalVulkanAssert(createFramebufferResult, "failed to create framebuffer");
        m_framebuffers.push_back(std::move(framebuffer));
    }
    m_paramCache.updateFramebufferDependentProperties();
}

void RenderSystem::createCommandBuffers()
{
    vk::Result allocateResult;
    extractResult(std::tie(allocateResult, m_commandBuffers),
            m_device->allocateCommandBuffersUnique(m_paramCache.allocateInfo));
    criticalVulkanAssert(allocateResult, "failed to allocate commandBuffers");

    for (size_t i = 0; i < m_commandBuffers.size(); ++i)
    {
        criticalVulkanAssert(m_commandBuffers[i]->begin(vk::CommandBufferBeginInfo{}),
                "failed to begin recording command buffer");

        m_commandBuffers[i]->beginRenderPass(m_paramCache.renderPassInfos[i], vk::SubpassContents::eInline);
        m_commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
        m_commandBuffers[i]->draw(3, 1, 0, 0);
        m_commandBuffers[i]->endRenderPass();
        criticalVulkanAssert(m_commandBuffers[i]->end(), "error recording command buffers");
    }
}

void RenderSystem::recreateSwapchain()
{
    createSwapchain();
    createRenderPass();
    createPipeline();
    createImageViews();
    createFramebuffers();
    createCommandBuffers();
}

RenderSystem::RenderSystem(const vkfw::Window& window) :
    m_paramCache(*this)
{
    m_paramCache.updateWindowDependentProperties(window);
    createInstance();
    m_surface = vkfw::createWindowSurfaceUnique(m_instance.get(), window);
    m_paramCache.updateSurfaceDependentProperties();
    pickPhysicalDeviceAndQueueFamily();
    createDevice();
    createCommandPool();
    createShaders();
    createSyncObjects();
    createSwapchain();
    createRenderPass();
    createPipeline();
    createImageViews();
    createFramebuffers();
    createCommandBuffers();
    m_graphicQueue = m_device->getQueue(m_familyIndeces.graphicsFamily, 0);
    window.callbacks()->on_window_refresh = [this](const vkfw::Window& window)
    {
        m_paramCache.updateWindowDependentProperties(window);
        m_device->waitIdle();
        recreateSwapchain();
        update(0.0f);
    };
    imageFences.assign(m_framebuffers.size(), vk::Fence{});
}

RenderSystem::~RenderSystem()
{
    m_device->waitIdle();
}

void RenderSystem::init(const vkfw::Window& window)
{
    s_instance.reset(new RenderSystem(window));
}

RenderSystem& RenderSystem::instance()
{
    return *s_instance;
}

void RenderSystem::update(float dt)
{
    criticalVulkanAssert(m_device->waitForFences({m_commandBufferFences[frameIndex].get()}, true, (std::numeric_limits<uint64_t>::max)()), 
            "error waiting for entering drawFrame");
    auto [acquringResult, imageIndex] = 
        m_device->acquireNextImageKHR(m_swapchain.get(), (std::numeric_limits<uint64_t>::max)(), m_imageAvailableSemaphores[frameIndex].get(), {});
    if (acquringResult == vk::Result::eErrorOutOfDateKHR ||
            acquringResult == vk::Result::eSuboptimalKHR)
    {
        return;
    }
    else
    {
        criticalVulkanAssert(acquringResult, "error acquring image from swapchain");
    }
    if (imageFences[imageIndex] != vk::Fence{})
    {
        criticalVulkanAssert(m_device->waitForFences({imageFences[imageIndex]}, true, (std::numeric_limits<uint64_t>::max)()), 
                "error waiting for image release");
    }
    imageFences[imageIndex] = m_commandBufferFences[frameIndex].get();
    // waiting for one stage
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submitInfo(1, &m_imageAvailableSemaphores[frameIndex].get(), &waitStage, 
            1, &m_commandBuffers[imageIndex].get(), 1, &m_renderFinishedSemaphores[frameIndex].get());
    criticalVulkanAssert(m_device->resetFences({m_commandBufferFences[frameIndex].get()}), "error resetting command buffer fence");
    criticalVulkanAssert(m_graphicQueue.submit(1, &submitInfo, m_commandBufferFences[frameIndex].get()),"failed to submit commands to queue");
    vk::PresentInfoKHR presentInfo(1, &m_renderFinishedSemaphores[frameIndex].get(), 1, &m_swapchain.get(), &imageIndex);
    vk::Result presentResult = m_graphicQueue.presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
            presentResult == vk::Result::eSuboptimalKHR)
    {
        return;
    }
    else
    {
        criticalVulkanAssert(presentResult, "failed to present image to Queue");
    }
    frameIndex = (frameIndex + 1) % parallelFrames;
}

