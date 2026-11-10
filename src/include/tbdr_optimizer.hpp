#pragma once

#include "vk_layer_dispatch.hpp"

namespace MaliTBDR {

class TBDROptimizer {
public:
    static VkResult InterceptCreateRenderPass(
        VkDevice device,
        const VkRenderPassCreateInfo* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkRenderPass* pRenderPass
    );

    static VkResult InterceptCreateRenderPass2(
        VkDevice device,
        const VkRenderPassCreateInfo2* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkRenderPass* pRenderPass
    );

    static void InterceptCmdBeginRenderPass(
        VkCommandBuffer commandBuffer,
        const VkRenderPassBeginInfo* pRenderPassBegin,
        VkSubpassContents contents
    );
};

} // namespace MaliTBDR
