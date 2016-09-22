#include "hlsl2_translate.h"
#include "hlsl2_genhelpers.h"

namespace hlsl2
{

void Transpiler::translateAluOp2_CUBE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   // TODO: Implement the relation semantics between CUBE and SAMPLE for CUBEMAP sampling...
   translateAluOp2_MOV(cf, group, SQ_CHAN::X, *group.units[SQ_CHAN::X]);
   translateAluOp2_MOV(cf, group, SQ_CHAN::Y, *group.units[SQ_CHAN::Y]);
   translateAluOp2_MOV(cf, group, SQ_CHAN::Z, *group.units[SQ_CHAN::Z]);
   translateAluOp2_MOV(cf, group, SQ_CHAN::W, *group.units[SQ_CHAN::W]);
}

void Transpiler::translateAluOp2_DOT4(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   auto outputUnit = SQ_CHAN::X;

   for (auto i = 0u; i < 4u; ++i) {
      if (group.units[i]->op2.WRITE_MASK()) {
         outputUnit = static_cast<SQ_CHAN>(i);
         break;
      }
   }

   auto src0x = genSrcVarStr(cf, group, *group.units[SQ_CHAN::X], 0);
   auto src0y = genSrcVarStr(cf, group, *group.units[SQ_CHAN::Y], 0);
   auto src0z = genSrcVarStr(cf, group, *group.units[SQ_CHAN::Z], 0);
   auto src0w = genSrcVarStr(cf, group, *group.units[SQ_CHAN::W], 0);

   auto src1x = genSrcVarStr(cf, group, *group.units[SQ_CHAN::X], 1);
   auto src1y = genSrcVarStr(cf, group, *group.units[SQ_CHAN::Y], 1);
   auto src1z = genSrcVarStr(cf, group, *group.units[SQ_CHAN::Z], 1);
   auto src1w = genSrcVarStr(cf, group, *group.units[SQ_CHAN::W], 1);

   // TODO: It's plausible that this could also accept integer inputs?
   auto output = fmt::format("dot(float4({}, {}, {}, {}), float4({}, {}, {}, {}))",
      src0x, src0y, src0z, src0w, src1x, src1y, src1z, src1w);
   insertDestAssignStmt(cf, group, outputUnit, *group.units[outputUnit], output);
}

void Transpiler::translateAluOp2_DOT4_IEEE(const ControlFlowInst &cf, const AluInstructionGroup &group, SQ_CHAN unit, const AluInst &inst)
{
   translateAluOp2_DOT4(cf, group, unit, inst);
}

} // namespace hlsl2