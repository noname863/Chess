#define VKFW_ASSERT_ON_RESULT(expr)
#define VULKAN_HPP_ASSERT(expr)
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan.hpp>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <filesystem>
#include <optional>
#include <limits>
#include <fstream>
#include <set>

std::filesystem::path executableFolder;

constexpr bool enableVulkanDebug = true;

void fassert(bool condition, const char * message)
{
    if (!condition)
    {
        puts(message);
        abort();
    }
}

template <typename T>
void criticalAssertEqual(T received, T desired, std::string message)
{
    message += "\nerror code is " + std::to_string(static_cast<int>(received)) + "\n";
    fassert(received == desired, message.c_str());
}

void criticalVkfwAssert(vkfw::Result received, std::string message)
{
    criticalAssertEqual(received, vkfw::Result::eSuccess, std::move(message));
}

void criticalVulkanAssert(vk::Result received, std::string message)
{
    criticalAssertEqual(received, vk::Result::eSuccess, std::move(message));
}

struct FamilyIndeces
{
    FamilyIndeces() = default;
    FamilyIndeces(uint32_t graphicsFamily, uint32_t presentationFamily) :
        graphicsFamily(graphicsFamily),
        presentationFamily(presentationFamily)
    {
        indexes.push_back(graphicsFamily);
        if (graphicsFamily != presentationFamily)
        {
            indexes.push_back(presentationFamily);
        }
    }
    FamilyIndeces(const FamilyIndeces&) = default;
    FamilyIndeces(FamilyIndeces&&) = default;
    std::vector<uint32_t> indexes;
    uint32_t graphicsFamily;
    uint32_t presentationFamily;

};

vkfw::UniqueWindow initWindow()
{
    criticalVkfwAssert(vkfw::setWindowHint(vkfw::OptionalWindowHint<vkfw::WindowHint::eResizable>{false}), 
            "error setting resizable hint");
    auto[result, window] = vkfw::createWindowUnique(800, 600, "Chess");
    criticalVkfwAssert(result, "error creating window");
    return std::move(window);
}

