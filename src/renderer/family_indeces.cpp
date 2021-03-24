#include <renderer/family_indeces.hpp>

FamilyIndeces::FamilyIndeces(uint32_t graphicsFamily, uint32_t presentationFamily) :
    graphicsFamily(graphicsFamily),
    presentationFamily(presentationFamily)
{
    indexes.push_back(graphicsFamily);
    if (graphicsFamily != presentationFamily)
    {
        indexes.push_back(presentationFamily);
    }
}

