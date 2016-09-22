#include "hlsl2_translate.h"
#include "hlsl2_genhelpers.h"

namespace hlsl2
{

void Transpiler::translateGenericExport(const ControlFlowInst &cf)
{
   GprMaskRef srcGpr;
   srcGpr.gpr = makeGprRef(cf.exp.word0.RW_GPR(), cf.exp.word0.RW_REL(), SQ_INDEX_MODE::LOOP);
   srcGpr.mask[SQ_CHAN::X] = cf.exp.swiz.SRC_SEL_X();
   srcGpr.mask[SQ_CHAN::Y] = cf.exp.swiz.SRC_SEL_Y();
   srcGpr.mask[SQ_CHAN::Z] = cf.exp.swiz.SRC_SEL_Z();
   srcGpr.mask[SQ_CHAN::W] = cf.exp.swiz.SRC_SEL_W();

   if (isSwizzleFullyMasked(srcGpr.mask)) {
      // We should just skip fully masked swizzles.
      return;
   }

   auto exportRef = makeExportRef(cf.exp.word0.TYPE(), cf.exp.word0.ARRAY_BASE());

   auto sourceVar = genGprRefStr(srcGpr.gpr);

   auto exportCount = cf.exp.word1.BURST_COUNT() + 1;
   for (auto i = 0u; i < exportCount; ++i) {
      // Generate the string export number, and record the usage
      std::string outputVar;

      if (exportRef.type == ExportRef::Type::Position) {
         decaf_check(mType == ShaderParser::Type::Vertex || mType == ShaderParser::Type::DataCache);
         decaf_check(exportRef.index < 4);

         mNumExportPos = std::max(mNumExportPos, exportRef.index + 1);

         outputVar = fmt::format("output.pos{}", exportRef.index);

         if (mIsScreenSpace) {
            sourceVar = fmt::format("({} * float4(screenMul.xy, 1, 1) - float4(1,1,1,1))", sourceVar);
         }
      } else if (exportRef.type == ExportRef::Type::Param) {
         decaf_check(mType == ShaderParser::Type::Vertex || mType == ShaderParser::Type::DataCache);
         decaf_check(exportRef.index < 32);

         mNumExportParam = std::max(mNumExportParam, exportRef.index + 1);

         outputVar = fmt::format("output.param{}", exportRef.index);
      } else if (exportRef.type == ExportRef::Type::Pixel) {
         decaf_check(mType == ShaderParser::Type::Pixel);
         decaf_check(exportRef.index < 8);

         mNumExportPix = std::max(mNumExportPix, exportRef.index + 1);

         outputVar = fmt::format("output.color{}", exportRef.index);
      } else {
         decaf_abort("Encountered unexpected export type");
      }

      // Write the assignment statement
      insertAssignStmt(outputVar, sourceVar, srcGpr.mask);

      // Increase the GPR read number for each export
      srcGpr.gpr.number++;

      // Increase the export output index
      exportRef.index++;
   }
}

void Transpiler::translateCf_EXP(const ControlFlowInst &cf)
{
   translateGenericExport(cf);
}

void Transpiler::translateCf_EXP_DONE(const ControlFlowInst &cf)
{
   translateGenericExport(cf);
}

} // namespace hlsl2