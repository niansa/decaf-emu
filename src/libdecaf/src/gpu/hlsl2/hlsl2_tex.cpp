#include "hlsl2_genhelpers.h"
#include "hlsl2_translate.h"

namespace hlsl2
{

static std::string
genDimUvChanStr(const std::string &source, SQ_TEX_DIM texDim)
{
   switch (texDim) {
   case latte::SQ_TEX_DIM::DIM_1D:
      return fmt::format("{}.x", source);
   case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
   case latte::SQ_TEX_DIM::DIM_2D:
   case latte::SQ_TEX_DIM::DIM_2D_MSAA:
      return fmt::format("{}.xy", source);
   case latte::SQ_TEX_DIM::DIM_3D:
   case latte::SQ_TEX_DIM::DIM_CUBEMAP:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
      return fmt::format("{}.xyz", source);
   default:
      decaf_abort("Unexpected texture sample dim");
   }
}

static std::string
genDimStChanStr(const std::string &source, SQ_TEX_DIM texDim)
{
   switch (texDim) {
   case latte::SQ_TEX_DIM::DIM_1D:
   case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
      return fmt::format("{}.x", source);
   case latte::SQ_TEX_DIM::DIM_2D:
   case latte::SQ_TEX_DIM::DIM_2D_MSAA:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
      return fmt::format("{}.xy", source);
   case latte::SQ_TEX_DIM::DIM_3D:
   case latte::SQ_TEX_DIM::DIM_CUBEMAP:
      return fmt::format("{}.xyz", source);
   default:
      decaf_abort("Unexpected texture sample dim");
   }
}

bool
isUsingOffsets(const TextureFetchInst &inst)
{
   return !!inst.word2.OFFSET_X() &&
      !!inst.word2.OFFSET_X() &&
      !!inst.word2.OFFSET_Y();
}

std::string
genOffsetStr(const TextureFetchInst &inst, SQ_TEX_DIM texDim)
{
   auto offsetX = inst.word2.OFFSET_X();
   auto offsetY = inst.word2.OFFSET_Y();
   auto offsetZ = inst.word2.OFFSET_Z();

   switch (texDim) {
   case latte::SQ_TEX_DIM::DIM_1D:
   case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
      return fmt::format("{}", offsetX);
   case latte::SQ_TEX_DIM::DIM_2D:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
   case latte::SQ_TEX_DIM::DIM_2D_MSAA:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
      return fmt::format("int2({}, {})", offsetX, offsetY);
   case latte::SQ_TEX_DIM::DIM_3D:
      return fmt::format("int3({}, {}, {})", offsetX, offsetY, offsetZ);
   default:
      decaf_abort("Encountered unexpected texture dim for offset");
   }
}

std::string
genLodBiasStr(uint32_t lodBias)
{
   auto biasFp = sg14::make_fixed<2, 4>::from_data(lodBias);
   return fmt::format("{}", static_cast<float>(biasFp));
}

void Transpiler::translateTexGenericSample(const ControlFlowInst &cf, const TextureFetchInst &inst, SampleMode sampleMode, SampleGenFunc genFunc)
{
   // inst.word0.FETCH_WHOLE_QUAD(); - Optimization which can be ignored
   // inst.word1.LOD_BIAS(); - Only used by certain SAMPLE instructions

   decaf_check(!inst.word0.BC_FRAC_MODE());

   auto textureId = inst.word0.RESOURCE_ID();
   auto samplerId = inst.word2.SAMPLER_ID();
   auto texDim = mTexDims[textureId];

   GprMaskRef srcGpr;
   srcGpr.gpr = makeGprRef(inst.word0.SRC_GPR(), inst.word0.SRC_REL(), SQ_INDEX_MODE::LOOP);
   srcGpr.mask[SQ_CHAN::X] = inst.word2.SRC_SEL_X();
   srcGpr.mask[SQ_CHAN::Y] = inst.word2.SRC_SEL_Y();
   srcGpr.mask[SQ_CHAN::Z] = inst.word2.SRC_SEL_Z();
   srcGpr.mask[SQ_CHAN::W] = inst.word2.SRC_SEL_W();

   insertAssignStmt("tmp", genGprRefStr(srcGpr.gpr), srcGpr.mask);

   switch (texDim) {
   case latte::SQ_TEX_DIM::DIM_1D:
      decaf_check(inst.word1.COORD_TYPE_X() == SQ_TEX_COORD_TYPE::NORMALIZED);
      break;
   case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
      decaf_check(inst.word1.COORD_TYPE_X() == SQ_TEX_COORD_TYPE::NORMALIZED);
      decaf_check(inst.word1.COORD_TYPE_Y() == SQ_TEX_COORD_TYPE::UNNORMALIZED);
      break;
   case latte::SQ_TEX_DIM::DIM_2D:
   case latte::SQ_TEX_DIM::DIM_2D_MSAA:
      break;
   case latte::SQ_TEX_DIM::DIM_3D:
   case latte::SQ_TEX_DIM::DIM_CUBEMAP:
      decaf_check(inst.word1.COORD_TYPE_X() == SQ_TEX_COORD_TYPE::NORMALIZED);
      decaf_check(inst.word1.COORD_TYPE_Y() == SQ_TEX_COORD_TYPE::NORMALIZED);
      decaf_check(inst.word1.COORD_TYPE_Z() == SQ_TEX_COORD_TYPE::NORMALIZED);
      break;
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
   case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
      decaf_check(inst.word1.COORD_TYPE_X() == SQ_TEX_COORD_TYPE::NORMALIZED);
      decaf_check(inst.word1.COORD_TYPE_Y() == SQ_TEX_COORD_TYPE::NORMALIZED);
      decaf_check(inst.word1.COORD_TYPE_Z() == SQ_TEX_COORD_TYPE::UNNORMALIZED);
      break;
   default:
      decaf_abort("Unexpected texture sample dim");
   }

   insertLineStart();
   mOut.write("tmp = {};", genFunc(inst, textureId, samplerId, texDim, "tmp"));
   insertLineEnd();

   GprMaskRef destGpr;
   destGpr.gpr = makeGprRef(inst.word1.DST_GPR(), inst.word1.DST_REL(), SQ_INDEX_MODE::LOOP);
   destGpr.mask[SQ_CHAN::X] = inst.word1.DST_SEL_X();
   destGpr.mask[SQ_CHAN::Y] = inst.word1.DST_SEL_Y();
   destGpr.mask[SQ_CHAN::Z] = inst.word1.DST_SEL_Z();
   destGpr.mask[SQ_CHAN::W] = inst.word1.DST_SEL_W();

   insertAssignStmt(genGprRefStr(destGpr.gpr), "tmp", destGpr.mask);

   registerSamplerUsage(textureId, samplerId, sampleMode);
}

void Transpiler::translateTex_FETCH4(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.Gather(sampler{}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.Gather(sampler{}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim));
      }
   });
}

