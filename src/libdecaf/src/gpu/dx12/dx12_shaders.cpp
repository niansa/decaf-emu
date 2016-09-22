#ifdef DECAF_DX12

#include "common/log.h"
#include "common/murmur3.h"
#include "dx12_driver.h"
#include "gpu/hlsl2/hlsl2_translate.h"
#include <d3dcompiler.h>

namespace gpu
{

namespace dx12
{

// These 3 hashing things should use a VertexShaderDesc instead... But thats
//  not yet implemeneted, so we can't currently do that...
uint64_t hashVertexShader(const gpu::VertexShaderDesc &shaderDesc)
{
   // TODO: Don't do this... CURRENTLY SUPER LAZY
   return reinterpret_cast<uint64_t>(&shaderDesc.binary[0]);

   // TODO: We need to use the output of the generation to clear bits
   //  from the input description that are not actually affecting the
   //  output from the shader generator.

   uint64_t key[2];
   MurmurHash3_x64_128(&shaderDesc, sizeof(hlsl2::VertexShader), 0, key);
   return key[0] ^ key[1];
}

uint64_t hashGeometryShader(const gpu::GeometryShaderDesc &shaderDesc)
{
   // TODO: Don't do this... CURRENTLY SUPER LAZY
   return reinterpret_cast<uint64_t>(&shaderDesc.binary[0]);

   uint64_t key[2];
   MurmurHash3_x64_128(&shaderDesc, sizeof(hlsl2::GeometryShader), 0, key);
   return key[0] ^ key[1];
}

uint64_t hashPixelShader(const gpu::PixelShaderDesc &shaderDesc)
{
   // TODO: Don't do this... CURRENTLY SUPER LAZY
   return reinterpret_cast<uint64_t>(&shaderDesc.binary[0]);

   uint64_t key[2];
   MurmurHash3_x64_128(&shaderDesc, sizeof(hlsl2::PixelShader), 0, key);
   return key[0] ^ key[1];
}

bool
Driver::checkActiveVertexShader()
{
   gsl::span<uint8_t> fsShaderBinary;
   gsl::span<uint8_t> vsShaderBinary;

   auto pgm_start_fs = getRegister<latte::SQ_PGM_START_FS>(latte::Register::SQ_PGM_START_FS);
   auto pgm_offset_fs = getRegister<latte::SQ_PGM_CF_OFFSET_FS>(latte::Register::SQ_PGM_CF_OFFSET_FS);
   auto pgm_size_fs = getRegister<latte::SQ_PGM_SIZE_FS>(latte::Register::SQ_PGM_SIZE_FS);
   fsShaderBinary = gsl::as_span(
      mem::translate<uint8_t>(pgm_start_fs.PGM_START() << 8),
      pgm_size_fs.PGM_SIZE() << 3);
   decaf_check(pgm_offset_fs.PGM_OFFSET() == 0);

   auto vgt_gs_mode = getRegister<latte::VGT_GS_MODE>(latte::Register::VGT_GS_MODE);
   if (vgt_gs_mode.MODE() == latte::VGT_GS_ENABLE_MODE::OFF) {
      // When GS is disabled, vertex shader comes from vertex shader register
      auto pgm_start_vs = getRegister<latte::SQ_PGM_START_VS>(latte::Register::SQ_PGM_START_VS);
      auto pgm_offset_vs = getRegister<latte::SQ_PGM_CF_OFFSET_VS>(latte::Register::SQ_PGM_CF_OFFSET_VS);
      auto pgm_size_vs = getRegister<latte::SQ_PGM_SIZE_VS>(latte::Register::SQ_PGM_SIZE_VS);
      vsShaderBinary = gsl::as_span(
         mem::translate<uint8_t>(pgm_start_vs.PGM_START() << 8),
         pgm_size_vs.PGM_SIZE() << 3);
      decaf_check(pgm_offset_vs.PGM_OFFSET() == 0);
   } else {
      // When GS is enabled, vertex shader comes from export shader register
      auto pgm_start_es = getRegister<latte::SQ_PGM_START_ES>(latte::Register::SQ_PGM_START_ES);
      auto pgm_offset_es = getRegister<latte::SQ_PGM_CF_OFFSET_ES>(latte::Register::SQ_PGM_CF_OFFSET_ES);
      auto pgm_size_es = getRegister<latte::SQ_PGM_SIZE_ES>(latte::Register::SQ_PGM_SIZE_ES);
      vsShaderBinary = gsl::as_span(
         mem::translate<uint8_t>(pgm_start_es.PGM_START() << 8),
         pgm_size_es.PGM_SIZE() << 3);
      decaf_check(pgm_offset_es.PGM_OFFSET() == 0);
   }

   gpu::VertexShaderDesc shaderDesc;

   shaderDesc.type = gpu::ShaderType::Vertex;
   shaderDesc.binary = vsShaderBinary;
   shaderDesc.fsBinary = fsShaderBinary;

   auto sq_config = getRegister<latte::SQ_CONFIG>(latte::Register::SQ_CONFIG);
   shaderDesc.aluInstPreferVector = sq_config.ALU_INST_PREFER_VECTOR();

   for (auto i = 0u; i < 32; ++i) {
      shaderDesc.vtxSemantics[i] = getRegister<latte::SQ_VTX_SEMANTIC_N>(latte::Register::SQ_VTX_SEMANTIC_0 + i * 4);
   }

   for (auto i = 0; i < latte::MaxTextures; ++i) {
      auto resourceOffset = (latte::SQ_RES_OFFSET::VS_TEX_RESOURCE_0 + i) * 7;
      auto sq_tex_resource_word0 = getRegister<latte::SQ_TEX_RESOURCE_WORD0_N>(latte::Register::SQ_TEX_RESOURCE_WORD0_0 + 4 * resourceOffset);
      shaderDesc.texDims[i] = sq_tex_resource_word0.DIM();
   }

   shaderDesc.vsOutConfig = getRegister<latte::SPI_VS_OUT_CONFIG>(latte::Register::SPI_VS_OUT_CONFIG);

   for (auto i = 0; i < 10; ++i) {
      shaderDesc.vsOutIds[i] = getRegister<latte::SPI_VS_OUT_ID_N>(latte::Register::SPI_VS_OUT_ID_0 + i * 4);
   }

   auto vgt_primitive_type = getRegister<latte::VGT_PRIMITIVE_TYPE>(latte::Register::VGT_PRIMITIVE_TYPE);
   if (vgt_primitive_type.PRIM_TYPE() == latte::VGT_DI_PRIMITIVE_TYPE::RECTLIST) {
      shaderDesc.isScreenSpace = true;
   } else {
      shaderDesc.isScreenSpace = false;
   }

   shaderDesc.instanceStepRates[0] = getRegister<uint32_t>(latte::Register::VGT_INSTANCE_STEP_RATE_0);
   shaderDesc.instanceStepRates[1] = getRegister<uint32_t>(latte::Register::VGT_INSTANCE_STEP_RATE_1);

   auto vsKey = hashVertexShader(shaderDesc);
   auto &foundShader = mVertexShaders[vsKey];

   if (!foundShader.object) {
      foundShader.desc = shaderDesc;
      hlsl2::translate(&shaderDesc, &foundShader.data);

      auto shaderCode = foundShader.data.code;

      ComPtr<ID3DBlob> errors;
      UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      auto compileRes = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &foundShader.object, &errors);
      if (FAILED(compileRes)) {
         decaf_abort(fmt::format("Vertex shader compile failed:\n{}", (const char*)errors->GetBufferPointer()));
      }
   }

