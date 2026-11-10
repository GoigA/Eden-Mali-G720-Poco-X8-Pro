#include "tbdr_optimizer.hpp"
#include <vector>

namespace MaliTBDR {

VkResult TBDROptimizer::InterceptCreateRenderPass(
    VkDevice device,
    const VkRenderPassCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkRenderPass* pRenderPass
) {
    auto& ctx = LayerContext::Get();

    if (!pCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkRenderPassCreateInfo optInfo = *pCreateInfo;
    std::vector<VkAttachmentDescription> optAttachments;

    if (pCreateInfo->attachmentCount > 0 && pCreateInfo->pAttachments != nullptr) {
        optAttachments.reserve(pCreateInfo->attachmentCount);
        for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i) {
            VkAttachmentDescription desc = pCreateInfo->pAttachments[i];

            // Na arquitetura Mali TBDR, se o layout inicial for UNDEFINED e loadOp for LOAD,
            // isso causa leitura inútil da LPDDR5 para o Tile Buffer da GPU.
            if (desc.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
                desc.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
                desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                ctx.RenderPassOptimizedCount++;
            }

            // Otimização de stencil em buffers transitórios de profundidade
            if (desc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
                desc.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
                desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }

            optAttachments.push_back(desc);
        }
        optInfo.pAttachments = optAttachments.data();
    }

    if (ctx.RealDispatch.CreateRenderPass) {
        return ctx.RealDispatch.CreateRenderPass(device, &optInfo, pAllocator, pRenderPass);
    }

    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult TBDROptimizer::InterceptCreateRenderPass2(
    VkDevice device,
    const VkRenderPassCreateInfo2* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkRenderPass* pRenderPass
) {
    auto& ctx = LayerContext::Get();

    if (!pCreateInfo) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkRenderPassCreateInfo2 optInfo = *pCreateInfo;
    std::vector<VkAttachmentDescription2> optAttachments;

    if (pCreateInfo->attachmentCount > 0 && pCreateInfo->pAttachments != nullptr) {
        optAttachments.reserve(pCreateInfo->attachmentCount);
        for (uint32_t i = 0; i < pCreateInfo->attachmentCount; ++i) {
            VkAttachmentDescription2 desc = pCreateInfo->pAttachments[i];

            if (desc.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED && 
                desc.loadOp == VK_ATTACHMENT_LOAD_OP_LOAD) {
                desc.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                ctx.RenderPassOptimizedCount++;
            }

            if (desc.stencilLoadOp == VK_ATTACHMENT_LOAD_OP_LOAD &&
                desc.initialLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
                desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            }

            optAttachments.push_back(desc);
        }
        optInfo.pAttachments = optAttachments.data();
    }

    if (ctx.RealDispatch.CreateRenderPass2) {
        return ctx.RealDispatch.CreateRenderPass2(device, &optInfo, pAllocator, pRenderPass);
    }

    return VK_ERROR_INITIALIZATION_FAILED;
}

void TBDROptimizer::InterceptCmdBeginRenderPass(
    VkCommandBuffer commandBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents contents
) {
    auto& ctx = LayerContext::Get();

    if (ctx.RealDispatch.CmdBeginRenderPass) {
        ctx.RealDispatch.CmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
    }
}

} // namespace MaliTBDR
