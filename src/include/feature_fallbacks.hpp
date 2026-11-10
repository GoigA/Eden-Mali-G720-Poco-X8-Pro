#pragma once

#include "vk_layer_dispatch.hpp"

namespace MaliTBDR {

class FeatureFallbacks {
public:
    static VkResult InterceptEnumerateDeviceExtensionProperties(
        VkPhysicalDevice physicalDevice,
        const char* pLayerName,
        uint32_t* pPropertyCount,
        VkExtensionProperties* pProperties
    );

    static void InterceptGetPhysicalDeviceFeatures(
        VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceFeatures* pFeatures
    );

    static void InterceptGetPhysicalDeviceFeatures2(
        VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceFeatures2* pFeatures
    );

    static void InterceptGetPhysicalDeviceProperties(
        VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceProperties* pProperties
    );

    static void InterceptGetPhysicalDeviceProperties2(
        VkPhysicalDevice physicalDevice,
        VkPhysicalDeviceProperties2* pProperties
    );

    static bool IsInjectedExtension(const char* extName);
};

} // namespace MaliTBDR