   mActiveVertexShader = &foundShader;

   return true;
}

bool
Driver::checkActiveGeometryShader()
{
   // Do not generate geometry shaders if they are disabled
   auto vgt_gs_mode = getRegister<latte::VGT_GS_MODE>(latte::Register::VGT_GS_MODE);
   if (vgt_gs_mode.MODE() == latte::VGT_GS_ENABLE_MODE::OFF) {
      mActiveGeometryShader = nullptr;
      return true;
   }

   decaf_abort("We do not currently support generation of geometry shaders.");

   gsl::span<uint8_t> gsShaderBinary;
   gsl::span<uint8_t> dcShaderBinary;

   // Geometry shader comes from geometry shader register
   auto pgm_start_gs = getRegister<latte::SQ_PGM_START_VS>(latte::Register::SQ_PGM_START_GS);
   auto pgm_offset_gs = getRegister<latte::SQ_PGM_CF_OFFSET_GS>(latte::Register::SQ_PGM_CF_OFFSET_GS);
   auto pgm_size_gs = getRegister<latte::SQ_PGM_SIZE_GS>(latte::Register::SQ_PGM_SIZE_GS);
   gsShaderBinary = gsl::as_span(
      mem::translate<uint8_t>(pgm_start_gs.PGM_START() << 8),
      pgm_size_gs.PGM_SIZE() << 3);
   decaf_check(pgm_offset_gs.PGM_OFFSET() == 0);

   // Data cache shader comes from vertex shader register
   auto pgm_start_vs = getRegister<latte::SQ_PGM_START_VS>(latte::Register::SQ_PGM_START_VS);
   auto pgm_offset_vs = getRegister<latte::SQ_PGM_CF_OFFSET_VS>(latte::Register::SQ_PGM_CF_OFFSET_VS);
   auto pgm_size_vs = getRegister<latte::SQ_PGM_SIZE_VS>(latte::Register::SQ_PGM_SIZE_VS);
   dcShaderBinary = gsl::as_span(
      mem::translate<uint8_t>(pgm_start_vs.PGM_START() << 8),
      pgm_size_vs.PGM_SIZE() << 3);
   decaf_check(pgm_offset_vs.PGM_OFFSET() == 0);

   // If Geometry shading is enabled, we need to have a geometry shader, and data-cache
   //  shaders must always be set if a geometry shader is used.
   decaf_check(!gsShaderBinary.empty());
   decaf_check(!dcShaderBinary.empty());

   // Need to generate the shader here...
   gpu::GeometryShaderDesc shaderDesc;

   shaderDesc.type = gpu::ShaderType::Geometry;
   shaderDesc.binary = gsShaderBinary;
   shaderDesc.dcBinary = dcShaderBinary;

   auto sq_config = getRegister<latte::SQ_CONFIG>(latte::Register::SQ_CONFIG);
   shaderDesc.aluInstPreferVector = sq_config.ALU_INST_PREFER_VECTOR();

   for (auto i = 0; i < latte::MaxTextures; ++i) {
      auto resourceOffset = (latte::SQ_RES_OFFSET::GS_TEX_RESOURCE_0 + i) * 7;
      auto sq_tex_resource_word0 = getRegister<latte::SQ_TEX_RESOURCE_WORD0_N>(latte::Register::SQ_TEX_RESOURCE_WORD0_0 + 4 * resourceOffset);
      shaderDesc.texDims[i] = sq_tex_resource_word0.DIM();
   }

   auto gsKey = hashGeometryShader(shaderDesc);
   auto &foundShader = mGeometryShaders[gsKey];

   if (!foundShader.object) {
      foundShader.desc = shaderDesc;
      hlsl2::translate(&shaderDesc, &foundShader.data);

      auto shaderCode = foundShader.data.code;

      ComPtr<ID3DBlob> errors;
      UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      auto compileRes = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, nullptr, nullptr, "GSMain", "gs_5_0", compileFlags, 0, &foundShader.object, &errors);
      if (FAILED(compileRes)) {
         decaf_abort(fmt::format("Geometry shader compile failed:\n{}", (const char*)errors->GetBufferPointer()));
      }
   }

