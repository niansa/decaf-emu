#pragma once

#include "latte_constants.h"
#include "latte_registers_spi.h"
#include "latte_registers_sq.h"
#include "microcode/latte_parser.h"
#include "microcode/latte_disassembler.h"
#include <array>
#include <map>
#include <string>
#include <vector>

using namespace latte;

namespace gpu
{

enum class ShaderType : uint32_t
{
   Unknown,
   Vertex,
   Geometry,
   Pixel
};

struct ShaderDesc
{
   ShaderType type = ShaderType::Unknown;
   gsl::span<const uint8_t> binary;
   bool aluInstPreferVector;
};

struct VertexShaderDesc : public ShaderDesc
{
   gsl::span<const uint8_t> fsBinary;
   std::array<latte::SQ_TEX_DIM, latte::MaxTextures> texDims;
   std::array<latte::SQ_VTX_SEMANTIC_N, 32> vtxSemantics;
   latte::SPI_VS_OUT_CONFIG vsOutConfig;
   std::array<latte::SPI_VS_OUT_ID_N, 10> vsOutIds;
   bool isScreenSpace;
   std::array<uint32_t, 2> instanceStepRates;
};

struct GeometryShaderDesc : public ShaderDesc
{
   gsl::span<const uint8_t> dcBinary;
   std::array<latte::SQ_TEX_DIM, latte::MaxTextures> texDims;
};

struct PixelShaderDesc : public ShaderDesc
{
   std::array<latte::SQ_TEX_DIM, latte::MaxTextures> texDims;
   latte::SPI_PS_IN_CONTROL_0 psInControl0;
   latte::SPI_PS_IN_CONTROL_1 psInControl1;
   std::array<latte::SPI_PS_INPUT_CNTL_N, 32> psInputCntls;
   std::map<uint32_t, uint32_t> vsExportMap;
   uint32_t vsNumPosExports;
   uint32_t vsNumParamExports;
   latte::REF_FUNC alphaRefFunc;
};

class CLikeShaderTranspiler : public ShaderParser
{
protected:
   static const auto IndentSize = 2u;

   enum class AluPredMode : uint32_t {
      Branch,
      Break,
      Continue
   };

   void insertLineStart() {
      mOut << mIndent;
   }

   void insertLineEnd() {
      mOut << '\n';
   }

   void increaseIndent() {
      mIndent.append(IndentSize, ' ');
   }

   void decreaseIndent() {
      decaf_check(mIndent.size() >= IndentSize);
      mIndent.resize(mIndent.size() - IndentSize);
   }

   virtual void translateCfAluGeneric(const ControlFlowInst &cf, AluPredMode predMode)
   {
      mCfPredMode = predMode;
      translateAluClause(cf);
   }

   virtual void translateCfPushGeneric()
   {
   }

   virtual void translateCfPopGeneric()
   {
   }

   virtual void translateCfElseGeneric()
   {
   }

   void translateCf_ALU(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Branch);
   }

   void translateCf_ALU_PUSH_BEFORE(const ControlFlowInst &cf) override
   {
      translateCfPushGeneric();
      translateCfAluGeneric(cf, AluPredMode::Branch);
   }

