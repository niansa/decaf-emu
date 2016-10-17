#ifdef DECAF_DX12

#include "common/log.h"
#include "common/murmur3.h"
#include "dx12_driver.h"
#include "dx12_utilities.h"
#include "gpu/hlsl2/hlsl2_translate.h"
#include "gpu/gpu_utilities.h"
#include <d3dcompiler.h>

#pragma optimize("", off)

namespace gpu
{

namespace dx12
{

bool
Driver::checkReadyDraw()
{
   if (!checkActiveRootSignature()) {
      gLog->warn("Skipping draw due to failed root signature picking.");
      return false;
   }

   if (!checkActivePipeline()) {
      gLog->warn("Skipping draw due to failed pipeline generation.");
      return false;
   }

   if (!checkActiveUniforms()) {
      gLog->warn("Skipping draw due to failed uniform setup.");
      return false;
   }

   if (!checkActiveVertexBuffers()) {
      gLog->warn("Skipping draw due to failed attributes setup.");
      return false;
   }

   if (!checkActiveRenderTargets()) {
      gLog->warn("Skipping draw due to failed render target setup.");
      return false;
   }

   if (!checkActiveSamplers()) {
      gLog->warn("Skipping draw due to failed samplers setup.");
      return false;
   }

   if (!checkActiveTextures()) {
      gLog->warn("Skipping draw due to failed texture setup.");
      return false;
   }

   if (!checkActiveView()) {
      gLog->warn("Skipping draw due to failed texture setup.");
      return false;
   }

   return true;
}

uint64_t hashPipeline(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &stateDesc)
{
   auto cleanDesc = stateDesc;

   // We intentionally just clear the input layout since it is sourced from
   //  the VS, so if the VS matches, then the input layout must as well.
   cleanDesc.InputLayout = D3D12_INPUT_LAYOUT_DESC{ nullptr, 0 };

   uint64_t key[2];
   MurmurHash3_x64_128(&cleanDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC), 0, key);
   return key[0] ^ key[1];
}

bool
Driver::checkActiveRootSignature()
{
   // TODO: Maybe make a RootSignature object which holds the slot locations
   //  for the various different root parameters we use?

   // Pick the pipeline mode based on registers
   auto sq_config = getRegister<latte::SQ_CONFIG>(latte::Register::SQ_CONFIG);
   auto vgt_gs_mode = getRegister<latte::VGT_GS_MODE>(latte::Register::VGT_GS_MODE);

   if (vgt_gs_mode.MODE() == VGT_GS_ENABLE_MODE::OFF) {
      if (sq_config.DX9_CONSTS()) {
         mActivePipelineMode = PipelineMode::Registers;
         mActiveRootSignature = mRegRootSignature.Get();
      } else {
         mActivePipelineMode = PipelineMode::Buffers;
         mActiveRootSignature = mUniRootSignature.Get();
      }
   } else {
      decaf_check(!vgt_gs_mode.COMPUTE_MODE());
      mActivePipelineMode = PipelineMode::Geometry;
      mActiveRootSignature = mGeomRootSignature.Get();
   }

   return true;
}

D3D12_BLEND
getDxBlendFunc(latte::CB_BLEND_FUNC func)
{
   switch (func) {
   case latte::CB_BLEND_FUNC::ZERO:
      return D3D12_BLEND_ZERO;
   case latte::CB_BLEND_FUNC::ONE:
      return D3D12_BLEND_ONE;
   case latte::CB_BLEND_FUNC::SRC_COLOR:
      return D3D12_BLEND_SRC_COLOR;
   case latte::CB_BLEND_FUNC::ONE_MINUS_SRC_COLOR:
      return D3D12_BLEND_INV_SRC_COLOR;
   case latte::CB_BLEND_FUNC::SRC_ALPHA:
      return D3D12_BLEND_SRC_ALPHA;
   case latte::CB_BLEND_FUNC::ONE_MINUS_SRC_ALPHA:
      return D3D12_BLEND_INV_SRC_ALPHA;
   case latte::CB_BLEND_FUNC::DST_ALPHA:
      return D3D12_BLEND_DEST_ALPHA;
   case latte::CB_BLEND_FUNC::ONE_MINUS_DST_ALPHA:
      return D3D12_BLEND_INV_DEST_ALPHA;
   case latte::CB_BLEND_FUNC::DST_COLOR:
      return D3D12_BLEND_DEST_COLOR;
   case latte::CB_BLEND_FUNC::ONE_MINUS_DST_COLOR:
      return D3D12_BLEND_INV_DEST_COLOR;
   case latte::CB_BLEND_FUNC::SRC_ALPHA_SATURATE:
      return D3D12_BLEND_SRC_ALPHA_SAT;
   case latte::CB_BLEND_FUNC::BOTH_SRC_ALPHA:
      decaf_abort("Unsupported BOTH_SRC_ALPHA blend function");
   case latte::CB_BLEND_FUNC::BOTH_INV_SRC_ALPHA:
      decaf_abort("Unsupported BOTH_INV_SRC_ALPHA blend function");
   case latte::CB_BLEND_FUNC::CONSTANT_COLOR:
      return D3D12_BLEND_BLEND_FACTOR;
   case latte::CB_BLEND_FUNC::ONE_MINUS_CONSTANT_COLOR:
      return D3D12_BLEND_INV_BLEND_FACTOR;
   case latte::CB_BLEND_FUNC::SRC1_COLOR:
      return D3D12_BLEND_SRC1_COLOR;
   case latte::CB_BLEND_FUNC::ONE_MINUS_SRC1_COLOR:
      return D3D12_BLEND_INV_SRC1_COLOR;
   case latte::CB_BLEND_FUNC::SRC1_ALPHA:
      return D3D12_BLEND_SRC1_ALPHA;
   case latte::CB_BLEND_FUNC::ONE_MINUS_SRC1_ALPHA:
      return D3D12_BLEND_INV_SRC1_ALPHA;
   case latte::CB_BLEND_FUNC::CONSTANT_ALPHA:
      return D3D12_BLEND_BLEND_FACTOR;
   case latte::CB_BLEND_FUNC::ONE_MINUS_CONSTANT_ALPHA:
      return D3D12_BLEND_INV_BLEND_FACTOR;
   default:
      decaf_abort("Unexpected blend function");
   }
}

D3D12_BLEND_OP
getDxBlendOp(latte::CB_COMB_FUNC func)
{
   switch (func) {
   case latte::CB_COMB_FUNC::DST_PLUS_SRC:
      return D3D12_BLEND_OP_ADD;
   case latte::CB_COMB_FUNC::SRC_MINUS_DST:
      return D3D12_BLEND_OP_SUBTRACT;
   case latte::CB_COMB_FUNC::MIN_DST_SRC:
      return D3D12_BLEND_OP_MIN;
   case latte::CB_COMB_FUNC::MAX_DST_SRC:
      return D3D12_BLEND_OP_MAX;
   case latte::CB_COMB_FUNC::DST_MINUS_SRC:
      return D3D12_BLEND_OP_REV_SUBTRACT;
   default:
      decaf_abort("Unexpected blend op");
   }
}

bool
Driver::checkActivePipeline()
{
   std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;

   // TODO: Properly configure the pipeline based on GPU7 registers...

   checkActiveVertexShader();
   checkActiveGeometryShader();
   checkActivePixelShader();

   // Setup the pipeline description object
   D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

   // Assign the correct root signature
   psoDesc.pRootSignature = mActiveRootSignature;

   // Set up the input element descriptions
   {
      auto vsInputs = mActiveVertexShader->data.inputData;

      for (auto i = 0u; i < vsInputs.size(); ++i) {
         auto &vsInput = vsInputs[i];
         D3D12_INPUT_ELEMENT_DESC inputDesc;

         inputDesc.SemanticName = "TEXCOORD";
         inputDesc.SemanticIndex = i;
         inputDesc.InputSlot = vsInput.bufferIndex;
         inputDesc.AlignedByteOffset = vsInput.offset;

         if (vsInput.elemWidth == 8) {
            if (vsInput.elemCount == 1) {
               inputDesc.Format = DXGI_FORMAT_R8_UINT;
            } else if (vsInput.elemCount == 2) {
               inputDesc.Format = DXGI_FORMAT_R8G8_UINT;
            } else if (vsInput.elemCount == 4) {
               inputDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT;
            } else {
               decaf_abort("Unexpected vs input 8-bit element count");
            }
         } else if (vsInput.elemWidth == 16) {
            if (vsInput.elemCount == 1) {
               inputDesc.Format = DXGI_FORMAT_R16_UINT;
            } else if (vsInput.elemCount == 2) {
               inputDesc.Format = DXGI_FORMAT_R16G16_UINT;
            } else if (vsInput.elemCount == 4) {
               inputDesc.Format = DXGI_FORMAT_R16G16B16A16_UINT;
            } else {
               decaf_abort("Unexpected vs input 16-bit element count")
            }
         } else if (vsInput.elemWidth == 32) {
            if (vsInput.elemCount == 1) {
               inputDesc.Format = DXGI_FORMAT_R32_UINT;
            } else if (vsInput.elemCount == 2) {
               inputDesc.Format = DXGI_FORMAT_R32G32_UINT;
            } else if (vsInput.elemCount == 3) {
               inputDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
            } else if (vsInput.elemCount == 4) {
               inputDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
            } else {
               decaf_abort("Unexpected vs input 16-bit element count");
            }
         } else {
            decaf_abort("Unexpected vs input element width");
         }

         if (vsInput.indexMode == hlsl2::InputData::IndexMode::PerVertex) {
            inputDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
         } else if (vsInput.indexMode == hlsl2::InputData::IndexMode::PerInstance) {
            inputDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
         } else {
            decaf_abort("Unexpected VS InputData indexing mode");
         }

         inputDesc.InstanceDataStepRate = vsInput.divisor;

         inputElementDescs.emplace_back(inputDesc);
      }

      psoDesc.InputLayout = { inputElementDescs.data(), (UINT)inputElementDescs.size() };
   }

   // Set up all our shaders
   if (mActiveVertexShader) {
      psoDesc.VS = CD3DX12_SHADER_BYTECODE(mActiveVertexShader->object.Get());
   }

   if (mActiveGeometryShader) {
      psoDesc.GS = CD3DX12_SHADER_BYTECODE(mActiveGeometryShader->object.Get());
   }

   if (mActivePixelShader) {
      psoDesc.PS = CD3DX12_SHADER_BYTECODE(mActivePixelShader->object.Get());
   }

   psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
   psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
   psoDesc.RasterizerState.DepthClipEnable = false;

   // Set up pipeline blend state
   {
      auto cb_color_control = getRegister<latte::CB_COLOR_CONTROL>(latte::Register::CB_COLOR_CONTROL);
      auto cb_target_mask = getRegister<latte::CB_TARGET_MASK>(latte::Register::CB_TARGET_MASK);

      // We do not currently support Logical OPs...
      decaf_check(cb_color_control.ROP3() == 0xCC);

      auto makeBlend = [&](uint32_t id) {
         auto blend = getRegister<latte::CB_BLENDN_CONTROL>(latte::Register::CB_BLEND0_CONTROL + 4 * id);

         D3D12_RENDER_TARGET_BLEND_DESC desc;

         // We do not support opacity weighting (not even sure how to apply this).
         decaf_check(!blend.OPACITY_WEIGHT());

         // Set up the basics
         desc.BlendEnable = cb_color_control.TARGET_BLEND_ENABLE() & (1 << id);
         desc.LogicOpEnable = false;
         desc.LogicOp = D3D12_LOGIC_OP_COPY;

         desc.SrcBlend = getDxBlendFunc(blend.COLOR_SRCBLEND());
         desc.DestBlend = getDxBlendFunc(blend.COLOR_DESTBLEND());
         desc.BlendOp = getDxBlendOp(blend.COLOR_COMB_FCN());

         if (blend.SEPARATE_ALPHA_BLEND()) {
            desc.SrcBlendAlpha = getDxBlendFunc(blend.ALPHA_SRCBLEND());
            desc.DestBlendAlpha = getDxBlendFunc(blend.ALPHA_DESTBLEND());
            desc.BlendOpAlpha = getDxBlendOp(blend.ALPHA_COMB_FCN());
         } else {
            auto convertBlendToAlpha = [](D3D12_BLEND func) {
               switch (func) {
               case D3D12_BLEND_SRC_COLOR:
                  return D3D12_BLEND_SRC_ALPHA;
               case D3D12_BLEND_INV_SRC_COLOR:
                  return D3D12_BLEND_INV_SRC_ALPHA;
               case D3D12_BLEND_DEST_COLOR:
                  return D3D12_BLEND_DEST_ALPHA;
               case D3D12_BLEND_INV_DEST_COLOR:
                  return D3D12_BLEND_INV_DEST_ALPHA;
               case  D3D12_BLEND_SRC1_COLOR:
                  return D3D12_BLEND_SRC1_ALPHA;
               case D3D12_BLEND_INV_SRC1_COLOR:
                  return D3D12_BLEND_INV_SRC1_ALPHA;
               default:
                  return func;
               }
            };

            desc.SrcBlendAlpha = convertBlendToAlpha(desc.SrcBlend);
            desc.DestBlendAlpha = convertBlendToAlpha(desc.DestBlend);
            desc.BlendOpAlpha = desc.BlendOp;
         }

         // This is a special case of blending being 'off'.  We handle this specially
         //  as there are checks to ensure that blending is off for some RTV formats.
         bool isDefaultBlend =
            desc.SrcBlend == D3D12_BLEND_ONE &&
            desc.DestBlend == D3D12_BLEND_ZERO &&
            desc.BlendOp == D3D12_BLEND_OP_ADD &&
            desc.SrcBlendAlpha == D3D12_BLEND_ONE &&
            desc.DestBlendAlpha == D3D12_BLEND_ZERO &&
            desc.BlendOp == D3D12_BLEND_OP_ADD;

         if (isDefaultBlend) {
            desc.BlendEnable = false;
         }

         // Conveniently, the register matches precisely with Direct3D 12...
         desc.RenderTargetWriteMask = (cb_target_mask.value >> (4 * id)) & 0xF;

         return desc;
      };

      psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

      if (cb_color_control.SPECIAL_OP() == CB_SPECIAL_OP::NORMAL) {
         psoDesc.BlendState.IndependentBlendEnable = !!cb_color_control.PER_MRT_BLEND();
         psoDesc.BlendState.RenderTarget[0] = makeBlend(0);

         if (psoDesc.BlendState.IndependentBlendEnable) {
            for (auto i = 1u; i < 8u; ++i) {
               psoDesc.BlendState.RenderTarget[i] = makeBlend(i);
            }
         }
      } else if (cb_color_control.SPECIAL_OP() == CB_SPECIAL_OP::DISABLE) {
         // Default render state is sufficient for DISABLE
      } else if (cb_color_control.SPECIAL_OP() == CB_SPECIAL_OP::FAST_CLEAR) {
         // Default render state is sufficient for FAST_CLEAR
      } else if (cb_color_control.SPECIAL_OP() == CB_SPECIAL_OP::FORCE_CLEAR) {
         // Default render state is sufficient for FORCE_CLEAR
      } else {
         decaf_abort("Encountered unexpected CB_SPECIAL_OP");
      }
   }

   psoDesc.DepthStencilState.DepthEnable = FALSE;
   psoDesc.DepthStencilState.StencilEnable = FALSE;

   psoDesc.SampleMask = UINT_MAX;

   // Set up our primitive type
   auto vgt_primitive_type = getRegister<latte::VGT_PRIMITIVE_TYPE>(latte::Register::VGT_PRIMITIVE_TYPE);
   switch (vgt_primitive_type.PRIM_TYPE()) {
   case latte::VGT_DI_PRIMITIVE_TYPE::POINTLIST:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::LINELIST:
   case latte::VGT_DI_PRIMITIVE_TYPE::LINESTRIP:
   case latte::VGT_DI_PRIMITIVE_TYPE::LINELIST_ADJ:
   case latte::VGT_DI_PRIMITIVE_TYPE::LINESTRIP_ADJ:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::TRILIST:
   case latte::VGT_DI_PRIMITIVE_TYPE::TRIFAN:
   case latte::VGT_DI_PRIMITIVE_TYPE::TRISTRIP:
   case latte::VGT_DI_PRIMITIVE_TYPE::TRILIST_ADJ:
   case latte::VGT_DI_PRIMITIVE_TYPE::TRISTRIP_ADJ:
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::RECTLIST:
   case latte::VGT_DI_PRIMITIVE_TYPE::QUADLIST:
   case latte::VGT_DI_PRIMITIVE_TYPE::QUADSTRIP:
      // We handle translation of these types during draw
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      break;
      //case latte::VGT_DI_PRIMITIVE_TYPE::NONE:
      //case latte::VGT_DI_PRIMITIVE_TYPE::TRI_WITH_WFLAGS:
      //case latte::VGT_DI_PRIMITIVE_TYPE::LINELOOP:
      //case latte::VGT_DI_PRIMITIVE_TYPE::POLYGON:
      //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V0:
      //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V1:
      //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V2:
      //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V3:
      //case latte::VGT_DI_PRIMITIVE_TYPE::FILL_RECT_LIST_2D:
      //case latte::VGT_DI_PRIMITIVE_TYPE::LINE_STRIP_2D:
      //case latte::VGT_DI_PRIMITIVE_TYPE::TRI_STRIP_2D:
   default:
      decaf_abort("Unexpected VGT primitive type");
   }


   // Set up the render target formats...
   {
      auto cb_target_mask = getRegister<latte::CB_TARGET_MASK>(latte::Register::CB_TARGET_MASK);
      auto cb_color_control = getRegister<latte::CB_COLOR_CONTROL>(latte::Register::CB_COLOR_CONTROL);
      auto mask = cb_target_mask.value;

      if (cb_color_control.SPECIAL_OP() == CB_SPECIAL_OP::DISABLE) {
         // When SPECIAL_OP is disabled, we skip all outputs
         mask = 0;
      }

      // Clear all of the render targets, since its only set below if a non-invalid
      //  render target is encountered.
      psoDesc.NumRenderTargets = 0;
      for (auto i = 0u; i < 8u; ++i) {
         psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
      }

      // Check for any render targets to activate.
      for (auto i = 0u; i < 1; ++i) {
         auto cb_color_base = getRegister<latte::CB_COLORN_BASE>(latte::Register::CB_COLOR0_BASE + i * 4);
         auto cb_color_size = getRegister<latte::CB_COLORN_SIZE>(latte::Register::CB_COLOR0_SIZE + i * 4);
         auto cb_color_info = getRegister<latte::CB_COLORN_INFO>(latte::Register::CB_COLOR0_INFO + i * 4);

         auto rtMask = mask >> (i * 4);
         if (rtMask == 0) {
            // All components are masked, no reason to do anything...
            psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
            continue;
         }

         if (!cb_color_base.BASE_256B()) {
            // No color buffer bound here.  Since there is no buffer bound, there is
            //  no where to infer the format from, so we need to just disable it...
            psoDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
            continue;
         }

         auto format = getColorBufferDataFormat(cb_color_info.FORMAT(), cb_color_info.NUMBER_TYPE());
         auto dxFormat = getDxSurfFormat(format.format, format.numFormat, format.formatComp, format.degamma);

         // TODO: Use the mActivePixelShader to determine which of these we need to actually care about...
         psoDesc.RTVFormats[i] = dxFormat;

         // We only need to make sure NumRenderTargets is the highest used render target.
         psoDesc.NumRenderTargets = i + 1;
      }
   }

   // TODO: I'm sure this sample count is meant to mean something...
   psoDesc.SampleDesc.Count = 1;


   // Done building the pipeline description, lets find us a pipeline to use.
   auto pipelineKey = hashPipeline(psoDesc);
   auto &foundPipeline = mPipelines[pipelineKey];

   if (!foundPipeline.object) {
      foundPipeline.data = psoDesc;
      foundPipeline.mode = mActivePipelineMode;
      foundPipeline.vertexShader = mActiveVertexShader;
      foundPipeline.geometryShader = mActiveGeometryShader;
      foundPipeline.pixelShader = mActivePixelShader;

      decaf_checkhr(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&foundPipeline.object)));
   }

   mActivePipeline = &foundPipeline;

   mCmdList->SetPipelineState(mActivePipeline->object.Get());
   mCmdList->SetGraphicsRootSignature(mActiveRootSignature);

   return true;
}

