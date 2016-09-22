#include "hlsl2_translate.h"
#include "hlsl2_genhelpers.h"

namespace hlsl2
{

void Transpiler::translateAluOp3_CNDE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} == 0) ? {} : {}", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_CNDGT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} > 0) ? {} : {}", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_CNDGE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} >= 0) ? {} : {}", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_CNDE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp3_CNDE(cf, group, unit, inst);
}

void Transpiler::translateAluOp3_CNDGT_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp3_CNDGT(cf, group, unit, inst);
}

void Transpiler::translateAluOp3_CNDGE_INT(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp3_CNDGE(cf, group, unit, inst);
}

void Transpiler::translateAluOp3_MULADD(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("{} * {} + {}", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_MULADD_M2(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} * {} + {}) * 2", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_MULADD_M4(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} * {} + {}) * 4", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

void Transpiler::translateAluOp3_MULADD_D2(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto src0 = genSrcVarStr(cf, group, inst, 0);
   auto src1 = genSrcVarStr(cf, group, inst, 1);
   auto src2 = genSrcVarStr(cf, group, inst, 2);

   auto output = fmt::format("({} * {} + {}) / 2", src0, src1, src2);
   insertDestAssignStmt(cf, group, unit, inst, output);
}

} // namespace hlsl2