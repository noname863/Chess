#define VKFW_ASSERT_ON_RESULT(expr)
#include <vkfw/vkfw.hpp>
#include <vulkan/vulkan.hpp>
#include <cassert>

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

// int rateDevice(vk::PhysicalDevice& device)
// {
// }

// bool deviceSuitable(vk::PhysicalDevice& device)
// {
//     bool result = true;
//     auto properties = device.getQueueFamilyProperties();
//     result &= std::find_if(properties.begin(), properties.end(), 
//             [](auto& queueFamily)
//             {
//                 return queueFamily.queueFlags & vk::QueueFlagBits::eGraphics;
//             }) != properties.end();
//     // possible another checks
// 
//     return result;
// }

std::pair<vk::PhysicalDevice, uint32_t> pickPhysicalDeviceAndQueueFamily(vk::Instance& instance)
{
    auto[result, physicalDevices] = instance.enumeratePhysicalDevices();
    criticalVulkanAssert(result, "error enumerating phisical devices");
    fassert(physicalDevices.size() != 0, "no physical devices found");
    for (auto& physicalDevice : physicalDevices)
    {
        auto queueFamiliesProperties = physicalDevice.getQueueFamilyProperties();
        for (uint32_t i = 0; i < queueFamiliesProperties.size(); ++i)
        {
            if (queueFamiliesProperties[i].queueFlags & vk::QueueFlagBits::eGraphics)
            {
                return {physicalDevice, i};
            }
        }
    }
    fassert(false, "no sutable device found");
    return {};
}

vk::UniqueDevice createDevice(vk::PhysicalDevice physicalDevice, uint32_t queueFamily)
{
    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamily, 1, &queuePriority);
    // default zero
    vk::PhysicalDeviceFeatures deviceFeatures{};
    vk::DeviceCreateInfo deviceCreateInfo({}, 1, &queueCreateInfo);
    auto[result, device] = physicalDevice.createDeviceUnique(deviceCreateInfo);
    criticalVulkanAssert(result, "failed to create logical device");
    return std::move(device);
}

int main()
{
    criticalVkfwAssert(vkfw::init(), "error in glfw init");
    vkfw::UniqueWindow mainWindow = initWindow();
    vk::UniqueInstance instance = createVkInstance();
    auto [physicalDevice, queueFamilyIndex] =
        pickPhysicalDeviceAndQueueFamily(instance.get());
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