   mActiveGeometryShader = &foundShader;

   return true;
}

bool
Driver::checkActivePixelShader()
{
   // This is an optimization to avoid generating the pixel shader when
   //  rasterization is disabled, we also disable rasterization on the pipeline
   auto pa_cl_clip_cntl = getRegister<latte::PA_CL_CLIP_CNTL>(latte::Register::PA_CL_CLIP_CNTL);
   if (pa_cl_clip_cntl.RASTERISER_DISABLE()) {
      mActivePixelShader = nullptr;
      return true;
   }

   gsl::span<uint8_t> psShaderBinary;

   auto pgm_start_ps = getRegister<latte::SQ_PGM_START_PS>(latte::Register::SQ_PGM_START_PS);
   auto pgm_offset_ps = getRegister<latte::SQ_PGM_CF_OFFSET_PS>(latte::Register::SQ_PGM_CF_OFFSET_PS);
   auto pgm_size_ps = getRegister<latte::SQ_PGM_SIZE_PS>(latte::Register::SQ_PGM_SIZE_PS);
   psShaderBinary = gsl::as_span(
      mem::translate<uint8_t>(pgm_start_ps.PGM_START() << 8),
      pgm_size_ps.PGM_SIZE() << 3);
   decaf_check(pgm_offset_ps.PGM_OFFSET() == 0);

   gpu::PixelShaderDesc shaderDesc;

   shaderDesc.type = gpu::ShaderType::Pixel;
   shaderDesc.binary = psShaderBinary;

   auto sq_config = getRegister<latte::SQ_CONFIG>(latte::Register::SQ_CONFIG);
   shaderDesc.aluInstPreferVector = sq_config.ALU_INST_PREFER_VECTOR();

   for (auto i = 0; i < latte::MaxTextures; ++i) {
      auto resourceOffset = (latte::SQ_RES_OFFSET::PS_TEX_RESOURCE_0 + i) * 7;
      auto sq_tex_resource_word0 = getRegister<latte::SQ_TEX_RESOURCE_WORD0_N>(latte::Register::SQ_TEX_RESOURCE_WORD0_0 + 4 * resourceOffset);
      shaderDesc.texDims[i] = sq_tex_resource_word0.DIM();
   }

   shaderDesc.psInControl0 = getRegister<latte::SPI_PS_IN_CONTROL_0>(latte::Register::SPI_PS_IN_CONTROL_0);
   shaderDesc.psInControl1 = getRegister<latte::SPI_PS_IN_CONTROL_1>(latte::Register::SPI_PS_IN_CONTROL_1);

   for (auto i = 0; i < 32; ++i) {
      shaderDesc.psInputCntls[i] = getRegister<latte::SPI_PS_INPUT_CNTL_N>(latte::Register::SPI_PS_INPUT_CNTL_0 + i * 4);
   }

   auto sx_alpha_test_control = getRegister<latte::SX_ALPHA_TEST_CONTROL>(latte::Register::SX_ALPHA_TEST_CONTROL);
   shaderDesc.alphaRefFunc = sx_alpha_test_control.ALPHA_FUNC();

   if (!sx_alpha_test_control.ALPHA_TEST_ENABLE() || sx_alpha_test_control.ALPHA_TEST_BYPASS()) {
	   shaderDesc.alphaRefFunc = latte::REF_FUNC::ALWAYS;
   }

   shaderDesc.vsNumPosExports = mActiveVertexShader->data.numPosExports;
   shaderDesc.vsNumParamExports = mActiveVertexShader->data.numParamExports;
   shaderDesc.vsExportMap = mActiveVertexShader->data.exportMap;

   uint64_t psKey = hashPixelShader(shaderDesc);
   auto &foundShader = mPixelShaders[psKey];

   if (!foundShader.object) {
      foundShader.desc = shaderDesc;
      hlsl2::translate(&shaderDesc, &foundShader.data);

      auto shaderCode = foundShader.data.code;

      ComPtr<ID3DBlob> errors;
      UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      auto compileRes = D3DCompile(shaderCode.c_str(), shaderCode.size(), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &foundShader.object, &errors);
      if (FAILED(compileRes)) {
         decaf_abort(fmt::format("Pixel shader compile failed:\n{}", (const char*)errors->GetBufferPointer()));
      }
   }

   mActivePixelShader = &foundShader;

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
