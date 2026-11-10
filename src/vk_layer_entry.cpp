#include "vk_layer_dispatch.hpp"
#include "barrier_reducer.hpp"
#include "tbdr_optimizer.hpp"
#include "feature_fallbacks.hpp"

#include <cstring>
#include <vector>
#include <dlfcn.h>

namespace MaliTBDR {

bool LayerContext::InitializeDriver() {
    if (RealDriverHandle != nullptr) {
        return true;
    }

    LOGI("Inicializando Mali-G720 TBDR Vulkan Interceptor...");

    // Candidatos de carregamento do driver Vulkan no Android
    const char* driverPaths[] = {
        "/vendor/lib64/hw/vulkan.mali.so",
        "/system/lib64/libvulkan.so",
        "libvulkan.so",
        nullptr
    };

    for (int i = 0; driverPaths[i] != nullptr; ++i) {
        RealDriverHandle = dlopen(driverPaths[i], RTLD_NOW | RTLD_LOCAL);
        if (RealDriverHandle) {
            LOGI("Driver Vulkan nativo carregado com sucesso a partir de: %s", driverPaths[i]);
            break;
        }
    }

    if (!RealDriverHandle) {
        RealDriverHandle = dlopen("libvulkan.so", RTLD_NOW | RTLD_GLOBAL);
    }

    if (!RealDriverHandle) {
        LOGE("Falha ao localizar libvulkan.so nativo no Android!");
        return false;
    }

    RealDispatch.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(RealDriverHandle, "vkGetInstanceProcAddr")
    );
    RealDispatch.GetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        dlsym(RealDriverHandle, "vkGetDeviceProcAddr")
    );

    if (!RealDispatch.GetInstanceProcAddr) {
        LOGE("Nao foi possivel obter vkGetInstanceProcAddr!");
        return false;
    }

    RealDispatch.CreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        RealDispatch.GetInstanceProcAddr(nullptr, "vkCreateInstance")
    );

    return true;
}

// Hook de CreateInstance
static VkResult VKAPI_CALL Hook_vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance
) {
    auto& ctx = LayerContext::Get();
    if (!ctx.InitializeDriver()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkResult res = ctx.RealDispatch.CreateInstance(pCreateInfo, pAllocator, pInstance);
    if (res == VK_SUCCESS && pInstance && *pInstance) {
        ctx.RealDispatch.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkDestroyInstance")
        );
        ctx.RealDispatch.CreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkCreateDevice")
        );
        ctx.RealDispatch.EnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkEnumerateDeviceExtensionProperties")
        );
        ctx.RealDispatch.GetPhysicalDeviceFeatures = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceFeatures")
        );
        ctx.RealDispatch.GetPhysicalDeviceFeatures2 = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceFeatures2")
        );
        ctx.RealDispatch.GetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties")
        );
        ctx.RealDispatch.GetPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            ctx.RealDispatch.GetInstanceProcAddr(*pInstance, "vkGetPhysicalDeviceProperties2")
        );
        LOGI("Instancia Vulkan interceptada com sucesso para Poco X8 Pro (Mali-G720).");
    }
    return res;
}

// Hook de CreateDevice: sanitiza extensoes e structs injetadas antes de repassar ao driver nativo
static VkResult VKAPI_CALL Hook_vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice
) {
    auto& ctx = LayerContext::Get();
    if (!ctx.RealDispatch.CreateDevice) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    if (!pCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDeviceCreateInfo sanitizedInfo = *pCreateInfo;
    std::vector<const char*> sanitizedExtensions;

    if (pCreateInfo->enabledExtensionCount > 0 && pCreateInfo->ppEnabledExtensionNames != nullptr) {
        sanitizedExtensions.reserve(pCreateInfo->enabledExtensionCount);
        for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; ++i) {
            const char* extName = pCreateInfo->ppEnabledExtensionNames[i];
            if (!FeatureFallbacks::IsInjectedExtension(extName)) {
                sanitizedExtensions.push_back(extName);
            } else {
                LOGI("Filtrando extensao emulada '%s' para chamada nativa vkCreateDevice", extName);
            }
        }
        sanitizedInfo.enabledExtensionCount = static_cast<uint32_t>(sanitizedExtensions.size());
        sanitizedInfo.ppEnabledExtensionNames = sanitizedExtensions.data();
    }

    // Sanitiza a cadeia pNext para remover estruturas de recursos injetados que a Mali nativa rejeitaria
    struct BaseHeader {
        VkStructureType sType;
        void* pNext;
    };

    BaseHeader* prev = nullptr;
    auto* curr = const_cast<BaseHeader*>(reinterpret_cast<const BaseHeader*>(sanitizedInfo.pNext));
    while (curr != nullptr) {
        if (curr->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT) {
            if (prev) {
                prev->pNext = curr->pNext;
            } else {
                sanitizedInfo.pNext = curr->pNext;
            }
        } else {
            prev = curr;
        }
        curr = reinterpret_cast<BaseHeader*>(curr->pNext);
    }

    VkResult res = ctx.RealDispatch.CreateDevice(physicalDevice, &sanitizedInfo, pAllocator, pDevice);
    if (res == VK_SUCCESS && pDevice && *pDevice) {
        ctx.RealDispatch.DestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkDestroyDevice")
        );
        ctx.RealDispatch.CreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCreateRenderPass")
        );
        ctx.RealDispatch.CreateRenderPass2 = reinterpret_cast<PFN_vkCreateRenderPass2>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCreateRenderPass2")
        );
        ctx.RealDispatch.CmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCmdBeginRenderPass")
        );
        ctx.RealDispatch.CmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCmdEndRenderPass")
        );
        ctx.RealDispatch.CmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCmdPipelineBarrier")
        );
        ctx.RealDispatch.CmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCmdPipelineBarrier2")
        );
        ctx.RealDispatch.CmdPipelineBarrier2KHR = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkCmdPipelineBarrier2KHR")
        );
        ctx.RealDispatch.QueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(
            ctx.RealDispatch.GetDeviceProcAddr(*pDevice, "vkQueueSubmit")
        );
        LOGI("Dispositivo Vulkan interceptado com sucesso. Pipeline TBDR ativo!");
    }
    return res;
}

} // namespace MaliTBDR

