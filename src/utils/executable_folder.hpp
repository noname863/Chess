#pragma once
#include <filesystem>
#include <string>

void setExecutableFolder(std::string executablePath);
std::filesystem::path getExecutableFolder();

