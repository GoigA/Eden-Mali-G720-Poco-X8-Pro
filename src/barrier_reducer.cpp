#include "barrier_reducer.hpp"
#include <vector>

namespace MaliTBDR {

VkPipelineStageFlags BarrierReducer::OptimizeStageFlags(VkPipelineStageFlags flags, bool isSource) {
    // Detecta sincronização excessiva global que força esvaziamento do tile buffer na Mali G720
    if (flags & VK_PIPELINE_STAGE_ALL_COMMANDS_BIT) {
        flags &= ~VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        if (isSource) {
            flags |= (VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                      VK_PIPELINE_STAGE_TRANSFER_BIT);
        } else {
            flags |= (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                      VK_PIPELINE_STAGE_TRANSFER_BIT);
        }
    }

    if (flags & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) {
        flags &= ~VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        if (isSource) {
            flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        } else {
            flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
    }

    // Se a flag ficou vazia por algum motivo de sanitização, garante estágio seguro
    if (flags == 0) {
        flags = isSource ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    return flags;
}

VkAccessFlags BarrierReducer::OptimizeAccessFlags(VkAccessFlags flags) {
    // Reduz flushes globais de memória na arquitetura CSF da GPU Dimensity
    if ((flags & (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)) == 
        (VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT)) {
        flags &= ~(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
        flags |= (VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | 
                  VK_ACCESS_SHADER_READ_BIT | 
                  VK_ACCESS_TRANSFER_READ_BIT | 
                  VK_ACCESS_TRANSFER_WRITE_BIT);
    }
    return flags;
}

void BarrierReducer::InterceptCmdPipelineBarrier(
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
) {
    auto& ctx = LayerContext::Get();

    VkPipelineStageFlags optSrcStage = OptimizeStageFlags(srcStageMask, true);
    VkPipelineStageFlags optDstStage = OptimizeStageFlags(dstStageMask, false);

    // Otimiza barreiras de imagem (evitando transições redundantes em tile buffers)
    std::vector<VkImageMemoryBarrier> optimizedImageBarriers;
    if (imageMemoryBarrierCount > 0 && pImageMemoryBarriers != nullptr) {
        optimizedImageBarriers.reserve(imageMemoryBarrierCount);
        for (uint32_t i = 0; i < imageMemoryBarrierCount; ++i) {
            VkImageMemoryBarrier barrier = pImageMemoryBarriers[i];

            // Coalescência: transição idêntica de layout sem troca de fila
            if (barrier.oldLayout == barrier.newLayout &&
                barrier.srcQueueFamilyIndex == barrier.dstQueueFamilyIndex) {
                barrier.srcAccessMask = OptimizeAccessFlags(barrier.srcAccessMask);
                barrier.dstAccessMask = OptimizeAccessFlags(barrier.dstAccessMask);
                ctx.RedundantTransitionsSkipped++;
            } else {
                barrier.srcAccessMask = OptimizeAccessFlags(barrier.srcAccessMask);
                barrier.dstAccessMask = OptimizeAccessFlags(barrier.dstAccessMask);
            }
            optimizedImageBarriers.push_back(barrier);
        }
    }

    ctx.BarriersReducedCount++;

    const VkImageMemoryBarrier* pFinalImageBarriers = 
        optimizedImageBarriers.empty() ? pImageMemoryBarriers : optimizedImageBarriers.data();

    // Envia chamada otimizada para o driver real
    if (ctx.RealDispatch.CmdPipelineBarrier) {
        ctx.RealDispatch.CmdPipelineBarrier(
            commandBuffer,
            optSrcStage,
            optDstStage,
            dependencyFlags,
            memoryBarrierCount,
            pMemoryBarriers,
            bufferMemoryBarrierCount,
            pBufferMemoryBarriers,
            imageMemoryBarrierCount,
            pFinalImageBarriers
        );
    }
}

void BarrierReducer::InterceptCmdPipelineBarrier2(
    VkCommandBuffer commandBuffer,
    const VkDependencyInfo* pDependencyInfo
) {
    auto& ctx = LayerContext::Get();

    if (!pDependencyInfo) return;

    VkDependencyInfo optDepInfo = *pDependencyInfo;
    std::vector<VkImageMemoryBarrier2> optImageBarriers;

    if (pDependencyInfo->imageMemoryBarrierCount > 0 && pDependencyInfo->pImageMemoryBarriers != nullptr) {
        optImageBarriers.reserve(pDependencyInfo->imageMemoryBarrierCount);
        for (uint32_t i = 0; i < pDependencyInfo->imageMemoryBarrierCount; ++i) {
            VkImageMemoryBarrier2 b = pDependencyInfo->pImageMemoryBarriers[i];
            b.srcStageMask = OptimizeStageFlags(static_cast<VkPipelineStageFlags>(b.srcStageMask), true);
            b.dstStageMask = OptimizeStageFlags(static_cast<VkPipelineStageFlags>(b.dstStageMask), false);
            b.srcAccessMask = OptimizeAccessFlags(static_cast<VkAccessFlags>(b.srcAccessMask));
            b.dstAccessMask = OptimizeAccessFlags(static_cast<VkAccessFlags>(b.dstAccessMask));
            optImageBarriers.push_back(b);
        }
        optDepInfo.pImageMemoryBarriers = optImageBarriers.data();
    }

    ctx.BarriersReducedCount++;

    if (ctx.RealDispatch.CmdPipelineBarrier2) {
        ctx.RealDispatch.CmdPipelineBarrier2(commandBuffer, &optDepInfo);
    } else if (ctx.RealDispatch.CmdPipelineBarrier2KHR) {
        ctx.RealDispatch.CmdPipelineBarrier2KHR(commandBuffer, &optDepInfo);
    }
}

} // namespace MaliTBDR
