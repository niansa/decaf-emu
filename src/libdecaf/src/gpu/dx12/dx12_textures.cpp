#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"
#include "dx12_utilities.h"

namespace gpu
{

namespace dx12
{

static D3D12_SHADER_COMPONENT_MAPPING
getDxComponentMapping(latte::SQ_SEL sel)
{
   switch (sel) {
   case latte::SQ_SEL::SEL_X:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0;
   case latte::SQ_SEL::SEL_Y:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_1;
   case latte::SQ_SEL::SEL_Z:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_2;
   case latte::SQ_SEL::SEL_W:
      return D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_3;
   case latte::SQ_SEL::SEL_0:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_0;
   case latte::SQ_SEL::SEL_1:
      return D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1;
   default:
      decaf_abort("Unexpected SEL value.");
   }
}

bool
Driver::checkActiveTextures()
{
   // TODO: Implement VS/GS textures..
   // TODO: Use appropriate shader stage output data to cull shader uploads
   // TODO: Properly assign all the data to here based on GPU7 registers...

   auto srvs = allocateSrvs(latte::MaxTextures);

   for (auto i = 0; i < latte::MaxTextures; ++i) {
      auto resourceOffset = (latte::SQ_RES_OFFSET::PS_TEX_RESOURCE_0 + i) * 7;
      auto sq_tex_resource_word0 = getRegister<latte::SQ_TEX_RESOURCE_WORD0_N>(latte::Register::SQ_TEX_RESOURCE_WORD0_0 + 4 * resourceOffset);
      auto sq_tex_resource_word1 = getRegister<latte::SQ_TEX_RESOURCE_WORD1_N>(latte::Register::SQ_TEX_RESOURCE_WORD1_0 + 4 * resourceOffset);
      auto sq_tex_resource_word2 = getRegister<latte::SQ_TEX_RESOURCE_WORD2_N>(latte::Register::SQ_TEX_RESOURCE_WORD2_0 + 4 * resourceOffset);
      auto sq_tex_resource_word3 = getRegister<latte::SQ_TEX_RESOURCE_WORD3_N>(latte::Register::SQ_TEX_RESOURCE_WORD3_0 + 4 * resourceOffset);
      auto sq_tex_resource_word4 = getRegister<latte::SQ_TEX_RESOURCE_WORD4_N>(latte::Register::SQ_TEX_RESOURCE_WORD4_0 + 4 * resourceOffset);
      auto sq_tex_resource_word5 = getRegister<latte::SQ_TEX_RESOURCE_WORD5_N>(latte::Register::SQ_TEX_RESOURCE_WORD5_0 + 4 * resourceOffset);
      auto sq_tex_resource_word6 = getRegister<latte::SQ_TEX_RESOURCE_WORD6_N>(latte::Register::SQ_TEX_RESOURCE_WORD6_0 + 4 * resourceOffset);

      auto baseAddress = sq_tex_resource_word2.BASE_ADDRESS() << 8;

      if (!baseAddress) {
         // TODO: I think we need to register a "NULL descriptor" here...
         continue;
      }

      // Decode resource registers
      auto pitch = (sq_tex_resource_word0.PITCH() + 1) * 8;
      auto width = sq_tex_resource_word0.TEX_WIDTH() + 1;
      auto height = sq_tex_resource_word1.TEX_HEIGHT() + 1;
      auto depth = sq_tex_resource_word1.TEX_DEPTH() + 1;

      auto format = sq_tex_resource_word1.DATA_FORMAT();
      auto tileMode = sq_tex_resource_word0.TILE_MODE();
      auto numFormat = sq_tex_resource_word4.NUM_FORMAT_ALL();
      auto formatComp = sq_tex_resource_word4.FORMAT_COMP_X();
      auto degamma = sq_tex_resource_word4.FORCE_DEGAMMA();
      auto dim = sq_tex_resource_word0.DIM();
      auto swizzle = sq_tex_resource_word2.SWIZZLE() << 8;
      auto isDepthBuffer = !!sq_tex_resource_word0.TILE_TYPE();
      auto samples = 0u;

      if (dim == latte::SQ_TEX_DIM::DIM_2D_MSAA || dim == latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA) {
         samples = 1 << sq_tex_resource_word5.LAST_LEVEL();
      }

      // Check to make sure the incoming swizzle makes sense...  If this assertion ever
      //  fails to hold, it indicates that the pipe bank swizzle bit might be being used
      //  and sending it through swizzle register is how they do that.  I can't find any
      //  case where the swizzle in the registers doesn't match the swizzle in the
      //  baseAddress, but it's confusing why the GPU needs the same information twice.
      decaf_check((baseAddress & 0x7FF) == swizzle);

      // Get the surface
      auto buffer = getSurfaceBuffer(baseAddress, dim, format, numFormat, formatComp, degamma, pitch, width, height, depth, samples, false, tileMode, false);

      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

      srvDesc.Format = getDxSurfFormat(format, numFormat, formatComp, degamma);

      switch (dim) {
      case latte::SQ_TEX_DIM::DIM_1D:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
         srvDesc.Texture1D.MipLevels = -1;
         srvDesc.Texture1D.MostDetailedMip = 0;
         srvDesc.Texture1D.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_2D:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
         srvDesc.Texture2D.MipLevels = -1;
         srvDesc.Texture2D.MostDetailedMip = 0;
         srvDesc.Texture2D.PlaneSlice = 0;
         srvDesc.Texture2D.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_3D:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
         srvDesc.Texture3D.MipLevels = -1;
         srvDesc.Texture3D.MostDetailedMip = 0;
         srvDesc.Texture3D.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_CUBEMAP:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
         srvDesc.TextureCube.MipLevels = -1;
         srvDesc.TextureCube.MostDetailedMip = 0;
         srvDesc.TextureCube.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
         srvDesc.Texture1DArray.ArraySize = height;
         srvDesc.Texture1DArray.FirstArraySlice = 0;
         srvDesc.Texture1DArray.MipLevels = -1;
         srvDesc.Texture1DArray.MostDetailedMip = 0;
         srvDesc.Texture1DArray.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
         srvDesc.Texture2DArray.ArraySize = depth;
         srvDesc.Texture2DArray.FirstArraySlice = 0;
         srvDesc.Texture2DArray.PlaneSlice = 0;
         srvDesc.Texture2DArray.MipLevels = -1;
         srvDesc.Texture2DArray.MostDetailedMip = 0;
         srvDesc.Texture2DArray.ResourceMinLODClamp = 0;
         break;
      case latte::SQ_TEX_DIM::DIM_2D_MSAA:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
         break;
      case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
         srvDesc.Texture2DMSArray.ArraySize = depth;
         srvDesc.Texture2DMSArray.FirstArraySlice = 0;
         break;
      }

      srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
         getDxComponentMapping(sq_tex_resource_word4.DST_SEL_X()),
         getDxComponentMapping(sq_tex_resource_word4.DST_SEL_Y()),
         getDxComponentMapping(sq_tex_resource_word4.DST_SEL_Z()),
         getDxComponentMapping(sq_tex_resource_word4.DST_SEL_W()));

      mDevice->CreateShaderResourceView(buffer->active->object.Get(), &srvDesc, srvs[i]);
   }

   if (mActivePipelineMode == PipelineMode::Registers) {
      mCmdList->SetGraphicsRootDescriptorTable(3, srvs);
   } else if (mActivePipelineMode == PipelineMode::Buffers) {
      mCmdList->SetGraphicsRootDescriptorTable(3, srvs);
   } else if (mActivePipelineMode == PipelineMode::Geometry) {
      mCmdList->SetGraphicsRootDescriptorTable(5, srvs);
   } else {
      decaf_abort("Unexpected pipeline mode");
   }

   // SECRETLY COPY TO VS FOR NOW
   if (mActivePipelineMode == PipelineMode::Registers) {
      mCmdList->SetGraphicsRootDescriptorTable(2, srvs);
   } else if (mActivePipelineMode == PipelineMode::Buffers) {
      mCmdList->SetGraphicsRootDescriptorTable(2, srvs);
   } else if (mActivePipelineMode == PipelineMode::Geometry) {
      mCmdList->SetGraphicsRootDescriptorTable(4, srvs);
   } else {
      decaf_abort("Unexpected pipeline mode");
   }


   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
