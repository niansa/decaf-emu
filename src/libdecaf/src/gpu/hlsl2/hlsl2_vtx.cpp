#include "gpu/gpu_utilities.h"
#include "gpu/latte_enum_sq.h"
#include "hlsl2_genhelpers.h"
#include "hlsl2_translate.h"

namespace hlsl2
{

void Transpiler::translateVtx_SEMANTIC(const ControlFlowInst &cf, const VertexFetchInst &inst)
{
   // Because this instruction has an SRC register to determine where to source
   //  the indexing data from, but we need a constant, we assume that it will always
   //  use R0, and that it will contain what it originally starts with during shader
   //  startup.  In order to ensure this is the case, we only allow SEMANTIC inst's
   //  to execute within a Fetch Shader for now, and we also ensure that the fetch
   //  shader is always invoked as the first instruction of a vertex shader.  We
   //  hope that this never needs to be fixed, otherwise things are going to get
   //  EXTREMELY complicated (need to do register expression propagation).
   decaf_check(mType == ShaderParser::Type::Fetch);

   // We do not support fetch constant fields as I don't even
   //  know what they are at the moment!
   decaf_check(!inst.word1.USE_CONST_FIELDS());

   // More stuff I have no clue about...
   decaf_check(!inst.word1.SRF_MODE_ALL());

   // We know what this one does, but I don't know how to implement it,
   //  and I do not think it will be needed right now...
   decaf_check(!inst.word2.CONST_BUF_NO_STRIDE());

   // We use the DATA_FORMAT to determine the fetch sizing.  This allows us
   //  to more easily handle the splitting of XYZW groups below.
   //inst.word2.MEGA_FETCH();
   //inst.word0.MEGA_FETCH_COUNT();

   auto srcGpr = makeGprRef(inst.word0.SRC_GPR(), inst.word0.SRC_REL(), SQ_INDEX_MODE::LOOP);
   auto srcSel = inst.word0.SRC_SEL_X();

   // We do not support indexing for fetch from buffers...
   decaf_check(srcGpr.indexMode == GprIndexMode::None);

   GprMaskRef destGpr;
   destGpr.gpr = findVtxSemanticGpr(inst.sem.SEMANTIC_ID());
   destGpr.mask[SQ_CHAN::X] = inst.word1.DST_SEL_X();
   destGpr.mask[SQ_CHAN::Y] = inst.word1.DST_SEL_Y();
   destGpr.mask[SQ_CHAN::Z] = inst.word1.DST_SEL_Z();
   destGpr.mask[SQ_CHAN::W] = inst.word1.DST_SEL_W();

   if (destGpr.gpr.number == 0xffffffff) {
      // This is not semantically mapped, so we can actually skip it entirely!
      return;
   }

   auto dataFormat = inst.word1.DATA_FORMAT();
   auto fmtData = gpu::getDataFormatInfo(dataFormat);

   // We currently only support vertex fetches inside a fetch shader...
   decaf_check(mType == ShaderParser::Type::Fetch);

   // Figure out which attribute buffer this is referencing
   auto bufferId = inst.word0.BUFFER_ID();
   auto attribBase = latte::SQ_RES_OFFSET::VS_ATTRIB_RESOURCE_0 - latte::SQ_RES_OFFSET::VS_TEX_RESOURCE_0;
   decaf_check(attribBase <= bufferId && bufferId <= attribBase + 16);

   auto attribBufferId = bufferId - attribBase;
   auto bufferOffset = inst.word2.OFFSET();

   InputData::IndexMode indexMode;
   uint32_t divisor;

   if (inst.word0.FETCH_TYPE() == SQ_VTX_FETCH_TYPE::VERTEX_DATA) {
      indexMode = InputData::IndexMode::PerVertex;
      divisor = 0;
   } else if (inst.word0.FETCH_TYPE() == SQ_VTX_FETCH_TYPE::INSTANCE_DATA) {
      indexMode = InputData::IndexMode::PerInstance;

      if (srcSel == SQ_SEL::SEL_Y) {
         divisor = mVsInstanceStepRates[0];
      } else if (srcSel == SQ_SEL::SEL_Z) {
         divisor = mVsInstanceStepRates[1];
      } else if (srcSel == SQ_SEL::SEL_W) {
         divisor = 0;
      } else {
         decaf_abort("Unexpected vertex fetch divisor selector");
      }
   } else {
      decaf_abort("Unexpected vertex fetch type");
   }

   mInputData.emplace_back(InputData{ attribBufferId, bufferOffset, fmtData.inputWidth, fmtData.inputCount, indexMode, divisor});
   auto inputId = mInputData.size() - 1;

   auto swapMode = inst.word2.ENDIAN_SWAP();
   auto formatComp = inst.word1.FORMAT_COMP_ALL();
   auto numFormat = inst.word1.NUM_FORMAT_ALL();

   auto genFieldSwap = [&](const std::string &source, SQ_CHAN chan, const gpu::DataFormatElem &elem) {
      decaf_check(swapMode != SQ_ENDIAN::NONE);

      if (chan >= fmtData.inputCount) {
         return std::string("0");
      }

      if (swapMode == SQ_ENDIAN::SWAP_8IN16) {
         decaf_check(fmtData.inputWidth == 16 || fmtData.inputWidth == 32);
         return fmt::format("bswap8in16({}.{})", source, genChanStr(chan));
      } else if (swapMode == SQ_ENDIAN::SWAP_8IN32) {
         decaf_check(fmtData.inputWidth == 32);
         return fmt::format("bswap8in32({}.{})", source, genChanStr(chan));
      } else {
         decaf_abort("Encountered unexpected endian swap mode");
      }
   };

   auto isNeedBitshift = [&](const gpu::DataFormatElem &elem) {
      if (elem.length == 0) {
         return false;
      }

      return elem.length != fmtData.inputWidth;
   };

   auto genFieldSel = [&](const std::string &source, SQ_CHAN chan, const gpu::DataFormatElem &elem) {
      auto inputChan = static_cast<SQ_CHAN>(elem.index);

      if (elem.length == 0) {
         return std::string("0");
      }

      if (elem.length == fmtData.inputWidth) {
         return fmt::format("{}.{}", source, genChanStr(inputChan));
      } else {
         return fmt::format("({}.{} >> {}) & 0x{:x}", source, genChanStr(inputChan), elem.start, (1 << elem.length) - 1);
      }
   };

   auto genFieldExp = [&](const std::string &source, SQ_CHAN chan, const gpu::DataFormatElem &elem) {
      if (elem.length == 0) {
         return std::string("0");
      }

      std::string field;
      auto fieldMax = static_cast<uint64_t>(1u) << elem.length;

      if (fmtData.type == gpu::DataFormatType::FLOAT) {
         if (elem.length == 16) {
            field = fmt::format("f16tof32({}.{})", source, genChanStr(chan));
         } else if (elem.length == 32) {
            field = fmt::format("asfloat({}.{})", source, genChanStr(chan));
         } else {
            decaf_abort("Unexpected float data width");
         }
      } else {
         if (formatComp == latte::SQ_FORMAT_COMP::SIGNED) {
            // Perform sign-extension and conversion to a signed integer
            if (elem.length == 32) {
               // If its 32 bits, we don't need to perform sign extension, just casting
               field = fmt::format("(int)({}.{})", source, genChanStr(chan));
            } else {
               field = fmt::format("(int)(({}.{} ^ {}) - {})", source, genChanStr(chan), fieldMax / 2, fieldMax / 2);
            }
         } else {
            // We are already in UINT format as we needed
            field = fmt::format("{}.{}", source, genChanStr(chan));
         }
      }

      if (numFormat == latte::SQ_NUM_FORMAT::NORM) {
         auto fieldMask = fieldMax - 1;

         if (formatComp == latte::SQ_FORMAT_COMP::SIGNED) {
            field = fmt::format("clamp((float)({}) / {}.0f, -1.0f, 1.0f)", field, fieldMask / 2);
         } else {
            field = fmt::format("(float)({}) / {}.0f", field, fieldMask);
         }
      } else if (numFormat == latte::SQ_NUM_FORMAT::INT) {
         if (formatComp == latte::SQ_FORMAT_COMP::SIGNED) {
            field = fmt::format("asfloat((int)({}))", field);
         } else {
            field = fmt::format("asfloat((uint)({}))", field);
         }
      } else if (numFormat == latte::SQ_NUM_FORMAT::SCALED) {
         if (fmtData.type == gpu::DataFormatType::FLOAT) {
            // The input format was already a float, we don't need to anything here
         } else {
            // The input format was an integer, we need to perform the cast
            field = fmt::format("(float)({})", field);
         }
      } else {
         decaf_abort("Unexpected vertex fetch numFormat");
      }

      return field;
   };

   auto phaseSource = fmt::format("input.data{}", inputId);

   if (swapMode != SQ_ENDIAN::NONE) {
      insertLineStart();
      mOut.write("utmp = uint4({}, {}, {}, {});",
         genFieldSwap(phaseSource, SQ_CHAN::X, fmtData.x),
         genFieldSwap(phaseSource, SQ_CHAN::Y, fmtData.y),
         genFieldSwap(phaseSource, SQ_CHAN::Z, fmtData.z),
         genFieldSwap(phaseSource, SQ_CHAN::W, fmtData.w));
      insertLineEnd();

      phaseSource = "utmp";
   }

   if (isNeedBitshift(fmtData.x) || isNeedBitshift(fmtData.y) || isNeedBitshift(fmtData.z) || isNeedBitshift(fmtData.w)) {
      insertLineStart();
      mOut.write("utmp = uint4({}, {}, {}, {});",
         genFieldSel(phaseSource, SQ_CHAN::X, fmtData.x),
         genFieldSel(phaseSource, SQ_CHAN::Y, fmtData.y),
         genFieldSel(phaseSource, SQ_CHAN::Z, fmtData.z),
         genFieldSel(phaseSource, SQ_CHAN::W, fmtData.w));
      insertLineEnd();

      phaseSource = "utmp";
   }

   insertLineStart();
   mOut.write("tmp = float4({}, {}, {}, {});",
      genFieldExp(phaseSource, SQ_CHAN::X, fmtData.x),
      genFieldExp(phaseSource, SQ_CHAN::Y, fmtData.y),
      genFieldExp(phaseSource, SQ_CHAN::Z, fmtData.z),
      genFieldExp(phaseSource, SQ_CHAN::W, fmtData.w));
   insertLineEnd();

   insertAssignStmt(genGprRefStr(destGpr.gpr), "tmp", destGpr.mask);
}

} // namespace hlsl2
