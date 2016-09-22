#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"

namespace gpu
{

namespace dx12
{

static D3D12_TEXTURE_ADDRESS_MODE
getDxTextureAddressMode(latte::SQ_TEX_CLAMP clamp)
{
   switch (clamp) {
   case latte::SQ_TEX_CLAMP::WRAP:
      return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
   case latte::SQ_TEX_CLAMP::MIRROR:
      return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
   case latte::SQ_TEX_CLAMP::CLAMP_LAST_TEXEL:
      return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   case latte::SQ_TEX_CLAMP::MIRROR_ONCE_LAST_TEXEL:
      return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
   case latte::SQ_TEX_CLAMP::CLAMP_HALF_BORDER:
      return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   case latte::SQ_TEX_CLAMP::MIRROR_ONCE_HALF_BORDER:
      return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
   case latte::SQ_TEX_CLAMP::CLAMP_BORDER:
      return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   case latte::SQ_TEX_CLAMP::MIRROR_ONCE_BORDER:
      return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
   default:
      decaf_abort("Unexpected texture clamp mode");
   }
}

static UINT
getDxMaxAnisotropy(latte::SQ_TEX_ANISO aniso)
{
   switch (aniso) {
   case latte::SQ_TEX_ANISO::ANISO_1_TO_1:
      return 1;
   case latte::SQ_TEX_ANISO::ANISO_2_TO_1:
      return 2;
   case latte::SQ_TEX_ANISO::ANISO_4_TO_1:
      return 4;
   case latte::SQ_TEX_ANISO::ANISO_8_TO_1:
      return 8;
   case latte::SQ_TEX_ANISO::ANISO_16_TO_1:
      return 16;
   default:
      decaf_abort("Unexpected texture anisotropy mode");
   }
}

static D3D12_COMPARISON_FUNC
getDxComparisonFunc(latte::REF_FUNC refFunc)
{
   switch (refFunc) {
   case latte::REF_FUNC::NEVER:
      return D3D12_COMPARISON_FUNC_NEVER;
   case latte::REF_FUNC::LESS:
      return D3D12_COMPARISON_FUNC_LESS;
   case latte::REF_FUNC::EQUAL:
      return D3D12_COMPARISON_FUNC_EQUAL;
   case latte::REF_FUNC::LESS_EQUAL:
      return D3D12_COMPARISON_FUNC_LESS_EQUAL;
   case latte::REF_FUNC::GREATER:
      return D3D12_COMPARISON_FUNC_GREATER;
   case latte::REF_FUNC::NOT_EQUAL:
      return D3D12_COMPARISON_FUNC_NOT_EQUAL;
   case latte::REF_FUNC::GREATER_EQUAL:
      return D3D12_COMPARISON_FUNC_EQUAL;
   case latte::REF_FUNC::ALWAYS:
      return D3D12_COMPARISON_FUNC_ALWAYS;
   default:
      decaf_abort("Unexpected texture comparison mode");
   }
}

static D3D12_FILTER
getDxTextureFilter(latte::SQ_TEX_XY_FILTER minFilter, latte::SQ_TEX_XY_FILTER magFilter, latte::SQ_TEX_Z_FILTER mipFilter)
{
   // TODO: Implement mapping of min/mag/mip texture filtering values.
   return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
}

bool
Driver::checkActiveSamplers()
{
   auto configureSampler = [&](uint32_t samplerIdx) {
      auto sq_tex_sampler_word0 =
         getRegister<latte::SQ_TEX_SAMPLER_WORD0_N>(latte::Register::SQ_TEX_SAMPLER_WORD0_0 + 4 * (samplerIdx * 3));
      auto sq_tex_sampler_word1 =
         getRegister<latte::SQ_TEX_SAMPLER_WORD1_N>(latte::Register::SQ_TEX_SAMPLER_WORD1_0 + 4 * (samplerIdx * 3));
      auto sq_tex_sampler_word2 =
         getRegister<latte::SQ_TEX_SAMPLER_WORD2_N>(latte::Register::SQ_TEX_SAMPLER_WORD2_0 + 4 * (samplerIdx * 3));

      D3D12_SAMPLER_DESC samplerDesc;

      samplerDesc.AddressU = getDxTextureAddressMode(sq_tex_sampler_word0.CLAMP_X());
      samplerDesc.AddressV = getDxTextureAddressMode(sq_tex_sampler_word0.CLAMP_Y());
      samplerDesc.AddressW = getDxTextureAddressMode(sq_tex_sampler_word0.CLAMP_Z());

      switch (sq_tex_sampler_word0.BORDER_COLOR_TYPE()) {
      case latte::SQ_TEX_BORDER_COLOR::TRANS_BLACK:
         samplerDesc.BorderColor[0] = 0.0f;
         samplerDesc.BorderColor[1] = 0.0f;
         samplerDesc.BorderColor[2] = 0.0f;
         samplerDesc.BorderColor[3] = 0.0f;
         break;
      case latte::SQ_TEX_BORDER_COLOR::OPAQUE_BLACK:
         samplerDesc.BorderColor[0] = 0.0f;
         samplerDesc.BorderColor[1] = 0.0f;
         samplerDesc.BorderColor[2] = 0.0f;
         samplerDesc.BorderColor[3] = 1.0f;
         break;
      case latte::SQ_TEX_BORDER_COLOR::OPAQUE_WHITE:
         samplerDesc.BorderColor[0] = 1.0f;
         samplerDesc.BorderColor[1] = 1.0f;
         samplerDesc.BorderColor[2] = 1.0f;
         samplerDesc.BorderColor[3] = 1.0f;
         break;
      case latte::SQ_TEX_BORDER_COLOR::REGISTER:
         samplerDesc.BorderColor[0] =
            getRegister<float>(latte::Register::TD_PS_SAMPLER_BORDER0_RED + 4 * (samplerIdx * 4));
         samplerDesc.BorderColor[1] =
            getRegister<float>(latte::Register::TD_PS_SAMPLER_BORDER0_GREEN + 4 * (samplerIdx * 4));
         samplerDesc.BorderColor[2] =
            getRegister<float>(latte::Register::TD_PS_SAMPLER_BORDER0_BLUE + 4 * (samplerIdx * 4));
         samplerDesc.BorderColor[3] =
            getRegister<float>(latte::Register::TD_PS_SAMPLER_BORDER0_ALPHA + 4 * (samplerIdx * 4));
         break;
      default:
         decaf_abort("Unexpected texture border color type");
      }

      samplerDesc.ComparisonFunc = getDxComparisonFunc(
         sq_tex_sampler_word0.DEPTH_COMPARE_FUNCTION());

      samplerDesc.Filter = getDxTextureFilter(
         sq_tex_sampler_word0.XY_MIN_FILTER(),
         sq_tex_sampler_word0.XY_MAG_FILTER(),
         sq_tex_sampler_word0.Z_FILTER());

      samplerDesc.MaxAnisotropy = getDxMaxAnisotropy(
         sq_tex_sampler_word0.MAX_ANISO_RATIO());

      samplerDesc.MinLOD = static_cast<float>(sq_tex_sampler_word1.MIN_LOD());
      samplerDesc.MaxLOD = static_cast<float>(sq_tex_sampler_word1.MAX_LOD());
      samplerDesc.MipLODBias = static_cast<float>(sq_tex_sampler_word1.LOD_BIAS());

      return samplerDesc;
   };

   // Set up our pixel shaders samplers (absolute sampler index 0-17)
   if (mActivePixelShader) {
      auto maxUsedSamplers = 0u;

      for (auto i = 0u; i < latte::MaxSamplers; ++i) {
         if (mActivePixelShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
            maxUsedSamplers = i + 1;
         }
      }

      if (maxUsedSamplers > 0) {
         auto samplers = allocateSamplers(maxUsedSamplers);

         for (auto i = 0u; i < maxUsedSamplers; ++i) {
            if (mActivePixelShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
               auto samplerDesc = configureSampler(0 + i);
               mDevice->CreateSampler(&samplerDesc, samplers[i]);
            }
         }

         mCmdList->SetGraphicsRootDescriptorTable(
            getDescriptorIndex(DescriptorType::PixelSamplers), samplers);
      }
   }

   // Set up our vertex shaders samplers (absolute sampler index 18-35)
   if (mActiveVertexShader) {
      auto maxUsedSamplers = 0u;

      for (auto i = 0u; i < latte::MaxSamplers; ++i) {
         if (mActiveVertexShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
            maxUsedSamplers = i + 1;
         }
      }

      if (maxUsedSamplers > 0) {
         auto samplers = allocateSamplers(maxUsedSamplers);

         for (auto i = 0u; i < maxUsedSamplers; ++i) {
            if (mActiveVertexShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
               auto samplerDesc = configureSampler(18 + i);
               mDevice->CreateSampler(&samplerDesc, samplers[i]);
            }
         }

         mCmdList->SetGraphicsRootDescriptorTable(
            getDescriptorIndex(DescriptorType::VertexSamplers), samplers);
      }
   }

   // Set up our geometry shaders samplers (absolute sampler index 36-53)
   if (mActiveGeometryShader) {
      auto maxUsedSamplers = 0u;

      for (auto i = 0u; i < latte::MaxSamplers; ++i) {
         if (mActiveGeometryShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
            maxUsedSamplers = i + 1;
         }
      }

      if (maxUsedSamplers > 0) {
         auto samplers = allocateSamplers(maxUsedSamplers);

         for (auto i = 0u; i < maxUsedSamplers; ++i) {
            if (mActiveGeometryShader->data.samplerState[i] != hlsl2::SampleMode::Unused) {
               auto samplerDesc = configureSampler(36 + i);
               mDevice->CreateSampler(&samplerDesc, samplers[i]);
            }
         }

         mCmdList->SetGraphicsRootDescriptorTable(
            getDescriptorIndex(DescriptorType::GeometrySamplers), samplers);
      }
   }

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
