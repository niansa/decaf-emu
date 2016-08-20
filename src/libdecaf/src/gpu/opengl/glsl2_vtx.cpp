#include "glsl2_translate.h"
#include "gpu/microcode/latte_instructions.h"

using namespace latte;

/*
Unimplemented:
FETCH
SEMANTIC
BUFINFO
*/

namespace glsl2
{

static void
VTX_FETCH(State &state, const ControlFlowInst &cf, const VertexFetchInst &inst)
{
   // We do not currently handle condition override anywhere
   //decaf_check(!cf.exp.word1.VALID_PIXEL_MODE());
   decaf_check(!cf.exp.word1.WHOLE_QUAD_MODE());

   if (cf.exp.word1.VALID_PIXEL_MODE()) {
      state.out << "/*VALID_PIXEL_MODE*/";
   }

   auto id = inst.word0.BUFFER_ID();

   if (state.shader->type == Shader::Type::VertexShader) {
      id += SQ_VS_RESOURCE_BASE;
   } else if (state.shader->type == Shader::Type::GeometryShader) {
      id += SQ_GS_RESOURCE_BASE;
   } else if (state.shader->type == Shader::Type::PixelShader) {
      id += SQ_PS_RESOURCE_BASE;
   } else {
      decaf_abort("Unexpected shader type for VTX_FETCH instruction");
   }

   if ((id >= SQ_VS_BUF_RESOURCE_0 && id < SQ_VS_BUF_RESOURCE_0 + 0x10) ||
      (id >= SQ_GS_BUF_RESOURCE_0 && id < SQ_GS_BUF_RESOURCE_0 + 0x10) ||
      (id >= SQ_PS_BUF_RESOURCE_0 && id < SQ_PS_BUF_RESOURCE_0 + 0x10)) {
      // Let's only support a very expected set of values
      decaf_check(inst.word0.FETCH_TYPE() == SQ_VTX_FETCH_NO_INDEX_OFFSET);
      decaf_check(inst.word1.USE_CONST_FIELDS() == 1);
      decaf_check(inst.word2.OFFSET() == 0);
      decaf_check(inst.word2.MEGA_FETCH() && (inst.word0.MEGA_FETCH_COUNT() + 1) == 16);

      auto dstSelX = inst.word1.DST_SEL_X();
      auto dstSelY = inst.word1.DST_SEL_Y();
      auto dstSelZ = inst.word1.DST_SEL_Z();
      auto dstSelW = inst.word1.DST_SEL_W();

      auto numDstSels = 4u;
      auto dstSelMask = condenseSelections(dstSelX, dstSelY, dstSelZ, dstSelW, numDstSels);

      if (numDstSels > 0) {
         auto dst = getExportRegister(inst.gpr.DST_GPR(), inst.gpr.DST_REL());
         auto src = getExportRegister(inst.word0.SRC_GPR(), inst.word0.SRC_REL());
         auto blockID = -1;

         if (id >= SQ_VS_BUF_RESOURCE_0 && id < SQ_VS_BUF_RESOURCE_0 + 0x10) {
            blockID = id - SQ_VS_BUF_RESOURCE_0;
         } else if (id >= SQ_GS_BUF_RESOURCE_0 && id < SQ_GS_BUF_RESOURCE_0 + 0x10) {
            blockID = id - SQ_GS_BUF_RESOURCE_0;
         } else if (id >= SQ_PS_BUF_RESOURCE_0 && id < SQ_PS_BUF_RESOURCE_0 + 0x10) {
            blockID = id - SQ_PS_BUF_RESOURCE_0;
         } else {
            decaf_abort("Unexpect buffer resource type");
         }

         if (state.shader) {
            state.shader->usedUniformBlocks[blockID] = true;
         }

         fmt::MemoryWriter tmp;
         tmp << "UB_" << blockID << ".values[floatBitsToInt(";
         insertSelectValue(tmp, src, inst.word0.SRC_SEL_X());
         tmp << ")]";

         insertLineStart(state);
         state.out << dst << "." << dstSelMask << " = ";
         insertSelectVector(state.out, tmp.str(), dstSelX, dstSelY, dstSelZ, dstSelW, numDstSels);
         state.out << ";";
         insertLineEnd(state);
      }
   } else if (id == SQ_GS_GSIN_RESOURCE) {
      // We only support unindexed offset lookups
      decaf_check(inst.word0.FETCH_TYPE() == SQ_VTX_FETCH_NO_INDEX_OFFSET);

      // I don't think there is really even such thing for a GSOUT resource...
      decaf_check(inst.word1.USE_CONST_FIELDS() == 1);

      // We only support fetching full vec4 values
      decaf_check(inst.word2.MEGA_FETCH() && (inst.word0.MEGA_FETCH_COUNT() + 1) == 16);

      // We do not support relative indexing
      decaf_check(!inst.word0.SRC_REL());

      auto dstSelX = inst.word1.DST_SEL_X();
      auto dstSelY = inst.word1.DST_SEL_Y();
      auto dstSelZ = inst.word1.DST_SEL_Z();
      auto dstSelW = inst.word1.DST_SEL_W();

      auto numDstSels = 4u;
      auto dstSelMask = condenseSelections(dstSelX, dstSelY, dstSelZ, dstSelW, numDstSels);

      if (numDstSels == 0) {
         decaf_abort("Encountered strange GSOUT FETCH with no destination writes");
      }

      auto vertexIdx = 0u;
      if (inst.word0.SRC_GPR() == 0) {
         if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_X) {
            vertexIdx = 0;
         } else if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_Y) {
            vertexIdx = 1;
         } else if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_W) {
            vertexIdx = 2;
         } else {
            decaf_abort("Unexpected GSIN GPR0 SEL_X");
         }
      } else if (inst.word0.SRC_GPR() == 1) {
         if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_X) {
            vertexIdx = 3;
         } else if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_Y) {
            vertexIdx = 4;
         } else if (inst.word0.SRC_SEL_X() == latte::SQ_SEL_Z) {
            vertexIdx = 5;
         } else {
            decaf_abort("Unexpected GSIN GPR1 SEL_X");
         }
      } else {
         decaf_abort(fmt::format("Unexpected GSIN fetch SRC_GPR {}", inst.word0.SRC_GPR()));
      }

      int valueIdx = inst.word2.OFFSET() / 16;

      insertLineStart(state);

      if (valueIdx == 0) {
         // GL is annoying and requires us to use a different place to read this...
         state.out << "texTmp = gl_in[" << vertexIdx << "].gl_Position;";
      } else {
         state.out << "texTmp = gsin_" << (valueIdx - 1) << "[" << vertexIdx << "];";
      }

      insertLineEnd(state);

      insertLineStart(state);
      insertRegister(state.out, inst.gpr.DST_GPR(), inst.gpr.DST_REL());
      state.out << "." << dstSelMask << " = ";
      insertSelectVector(state.out, "texTmp", dstSelX, dstSelY, dstSelZ, dstSelW, numDstSels);
      state.out << ";";
      insertLineEnd(state);
   } else {
      decaf_abort(fmt::format("Unsupported VTX_FETCH buffer id {}", id));
   }
}

void
registerVtxFunctions()
{
   registerInstruction(latte::SQ_VTX_INST_FETCH, VTX_FETCH);
}

} // namespace glsl2
