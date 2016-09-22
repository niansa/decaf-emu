#include "gpu/gpu_shadertranspiler.h"
#include "hlsl2_genhelpers.h"
#include "hlsl2_translate.h"

using namespace latte;

namespace hlsl2
{

void Transpiler::insertDestAssignStmt(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst, const std::string &source, bool forAr)
{
   auto stmt = source;

   if (inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2) {
      switch (inst.op2.OMOD()) {
      case SQ_ALU_OMOD::D2:
         stmt = fmt::format("({}) / 2", stmt);
         break;
      case SQ_ALU_OMOD::M2:
         stmt = fmt::format("({}) * 2", stmt);
         break;
      case SQ_ALU_OMOD::M4:
         stmt = fmt::format("({}) * 4", stmt);
         break;
      case SQ_ALU_OMOD::OFF:
         // Nothing to do
         break;
      default:
         decaf_abort("Unexpected dest var OMOD");
      }
   }

   if (inst.word1.CLAMP()) {
      stmt = fmt::format("clamp({}, 0, 1)", stmt);
   }

   if (forAr) {
      // Instruction is responsible for dispatching the result as a
      //  UINT automatically, so we can safely directly store it
      //  as AR is typed as UINT in the shader.
   } else {
      // Instruction returns the value in whatever type is intended
      //  by the instruction.  We use meta-data to translate that
      //  type to the float type used for storage in shader.
      auto flags = getInstructionFlags(inst);
      if (flags & SQ_ALU_FLAG_INT_OUT) {
         stmt = fmt::format("asfloat({})", stmt);
      } else if (flags & SQ_ALU_FLAG_UINT_OUT) {
         stmt = fmt::format("asfloat({})", stmt);
      }
   }

   auto target = forAr ? genARChanStr(unit) : genPVChanStr(unit);

   insertLineStart();
   mOut.write("{} = {};", target, stmt);
   insertLineEnd();

   if (inst.word1.ENCODING() != SQ_ALU_ENCODING::OP2 || inst.op2.WRITE_MASK()) {
      GprChanRef destGpr;
      destGpr.gpr = makeGprRef(inst.word1.DST_GPR(), inst.word1.DST_REL(), inst.word0.INDEX_MODE());
      destGpr.chan = inst.word1.DST_CHAN();

      mPostGroupOut.push_back(fmt::format("{}.{} = {};",
         genGprRefStr(destGpr.gpr), genChanStr(destGpr.chan), target));
   }
}

void Transpiler::insertPredAssignStmt(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst, const std::string &source)
{
   decaf_check(inst.word1.ENCODING() == SQ_ALU_ENCODING::OP2);

   if (inst.op2.UPDATE_PRED()) {
      insertLineStart();
      mOut.write("predicateRegister = {};", source);
      insertLineEnd();
   }

   if (inst.op2.UPDATE_EXECUTE_MASK()) {
      insertLineStart();
      if (mCfPredMode == AluPredMode::Branch) {
         mOut.write("pixelState = ({} ? Active : InactiveBranch);", source);
      } else if (mCfPredMode == AluPredMode::Break) {
         mOut.write("pixelState = ({} ? Active : InactiveBreak);", source);
      } else if (mCfPredMode == AluPredMode::Continue) {
         mOut.write("pixelState = ({} ? Active : InactiveContinue);", source);
      } else {
         decaf_abort("Unexpected CF predicate mode");
      }
      insertLineEnd();
   }
}

void Transpiler::insertAssignStmt(const std::string &destVar, const std::string &srcVar, std::array<SQ_SEL, 4> srcMask)
{
   std::array<SQ_CHAN, 4> dstMask;
   uint32_t numSwizzle = condenseSwizzleMask(srcMask, dstMask, 4);

   // Trying to insert an assignment for a fully masked swizzle is an error.
   decaf_check(numSwizzle);

   auto dstSwizStr = genSwizzleStr(dstMask, numSwizzle);

   std::array<SQ_CHAN, 4> simpleSwiz = { SQ_CHAN::X };
   bool isSimple = simplifySwizzle(simpleSwiz, srcMask, numSwizzle);

   std::string srcStmt;

   if (isSimple) {
      auto srcSwizStr = genSwizzleStr(simpleSwiz, numSwizzle);
      srcStmt = fmt::format("{}.{}", srcVar, srcSwizStr);
   } else {
      std::array<std::string, 4> srcStmts;

      for (auto i = 0u; i < numSwizzle; ++i) {
         switch (srcMask[i]) {
         case SQ_SEL::SEL_0:
            srcStmts[i] = "0";
            break;
         case SQ_SEL::SEL_1:
            srcStmts[i] = "1";
            break;
         case SQ_SEL::SEL_X:
            srcStmts[i] = fmt::format("{}.x", srcVar);
            break;
         case SQ_SEL::SEL_Y:
            srcStmts[i] = fmt::format("{}.y", srcVar);
            break;
         case SQ_SEL::SEL_Z:
            srcStmts[i] = fmt::format("{}.z", srcVar);
            break;
         case SQ_SEL::SEL_W:
            srcStmts[i] = fmt::format("{}.w", srcVar);
            break;
         default:
            decaf_abort("Unexpected source selector");
         }
      }

      if (numSwizzle == 1) {
         srcStmt = srcStmts[0];
      } else if (numSwizzle == 2) {
         srcStmt = fmt::format("float2({}, {})", srcStmts[0], srcStmts[1]);
      } else if (numSwizzle == 3) {
         srcStmt = fmt::format("float3({}, {}, {})", srcStmts[0], srcStmts[1], srcStmts[2]);
      } else if (numSwizzle == 4) {
         srcStmt = fmt::format("float4({}, {}, {}, {})", srcStmts[0], srcStmts[1], srcStmts[2], srcStmts[3]);
      } else {
         decaf_abort("Unexpected swizzle count");
      }
   }

   insertLineStart();
   mOut.write("{}.{} = {};", destVar, dstSwizStr, srcStmt);
   insertLineEnd();
}

void Transpiler::recordDataUsage(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // We keep track of which buffer indexes are referenced here.  This allows us to do
   //  some optimizations around which buffers we upload, and put in the shader code.
   auto numInstSrc = getInstructionNumSrcs(inst);
   for (auto i = 0u; i < numInstSrc; ++i) {
      auto src = latte::makeSrcVar(cf, group, inst, i);
      if (src.type == SrcVarRef::Type::CBUFFER) {
         auto &cbuffer = src.cbufferChan.cbuffer;
         if (cbuffer.indexMode == CbufferIndexMode::None) {
            mCbufferNumUsed[cbuffer.bufferId] = std::max(mCbufferNumUsed[cbuffer.bufferId], cbuffer.index + 1);
         } else {
            mCbufferNumUsed[cbuffer.bufferId] = latte::MaxUniformBlockSize / 4 / sizeof(float);
         }
      } else if (src.type == SrcVarRef::Type::CFILE) {
         auto &cfile = src.cfileChan.cfile;
         if (cfile.indexMode == CfileIndexMode::None) {
            mCfileNumUsed = std::max(mCfileNumUsed, cfile.index + 1);
         } else {
            mCfileNumUsed = latte::MaxUniformRegisters;
         }
      }
   }
}

void Transpiler::translateAluInst(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   bool isReducInst = (unit == 0xffffffff);

   if (!isReducInst) {
      recordDataUsage(cf, group, unit, inst);

      decaf_check(!inst.word0.PRED_SEL());
   } else {
      recordDataUsage(cf, group, SQ_CHAN::X, *group.units[SQ_CHAN::X]);
      recordDataUsage(cf, group, SQ_CHAN::Y, *group.units[SQ_CHAN::Y]);
      recordDataUsage(cf, group, SQ_CHAN::Z, *group.units[SQ_CHAN::Z]);
      recordDataUsage(cf, group, SQ_CHAN::W, *group.units[SQ_CHAN::W]);

      decaf_check(!group.units[SQ_CHAN::X]->word0.PRED_SEL());
      decaf_check(!group.units[SQ_CHAN::Y]->word0.PRED_SEL());
      decaf_check(!group.units[SQ_CHAN::Z]->word0.PRED_SEL());
      decaf_check(!group.units[SQ_CHAN::W]->word0.PRED_SEL());
   }

   /*
   insertLineStart();
   mOut << "if (activePredicate) {";
   insertLineEnd();
   increaseIndent();
   */

   CLikeShaderTranspiler::translateAluInst(cf, group, unit, inst);

   /*
   decreaseIndent();
   insertLineStart();
   mOut << "}";
   insertLineEnd();
   */
}

void Transpiler::translateAluGroup(const ControlFlowInst &cf, const AluInstructionGroup &group)
{
   // We deal with aliasing by reading from PSo and writing to PS, then we
   //  copy the values over after the end of every ALU group.  We copy at the
   //  top of every block to make newline handling with CLike easier.
   mPostGroupOut.push_back("PVo = PV; PSo = PS;");

   CLikeShaderTranspiler::translateAluGroup(cf, group);
}

void Transpiler::translateCfAluGeneric(const ControlFlowInst &cf, AluPredMode predMode)
{
   insertLineStart();
   mOut.write("if (pixelState == Active) {{");
   insertLineEnd();
   increaseIndent();

   CLikeShaderTranspiler::translateCfAluGeneric(cf, predMode);

   decreaseIndent();
   insertLineStart();
   mOut.write("}}");
   insertLineEnd();
}

void Transpiler::translateCfNormalInst(const ControlFlowInst& cf)
{
   // cf.word1.BARRIER(); - No need to handle coherency in HLSL
   // cf.word1.WHOLE_QUAD_MODE(); - No need to handle optimization
   // cf.word1.VALID_PIXEL_MODE(); - No need to handle optimization

   CLikeShaderTranspiler::translateCfNormalInst(cf);
}
void Transpiler::translateCfExportInst(const ControlFlowInst& cf)
{
   // cf.word1.BARRIER(); - No need to handle coherency in HLSL
   // cf.word1.WHOLE_QUAD_MODE(); - No need to handle optimization
   // cf.word1.VALID_PIXEL_MODE(); - No need to handle optimization

   CLikeShaderTranspiler::translateCfExportInst(cf);
}
void Transpiler::translateCfAluInst(const ControlFlowInst& cf)
{
   // cf.word1.BARRIER(); - No need to handle coherency in HLSL
   // cf.word1.WHOLE_QUAD_MODE(); - No need to handle optimization

   CLikeShaderTranspiler::translateCfAluInst(cf);
}

void Transpiler::translate()
{
   CLikeShaderTranspiler::translate();
}

bool Transpiler::translate(gpu::ShaderDesc *shaderDesc, Shader *shader)
{
   fmt::MemoryWriter out;

   Transpiler state, fsState, dcState;

   if (shaderDesc->type == gpu::ShaderType::Vertex) {
      state.mType = Transpiler::Type::Vertex;
   } else if (shaderDesc->type == gpu::ShaderType::Geometry) {
      state.mType = Transpiler::Type::Geometry;
   } else if (shaderDesc->type == gpu::ShaderType::Pixel) {
      state.mType = Transpiler::Type::Pixel;
   } else {
      decaf_abort("Unexpected shader type");
   }

   state.applyShaderDesc(shaderDesc);

   state.mIndent.append(IndentSize, ' ');
   state.translate();

   if (state.mCallsFs) {
      decaf_check(shaderDesc->type == gpu::ShaderType::Vertex);
      auto vsShader = reinterpret_cast<gpu::VertexShaderDesc *>(shaderDesc);

      // Apply the vs shader description
      fsState.mType = Transpiler::Type::Fetch;
      fsState.applyShaderDesc(shaderDesc);

      // Modify the shader to use the fetch shader data instead
      fsState.mBinary = vsShader->fsBinary;
      fsState.mIsFunction = true;

      fsState.mIndent.append(IndentSize, ' ');
      fsState.translate();

      // Due to the restrictions on calling of fetch shaders, we are unable
      //  to allow input data from the main program.  See the elaborated
      //  explanation in the CALL_FS method for more details...
      decaf_check(state.mInputData.size() == 0);
      std::swap(state.mInputData, fsState.mInputData);
   }


   // Some global macros we need
   out << "#define bswap8in16(v) (((v & 0xFF00FF00) >> 8) | ((v & 0x00FF00FF) << 8))\n";
   out << "#define bswap8in32(v) (((v & 0xFF000000) >> 24) | ((v & 0x00FF0000) >> 8) | ((v & 0x0000FF00) << 8) | ((v & 0x000000FF) << 24))\n";
   out << "\n";

   out << "#define PUSH(stack, stackIndex, state) stack[stackIndex++] = state\n";
   out << "#define POP(stack, stackIndex, state) state = stack[--stackIndex]\n";
   out << "#define Active 0\n";
   out << "#define InactiveBranch 1\n";
   out << "#define InactiveBreak 2\n";
   out << "#define InactiveContinue 3\n";
   out << "\n";

   // Uniform Buffers
   for (auto i = 0; i < 16; ++i) {
      auto cbufferNumUsed = state.mCbufferNumUsed[i];
      cbufferNumUsed = std::max(cbufferNumUsed, fsState.mCbufferNumUsed[i]);
      cbufferNumUsed = std::max(cbufferNumUsed, dcState.mCbufferNumUsed[i]);

      if (cbufferNumUsed > 0) {
         out << "cbuffer cb" << i << " : register(b" << i << ") {\n";
         out << "  float4 CBUF" << i << "[" << cbufferNumUsed << "];\n";
         out << "}\n";
      }
   }

   // Uniform Registers
   {
      auto cfileNumUsed = state.mCfileNumUsed;
      cfileNumUsed = std::max(cfileNumUsed, fsState.mCfileNumUsed);
      cfileNumUsed = std::max(cfileNumUsed, dcState.mCfileNumUsed);

      if (cfileNumUsed > 0) {
         out << "cbuffer cb0 : register(b0) {\n";
         out << "  float4 CFILE[" << cfileNumUsed << "];\n";
         out << "}\n";
      }
   }

   // Newline after all input uniforms
   out << "\n";


   // Textures
   {
      for (auto i = 0u; i < latte::MaxTextures; ++i) {
         auto sampleState = state.mSamplerState[i];
         if (sampleState == SampleMode::Unused) {
            // We can just ignore this
         } else if (sampleState == SampleMode::Normal || sampleState == SampleMode::Compare) {
            switch (state.mTexDims[i]) {
            case latte::SQ_TEX_DIM::DIM_1D:
               out.write("Texture1D texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_2D:
               out.write("Texture2D texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_3D:
               out.write("Texture3D texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_CUBEMAP:
               out.write("TextureCube texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_1D_ARRAY:
               out.write("Texture1DArray texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_2D_ARRAY:
               out.write("Texture2DArray texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_2D_MSAA:
               out.write("Texture2DMS texture{} : register(t{});\n", i, i);
               break;
            case latte::SQ_TEX_DIM::DIM_2D_ARRAY_MSAA:
               out.write("Texture2DMSArray texture{} : register(t{});\n", i, i);
               break;
            default:
               decaf_abort("Unexpected texture dim type");
            }
         } else {
            decaf_abort("Unexpected sampler state");
         }
      }

      out << "\n";
   }

   // Samplers
   {
      for (auto i = 0u; i < latte::MaxSamplers; ++i) {
         auto sampleState = state.mSamplerState[i];
         if (sampleState == SampleMode::Unused) {
            // We can just ignore this
         } else if (sampleState == SampleMode::Normal) {
            out.write("SamplerState sampler{} : register(s{});\n", i, i);
         } else if (sampleState == SampleMode::Compare) {
            out.write("SamplerComparisonState sampler{} : register(s{});\n", i, i);
         } else {
            decaf_abort("Unexpected sampler state");
         }
      }
      out << "\n";
   }


   auto funcVars = []() {
      fmt::MemoryWriter vout;
      vout << "  float4 tmp;\n";
      vout << "  uint4 utmp;\n";
      vout << "  bool btmp;\n";
      vout << "  uint4 AR;\n";
      vout << "  uint AL;\n";
      vout << "  float4 PV, PVo;\n";
      vout << "  float PS, PSo;\n";
      vout << "  bool predicateRegister;\n";
      vout << "  uint pixelState = Active;\n";
      vout << "  uint stackIndex = 0;\n";
      vout << "  uint stack[16];\n";
      vout << "\n";
      return vout.str();
   };

   out << "cbuffer cbInfo : register(b13) {\n";
   out << "  float2 screenMul;\n";
   out << "  float alphaRef;\n";
   out << "}\n";

   if (shaderDesc->type == gpu::ShaderType::Vertex) {
      // Input Data
      out << "struct VSInput {\n";

      for (auto i = 0u; i < state.mInputData.size(); ++i) {
         auto &elem = state.mInputData[i];

         std::string inputElemType;
         if (elem.elemCount == 1) {
            inputElemType = "uint";
         } else if (elem.elemCount == 2) {
            inputElemType = "uint2";
         } else if (elem.elemCount == 3) {
            inputElemType = "uint3";
         } else if (elem.elemCount == 4) {
            inputElemType = "uint4";
         } else {
            decaf_abort("Unexpected element count in input data");
         }

         out.write("  {} data{} : TEXCOORD{};\n", inputElemType, i, i);
      }

      out << "};\n";
      out << "\n";


      // Output Data
      out << "struct VSOutput {\n";

      for (auto i = 0u; i < state.mNumExportPos; ++i) {
         if (i == 0) {
            out.write("  float4 pos0 : SV_POSITION;\n");
         } else {
            out.write("  float4 pos{} : POSITION{};\n", i, i);
         }
      }

      // We do not currently support exporting fog
      decaf_check(!state.mVsOutConfig.VS_EXPORTS_FOG());

      if (state.mNumExportParam > 0) {
         // Theoretically this assertion should hold... Who knows though :/
         decaf_check(state.mNumExportParam == state.mVsOutConfig.VS_EXPORT_COUNT() + 1);

         // We only support per-vector currently
         decaf_check(!state.mVsOutConfig.VS_PER_COMPONENT());

         for (auto i = 0u; i < state.mNumExportParam; ++i) {
            out.write("  float4 param{} : TEXCOORD{};\n", i, i);

            uint32_t semanticId = 0xffffffff;

            if ((i & 3) == 0) {
               semanticId = state.mVsOutIds[i >> 2].SEMANTIC_0();
            } else if ((i & 3) == 1) {
               semanticId = state.mVsOutIds[i >> 2].SEMANTIC_1();
            } else if ((i & 3) == 2) {
               semanticId = state.mVsOutIds[i >> 2].SEMANTIC_2();
            } else if ((i & 3) == 3) {
               semanticId = state.mVsOutIds[i >> 2].SEMANTIC_3();
            }

            // This is a temporary check.  Its possible that 0xFF is a valid value
            //  meaning not to export a value, but it seems improbable since the
            //  output semantics are tied to the shader itself.  So....
            decaf_check(semanticId < 0xff);

            state.mVsExportsMap[semanticId] = i;
         }
      }

      out << "};\n";
      out << "\n";

      // Add the FS code if that is relevant.
      if (state.mCallsFs) {
         out << "void FSMain(VSInput input, inout float4 R[128]) {\n";
         out << funcVars();
         out << fsState.mOut.str();
         out << "}\n";
         out << "\n";
      }

      out << "VSOutput VSMain(VSInput input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {\n";
      out << "  VSOutput output;\n";
      out << "  float4 R[128];\n";
      out << funcVars();
      out << "  R[0] = float4(asfloat(vertexId), asfloat(instanceId), 0, 0);\n";
      out << "\n";
      out << state.mOut.str();
      out << "  return output;\n";
      out << "}\n";
      out << "\n";

      VertexShader *vsShader = reinterpret_cast<VertexShader*>(shader);

      vsShader->code = out.str();
      vsShader->callsFs = state.mCallsFs;
      vsShader->cfileUsed = state.mCfileNumUsed;
      vsShader->cbufferUsed = state.mCbufferNumUsed;
      vsShader->samplerState = state.mSamplerState;
      vsShader->textureState = state.mTextureState;
      vsShader->inputData = state.mInputData;
      vsShader->numPosExports = state.mNumExportPos;
      vsShader->numParamExports = state.mNumExportParam;
      vsShader->exportMap = state.mVsExportsMap;
   } else if (shaderDesc->type == gpu::ShaderType::Pixel) {
      std::vector<std::string> inputGprAssigns;

      // Input Data
      out << "struct PSInput {\n";

      // We don't currently support baryocentric sampling
      decaf_check(!state.mPsInControl0.BARYC_AT_SAMPLE_ENA());

      // These simply control feature enablement for the interpolation
      //  of the PS inputs down below:
      // state.mPsInControl0.BARYC_SAMPLE_CNTL()
      // state.mPsInControl0.LINEAR_GRADIENT_ENA()
      // state.mPsInControl0.PERSP_GRADIENT_ENA()

      // We do not currently support importing the position...
      decaf_check(!state.mPsInControl0.POSITION_ENA());
      //state.mPsInControl0.POSITION_ADDR();
      //state.mPsInControl0.POSITION_CENTROID();
      //state.mPsInControl0.POSITION_SAMPLE();

      // We do not currently support fixed point positions
      decaf_check(!state.mPsInControl1.FIXED_PT_POSITION_ENA());
      //state.mPsInControl1.FIXED_PT_POSITION_ADDR();

      // I don't even know how to handle this being off!?
      //state.mPsInControl1.FOG_ADDR();

      // Not actually sure what this is, but lets make sure its off...
      decaf_check(!state.mPsInControl1.FRONT_FACE_ENA());
      //state.mPsInControl1.FRONT_FACE_ADDR();
      //state.mPsInControl1.FRONT_FACE_ALL_BITS();
      //state.mPsInControl1.FRONT_FACE_CHAN();

      // We do not currently support pixel indexing
      decaf_check(!state.mPsInControl1.GEN_INDEX_PIX());
      //state.mPsInControl1.GEN_INDEX_PIX_ADDR();

      // I don't believe this is used on our GPU...
      //state.mPsInControl1.POSITION_ULC();

      for (auto i = 0u; i < state.mPsNumVsExportPos; ++i) {
         if (i == 0) {
            out.write("  float4 pos0 : SV_POSITION;\n");
         } else {
            out.write("  float4 pos{} : POSITION{};\n", i, i);
         }
      }

      std::map<uint32_t, latte::SPI_PS_INPUT_CNTL_N*> inputCntlMap;

      auto numInputs = state.mPsInControl0.NUM_INTERP();
      for (auto i = 0u; i < numInputs; ++i) {
         auto &inputCntl = state.mPsInputCntls[i];

         auto exportIter = state.mPsVsExportsMap.find(inputCntl.SEMANTIC());
         if (exportIter != state.mPsVsExportsMap.end()) {
            auto texCoordIdx = exportIter->second;

            // We first verify that the shader is not duplicating it's input
            //  semantics.  HLSL shaders do not allow this.
            decaf_check(!inputCntlMap[texCoordIdx]);

            inputCntlMap[texCoordIdx] = &inputCntl;
         }
      }

      for (auto i = 0u; i < state.mPsNumVsExportParam; ++i) {
         auto inputCntlPtr = inputCntlMap[i];
         if (inputCntlPtr) {
            auto &inputCntl = *inputCntlPtr;

            decaf_check(!inputCntl.CYL_WRAP());
            decaf_check(!inputCntl.PT_SPRITE_TEX());

            std::string modifiers;

            if (inputCntl.FLAT_SHADE()) {
               decaf_check(!inputCntl.SEL_LINEAR());
               decaf_check(!inputCntl.SEL_SAMPLE());

               // Using centroid qualifier with flat shading doesn't make sense
               decaf_check(!inputCntl.SEL_CENTROID());

               modifiers = "nointerpolation";
            } else if (inputCntl.SEL_LINEAR()) {
               decaf_check(!inputCntl.FLAT_SHADE());
               decaf_check(!inputCntl.SEL_SAMPLE());

               if (inputCntl.SEL_CENTROID()) {
                  modifiers = "noperspective centroid";
               } else {
                  modifiers = "noperspective";
               }
            } else if (inputCntl.SEL_SAMPLE()) {
               decaf_check(!inputCntl.FLAT_SHADE());
               decaf_check(!inputCntl.SEL_LINEAR());

               // Using centroid qualifier with sample shading doesn't make sense
               decaf_check(!inputCntl.SEL_CENTROID());

               modifiers = "sample";
            } else {
               if (inputCntl.SEL_CENTROID()) {
                  modifiers = "linear centroid";
               } else {
                  modifiers = "linear";
               }
            }

            out.write("  {} float4 param{} : TEXCOORD{};\n", modifiers, i, i);
         } else {
            out.write("  float4 param{} : TEXCOORD{};\n", i, i);
         }
      }

      for (auto i = 0u; i < numInputs; ++i) {
         auto inputCntl = state.mPsInputCntls[i];

         auto exportIter = state.mPsVsExportsMap.find(inputCntl.SEMANTIC());
         if (exportIter != state.mPsVsExportsMap.end()) {
            auto texCoordIdx = exportIter->second;

            inputGprAssigns.push_back(fmt::format("  R[{}] = input.param{};", i, texCoordIdx));
         } else {
            switch (inputCntl.DEFAULT_VAL()) {
            case 0:
               inputGprAssigns.push_back(fmt::format("  R[{}] = float4(0, 0, 0, 0);", i));
               break;
            case 1:
               inputGprAssigns.push_back(fmt::format("  R[{}] = float4(0, 0, 0, 1);", i));
               break;
            case 2:
               inputGprAssigns.push_back(fmt::format("  R[{}] = float4(1, 1, 1, 0);", i));
               break;
            case 3:
               inputGprAssigns.push_back(fmt::format("  R[{}] = float4(1, 1, 1, 1);", i));
               break;
            default:
               decaf_abort("Unexpected PS input semantic default");
            }
         }
      }

      out << "};\n";
      out << "\n";


      // Output Data
      out << "struct PSOutput {\n";

      for (auto i = 0u; i < state.mNumExportPix; ++i) {
         out << "  float4 color" << i << " : SV_TARGET" << i << ";\n";
      }

      out << "};\n";
      out << "\n";

      out << "PSOutput PSMain(PSInput input) {\n";
      out << "  PSOutput output;\n";
      out << "  float4 R[128];\n";
      out << funcVars();

      for (auto &inputAssign : inputGprAssigns) {
         out << inputAssign << "\n";
      }
      out << "\n";

      out << state.mOut.str();

	  if (state.mAlphaRefFunc != latte::REF_FUNC::ALWAYS) {
		  if (state.mAlphaRefFunc == latte::REF_FUNC::NEVER) {
			  out.write("  // ALPHA_REF::NEVER\n");
			  out.write("  discard;\n");
		  } else {
			  for (auto i = 0u; i < state.mNumExportPix; ++i) {
				  switch (state.mAlphaRefFunc) {
				  case latte::REF_FUNC::LESS:
					  out.write("  // ALPHA_REF::LESS\n");
					  out.write("  if(!(output.color{}.w < alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::EQUAL:
					  out.write("  // ALPHA_REF::EQUAL\n");
					  out.write("  if(!(output.color{}.w == alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::LESS_EQUAL:
					  out.write("  // ALPHA_REF::LESS_EQUAL\n");
					  out.write("  if(!(output.color{}.w <= alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::GREATER:
					  out.write("  // ALPHA_REF::GREATER\n");
					  out.write("  if(!(output.color{}.w > alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::NOT_EQUAL:
					  out.write("  // ALPHA_REF::NOT_EQUAL\n");
					  out.write("  if(!(output.color{}.w != alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::GREATER_EQUAL:
					  out.write("  // ALPHA_REF::GREATER_EQUAL\n");
					  out.write("  if(!(output.color{}.w >= alphaRef)) discard;\n", i);
					  break;
				  case latte::REF_FUNC::NEVER:
				  case latte::REF_FUNC::ALWAYS:
					  // should be filtered...
				  default:
					  decaf_abort("Unexpected pixel shader alpha ref func");
				  }
			  }
		  }

		  out << "\n";
	  }

      out << "  return output;\n";
      out << "}\n";
      out << "\n";

      auto psShader = reinterpret_cast<PixelShader *>(shader);

      psShader->code = out.str();
      psShader->cfileUsed = state.mCfileNumUsed;
      psShader->cbufferUsed = state.mCbufferNumUsed;
      psShader->samplerState = state.mSamplerState;
      psShader->textureState = state.mTextureState;
   } else {
      decaf_abort("Attempted to generate shader of unsupported type");
   }

   return true;
}

bool translate(gpu::ShaderDesc *shaderDesc, Shader *shader)
{
   return Transpiler::translate(shaderDesc, shader);
}

gpu::VertexShaderDesc cleanDesc(const gpu::VertexShaderDesc &desc, const VertexShader &shader)
{
   auto out = desc;

   if (!shader.callsFs) {
      out.fsBinary = gsl::span<const uint8_t>(nullptr, 0);
   }

   // TODO: Implement cleaning for the rest of vertex shader descriptions.

   return out;
}

gpu::GeometryShaderDesc cleanDesc(const gpu::GeometryShaderDesc &desc, const GeometryShader &shader)
{
   // TODO: Implement cleaning of Geometry Shader descriptions.
   return desc;
}

gpu::PixelShaderDesc cleanDesc(const gpu::PixelShaderDesc &desc, const PixelShader &shader)
{
   // TODO: Implement cleaning of Pixel Shader descriptions.
   return desc;
}

} // namespace hlsl2