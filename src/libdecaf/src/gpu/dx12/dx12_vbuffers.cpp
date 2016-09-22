#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"

namespace gpu
{

namespace dx12
{

bool
Driver::checkActiveVertexBuffers()
{
   // TODO: Use appropriate shader stage output data to cull shader uploads

   std::array<D3D12_VERTEX_BUFFER_VIEW, latte::MaxAttributes> views;
   std::array<BufferView, latte::MaxAttributes> bviews;

   for (auto i = 0u; i < latte::MaxAttributes; ++i) {
      auto resourceOffset = (latte::SQ_RES_OFFSET::VS_ATTRIB_RESOURCE_0 + i) * 7;
      auto sq_vtx_constant_word0 = getRegister<latte::SQ_VTX_CONSTANT_WORD0_N>(latte::Register::SQ_VTX_CONSTANT_WORD0_0 + 4 * resourceOffset);
      auto sq_vtx_constant_word1 = getRegister<latte::SQ_VTX_CONSTANT_WORD1_N>(latte::Register::SQ_VTX_CONSTANT_WORD1_0 + 4 * resourceOffset);
      auto sq_vtx_constant_word2 = getRegister<latte::SQ_VTX_CONSTANT_WORD2_N>(latte::Register::SQ_VTX_CONSTANT_WORD2_0 + 4 * resourceOffset);

      auto dataPtr = mem::translate(sq_vtx_constant_word0.BASE_ADDRESS());
      auto size = sq_vtx_constant_word1.SIZE() + 1;
      auto stride = sq_vtx_constant_word2.STRIDE();

      if (dataPtr) {
         auto view = getDataBuffer(dataPtr, size);

         bviews[i] = view;
         views[i].BufferLocation = view.object->GetGPUVirtualAddress();
         views[i].SizeInBytes = size;
         views[i].StrideInBytes = stride;
      } else {
         views[i].BufferLocation = 0;
         views[i].SizeInBytes = 0;
         views[i].StrideInBytes = 0;
      }
   }

   mCmdList->IASetVertexBuffers(0, latte::MaxAttributes, views.data());
   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