   void translateCf_ALU_POP_AFTER(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Branch);
      translateCfPopGeneric();
   }

   void translateCf_ALU_POP2_AFTER(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Branch);
      translateCfPopGeneric();
      translateCfPopGeneric();
   }

   void translateCf_ALU_EXT(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Branch);
   }

   void translateCf_ALU_CONTINUE(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Continue);
   }

   void translateCf_ALU_BREAK(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Break);
   }

   void translateCf_ALU_ELSE_AFTER(const ControlFlowInst &cf) override
   {
      translateCfAluGeneric(cf, AluPredMode::Branch);
      translateCfElseGeneric();
   }

   void translateCf_TEX(const ControlFlowInst &cf) override
   {
      translateTexClause(cf);
   }

   void translateCf_VTX(const ControlFlowInst &cf) override
   {
      translateVtxClause(cf);
   }

   void translateCf_VTX_TC(const ControlFlowInst &cf) override
   {
      translateVtxClause(cf);
   }

   void translateTexInst(const ControlFlowInst &cf, const TextureFetchInst &inst) override
   {
      insertLineStart();
      mOut.write("// ");
      latte::disassembler::disassembleTexInstruction(mOut, cf, inst);
      insertLineEnd();

      ShaderParser::translateTexInst(cf, inst);

      mOut.write("\n");
   }

   void translateVtxInst(const ControlFlowInst &cf, const VertexFetchInst &inst) override
   {
      insertLineStart();
      mOut.write("// ");
      latte::disassembler::disassembleVtxInstruction(mOut, cf, inst);
      insertLineEnd();

      ShaderParser::translateVtxInst(cf, inst);

      mOut.write("\n");
   }

   void insertAluInstDisasm(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
   {
      insertLineStart();
      static const char * CHAN_NAMES[] = { "X", "Y", "Z", "W", "T" };
      mOut.write("// .{} ", CHAN_NAMES[unit]);
      latte::disassembler::disassembleAluInstruction(mOut, cf, inst, mGroupPC, unit, group.literals);
      insertLineEnd();
   }

   void translateAluInst(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst) override
   {
      bool isReducInst = (unit == 0xffffffff);

      if (!isReducInst) {
         if (inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2 && inst.op2.ALU_INST() == SQ_OP2_INST_NOP) {
            // To avoid poluting the log too much, we simply ignore NOP
            //  ALU operations.  Note that we don't even call the underlying
            //  base transpiler in this case, since it shouldn't do anything.
            return;
         }

         insertAluInstDisasm(cf, group, unit, inst);
      } else {
         insertAluInstDisasm(cf, group, SQ_CHAN::X, *group.units[SQ_CHAN::X]);
         insertAluInstDisasm(cf, group, SQ_CHAN::Y, *group.units[SQ_CHAN::Y]);
         insertAluInstDisasm(cf, group, SQ_CHAN::Z, *group.units[SQ_CHAN::Z]);
         insertAluInstDisasm(cf, group, SQ_CHAN::W, *group.units[SQ_CHAN::W]);
      }

      ShaderParser::translateAluInst(cf, group, unit, inst);
   }

   void translateAluGroup(const ControlFlowInst &cf, const AluInstructionGroup &group) override
   {
      insertLineStart();
      mOut.write("// {:02}", mGroupPC);
      insertLineEnd();

      ShaderParser::translateAluGroup(cf, group);

      for (auto &line : mPostGroupOut) {
         insertLineStart();
         mOut << line;
         insertLineEnd();
      }
      mPostGroupOut.clear();

      mOut.write("\n");
   }

   void translateCfNormalInst(const ControlFlowInst& cf) override
   {
      insertLineStart();
      mOut.write("// {:02} ", mCfPC);
      latte::disassembler::disassembleCF(mOut, cf);
      insertLineEnd();

      ShaderParser::translateCfNormalInst(cf);

      mOut.write("\n");
   }

   void translateCfExportInst(const ControlFlowInst& cf) override
   {
      insertLineStart();
      mOut.write("// {:02} ", mCfPC);
      latte::disassembler::disassembleExpInstruction(mOut, cf);
      insertLineEnd();

      ShaderParser::translateCfExportInst(cf);

      mOut.write("\n");
   }

   void translateCfAluInst(const ControlFlowInst& cf) override
   {
      insertLineStart();
      mOut.write("// {:02} ", mCfPC);
      latte::disassembler::disassembleCfALUInstruction(mOut, cf);
      insertLineEnd();

      ShaderParser::translateCfAluInst(cf);

      mOut.write("\n");
   }

   GprRef findVtxSemanticGpr(uint32_t semanticId) const
   {
      for (auto i = 0u; i < mVtxSemantics.size(); ++i) {
         if (mVtxSemantics[i].SEMANTIC_ID() == semanticId) {
            return makeGprRef(1 + i, SQ_REL::ABS, SQ_INDEX_MODE::LOOP);
         }
      }

      return makeGprRef(0xffffffff, SQ_REL::ABS, SQ_INDEX_MODE::LOOP);
   }

protected:
   void applyShaderDesc(ShaderDesc *shaderDesc)
   {
      mBinary = shaderDesc->binary;
      mIsFunction = false;
      mAluInstPreferVector = shaderDesc->aluInstPreferVector;

      if (shaderDesc->type == gpu::ShaderType::Vertex) {
         auto vsShaderDesc = reinterpret_cast<gpu::VertexShaderDesc *>(shaderDesc);
         mVtxSemantics = vsShaderDesc->vtxSemantics;
         mTexDims = vsShaderDesc->texDims;
         mVsOutConfig = vsShaderDesc->vsOutConfig;
         mVsOutIds = vsShaderDesc->vsOutIds;
         mIsScreenSpace = vsShaderDesc->isScreenSpace;
      } else if (shaderDesc->type == gpu::ShaderType::Geometry) {
         auto gsShaderDesc = reinterpret_cast<gpu::GeometryShaderDesc *>(shaderDesc);
         mTexDims = gsShaderDesc->texDims;
      } else if (shaderDesc->type == gpu::ShaderType::Pixel) {
         auto psShaderDesc = reinterpret_cast<gpu::PixelShaderDesc *>(shaderDesc);
         mTexDims = psShaderDesc->texDims;
         mPsInControl0 = psShaderDesc->psInControl0;
         mPsInControl1 = psShaderDesc->psInControl1;
         mPsInputCntls = psShaderDesc->psInputCntls;
         mPsNumVsExportPos = psShaderDesc->vsNumPosExports;
         mPsNumVsExportParam = psShaderDesc->vsNumParamExports;
         mPsVsExportsMap = psShaderDesc->vsExportMap;
		 mAlphaRefFunc = psShaderDesc->alphaRefFunc;
      } else {
         decaf_abort("Unexpected shader type during translation");
      }
   }

   fmt::MemoryWriter mOut;
   std::string mIndent;
   std::vector<std::string> mPostGroupOut;
   AluPredMode mCfPredMode;

   bool mIsScreenSpace;
   std::array<latte::SQ_VTX_SEMANTIC_N, 32> mVtxSemantics;
   std::array<latte::SQ_TEX_DIM, latte::MaxTextures> mTexDims;
   latte::SPI_VS_OUT_CONFIG mVsOutConfig;
   std::array<latte::SPI_VS_OUT_ID_N, 10> mVsOutIds;
   std::array<uint32_t, 2> mVsInstanceStepRates;
   latte::SPI_PS_IN_CONTROL_0 mPsInControl0;
   latte::SPI_PS_IN_CONTROL_1 mPsInControl1;
   std::array<latte::SPI_PS_INPUT_CNTL_N, 32> mPsInputCntls;
   std::map<uint32_t, uint32_t> mPsVsExportsMap;
   uint32_t mPsNumVsExportPos = 0;
   uint32_t mPsNumVsExportParam = 0;
   latte::REF_FUNC mAlphaRefFunc;

};

} // namespace gpu