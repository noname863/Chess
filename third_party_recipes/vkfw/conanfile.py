from conans import ConanFile, tools


class VkfwConan(ConanFile):
    name = 'vkfw'
    version = '1.0.0'
    description = 'C++ Bindings for GLFW'
    license = 'C++ Bindings for GLFW'
    settings = None
    options = {
        "no_exceptions": [True, False],
        "opengl_enabled": [True, False]
    }
    default_options = {
        "no_exceptions": False,
        "opengl_enabled": False
    }

    def source(self):
        tools.download('https://raw.githubusercontent.com/Cvelth/vkfw/main/include/vkfw/vkfw.hpp', 'vkfw.hpp')

    def build(self):
        return

    def package(self):
        self.copy(pattern='vkfw.hpp', dst='include/vkfw')

    def package_info(self):
        if (self.options.no_exceptions):
            self.cpp_info.defines.append("VKFW_NO_EXCEPTIONS")
        if (self.options.opengl_enabled):
            self.cpp_info.defines.append("VKFW_NO_INCLUDE_VULKAN")
            self.cpp_info.defines.append("VKFW_INCLUDE_GL")
            self.ccp_info.defines.append("VKFW_NO_INCLUDE_VULKAN_HPP")

