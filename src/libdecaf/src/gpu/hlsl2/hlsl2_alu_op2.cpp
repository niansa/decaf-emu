#include "hlsl2_translate.h"
#include "hlsl2_genhelpers.h"

namespace hlsl2
{

void Transpiler::translateAluOp2_ADD(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} + {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_ADD_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_ADD(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_AND_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} & {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_ASHR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} >> {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_COS(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("cos({} / 0.1591549367)", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_EXP_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("exp({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_FLT_TO_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("(int){}", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_FRACT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("frac({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_INT_TO_FLT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("(float){}", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_LOG_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // TODO: Implement log clamp-to-maxval
   translateAluOp2_LOG_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_LOG_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("log({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_LSHL_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} << {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_MAX(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("max({}, {})", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_MAX_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MAX(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MAX_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MAX(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MAX_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MAX(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MIN(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("min({}, {})", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_MIN_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MIN(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MIN_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MIN(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MIN_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_MIN(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_MOV(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   insertDestAssignStmt(cf, group, unit, inst, src0);
}

void Transpiler::translateAluOp2_MOVA_FLOOR(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("clamp((int)floor({}), -256, 255)", src0);
   insertDestAssignStmt(cf, group, unit, inst, output, true);
}

void Transpiler::translateAluOp2_MUL(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} * {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_MUL_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // TODO: Figure out how to implement IEEE semantics
   translateAluOp2_MUL(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_NOT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("~{}", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_OR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} | {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_PRED_SETE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   insertLineStart();
   mOut.write("btmp = ({} == {});", src0, src1);
   insertLineEnd();

   insertPredAssignStmt(cf, group, unit, inst, "btmp");
   insertDestAssignStmt(cf, group, unit, inst, "btmp ? 1.0f : 0.0f");
}

void Transpiler::translateAluOp2_PRED_SETE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_PRED_SETGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   insertLineStart();
   mOut.write("btmp = ({} >= {});", src0, src1);
   insertLineEnd();

   insertPredAssignStmt(cf, group, unit, inst, "btmp");
   insertDestAssignStmt(cf, group, unit, inst, "btmp ? 1.0f : 0.0f");
}

void Transpiler::translateAluOp2_PRED_SETGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETGE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_PRED_SETGE_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETGE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_PRED_SETGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   insertLineStart();
   mOut.write("btmp = ({} > {});", src0, src1);
   insertLineEnd();

   insertPredAssignStmt(cf, group, unit, inst, "btmp");
   insertDestAssignStmt(cf, group, unit, inst, "btmp ? 1.0f : 0.0f");
}

void Transpiler::translateAluOp2_PRED_SETGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETGT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_PRED_SETGT_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETGT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_PRED_SETNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   insertLineStart();
   mOut.write("btmp = ({} != {});", src0, src1);
   insertLineEnd();

   insertPredAssignStmt(cf, group, unit, inst, "btmp");
   insertDestAssignStmt(cf, group, unit, inst, "btmp ? 1.0f : 0.0f");
}

void Transpiler::translateAluOp2_PRED_SETNE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_PRED_SETNE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIP_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // TODO: Implement reciprocal clamp-to-maxval
   translateAluOp2_RECIP_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIP_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("rcp({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_RECIP_FF(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // TODO: Implement reciprocal clamp-to-zero
   translateAluOp2_RECIP_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIP_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_RECIP_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIP_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_RECIP_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIPSQRT_CLAMPED(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // Implement reciprocal sqrt clamp-to-maxval
   translateAluOp2_RECIPSQRT_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RECIPSQRT_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("rsqrt({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_RECIPSQRT_FF(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // Implement reciprocal sqrt clamp-to-zero
   translateAluOp2_RECIPSQRT_IEEE(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_RNDNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("round({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} == {}) ? 1.0f : 0.0f", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETE_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} == {}) ? 0xffffffff : 0x00000000", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} >= {}) ? 1.0f : 0.0f", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETGE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETGE_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} >= {}) ? 0xffffffff : 0x00000000", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETGE_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETGE_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} > {}) ? 1.0f : 0.0f", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETGT_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETGT_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} > {}) ? 0xffffffff : 0x00000000", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETGT_UINT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETGT_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETNE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} != {}) ? 1.0f : 0.0f", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SETNE_DX10(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_SETNE_INT(cf, group, unit, inst);
}

void Transpiler::translateAluOp2_SETNE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("({} != {}) ? 0xffffffff : 0x00000000", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SIN(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("sin({} / 0.1591549367)", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SQRT_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);

   auto output = fmt::format("sqrt({})", src0);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_SUB_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} - {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp2_XOR_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);

   auto output = fmt::format("{} ^ {}", src0, src1);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

} // namespace hlsl2