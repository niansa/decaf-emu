#pragma once

#include "common/decaf_assert.h"
#include "gpu/microcode/latte_decoders.h"
#include "gpu/latte_registers.h"
#include <array>
#include <string>

namespace hlsl2
{

inline const std::string&
genChanStr(latte::SQ_CHAN chan)
{
   decaf_check(chan != latte::SQ_CHAN::T);
   static const std::array<std::string, 4> CHAN_NAMES = { "x", "y", "z", "w" };
   return CHAN_NAMES[chan];
}

inline std::string
genSwizzleStr(const std::array<latte::SQ_CHAN, 4> &dest, uint32_t numSwizzle)
{
   std::string swizzleStr;

   for (auto i = 0u; i < numSwizzle; ++i) {
      swizzleStr += genChanStr(dest[i]);
   }

   return swizzleStr;
}

inline std::string
genPVChanStr(latte::SQ_CHAN chan)
{
   if (chan == latte::SQ_CHAN::T) {
      return "PS";
   } else {
      return fmt::format("PV.{}", genChanStr(chan));
   }
}

inline std::string
genARChanStr(latte::SQ_CHAN chan)
{
   decaf_check(chan != latte::SQ_CHAN::T);
   return fmt::format("AR.{}", genChanStr(chan));
}

inline std::string
genGprRefStr(const latte::GprRef &gpr)
{
   switch (gpr.indexMode) {
   case latte::GprIndexMode::None:
      return fmt::format("R[{}]", gpr.number);
   case latte::GprIndexMode::AR_X:
      return fmt::format("R[{} + AR.x]", gpr.number);
   case latte::GprIndexMode::AL:
      return fmt::format("R[{} + AL]", gpr.number);
   default:
      decaf_abort("Unexpected GPR index mode");
   }
}

inline std::string
genGprSelRefStr(const latte::GprSelRef &gpr)
{
   switch (gpr.sel) {
   case latte::SQ_SEL::SEL_0:
      return "0";
   case latte::SQ_SEL::SEL_1:
      return "1";
   case latte::SQ_SEL::SEL_X:
      return fmt::format("{}.x", genGprRefStr(gpr.gpr));
   case latte::SQ_SEL::SEL_Y:
      return fmt::format("{}.y", genGprRefStr(gpr.gpr));
   case latte::SQ_SEL::SEL_Z:
      return fmt::format("{}.z", genGprRefStr(gpr.gpr));
   case latte::SQ_SEL::SEL_W:
      return fmt::format("{}.w", genGprRefStr(gpr.gpr));
   default:
      decaf_abort("Unexpected selector value for GPR reference");
   }
}

inline std::string
genCbufferRefStr(const latte::CbufferRef &cbuffer)
{
   switch (cbuffer.indexMode) {
   case latte::CbufferIndexMode::None:
      return fmt::format("CBUF{}[{}]", cbuffer.bufferId, cbuffer.index);
   case latte::CbufferIndexMode::AL:
      return fmt::format("CBUF{}[{} + AL]", cbuffer.bufferId, cbuffer.index);
   default:
      decaf_abort("Unexpected cbuffer index mode");
   }
}

inline std::string
genCfileRefStr(const latte::CfileRef &cfile)
{
   switch (cfile.indexMode) {
   case latte::CfileIndexMode::None:
      return fmt::format("CFILE[{}]", cfile.index);
   case latte::CfileIndexMode::AR_X:
      return fmt::format("CFILE[{} + AR.x]", cfile.index);
   case latte::CfileIndexMode::AR_Y:
      return fmt::format("CFILE[{} + AR.y]", cfile.index);
   case latte::CfileIndexMode::AR_Z:
      return fmt::format("CFILE[{} + AR.z]", cfile.index);
   case latte::CfileIndexMode::AR_W:
      return fmt::format("CFILE[{} + AR.w]", cfile.index);
   case latte::CfileIndexMode::AL:
      return fmt::format("CFILE[{} + AL]", cfile.index);
   default:
      decaf_abort("Unexpected cfile index mode");
   }
}

inline std::string
genSrcVarStr(const latte::ControlFlowInst &cf, const latte::AluInstructionGroup &group, const latte::AluInst &inst, uint32_t srcIndex)
{
   auto source = makeSrcVar(cf, group, inst, srcIndex);

   std::string varString;

   if (source.type == latte::SrcVarRef::Type::GPR) {
      varString = fmt::format("{}.{}",
         genGprRefStr(source.gprChan.gpr), genChanStr(source.gprChan.chan));
   } else if (source.type == latte::SrcVarRef::Type::CBUFFER) {
      varString = fmt::format("{}.{}",
         genCbufferRefStr(source.cbufferChan.cbuffer), genChanStr(source.cbufferChan.chan));
   } else if (source.type == latte::SrcVarRef::Type::CFILE) {
      varString = fmt::format("{}.{}",
         genCfileRefStr(source.cfileChan.cfile), genChanStr(source.cfileChan.chan));
   } else if (source.type == latte::SrcVarRef::Type::PREVRES) {
      switch (source.prevres.unit) {
      case latte::SQ_CHAN::X:
         varString = "PVo.x";
         break;
      case latte::SQ_CHAN::Y:
         varString = "PVo.y";
         break;
      case latte::SQ_CHAN::Z:
         varString = "PVo.z";
         break;
      case latte::SQ_CHAN::W:
         varString = "PVo.w";
         break;
      case latte::SQ_CHAN::T:
         varString = "PSo";
         break;
      default:
         decaf_abort("Unexpected source prevres var unit");
      }
   } else if (source.type == latte::SrcVarRef::Type::VALUE) {
      if (source.valueType == latte::VarRefType::FLOAT) {
         varString = fmt::format("{}", source.value.floatValue);
      } else if (source.valueType == latte::VarRefType::INT) {
         varString = fmt::format("{}", source.value.intValue);
      } else if (source.valueType == latte::VarRefType::UINT) {
         varString = fmt::format("0x{:08x}", source.value.uintValue);
      } else {
         decaf_abort("Unexpected source value type");
      }
   } else {
      decaf_abort("Unexpected source var type");
   }

   if (source.valueType == latte::VarRefType::FLOAT) {
      // We are naturally a float for HLSL conversion
   } else if (source.valueType == latte::VarRefType::INT) {
      varString = fmt::format("asint({})", varString);
   } else if (source.valueType == latte::VarRefType::UINT) {
      varString = fmt::format("asuint({})", varString);
   } else {
      decaf_abort("Unexpected source value type");
   }

   if (source.isAbsolute) {
      varString = fmt::format("abs({})", varString);
   }

   if (source.isNegated) {
      varString = fmt::format("-({})", varString);
   }

   return varString;
}

} // namespace hlsl2