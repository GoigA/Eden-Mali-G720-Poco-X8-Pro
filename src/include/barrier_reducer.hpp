#pragma once

#include "vk_layer_dispatch.hpp"
#include <vector>

namespace MaliTBDR {

class BarrierReducer {
public:
    static void InterceptCmdPipelineBarrier(
        VkCommandBuffer commandBuffer,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkDependencyFlags dependencyFlags,
        uint32_t memoryBarrierCount,
        const VkMemoryBarrier* pMemoryBarriers,
        uint32_t bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier* pBufferMemoryBarriers,
        uint32_t imageMemoryBarrierCount,
        const VkImageMemoryBarrier* pImageMemoryBarriers
    );

    static void InterceptCmdPipelineBarrier2(
        VkCommandBuffer commandBuffer,
        const VkDependencyInfo* pDependencyInfo
    );

private:
    static VkPipelineStageFlags OptimizeStageFlags(VkPipelineStageFlags flags, bool isSource);
    static VkAccessFlags OptimizeAccessFlags(VkAccessFlags flags);
};

} // namespace MaliTBDR