vk::UniqueInstance createVkInstance()
{
    vk::ApplicationInfo appInfo(
            "Chess",
            VK_MAKE_VERSION(1, 0, 0),
            "No Engine",
            VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_1
    );
    uint32_t glfwExtentionCount = 0;
    const char * const* glfwExtentions;
    glfwExtentions = vkfw::getRequiredInstanceExtensions(&glfwExtentionCount);
    vk::InstanceCreateInfo createInfo(
            {}, &appInfo, 0, nullptr, 
            glfwExtentionCount, glfwExtentions);
    std::vector<const char*> validationLayers = 
    {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_standard_validation",
        // "VK_LAYER_LUNARG_api_dump"
    };
    auto [enumerateLayersResult, layersProperties] = vk::enumerateInstanceLayerProperties();
    criticalVulkanAssert(enumerateLayersResult, "failed to get list of layers");
    if constexpr (enableVulkanDebug)
    {
        // TODO: FIX THIS
        //
        // auto charStringCompare = [](const char* str1, const char* str2)
        //         {
        //             return std::strcmp(str1, str2) < 0;
        //         };
        // auto findPredicate = [](const vk::LayerProperties& prop, const char* str)
        //         {
        //             return std::strcmp(str, prop.layerName) == 0;
        //         };
        // std::sort(validationLayers.begin(), validationLayers.end(), charStringCompare);
        // auto [enumerateLayersResult, layersProperties] = vk::enumerateInstanceLayerProperties();
        // criticalVulkanAssert(enumerateLayersResult, "failed to get list of layers");
        // std::sort(layersProperties.begin(), layersProperties.end(), 
        //         [charStringCompare](const vk::LayerProperties& prop1, const vk::LayerProperties& prop2)
        //         {
        //             return charStringCompare(prop1.layerName, prop2.layerName);
        //         });
        // if (std::search(layersProperties.begin(), layersProperties.end(),
        //         validationLayers.begin(), validationLayers.end(),
        //         findPredicate) != layersProperties.end())
        // {
        // }
        // else
        // {
        //     std::cout << "WARNING: validation layers requested, but some of them not found" << std::endl;
        // }
        // 
        // std::vector<const char *> validationLayers;
        createInfo.enabledLayerCount = validationLayers.size();
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    auto[result, vkInstance] = vk::createInstanceUnique(createInfo);
    criticalVulkanAssert(result, "error creating vulkan instance");
    return std::move(vkInstance);
}

std::vector<const char *> initListOfRequiredDeviceExtentions()
{
    std::vector<const char *> result{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    std::sort(result.begin(), result.end(),
            [](const char* str1, const char* str2)
            {
                return std::strcmp(str1, str2) < 0;
            });
    return result;
}

const std::vector<const char *> requiredDeviceExtentions =
    initListOfRequiredDeviceExtentions();

bool isRequiredDeviceExtentionsSupported(vk::PhysicalDevice physicalDevice)
{
    auto[getExtentionsResult, extentions] = physicalDevice.enumerateDeviceExtensionProperties();
    criticalVulkanAssert(getExtentionsResult, "error enumerating device extentions");
    std::vector<const char *> extentionNames;
    extentionNames.reserve(extentions.size());
    for (auto& extention : extentions)
    {
        extentionNames.push_back(extention.extensionName);
    }
    std::sort(extentionNames.begin(), extentionNames.end(),
            [](const char* str1, const char* str2)
            {
                return std::strcmp(str1, str2) < 0;
            });
    return std::search(extentionNames.begin(), extentionNames.end(),
            requiredDeviceExtentions.begin(), requiredDeviceExtentions.end(),
            [](const char* str1, const char* str2)
            {
                return std::strcmp(str1, str2) == 0;
            }) != extentionNames.end();
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

std::pair<vk::PhysicalDevice, FamilyIndeces>
pickPhysicalDeviceAndQueueFamily(vk::Instance& instance, vk::SurfaceKHR& surface)
{
    auto[result, physicalDevices] = instance.enumeratePhysicalDevices();
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
            if (!surfaceAndSwapChainCompatible(physicalDevice, surface))
            {
                continue;
            }
        }
        auto optionalFamilyIndeces = getFamilyIndeces(physicalDevice, surface);
        if (optionalFamilyIndeces.has_value())
        {
            return std::make_pair(physicalDevice, optionalFamilyIndeces.value());
        }
    }
    fassert(false, "no suitable device found");
    return {};
}

vk::UniqueDevice createDevice(vk::PhysicalDevice physicalDevice, FamilyIndeces queueFamilyIndeces)
{
    float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueInfos;
    queueInfos.reserve(queueFamilyIndeces.indexes.size());
    for (uint32_t index : queueFamilyIndeces.indexes)
    {
        queueInfos.push_back(vk::DeviceQueueCreateInfo({}, index, 1, &queuePriority));
    }
    // default zero
    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo deviceCreateInfo({}, 
            static_cast<uint32_t>(queueInfos.size()), queueInfos.data(), 
            0, nullptr, static_cast<uint32_t>(requiredDeviceExtentions.size()),
            requiredDeviceExtentions.data(), &deviceFeatures);

    auto[result, device] = physicalDevice.createDeviceUnique(deviceCreateInfo);
    criticalVulkanAssert(result, "failed to create logical device");
    return std::move(device);
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

vk::Extent2D createNewExtent2D(vkfw::Window& window, const vk::SurfaceCapabilitiesKHR capabilities)
{
        auto [result, sizeTuple] = window.getFramebufferSize();
        auto [width, height] = sizeTuple;
        vk::Extent2D resultExtent;
        resultExtent.width = std::clamp(static_cast<uint32_t>(width), 
                capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        resultExtent.height = std::clamp(static_cast<uint32_t>(height), 
                capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return resultExtent;
}

vk::Extent2D getSwapExtent2D(vkfw::Window& window, const vk::SurfaceCapabilitiesKHR capabilities)
{
    if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
    {
        return capabilities.currentExtent;
    }
    else
    {
        return createNewExtent2D(window, capabilities);
    }
}

std::tuple<vk::UniqueSwapchainKHR, vk::Format, vk::Extent2D> 
createSwapChain(
        vk::PhysicalDevice& physicalDevice, vk::Device& device, FamilyIndeces familyIndeces,
        vkfw::Window& window, vk::SurfaceKHR& surface)
{
    auto[getCapabilitiesResult, capabilities] =
        physicalDevice.getSurfaceCapabilitiesKHR(surface);
    criticalVulkanAssert(getCapabilitiesResult, "error receiving capabilities");
    uint32_t minImageCount = capabilities.minImageCount;
    if (capabilities.maxImageCount == 0 ||
        capabilities.minImageCount != capabilities.maxImageCount)
    {
        ++minImageCount;
    }
    auto [getSurfaceFormatsResult, surfaceFormats] = 
        physicalDevice.getSurfaceFormatsKHR(surface);
    criticalVulkanAssert(getSurfaceFormatsResult, "error getting surface formats");
    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(surfaceFormats);
    auto [getPresentModesResult, presentModes] = 
        physicalDevice.getSurfacePresentModesKHR(surface);
    criticalVulkanAssert(getPresentModesResult, "error getting present modes");
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(presentModes);
    vk::Extent2D extent = getSwapExtent2D(window, capabilities);
    vk::SwapchainCreateInfoKHR createInfo({}, surface, minImageCount, 
            surfaceFormat.format, surfaceFormat.colorSpace, extent, 1,
            vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive,
            static_cast<uint32_t>(familyIndeces.indexes.size()), familyIndeces.indexes.data(),
            capabilities.currentTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, presentMode, true, {});
    auto [createSwapChainResult, swapChain] = device.createSwapchainKHRUnique(createInfo);
    criticalVulkanAssert(createSwapChainResult, "failed to create swapchain");
    return std::make_tuple(std::move(swapChain), surfaceFormat.format, extent);
}

std::vector<vk::UniqueImageView> createImageViews(vk::Device& device, 
        vk::SwapchainKHR& swapchain, vk::Format format, vk::Extent2D extent)
{
    auto[getSwapChainResult, swapchainImages] = device.getSwapchainImagesKHR(swapchain);
    std::vector<vk::UniqueImageView> result;
    result.reserve(swapchainImages.size());
    for (auto& image : swapchainImages)
    {
        vk::ImageSubresourceRange subresourceRange(vk::ImageAspectFlagBits::eColor, 
                0, 1, 0, 1);
        vk::ImageViewCreateInfo createInfo({}, image, vk::ImageViewType::e2D,
                format, {}, subresourceRange); 
        auto [createViewResult, imageView] = device.createImageViewUnique(createInfo);
        criticalVulkanAssert(createViewResult, "failed to create imageView for swapchain");
        result.push_back(std::move(imageView));
    }
    return result;
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

vk::UniqueShaderModule createShaderModule(vk::Device& device, const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo({}, code.size(),
            reinterpret_cast<const uint32_t*>(code.data()));
    auto[result, shaderModule] = device.createShaderModuleUnique(createInfo);
    criticalVulkanAssert(result, "failed to create shader");
    return std::move(shaderModule);
}

vk::UniqueRenderPass createRenderPass(vk::Device& device, vk::Format swapChainImageFormat)
{
    vk::AttachmentDescription colorAttachment({}, 
            swapChainImageFormat, vk::SampleCountFlagBits::e1, 
            vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
    vk::AttachmentReference colorAttachmentRef(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::SubpassDescription subpassDescription({},
        vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorAttachmentRef);
    vk::SubpassDependency dependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
            vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, vk::AccessFlagBits::eColorAttachmentWrite);
    vk::RenderPassCreateInfo renderPassCreateInfo({},
            1, &colorAttachment, 1, &subpassDescription, 1, &dependency);
    auto[creationResult, renderPass] = device.createRenderPassUnique(renderPassCreateInfo);
    criticalVulkanAssert(creationResult, "failed to create renderPass");
    return std::move(renderPass);
}

std::tuple<vk::UniquePipeline , vk::UniquePipelineLayout>
createGraphicPipeline(vk::Device& device, vk::RenderPass& renderPass, vk::Extent2D extent)
{
    std::filesystem::path pathToShaders = executableFolder / "assets" / "shaders";
    auto vertShaderCode = readFile(pathToShaders / "shader.vert.spv");
    auto fragShaderCode = readFile(pathToShaders / "shader.frag.spv");

    vk::UniqueShaderModule vertShader = createShaderModule(device, vertShaderCode);
    vk::UniqueShaderModule fragShader = createShaderModule(device, fragShaderCode);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo({},
            vk::ShaderStageFlagBits::eVertex, vertShader.get(), "main");
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo({},
            vk::ShaderStageFlagBits::eFragment, fragShader.get(), "main");

    vk::PipelineShaderStageCreateInfo shaderStageInfos[] =
        {vertShaderStageInfo, fragShaderStageInfo};

    vk::PipelineVertexInputStateCreateInfo vertexInputStageInfo({},
            0, nullptr, 0, nullptr);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemplyStateInfo({},
            vk::PrimitiveTopology::eTriangleList, false);

    float extentSideSize;
    float viewportX;
    float viewportY;
    if (extent.width < extent.height)
    {
        extentSideSize = static_cast<float>(extent.width);
        viewportX = 0.0f;
        viewportY = (static_cast<float>(extent.height) - extentSideSize) / 2;
    }
    else
    {
        extentSideSize = static_cast<float>(extent.height);
        viewportX = (static_cast<float>(extent.width) - extentSideSize) / 2;
        viewportY = 0.0f;
    }

    vk::Viewport viewport(viewportX, viewportY, extentSideSize, extentSideSize, 0.0f, 1.0f);
    // vk::Viewport viewport(0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f);
    vk::Rect2D scissor({0, 0}, extent);
    vk::PipelineViewportStateCreateInfo viewportStateInfo({}, 
            1, &viewport, 1, &scissor);

    vk::PipelineRasterizationStateCreateInfo rasterizerInfo({},
            false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f);

    vk::PipelineMultisampleStateCreateInfo multisamplingInfo({},
            vk::SampleCountFlagBits::e1, false);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment(false,
            vk::BlendFactor::eOne, vk::BlendFactor::eZero, 
            vk::BlendOp::eAdd, vk::BlendFactor::eOne, vk::BlendFactor::eZero,
            vk::BlendOp::eAdd, 
            vk::ColorComponentFlagBits::eR |
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eA);

    vk::PipelineColorBlendStateCreateInfo colorBlendStateCreateInfo({}, 
            false, vk::LogicOp::eCopy, 1, &colorBlendAttachment,
            {0.0f, 0.0f, 0.0f, 0.0f});

    vk::DynamicState dynamicStates[] = {
        // vk::DynamicState::eViewport, 
        vk::DynamicState::eLineWidth
    };
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo({}, 1, dynamicStates);

    // all zeros
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    auto [pipelineLayoutCreateResult, pipelineLayout] = 
        device.createPipelineLayoutUnique(pipelineLayoutInfo);

    criticalVulkanAssert(pipelineLayoutCreateResult, "error creating pipeline layout");

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, 
            shaderStageInfos, &vertexInputStageInfo, &inputAssemplyStateInfo,
            nullptr, &viewportStateInfo, &rasterizerInfo, &multisamplingInfo, 
            nullptr, &colorBlendStateCreateInfo, &dynamicStateInfo, 
            pipelineLayout.get(), renderPass, 0, {}, -1);

    auto [graphicPipelineCreateResult, graphicPipeline] = 
        device.createGraphicsPipelineUnique({}, pipelineInfo);

    criticalVulkanAssert(graphicPipelineCreateResult, "failed to create graphic pipeline");

    return std::make_tuple(std::move(graphicPipeline), std::move(pipelineLayout));
}

std::vector<vk::UniqueFramebuffer>
createFramebuffers(vk::Device& device, vk::RenderPass& renderPass, 
        vk::Extent2D extent, const std::vector<vk::UniqueImageView>& imageViews)
{
    std::vector<vk::UniqueFramebuffer> result;
    result.reserve(imageViews.size());
    for (const auto& imageView : imageViews)
    {
        vk::FramebufferCreateInfo createInfo({}, renderPass, 1,
                &imageView.get(), extent.width, extent.height, 1);

        auto [createFramebufferResult, framebuffer] =
            device.createFramebufferUnique(createInfo);
        criticalVulkanAssert(createFramebufferResult, "failed to create framebuffer");
        result.push_back(std::move(framebuffer));
    }
    return result;
}

vk::UniqueCommandPool createCommandPool(vk::Device device, FamilyIndeces familyIndeces)
{
    vk::CommandPoolCreateInfo poolCreateInfo({}, familyIndeces.graphicsFamily);
    auto [createCommandPoolResult, commandPool] = 
        device.createCommandPoolUnique(poolCreateInfo);
    return std::move(commandPool);
}

std::vector<vk::UniqueCommandBuffer> createCommandBuffers(vk::Device& device, vk::CommandPool& commandPool, vk::Pipeline& pipeline,
        vk::RenderPass& renderPass, vk::Extent2D extent, const std::vector<vk::UniqueFramebuffer>& framebuffers)
{
    vk::CommandBufferAllocateInfo allocateInfo(commandPool, vk::CommandBufferLevel::ePrimary,
            static_cast<uint32_t>(framebuffers.size()));

    auto[allocateResult, commandBuffers] = device.allocateCommandBuffersUnique(allocateInfo);
    criticalVulkanAssert(allocateResult, "failed to allocate commandBuffers");

    for (size_t i = 0; i < commandBuffers.size(); ++i)
    {
        vk::CommandBufferBeginInfo beginInfo;
        criticalVulkanAssert(commandBuffers[i]->begin(beginInfo), "failed to begin recording command buffer");

        vk::ClearValue clearColor(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
        vk::Rect2D renderArea({0, 0}, extent);

        vk::RenderPassBeginInfo renderPassInfo(
                renderPass, framebuffers[i].get(), renderArea, 1, &clearColor);
        
        commandBuffers[i]->beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        commandBuffers[i]->bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        commandBuffers[i]->draw(3, 1, 0, 0);
        commandBuffers[i]->endRenderPass();
        criticalVulkanAssert(commandBuffers[i]->end(), "error recording command buffers");
    }

    return std::move(commandBuffers);
}

std::tuple<
    std::vector<vk::UniqueSemaphore>, 
    std::vector<vk::UniqueSemaphore>,
    std::vector<vk::UniqueFence>>
createSyncObjects(vk::Device& device, size_t parallelFrames)
{
    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
    imageAvailableSemaphores.reserve(parallelFrames);
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
    renderFinishedSemaphores.reserve(parallelFrames);
    std::vector<vk::UniqueFence> commandBufferFences;
    commandBufferFences.reserve(parallelFrames);
    for (size_t i = 0; i < parallelFrames; ++i)
    {
        auto[result1, imageAvailableSemaphore] = device.createSemaphoreUnique({});
        criticalVulkanAssert(result1, "failed to create imageAvailableSemaphore");
        imageAvailableSemaphores.push_back(std::move(imageAvailableSemaphore));
        auto[result2, renderFinishedSemaphore] = device.createSemaphoreUnique({});
        criticalVulkanAssert(result2, "failed to create renderFinishedSemaphore");
        renderFinishedSemaphores.push_back(std::move(renderFinishedSemaphore));
        auto[result3, commandBufferFence] = device.createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
        criticalVulkanAssert(result3, "failed to create commandBufferFence");
        commandBufferFences.push_back(std::move(commandBufferFence));
    }
    return std::make_tuple(std::move(imageAvailableSemaphores), std::move(renderFinishedSemaphores), std::move(commandBufferFences));
}

bool drawFrame(
        vk::Device& device,
        vk::SwapchainKHR& swapchain,
        vk::Queue& graphicQueue,
        vk::Semaphore& imageAvailableSemaphore, 
        vk::Semaphore& renderFinishedSemaphore, 
        vk::Fence& commandBufferFence, 
        std::vector<vk::Fence>& imageFences,
        std::vector<vk::UniqueCommandBuffer>& commandBuffers)
{
    criticalVulkanAssert(device.waitForFences({commandBufferFence}, true, (std::numeric_limits<uint64_t>::max)()), 
            "error waiting for entering drawFrame");
    auto [acquringResult, imageIndex] = 
        device.acquireNextImageKHR(swapchain, (std::numeric_limits<uint64_t>::max)(), imageAvailableSemaphore, {});
    if (acquringResult == vk::Result::eErrorOutOfDateKHR || acquringResult == vk::Result::eSuboptimalKHR)
    {
        return true;
    }
    else
    {
        criticalVulkanAssert(acquringResult, "error acquring image from swapchain");
    }
    if (imageFences[imageIndex] != vk::Fence{})
    {
        criticalVulkanAssert(device.waitForFences({imageFences[imageIndex]}, true, (std::numeric_limits<uint64_t>::max)()), 
                "error waiting for image release");
    }
    imageFences[imageIndex] = commandBufferFence;
    // waiting for one stage
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    vk::SubmitInfo submitInfo(1, &imageAvailableSemaphore, &waitStage, 
            1, &commandBuffers[imageIndex].get(), 1, &renderFinishedSemaphore);
    criticalVulkanAssert(device.resetFences({commandBufferFence}), "error resetting command buffer fence");
    criticalVulkanAssert(graphicQueue.submit(1, &submitInfo, commandBufferFence),"failed to submit commands to queue");
    vk::PresentInfoKHR presentInfo(1, &renderFinishedSemaphore, 1, &swapchain, &imageIndex);
    vk::Result presentResult = graphicQueue.presentKHR(presentInfo);
    if (presentResult == vk::Result::eErrorOutOfDateKHR ||
            presentResult == vk::Result::eSuboptimalKHR)
    {
        return true;
    }
    criticalVulkanAssert(presentResult, "failed to present image to Queue");
    return false;
}

auto recreateSwapchain(vk::Device& device, FamilyIndeces queueFamilyIndeces, vkfw::Window& mainWindow,
        vk::PhysicalDevice& physicalDevice, vk::SurfaceKHR& surface, vk::CommandPool& commandPool)
{
    auto [swapchain, format, extent] = createSwapChain(physicalDevice, device, 
        queueFamilyIndeces, mainWindow, surface);
    vk::UniqueRenderPass renderPass = createRenderPass(device, format);
    auto[graphicPipeline, pipelineLayout] = 
        createGraphicPipeline(device, renderPass.get(), extent);
    std::vector<vk::UniqueImageView> swapchainImageViews = createImageViews(
        device, swapchain.get(), format, extent);
    std::vector<vk::UniqueFramebuffer> framebuffers =
        createFramebuffers(device, renderPass.get(), extent, swapchainImageViews);
    std::vector<vk::UniqueCommandBuffer> commandBuffers = 
        createCommandBuffers(device, commandPool, graphicPipeline.get(),
                renderPass.get(), extent, framebuffers);
    return std::make_tuple(std::move(swapchain), format, extent, std::move(renderPass), std::move(graphicPipeline),
            std::move(swapchainImageViews), std::move(framebuffers), std::move(commandBuffers));
}

int main(int argc, char* argv[])
{
    executableFolder = std::filesystem::absolute(std::filesystem::path(argv[0]).remove_filename());
    criticalVkfwAssert(vkfw::init(), "error in glfw init");
    bool swapchainNeedRecreation = false;
    vkfw::UniqueWindow mainWindow = initWindow();
    mainWindow->callbacks()->on_framebuffer_resize = 
        [&swapchainNeedRecreation](const vkfw::Window& window, size_t, size_t)
        {
            swapchainNeedRecreation = true;
        };
    vk::UniqueInstance instance = createVkInstance();
    vk::UniqueSurfaceKHR surface = vkfw::createWindowSurfaceUnique(
        instance.get(), mainWindow.get());
    auto [physicalDevice, queueFamilyIndeces] =
        pickPhysicalDeviceAndQueueFamily(instance.get(), surface.get());
    vk::UniqueDevice device = createDevice(physicalDevice, queueFamilyIndeces);
    auto [swapchain, format, extent] = createSwapChain(physicalDevice, device.get(), 
        queueFamilyIndeces, mainWindow.get(), surface.get());
    vk::UniqueRenderPass renderPass = createRenderPass(device.get(), format);
    auto[graphicPipeline, pipelineLayout] = 
        createGraphicPipeline(device.get(), renderPass.get(), extent);
    std::vector<vk::UniqueImageView> swapchainImageViews = createImageViews(
        device.get(), swapchain.get(), format, extent);
    std::vector<vk::UniqueFramebuffer> framebuffers =
        createFramebuffers(device.get(), renderPass.get(), extent, swapchainImageViews);
    vk::UniqueCommandPool commandPool = createCommandPool(device.get(), queueFamilyIndeces);
    std::vector<vk::UniqueCommandBuffer> commandBuffers = 
        createCommandBuffers(device.get(), commandPool.get(), graphicPipeline.get(),
                renderPass.get(), extent, framebuffers);
    constexpr size_t inFlightFrames = 2;
    auto[imageAvailableSemaphores, renderFinishedSemaphores, commandBufferFences] = 
        createSyncObjects(device.get(), inFlightFrames);

    vk::Queue graphicQueue = device->getQueue(queueFamilyIndeces.graphicsFamily, 0);
    std::vector<vk::Fence> imageFences(framebuffers.size(), vk::Fence{});

    size_t frameNumber = 0;
    while (true)
    {
        auto[shouldCloseResult, shouldClose] = mainWindow->shouldClose();
        criticalVkfwAssert(shouldCloseResult, "error in glfwWindowShouldClose)");
        if (shouldClose)
        {
            break;
        }
        else
        {
            // actual main loop
            criticalVkfwAssert(vkfw::pollEvents(), "error in glfwPollEvents");
            swapchainNeedRecreation |= drawFrame(device.get(), swapchain.get(), graphicQueue, imageAvailableSemaphores[frameNumber].get(),
                    renderFinishedSemaphores[frameNumber].get(), commandBufferFences[frameNumber].get(), imageFences, commandBuffers);
            if (swapchainNeedRecreation)
            {
                criticalVulkanAssert(device->waitIdle(), "error waiting for device");
                // TODO: implement resource reuse
                // order is important
                commandBuffers.clear();
                framebuffers.clear();
                swapchainImageViews.clear();
                renderPass.reset();
                swapchain.reset();
                std::tie(swapchain, format, extent, renderPass, graphicPipeline, swapchainImageViews, framebuffers, commandBuffers) =
                    recreateSwapchain(device.get(), queueFamilyIndeces, 
                            mainWindow.get(), physicalDevice, surface.get(), commandPool.get());
            }
            frameNumber = (frameNumber + 1) % inFlightFrames;
        }
    }

    device->waitIdle();
}
