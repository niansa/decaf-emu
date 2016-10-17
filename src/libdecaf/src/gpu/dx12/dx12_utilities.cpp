#ifdef DECAF_DX12

#include "common/decaf_assert.h"
#include "common/log.h"
#include "dx12_utilities.h"

#pragma optimize("", off)

namespace gpu
{

namespace dx12
{

static DXGI_FORMAT
getDxSurfFormat(latte::SQ_NUM_FORMAT numFormat,
                latte::SQ_FORMAT_COMP formatComp,
                uint32_t degamma,
                DXGI_FORMAT unorm,
                DXGI_FORMAT snorm,
                DXGI_FORMAT uint,
                DXGI_FORMAT sint,
                DXGI_FORMAT srgb,
                DXGI_FORMAT scaled)
{
   if (!degamma) {
      if (numFormat == latte::SQ_NUM_FORMAT::NORM) {
         if (formatComp == latte::SQ_FORMAT_COMP::SIGNED) {
            return snorm;
         } else if (formatComp == latte::SQ_FORMAT_COMP::UNSIGNED) {
            return unorm;
         } else {
            return DXGI_FORMAT_UNKNOWN;
         }
      } else if (numFormat == latte::SQ_NUM_FORMAT::INT) {
         if (formatComp == latte::SQ_FORMAT_COMP::SIGNED) {
            return sint;
         } else if (formatComp == latte::SQ_FORMAT_COMP::UNSIGNED) {
            return uint;
         } else {
            return DXGI_FORMAT_UNKNOWN;
         }
      } else if (numFormat == latte::SQ_NUM_FORMAT::SCALED) {
         if (formatComp == latte::SQ_FORMAT_COMP::UNSIGNED) {
            return scaled;
         } else {
            return DXGI_FORMAT_UNKNOWN;
         }
      } else {
         return DXGI_FORMAT_UNKNOWN;
      }
   } else {
      if (numFormat == 0 && formatComp == 0) {
         return srgb;
      } else {
         return DXGI_FORMAT_UNKNOWN;
      }
   }
}

D3D12_RESOURCE_DIMENSION
getDxSurfDim(latte::SQ_TEX_DIM dim)
{
   switch (dim) {
   case latte::SQ_TEX_DIM::DIM_1D:
      return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
   case latte::SQ_TEX_DIM::DIM_2D:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   case latte::SQ_TEX_DIM::DIM_3D:
      return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
   case latte::SQ_TEX_DIM::DIM_CUBEMAP:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
      return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   case latte::SQ_TEX_DIM::DIM_2D_MSAA:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
      return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   }

   decaf_abort(fmt::format("Failed to pick dx dim for latte dim {}", dim));
}

DXGI_FORMAT
getDxSurfFormat(latte::SQ_DATA_FORMAT format,
                latte::SQ_NUM_FORMAT numFormat,
                latte::SQ_FORMAT_COMP formatComp,
                uint32_t degamma)
{
   static const DXGI_FORMAT BADFMT = DXGI_FORMAT_UNKNOWN;
   auto pick = [=](DXGI_FORMAT unorm, DXGI_FORMAT snorm, DXGI_FORMAT uint, DXGI_FORMAT sint, DXGI_FORMAT srgb, DXGI_FORMAT scaled) {
      auto pickedFormat = getDxSurfFormat(numFormat, formatComp, degamma, unorm, snorm, uint, sint, srgb, scaled);
      if (pickedFormat == BADFMT) {
         decaf_abort(fmt::format("Failed to pick dx format for latte format: {} {} {} {}", format, numFormat, formatComp, degamma));
      }
      return pickedFormat;
   };

   bool isSigned = formatComp == latte::SQ_FORMAT_COMP::SIGNED;

   switch (format) {
   case latte::SQ_DATA_FORMAT::FMT_8:
      return pick(DXGI_FORMAT_R8_UNORM, DXGI_FORMAT_R8_SNORM, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R8_SINT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_4_4:
      //case latte::SQ_DATA_FORMAT::FMT_3_3_2:
   case latte::SQ_DATA_FORMAT::FMT_16:
      return pick(DXGI_FORMAT_R16_UNORM, DXGI_FORMAT_R16_SNORM, DXGI_FORMAT_R16_UINT, DXGI_FORMAT_R16_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_16_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R16_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_8_8:
      return pick(DXGI_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_SNORM, DXGI_FORMAT_R8G8_UINT, DXGI_FORMAT_R8G8_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_5_6_5:
      return pick(DXGI_FORMAT_B5G6R5_UNORM, BADFMT, BADFMT, BADFMT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_6_5_5:
      //case latte::SQ_DATA_FORMAT::FMT_1_5_5_5:
      //case latte::SQ_DATA_FORMAT::FMT_4_4_4_4:
      //case latte::SQ_DATA_FORMAT::FMT_5_5_5_1:
   case latte::SQ_DATA_FORMAT::FMT_32:
      return pick(BADFMT, BADFMT, DXGI_FORMAT_R32_UINT, DXGI_FORMAT_R32_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_32_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R32_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_16_16:
      return pick(DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_UINT, DXGI_FORMAT_R16G16_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_16_16_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R16G16_FLOAT);
      //case latte::SQ_DATA_FORMAT::FMT_8_24:
      //case latte::SQ_DATA_FORMAT::FMT_8_24_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_24_8:
      return pick(DXGI_FORMAT_D24_UNORM_S8_UINT, BADFMT, BADFMT, BADFMT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_24_8_FLOAT:
      //case latte::SQ_DATA_FORMAT::FMT_10_11_11:
   case latte::SQ_DATA_FORMAT::FMT_10_11_11_FLOAT:
      // TODO: This is not strictly correct, but may work for now... (Reversed bit order)
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R11G11B10_FLOAT);
      //case latte::SQ_DATA_FORMAT::FMT_11_11_10:
   case latte::SQ_DATA_FORMAT::FMT_11_11_10_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R11G11B10_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_2_10_10_10:
      // TODO: This is not strictly correct, but may work for now... (Reversed bit order)
      return pick(DXGI_FORMAT_R10G10B10A2_UNORM, BADFMT, DXGI_FORMAT_R10G10B10A2_UINT, BADFMT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_8_8_8_8:
      return pick(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_SNORM, DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_R8G8B8A8_SINT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_10_10_10_2:
      return pick(DXGI_FORMAT_R10G10B10A2_UNORM, BADFMT, DXGI_FORMAT_R10G10B10A2_UINT, BADFMT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_X24_8_32_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_32_32:
      return pick(BADFMT, BADFMT, DXGI_FORMAT_R32G32_UINT, DXGI_FORMAT_R32G32_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_32_32_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R32G32_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_16_16_16_16:
      return pick(DXGI_FORMAT_R16G16B16A16_UNORM, DXGI_FORMAT_R16G16B16A16_SNORM, DXGI_FORMAT_R16G16B16A16_UINT, DXGI_FORMAT_R16G16B16A16_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_16_16_16_16_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R16G16B16A16_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_32_32_32_32:
      return pick(BADFMT, BADFMT, DXGI_FORMAT_R32G32B32A32_UINT, DXGI_FORMAT_R32G32B32A32_SINT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_32_32_32_32_FLOAT:
      return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_R32G32B32A32_FLOAT);
   case latte::SQ_DATA_FORMAT::FMT_1:
      return pick(DXGI_FORMAT_R1_UNORM, BADFMT, BADFMT, BADFMT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_GB_GR:
      //case latte::SQ_DATA_FORMAT::FMT_BG_RG:
      //case latte::SQ_DATA_FORMAT::FMT_32_AS_8:
      //case latte::SQ_DATA_FORMAT::FMT_32_AS_8_8:
      //case latte::SQ_DATA_FORMAT::FMT_5_9_9_9_SHAREDEXP:
      //case latte::SQ_DATA_FORMAT::FMT_8_8_8:
      //case latte::SQ_DATA_FORMAT::FMT_16_16_16:
      //case latte::SQ_DATA_FORMAT::FMT_16_16_16_FLOAT:
      //case latte::SQ_DATA_FORMAT::FMT_32_32_32:
      //case latte::SQ_DATA_FORMAT::FMT_32_32_32_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_BC1:
      return pick(DXGI_FORMAT_BC1_UNORM, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_BC1_UNORM_SRGB, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_BC2:
      return pick(DXGI_FORMAT_BC2_UNORM, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_BC2_UNORM_SRGB, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_BC3:
      return pick(DXGI_FORMAT_BC3_UNORM, BADFMT, BADFMT, BADFMT, DXGI_FORMAT_BC3_UNORM_SRGB, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_BC4:
      return pick(DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC4_SNORM, BADFMT, BADFMT, BADFMT, BADFMT);
   case latte::SQ_DATA_FORMAT::FMT_BC5:
      return pick(DXGI_FORMAT_BC5_UNORM, DXGI_FORMAT_BC5_SNORM, BADFMT, BADFMT, BADFMT, BADFMT);
      //case latte::SQ_DATA_FORMAT::FMT_APC0:
      //case latte::SQ_DATA_FORMAT::FMT_APC1:
      //case latte::SQ_DATA_FORMAT::FMT_APC2:
      //case latte::SQ_DATA_FORMAT::FMT_APC3:
      //case latte::SQ_DATA_FORMAT::FMT_APC4:
      //case latte::SQ_DATA_FORMAT::FMT_APC5:
      //case latte::SQ_DATA_FORMAT::FMT_APC6:
      //case latte::SQ_DATA_FORMAT::FMT_APC7:
      //case latte::SQ_DATA_FORMAT::FMT_CTX1:
   }

   return pick(BADFMT, BADFMT, BADFMT, BADFMT, BADFMT, BADFMT);
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
