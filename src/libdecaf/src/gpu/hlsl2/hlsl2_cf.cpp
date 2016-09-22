#include "hlsl2_translate.h"

namespace hlsl2
{

void Transpiler::translateCf_CALL_FS(const ControlFlowInst &cf)
{
   // We only allow CALL_FS as the very first CF instruction in a vertex shader.
   //  Take a look at translateVtx_SEMANTIC for more details on why this is.
   decaf_check(mCfPC == 0);

   insertLineStart();
   mOut.write("FSMain(input, R);");
   insertLineEnd();
}

void Transpiler::translateCf_ELSE(const ControlFlowInst &cf)
{
   translateCfElseGeneric();
}

void Transpiler::translateCf_JUMP(const ControlFlowInst &cf)
{
   // This is actually only an optimization so we can ignore it.
}

void Transpiler::translateCf_KILL(const ControlFlowInst &cf)
{
   insertLineStart();
   mOut.write("discard;");
   insertLineEnd();
}

void Transpiler::translateCf_NOP(const ControlFlowInst &cf)
{
   // Who knows why they waste space with explicitly encoded NOP's...
}

void Transpiler::translateCf_PUSH(const ControlFlowInst &cf)
{
   auto popCount = cf.word1.POP_COUNT();
   for (auto i = 0u; i < popCount; ++i) {
      translateCfPopGeneric();
   }

   translateCfPushGeneric();
}

void Transpiler::translateCf_POP(const ControlFlowInst &cf)
{
   auto popCount = cf.word1.POP_COUNT();
   for (auto i = 0u; i < popCount; ++i) {
      translateCfPopGeneric();
   }
}

void Transpiler::translateCf_RETURN(const ControlFlowInst &cf)
{
   insertLineStart();
   mOut.write("return;");
   insertLineEnd();
}

void Transpiler::translateCf_TEX(const ControlFlowInst &cf)
{
   insertLineStart();
   mOut.write("if (pixelState == Active) {{");
   insertLineEnd();
   increaseIndent();

   CLikeShaderTranspiler::translateCf_TEX(cf);

   decreaseIndent();
   insertLineStart();
   mOut.write("}}");
   insertLineEnd();
}

void Transpiler::translateCf_VTX(const ControlFlowInst &cf)
{
   insertLineStart();
   mOut.write("if (pixelState == Active) {{");
   insertLineEnd();
   increaseIndent();

   CLikeShaderTranspiler::translateCf_VTX(cf);

   decreaseIndent();
   insertLineStart();
   mOut.write("}}");
   insertLineEnd();
}

void Transpiler::translateCf_VTX_TC(const ControlFlowInst &cf)
{
   insertLineStart();
   mOut.write("if (pixelState == Active) {{");
   insertLineEnd();
   increaseIndent();

   CLikeShaderTranspiler::translateCf_VTX_TC(cf);

   decreaseIndent();
   insertLineStart();
   mOut.write("}}");
   insertLineEnd();
}

void Transpiler::translateCfPushGeneric()
{
   insertLineStart();
   mOut.write("PUSH(stack, stackIndex, pixelState);");
   insertLineEnd();
}

void Transpiler::translateCfPopGeneric()
{
   insertLineStart();
   mOut.write("POP(stack, stackIndex, pixelState);");
   insertLineEnd();
}

void Transpiler::translateCfElseGeneric()
{
   insertLineStart();
   mOut.write("if (stack[stackIndex - 1] == Active) {{");
   insertLineEnd();
   increaseIndent();

   insertLineStart();
   mOut.write("pixelState = (pixelState == Active) ? InactiveBranch : Active;");
   insertLineEnd();

   decreaseIndent();
   insertLineStart();
   mOut.write("}}");
   insertLineEnd();
}


} // namespace hlsl2