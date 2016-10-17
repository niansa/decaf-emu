#pragma once

#include "gpu/gpu_shadertranspiler.h"
#include "gpu/latte_registers_sq.h"
#include <functional>
#include <gsl.h>
#include <map>
#include <spdlog/fmt/fmt.h>
#include <vector>

namespace hlsl2
{

enum class SampleMode : uint32_t
{
   Unused,
   Normal,
   Compare
};

struct InputData
{
   enum class IndexMode : uint32_t {
      PerVertex,
      PerInstance,
   };

   uint32_t bufferIndex;
   uint32_t offset;
   uint32_t elemWidth;
   uint32_t elemCount;
   IndexMode indexMode;
   uint32_t divisor;
};

struct Shader
{
   std::string code;
};

struct VertexShader : public Shader
{
   bool callsFs;
   uint32_t cfileUsed;
   std::array<uint32_t, latte::MaxUniformBlocks> cbufferUsed;
   std::array<SampleMode, latte::MaxSamplers> samplerState;
   std::array<SampleMode, latte::MaxTextures> textureState;
   std::vector<InputData> inputData;
   uint32_t numPosExports;
   uint32_t numParamExports;
   std::map<uint32_t, uint32_t> exportMap;
};

struct GeometryShader : public Shader
{
   uint32_t cfileUsed;
   std::array<uint32_t, latte::MaxUniformBlocks> cbufferUsed;
   std::array<SampleMode, latte::MaxSamplers> samplerState;
   std::array<SampleMode, latte::MaxTextures> textureState;
};

struct PixelShader : public Shader
{
   uint32_t cfileUsed;
   std::array<uint32_t, latte::MaxUniformBlocks> cbufferUsed;
   std::array<SampleMode, latte::MaxSamplers> samplerState;
   std::array<SampleMode, latte::MaxTextures> textureState;
};

class Transpiler : public gpu::CLikeShaderTranspiler
{
public:
   void insertAssignStmt(const std::string &destVar, const std::string &srcVar, std::array<SQ_SEL, 4> srcMask);
   void insertPredAssignStmt(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst, const std::string &source);
   void insertDestAssignStmt(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst, const std::string &source, bool forAr = false);

   //textureId, samplerId, uvStr, offsetStr
   typedef std::string SampleGenFunc(
      const TextureFetchInst &inst,
      uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim,
      const std::string &source);
   void translateTexGenericSample(const ControlFlowInst &cf, const TextureFetchInst &inst, SampleMode sampleMode, SampleGenFunc genFunc);
   void translateTex_FETCH4(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_L(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_LB(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_G(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_G_L(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_G_LB(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_G_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_L(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_LB(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_G(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_G_L(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_G_LB(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SAMPLE_C_G_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_SET_CUBEMAP_INDEX(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_VTX_FETCH(const ControlFlowInst &cf, const TextureFetchInst &inst) override;
   void translateTex_VTX_SEMANTIC(const ControlFlowInst &cf, const TextureFetchInst &inst) override;

   void translateVtx_FETCH(const ControlFlowInst &cf, const VertexFetchInst &inst) override;
   void translateVtx_SEMANTIC(const ControlFlowInst &cf, const VertexFetchInst &inst) override;

   void translateAluOp2_ADD(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_ADD_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_AND_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_ASHR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_CEIL(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_COS(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_EXP_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_FLOOR(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_FLT_TO_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_FRACT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_INT_TO_FLT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_LOG_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_LOG_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_LSHL_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MAX(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MAX_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MAX_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MAX_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MIN(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MIN_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MIN_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MIN_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MOV(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MOVA_FLOOR(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MUL(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_MUL_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_NOT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_OR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGE_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETGT_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_PRED_SETNE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIP_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIP_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIP_FF(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIP_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIP_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIPSQRT_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIPSQRT_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RECIPSQRT_FF(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_RNDNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGE_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGT_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETGT_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETNE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SETNE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SIN(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SQRT_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_SUB_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_TRUNC(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_XOR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;

   void translateAluOp2_CUBE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_DOT4(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp2_DOT4_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;

   void translateAluOp3_CNDE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_CNDGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_CNDGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_CNDE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_CNDGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_CNDGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_MULADD(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_MULADD_M2(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_MULADD_M4(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluOp3_MULADD_D2(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;

   void recordDataUsage(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst);
   void translateAluInst(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override;
   void translateAluGroup(const ControlFlowInst &cf, const AluInstructionGroup &group) override;

   void translateGenericExport(const ControlFlowInst &cf);

   void translateCfPushGeneric() override;
   void translateCfPopGeneric() override;
   void translateCfElseGeneric() override;

   void translateCf_CALL_FS(const ControlFlowInst &cf) override;
   void translateCf_ELSE(const ControlFlowInst &cf) override;
   void translateCf_JUMP(const ControlFlowInst &cf) override;
   void translateCf_KILL(const ControlFlowInst &cf) override;
   void translateCf_NOP(const ControlFlowInst &cf) override;
   void translateCf_PUSH(const ControlFlowInst &cf) override;
   void translateCf_POP(const ControlFlowInst &cf) override;
   void translateCf_RETURN(const ControlFlowInst &cf) override;
   void translateCf_TEX(const ControlFlowInst &cf) override;
   void translateCf_VTX(const ControlFlowInst &cf) override;
   void translateCf_VTX_TC(const ControlFlowInst &cf) override;

   void translateCf_EXP(const ControlFlowInst &cf) override;
   void translateCf_EXP_DONE(const ControlFlowInst &cf) override;

   void translateCfAluGeneric(const ControlFlowInst &cf, AluPredMode predMode) override;
   void translateCfNormalInst(const ControlFlowInst& cf) override;
   void translateCfExportInst(const ControlFlowInst& cf) override;
   void translateCfAluInst(const ControlFlowInst& cf) override;

   inline void registerSamplerUsage(uint32_t textureId, uint32_t samplerId, SampleMode sampleMode)
   {
      decaf_check(mTextureState[textureId] == SampleMode::Unused || mTextureState[textureId] == sampleMode);
      mTextureState[textureId] = sampleMode;

      decaf_check(mSamplerState[samplerId] == SampleMode::Unused || mSamplerState[samplerId] == sampleMode);
      mSamplerState[samplerId] = sampleMode;
   }

   void translate() override;
   static bool translate(gpu::ShaderDesc *shaderDesc, Shader *shader);

protected:
   std::array<SampleMode, latte::MaxSamplers> mSamplerState = {{ SampleMode::Unused }};
   std::array<SampleMode, latte::MaxTextures> mTextureState = {{ SampleMode::Unused }};
   std::array<uint32_t, latte::MaxUniformBlocks> mCbufferNumUsed = {{ 0 }};
   uint32_t mCfileNumUsed = 0;
   std::vector<InputData> mInputData;
   std::map<uint32_t, uint32_t> mVsExportsMap;
   uint32_t mNumExportPos = 0;
   uint32_t mNumExportParam = 0;
   uint32_t mNumExportPix = 0;

};

bool
translate(gpu::ShaderDesc *shaderDesc, Shader *shader);

gpu::VertexShaderDesc
cleanDesc(const gpu::VertexShaderDesc &desc, const VertexShader &shader);

gpu::GeometryShaderDesc
cleanDesc(const gpu::GeometryShaderDesc &desc, const GeometryShader &shader);

gpu::PixelShaderDesc
cleanDesc(const gpu::PixelShaderDesc &desc, const PixelShader &shader);

} // namespace hlsl2