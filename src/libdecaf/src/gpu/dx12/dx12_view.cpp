#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"

namespace gpu
{

namespace dx12
{

bool
Driver::checkActiveView()
{
   auto pa_cl_vport_xscale = getRegister<latte::PA_CL_VPORT_XSCALE_N>(latte::Register::PA_CL_VPORT_XSCALE_0);
   auto pa_cl_vport_yscale = getRegister<latte::PA_CL_VPORT_YSCALE_N>(latte::Register::PA_CL_VPORT_YSCALE_0);
   auto pa_cl_vport_zscale = getRegister<latte::PA_CL_VPORT_ZSCALE_N>(latte::Register::PA_CL_VPORT_ZSCALE_0);
   auto pa_cl_vport_xoffset = getRegister<latte::PA_CL_VPORT_XOFFSET_N>(latte::Register::PA_CL_VPORT_XOFFSET_0);
   auto pa_cl_vport_yoffset = getRegister<latte::PA_CL_VPORT_YOFFSET_N>(latte::Register::PA_CL_VPORT_YOFFSET_0);
   auto pa_cl_vport_zoffset = getRegister<latte::PA_CL_VPORT_ZOFFSET_N>(latte::Register::PA_CL_VPORT_ZOFFSET_0);
   auto pa_sc_vport_zmin = getRegister<latte::PA_SC_VPORT_ZMIN_N>(latte::Register::PA_SC_VPORT_ZMIN_0);
   auto pa_sc_vport_zmax = getRegister<latte::PA_SC_VPORT_ZMAX_N>(latte::Register::PA_SC_VPORT_ZMAX_0);

   D3D12_VIEWPORT viewport;
   viewport.Width = pa_cl_vport_xscale.VPORT_XSCALE() * 2.0f;
   viewport.Height = pa_cl_vport_yscale.VPORT_YSCALE() * 2.0f;
   viewport.TopLeftX = pa_cl_vport_xoffset.VPORT_XOFFSET() - pa_cl_vport_xscale.VPORT_XSCALE();
   viewport.TopLeftY = pa_cl_vport_yoffset.VPORT_YOFFSET() - pa_cl_vport_yscale.VPORT_YSCALE();

   // TODO: Investigate whether we should be using ZOFFSET/ZSCALE to calculate these?
   if (pa_cl_vport_zscale.VPORT_ZSCALE() > 0.0f) {
      viewport.MinDepth = pa_sc_vport_zmin.VPORT_ZMIN();
      viewport.MaxDepth = pa_sc_vport_zmax.VPORT_ZMAX();
   } else {
      viewport.MaxDepth = pa_sc_vport_zmin.VPORT_ZMIN();
      viewport.MinDepth = pa_sc_vport_zmax.VPORT_ZMAX();
   }

   mCmdList->RSSetViewports(1, &viewport);

   auto pa_sc_generic_scissor_tl = getRegister<latte::PA_SC_GENERIC_SCISSOR_TL>(latte::Register::PA_SC_GENERIC_SCISSOR_TL);
   auto pa_sc_generic_scissor_br = getRegister<latte::PA_SC_GENERIC_SCISSOR_BR>(latte::Register::PA_SC_GENERIC_SCISSOR_BR);

   D3D12_RECT scissor;
   scissor.left = pa_sc_generic_scissor_tl.TL_X();
   scissor.top = pa_sc_generic_scissor_tl.TL_Y();
   scissor.right = pa_sc_generic_scissor_br.BR_X();
   scissor.bottom = pa_sc_generic_scissor_br.BR_Y();
   mCmdList->RSSetScissorRects(1, &scissor);

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