#ifndef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#endif

extern "C" {

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance instance,
    const char* pName
) {
    if (!pName) return nullptr;

    if (strcmp(pName, "vkGetInstanceProcAddr") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetInstanceProcAddr);
    }
    if (strcmp(pName, "vkCreateInstance") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::Hook_vkCreateInstance);
    }
    if (strcmp(pName, "vkCreateDevice") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::Hook_vkCreateDevice);
    }
    if (strcmp(pName, "vkEnumerateDeviceExtensionProperties") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::FeatureFallbacks::InterceptEnumerateDeviceExtensionProperties);
    }
    if (strcmp(pName, "vkGetPhysicalDeviceFeatures") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::FeatureFallbacks::InterceptGetPhysicalDeviceFeatures);
    }
    if (strcmp(pName, "vkGetPhysicalDeviceFeatures2") == 0 ||
        strcmp(pName, "vkGetPhysicalDeviceFeatures2KHR") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::FeatureFallbacks::InterceptGetPhysicalDeviceFeatures2);
    }
    if (strcmp(pName, "vkGetPhysicalDeviceProperties") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::FeatureFallbacks::InterceptGetPhysicalDeviceProperties);
    }
    if (strcmp(pName, "vkGetPhysicalDeviceProperties2") == 0 ||
        strcmp(pName, "vkGetPhysicalDeviceProperties2KHR") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::FeatureFallbacks::InterceptGetPhysicalDeviceProperties2);
    }

    auto& ctx = MaliTBDR::LayerContext::Get();
    if (!ctx.RealDriverHandle) {
        ctx.InitializeDriver();
    }
    if (ctx.RealDispatch.GetInstanceProcAddr) {
        return ctx.RealDispatch.GetInstanceProcAddr(instance, pName);
    }
    return nullptr;
}

VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice device,
    const char* pName
) {
    if (!pName) return nullptr;

    if (strcmp(pName, "vkGetDeviceProcAddr") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(vkGetDeviceProcAddr);
    }
    if (strcmp(pName, "vkCmdPipelineBarrier") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::BarrierReducer::InterceptCmdPipelineBarrier);
    }
    if (strcmp(pName, "vkCmdPipelineBarrier2") == 0 || 
        strcmp(pName, "vkCmdPipelineBarrier2KHR") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::BarrierReducer::InterceptCmdPipelineBarrier2);
    }
    if (strcmp(pName, "vkCreateRenderPass") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::TBDROptimizer::InterceptCreateRenderPass);
    }
    if (strcmp(pName, "vkCreateRenderPass2") == 0 ||
        strcmp(pName, "vkCreateRenderPass2KHR") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::TBDROptimizer::InterceptCreateRenderPass2);
    }
    if (strcmp(pName, "vkCmdBeginRenderPass") == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(MaliTBDR::TBDROptimizer::InterceptCmdBeginRenderPass);
    }

    auto& ctx = MaliTBDR::LayerContext::Get();
    if (ctx.RealDispatch.GetDeviceProcAddr) {
        return ctx.RealDispatch.GetDeviceProcAddr(device, pName);
    }
    return nullptr;
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties
) {
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;

    if (!pProperties) {
        *pPropertyCount = 1;
        return VK_SUCCESS;
    }

    if (*pPropertyCount < 1) return VK_INCOMPLETE;

    strncpy(pProperties[0].layerName, "VK_LAYER_MALI_g720_tbdr_opt", VK_MAX_EXTENSION_NAME_SIZE);
    pProperties[0].specVersion = VK_MAKE_VERSION(1, 3, 0);
    pProperties[0].implementationVersion = 1;
    strncpy(pProperties[0].description, "Mali-G720 Dimensity TBDR Optimization & Barrier Reducer Layer", VK_MAX_DESCRIPTION_SIZE);

    *pPropertyCount = 1;
    return VK_SUCCESS;
}

VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties
) {
    (void)pLayerName;
    if (!pPropertyCount) return VK_ERROR_INITIALIZATION_FAILED;
    if (!pProperties) {
        *pPropertyCount = 0;
        return VK_SUCCESS;
    }
    *pPropertyCount = 0;
    return VK_SUCCESS;
}

} // extern "C"
