#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"

namespace gpu
{

namespace dx12
{

bool
Driver::checkActiveRenderTargets()
{
   // TODO: Implement depth/stencil buffers

   std::array<SurfaceTexture *, latte::MaxRenderTargets> targets = { { nullptr } };
   auto maxUsedTarget = 0u;

   for (auto i = 0u; i < latte::MaxRenderTargets; ++i) {
      auto cb_color_base = getRegister<latte::CB_COLORN_BASE>(latte::Register::CB_COLOR0_BASE + i * 4);
      auto cb_color_size = getRegister<latte::CB_COLORN_SIZE>(latte::Register::CB_COLOR0_SIZE + i * 4);
      auto cb_color_info = getRegister<latte::CB_COLORN_INFO>(latte::Register::CB_COLOR0_INFO + i * 4);

      if (mActivePipeline->data.RTVFormats[i] == DXGI_FORMAT_UNKNOWN) {
         // The pipeline does a bunch of checks to preemptively disable render
         //  targets that do not actually do anything, we simply use that to
         //  determine if we actually need to bind a render target.
         continue;
      }

      // The pipeline should have done any neccessary disabling of invalid RTs.
      decaf_check(cb_color_base.BASE_256B());

      // Grab the appropriate color surface...
      targets[i] = getColorBuffer(cb_color_base, cb_color_size, cb_color_info, false);

      // Count this is a target we need to use
      maxUsedTarget = i + 1;
   }

   // Bind the depth stencil target
   SurfaceTexture *depthStencilTarget = nullptr;

   // TODO: Actually locate the depth stencil target here.

   // Set up the descriptor lists for color buffers
   D3D12_CPU_DESCRIPTOR_HANDLE cbRtvList[latte::MaxRenderTargets] = { 0 };

   for (auto i = 0u; i < maxUsedTarget; ++i) {
      auto rtv = allocateRtvs(1);

      if (targets[i]) {
         transitionSurfaceTexture(targets[i], D3D12_RESOURCE_STATE_RENDER_TARGET);
         mDevice->CreateRenderTargetView(targets[i]->object.Get(), nullptr, rtv);
      } else {
         // TODO: Figure out how to handle non-sequential targets
         decaf_abort("Unsequential render target usage is not yet supported");
         //mDevice->CreateRenderTargetView(nullptr, nullptr, rtv);
      }

      cbRtvList[i] = rtv;
   }

   if (depthStencilTarget) {
      auto dbRtv = allocateRtvs(1);

      mDevice->CreateRenderTargetView(depthStencilTarget->object.Get(), nullptr, dbRtv);

      // Bind our color and depth targets to the command list
      D3D12_CPU_DESCRIPTOR_HANDLE dbRtvList[1] = { dbRtv };
      mCmdList->OMSetRenderTargets(maxUsedTarget, cbRtvList, false, dbRtvList);
   } else {
      // Bind only a color target to the command list
      mCmdList->OMSetRenderTargets(maxUsedTarget, cbRtvList, false, nullptr);
   }

   float cbBlendColor[] = {
      getRegister<float>(latte::Register::CB_BLEND_RED),
      getRegister<float>(latte::Register::CB_BLEND_GREEN),
      getRegister<float>(latte::Register::CB_BLEND_BLUE),
      getRegister<float>(latte::Register::CB_BLEND_ALPHA),
   };
   mCmdList->OMSetBlendFactor(cbBlendColor);

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