uint32_t
Driver::getDescriptorIndex(DescriptorType descriptorType)
{
   if (mActivePipelineMode == PipelineMode::Registers) {
      switch (descriptorType) {
      case DescriptorType::VertexRegs:
         return 0;
      case DescriptorType::PixelRegs:
         return 1;
      case DescriptorType::VertexTextures:
         return 2;
      case DescriptorType::PixelTextures:
         return 3;
      case DescriptorType::VertexSamplers:
         return 4;
      case DescriptorType::PixelSamplers:
         return 5;
      case DescriptorType::SystemData:
         return 6;
      default:
         decaf_abort("Unexpected descriptor type for register mode");
      }
   } else if (mActivePipelineMode == PipelineMode::Buffers) {
      switch (descriptorType) {
      case DescriptorType::VertexUniforms:
         return 0;
      case DescriptorType::PixelUniforms:
         return 1;
      case DescriptorType::VertexTextures:
         return 2;
      case DescriptorType::PixelTextures:
         return 3;
      case DescriptorType::VertexSamplers:
         return 4;
      case DescriptorType::PixelSamplers:
         return 5;
      case DescriptorType::SystemData:
         return 6;
      default:
         decaf_abort("Unexpected descriptor type for buffer mode");
      }
   } else if (mActivePipelineMode == PipelineMode::Geometry) {
      switch (descriptorType) {
      case DescriptorType::VertexUniforms:
         return 0;
      case DescriptorType::GeometryUniforms:
         return 1;
      case DescriptorType::PixelUniforms:
         return 2;
      case DescriptorType::VertexTextures:
         return 3;
      case DescriptorType::GeometryTextures:
         return 4;
      case DescriptorType::PixelTextures:
         return 5;
      case DescriptorType::VertexSamplers:
         return 6;
      case DescriptorType::GeometrySamplers:
         return 7;
      case DescriptorType::PixelSamplers:
         return 8;
      case DescriptorType::SystemData:
         return 9;
      default:
         decaf_abort("Unexpected descriptor type for geometry mode");
      }
   } else {
      decaf_abort("Unexpected active pipeline mode");
   }
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
