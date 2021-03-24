#include <vkfw/vkfw.hpp>
#include <utils/assert.hpp>
#include <utils/executable_folder.hpp>
#include <renderer/render_system.hpp>

void criticalVkfwAssert(vkfw::Result received, std::string message)
{
    criticalAssertEqual(received, vkfw::Result::eSuccess, std::move(message));
}

vkfw::UniqueWindow initWindow()
{
    auto[result, window] = vkfw::createWindowUnique(800, 600, "Chess");
    criticalVkfwAssert(result, "error creating window");
    return std::move(window);
}

int main(int argc, char* argv[])
{
    setExecutableFolder(argv[0]);
    criticalVkfwAssert(vkfw::init(), "error in glfw init");
    vkfw::UniqueWindow mainWindow = initWindow();
    RenderSystem::init(mainWindow.get());
    RenderSystem& renderSystem = RenderSystem::instance();
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
            renderSystem.update(0.0f);
        }
    }
}
