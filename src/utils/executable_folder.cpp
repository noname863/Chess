#include <utils/executable_folder.hpp>

std::filesystem::path s_executableFolder;

void setExecutableFolder(std::string executablePath)
{
    s_executableFolder = std::filesystem::absolute(std::filesystem::path(executablePath).remove_filename());
}

std::filesystem::path getExecutableFolder()
{
    return s_executableFolder;
}

