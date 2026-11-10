#pragma once

#include <vulkan/vulkan.h>
#include <android/log.h>
#include <mutex>
#include <unordered_map>
#include <string>
#include <dlfcn.h>

#define LOG_TAG "MaliG720-TBDR-Layer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace MaliTBDR {

// Tabela de Dispatch com ponteiros para as funções reais do driver do sistema
struct DispatchTable {
    // Core Instance
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;
    PFN_vkCreateInstance CreateInstance = nullptr;
    PFN_vkDestroyInstance DestroyInstance = nullptr;
    PFN_vkCreateDevice CreateDevice = nullptr;
    PFN_vkDestroyDevice DestroyDevice = nullptr;

    // Physical Device Queries
    PFN_vkEnumerateDeviceExtensionProperties EnumerateDeviceExtensionProperties = nullptr;
    PFN_vkGetPhysicalDeviceFeatures GetPhysicalDeviceFeatures = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2 GetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceProperties2 GetPhysicalDeviceProperties2 = nullptr;

    // RenderPass / TBDR Subpasses
    PFN_vkCreateRenderPass CreateRenderPass = nullptr;
    PFN_vkCreateRenderPass2 CreateRenderPass2 = nullptr;
    PFN_vkCmdBeginRenderPass CmdBeginRenderPass = nullptr;
    PFN_vkCmdEndRenderPass CmdEndRenderPass = nullptr;

    // Barriers & Pipeline Synchronization
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier = nullptr;
    PFN_vkCmdPipelineBarrier2 CmdPipelineBarrier2 = nullptr;
    PFN_vkCmdPipelineBarrier2KHR CmdPipelineBarrier2KHR = nullptr;

    // Queue submission
    PFN_vkQueueSubmit QueueSubmit = nullptr;
};

class LayerContext {
public:
    static LayerContext& Get() {
        static LayerContext instance;
        return instance;
    }

    bool InitializeDriver();
    void* RealDriverHandle = nullptr;
    DispatchTable RealDispatch;

    // Estatísticas de otimização em tempo de execução
    uint64_t BarriersReducedCount = 0;
    uint64_t RenderPassOptimizedCount = 0;
    uint64_t RedundantTransitionsSkipped = 0;

private:
    LayerContext() = default;
    ~LayerContext() {
        if (RealDriverHandle) {
            dlclose(RealDriverHandle);
            RealDriverHandle = nullptr;
        }
    }
};

} // namespace MaliTBDR