void Transpiler::translateTex_SAMPLE(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.Sample(sampler{}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.Sample(sampler{}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim));
      }
   });
}

void Transpiler::translateTex_SAMPLE_L(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.SampleLevel(sampler{}, {}, {}.w, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source,
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.SampleLevel(sampler{}, {}, {}.w)",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source);
      }
   });
}

void Transpiler::translateTex_SAMPLE_LB(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.Sample(sampler{}, {}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genLodBiasStr(instr.word1.LOD_BIAS()),
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.Sample(sampler{}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genLodBiasStr(instr.word1.LOD_BIAS()));
      }
   });
}

void Transpiler::translateTex_SAMPLE_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.SampleLevel(sampler{}, {}, 0, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.SampleLevel(sampler{}, {}, 0)",
            textureId, samplerId,
            genDimUvChanStr(source, texDim));
      }
   });
}

void Transpiler::translateTex_SAMPLE_G(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Normal,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.SampleGrad(sampler{}, {}, {}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genDimStChanStr("gradX", texDim),
            genDimStChanStr("gradY", texDim),
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.SampleGrad(sampler{}, {}, {}, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            genDimStChanStr("gradX", texDim),
            genDimStChanStr("gradY", texDim));
      }
   });
}

void Transpiler::translateTex_SAMPLE_G_L(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_G_L");
}

void Transpiler::translateTex_SAMPLE_G_LB(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_G_LB");
}

void Transpiler::translateTex_SAMPLE_G_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_G_LZ");
}

void Transpiler::translateTex_SAMPLE_C(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Compare,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.SampleCmp(sampler{}, {}, {}.w, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source,
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.SampleCmp(sampler{}, {}, {}.w)",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source);
      }
   });
}

void Transpiler::translateTex_SAMPLE_C_L(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_C_L");
}

void Transpiler::translateTex_SAMPLE_C_LB(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_C_LB");
}

void Transpiler::translateTex_SAMPLE_C_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   translateTexGenericSample(cf, inst, SampleMode::Compare,
      [](const TextureFetchInst &instr, uint32_t textureId, uint32_t samplerId, SQ_TEX_DIM texDim, const std::string &source) {
      if (isUsingOffsets(instr)) {
         return fmt::format("texture{}.SampleCmpLevelZero(sampler{}, {}, {}.w, {})",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source,
            genOffsetStr(instr, texDim));
      } else {
         return fmt::format("texture{}.SampleCmpLevelZero(sampler{}, {}, {}.w)",
            textureId, samplerId,
            genDimUvChanStr(source, texDim),
            source);
      }
   });
}

void Transpiler::translateTex_SAMPLE_C_G(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_C_G");
}

void Transpiler::translateTex_SAMPLE_C_G_L(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_C_G_L");
}

void Transpiler::translateTex_SAMPLE_C_G_LB(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_G_LB");
}

void Transpiler::translateTex_SAMPLE_C_G_LZ(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   decaf_abort("DX12 does not support SAMPLE_C_G_LZ");
}

void Transpiler::translateTex_SET_CUBEMAP_INDEX(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   // TODO: It is possible that we are supposed to somehow force
   //  a specific face to be used in spite of the coordinates.
}

void Transpiler::translateTex_VTX_FETCH(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   // The TextureFetchInst perfectly matches the VertexFetchInst
   translateVtx_FETCH(cf, *reinterpret_cast<const VertexFetchInst*>(&inst));
}

void Transpiler::translateTex_VTX_SEMANTIC(const ControlFlowInst &cf, const TextureFetchInst &inst)
{
   // The TextureFetchInst perfectly matches the VertexFetchInst
   translateVtx_SEMANTIC(cf, *reinterpret_cast<const VertexFetchInst*>(&inst));
}

} // namespace hlsl2
