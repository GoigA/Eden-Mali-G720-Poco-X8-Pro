#include "feature_fallbacks.hpp"
#include <cstring>
#include <vector>

namespace MaliTBDR {

// Extensões adicionais que emuladores de Switch (Eden/Yuzu) necessitam para considerar a GPU adequada
static const char* s_InjectedExtensions[] = {
    "VK_EXT_vertex_attribute_divisor",
    "VK_EXT_custom_border_color",
    "VK_EXT_extended_dynamic_state",
    "VK_EXT_extended_dynamic_state2",
    "VK_EXT_depth_bounds_test",
    "VK_EXT_4444_formats"
};

bool FeatureFallbacks::IsInjectedExtension(const char* extName) {
    if (!extName) return false;
    for (const char* injected : s_InjectedExtensions) {
        if (strcmp(extName, injected) == 0) {
            return true;
        }
    }
    return false;
}

VkResult FeatureFallbacks::InterceptEnumerateDeviceExtensionProperties(
    VkPhysicalDevice physicalDevice,
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties
) {
    auto& ctx = LayerContext::Get();

    if (!ctx.RealDispatch.EnumerateDeviceExtensionProperties) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t nativeCount = 0;
    VkResult res = ctx.RealDispatch.EnumerateDeviceExtensionProperties(
        physicalDevice, pLayerName, &nativeCount, nullptr
    );

    if (res != VK_SUCCESS) return res;

    std::vector<VkExtensionProperties> allExtensions(nativeCount);
    res = ctx.RealDispatch.EnumerateDeviceExtensionProperties(
        physicalDevice, pLayerName, &nativeCount, allExtensions.data()
    );

    if (res != VK_SUCCESS) return res;

    // Injeta extensões de compatibilidade se ainda não estiverem na lista nativa
    for (const char* extName : s_InjectedExtensions) {
        bool found = false;
        for (const auto& existing : allExtensions) {
            if (strcmp(existing.extensionName, extName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            VkExtensionProperties injected{};
            strncpy(injected.extensionName, extName, VK_MAX_EXTENSION_NAME_SIZE - 1);
            injected.specVersion = 1;
            allExtensions.push_back(injected);
        }
    }

    if (pProperties == nullptr) {
        *pPropertyCount = static_cast<uint32_t>(allExtensions.size());
        return VK_SUCCESS;
    }

    uint32_t toCopy = (*pPropertyCount < allExtensions.size()) ? *pPropertyCount : static_cast<uint32_t>(allExtensions.size());
    memcpy(pProperties, allExtensions.data(), toCopy * sizeof(VkExtensionProperties));
    *pPropertyCount = toCopy;

    return (toCopy < allExtensions.size()) ? VK_INCOMPLETE : VK_SUCCESS;
}

void FeatureFallbacks::InterceptGetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures* pFeatures
) {
    auto& ctx = LayerContext::Get();

    if (ctx.RealDispatch.GetPhysicalDeviceFeatures) {
        ctx.RealDispatch.GetPhysicalDeviceFeatures(physicalDevice, pFeatures);
    }

    if (pFeatures) {
        pFeatures->samplerAnisotropy = VK_TRUE;
        pFeatures->independentBlend = VK_TRUE;
        pFeatures->textureCompressionBC = VK_FALSE;
        pFeatures->textureCompressionASTC_LDR = VK_TRUE;
        pFeatures->depthClamp = VK_TRUE;
    }
}

void FeatureFallbacks::InterceptGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures
) {
    auto& ctx = LayerContext::Get();

    if (ctx.RealDispatch.GetPhysicalDeviceFeatures2) {
        ctx.RealDispatch.GetPhysicalDeviceFeatures2(physicalDevice, pFeatures);
    }

    if (pFeatures) {
        InterceptGetPhysicalDeviceFeatures(physicalDevice, &pFeatures->features);

        // Percorre a cadeia de estruturas pNext para preencher recursos de extensões injetadas
        struct VkBaseHeader {
            VkStructureType sType;
            void* pNext;
        };

        auto* current = reinterpret_cast<VkBaseHeader*>(pFeatures->pNext);
        while (current != nullptr) {
            if (current->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT) {
                auto* divFeatures = reinterpret_cast<VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT*>(current);
                divFeatures->vertexAttributeInstanceRateDivisor = VK_TRUE;
                divFeatures->vertexAttributeInstanceRateZeroDivisor = VK_TRUE;
            }
            current = reinterpret_cast<VkBaseHeader*>(current->pNext);
        }
    }
}

void FeatureFallbacks::InterceptGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties* pProperties
) {
    auto& ctx = LayerContext::Get();

    if (ctx.RealDispatch.GetPhysicalDeviceProperties) {
        ctx.RealDispatch.GetPhysicalDeviceProperties(physicalDevice, pProperties);
    }

    if (pProperties) {
        LOGI("Dispositivo detectado: %s (Driver Version: 0x%08x)", 
             pProperties->deviceName, pProperties->driverVersion);
    }
}

void FeatureFallbacks::InterceptGetPhysicalDeviceProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties2* pProperties
) {
    auto& ctx = LayerContext::Get();

    if (ctx.RealDispatch.GetPhysicalDeviceProperties2) {
        ctx.RealDispatch.GetPhysicalDeviceProperties2(physicalDevice, pProperties);
    }

    if (pProperties) {
        InterceptGetPhysicalDeviceProperties(physicalDevice, &pProperties->properties);

        struct VkBaseHeader {
            VkStructureType sType;
            void* pNext;
        };

        auto* current = reinterpret_cast<VkBaseHeader*>(pProperties->pNext);
        while (current != nullptr) {
            if (current->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_PROPERTIES_EXT) {
                auto* divProps = reinterpret_cast<VkPhysicalDeviceVertexAttributeDivisorPropertiesEXT*>(current);
                divProps->maxVertexAttribDivisor = 65535;
            }
            current = reinterpret_cast<VkBaseHeader*>(current->pNext);
        }
    }
}

} // namespace MaliTBDR
