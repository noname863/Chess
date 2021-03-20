#define VKFW_ASSERT_ON_RESULT(expr)
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan.hpp>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <set>

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
    FamilyIndeces(std::initializer_list<uint32_t> list)
    {
        for (uint32_t number : list)
        {
            if (std::find(indexes.begin(), indexes.end(), number) == indexes.end())
            {
                indexes.push_back(number);
            }
        }
    }
    FamilyIndeces(const FamilyIndeces&) = default;
    FamilyIndeces(FamilyIndeces&&) = default;
    std::vector<uint32_t> indexes;
};

vkfw::UniqueWindow initWindow()
{
    // criticalVkfwAssert(vkfw::setWindowHint(vkfw::OptionalWindowHint<vkfw::WindowHint::eResizable>{false}), 
    //         "error setting resizable hint");
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
    uint32_t indexSurfaceSupported = std::numeric_limits<uint32_t>::max();
    uint32_t indexGraphicsSupported = std::numeric_limits<uint32_t>::max();
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
            return FamilyIndeces{i};
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
            return {physicalDevice, optionalFamilyIndeces.value()};
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
    vk::DeviceCreateInfo deviceCreateInfo({}, queueInfos.size(), queueInfos.data(), 
            0, nullptr, requiredDeviceExtentions.size(), requiredDeviceExtentions.data());
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
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
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
    return std::move(result);
}

int main()
{
    criticalVkfwAssert(vkfw::init(), "error in glfw init");
    vkfw::UniqueWindow mainWindow = initWindow();
    vk::UniqueInstance instance = createVkInstance();
    vk::UniqueSurfaceKHR surface = vkfw::createWindowSurfaceUnique(
            instance.get(), mainWindow.get());
    auto [physicalDevice, queueFamilyIndeces] =
        pickPhysicalDeviceAndQueueFamily(instance.get(), surface.get());
    vk::UniqueDevice device = createDevice(physicalDevice, queueFamilyIndeces);
    auto [swapchain, format, extent] = createSwapChain(physicalDevice, device.get(), 
            queueFamilyIndeces, mainWindow.get(), surface.get());

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
        }
    }
}
