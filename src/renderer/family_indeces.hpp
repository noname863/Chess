#pragma once
#include <cstdint>
#include <vector>

struct FamilyIndeces
{
    FamilyIndeces() = default;
    FamilyIndeces(uint32_t graphicsFamily, uint32_t presentationFamily);
    FamilyIndeces(const FamilyIndeces&) = default;
    FamilyIndeces(FamilyIndeces&&) = default;
    FamilyIndeces& operator=(const FamilyIndeces&) = default;
    FamilyIndeces& operator=(FamilyIndeces&&) = default;
    std::vector<uint32_t> indexes;
    uint32_t graphicsFamily;
    uint32_t presentationFamily;

};

