#ifdef DECAF_DX12

#include "common/log.h"
#include "common/murmur3.h"
#include "dx12_driver.h"

#pragma optimize("", off)

namespace gpu
{

namespace dx12
{

bool
Driver::checkActiveUniforms()
{
   // TODO: Use appropriate shader stage output data to cull shader uploads

   if (mActivePipelineMode == PipelineMode::Registers) {
      auto uploadRegisters = [&](float *values, uint32_t numValues) {
         auto uploadSize = numValues * 4 * sizeof(float);
         auto uploadBuffer = allocateUploadBuffer(uploadSize);

         static const CD3DX12_RANGE EmptyRange = { 0, 0 };
         void *uploadData = nullptr;
         uploadBuffer->Map(0, &EmptyRange, &uploadData);

         memcpy(uploadData, values, uploadSize);

         uploadBuffer->Unmap(0, nullptr);

         return uploadBuffer;
      };

      if (mActiveVertexShader) {
         auto values = reinterpret_cast<float *>(&mRegisters[latte::Register::SQ_ALU_CONSTANT0_256 / 4]);
         auto uploadBuffer = uploadRegisters(values, 256);
         mCmdList->SetGraphicsRootConstantBufferView(0, uploadBuffer->GetGPUVirtualAddress());
      }

      if (mActivePixelShader) {
         auto values = reinterpret_cast<float *>(&mRegisters[latte::Register::SQ_ALU_CONSTANT0_0 / 4]);
         auto uploadBuffer = uploadRegisters(values, 256);
         mCmdList->SetGraphicsRootConstantBufferView(1, uploadBuffer->GetGPUVirtualAddress());
      }
   } else if (mActivePipelineMode == PipelineMode::Buffers || mActivePipelineMode == PipelineMode::Geometry) {
      if (mActiveVertexShader) {
         auto srvs = allocateSrvs(latte::MaxUniformBlocks);

         for (auto i = 0u; i < latte::MaxUniformBlocks; ++i) {
            auto sq_alu_const_cache_vs = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_CACHE_VS_0 + 4 * i);
            auto sq_alu_const_buffer_size_vs = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_BUFFER_SIZE_VS_0 + 4 * i);
            auto bufferPtr = mem::translate(sq_alu_const_cache_vs << 8);
            auto bufferSize = sq_alu_const_buffer_size_vs * 16 * 4;
            bindDataBuffer(bufferPtr, bufferSize, srvs[i]);
         }

         if (mActivePipelineMode == PipelineMode::Buffers) {
            mCmdList->SetGraphicsRootDescriptorTable(0, srvs);
         } else if (mActivePipelineMode == PipelineMode::Geometry) {
            mCmdList->SetGraphicsRootDescriptorTable(0, srvs);
         } else {
            decaf_abort("Impossible pipeline mode...");
         }
      }

      if (mActiveGeometryShader) {
         auto srvs = allocateSrvs(latte::MaxUniformBlocks);

         for (auto i = 0u; i < latte::MaxUniformBlocks; ++i) {
            auto sq_alu_const_cache_gs = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_CACHE_GS_0 + 4 * i);
            auto sq_alu_const_buffer_size_gs = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_BUFFER_SIZE_GS_0 + 4 * i);
            auto bufferPtr = mem::translate(sq_alu_const_cache_gs << 8);
            auto bufferSize = sq_alu_const_buffer_size_gs * 16 * 4;
            bindDataBuffer(bufferPtr, bufferSize, srvs[i]);
         }

         if (mActivePipelineMode == PipelineMode::Buffers) {
            decaf_abort("Encountered geometry pipeline mode with no geometry shader");
         } else if (mActivePipelineMode == PipelineMode::Geometry) {
            mCmdList->SetGraphicsRootDescriptorTable(1, srvs);
         } else {
            decaf_abort("Impossible pipeline mode...");
         }
      }

      if (mActivePixelShader) {
         auto srvs = allocateSrvs(latte::MaxUniformBlocks);

         for (auto i = 0u; i < latte::MaxUniformBlocks; ++i) {
            auto sq_alu_const_cache_ps = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_CACHE_PS_0 + 4 * i);
            auto sq_alu_const_buffer_size_ps = getRegister<uint32_t>(latte::Register::SQ_ALU_CONST_BUFFER_SIZE_PS_0 + 4 * i);
            auto bufferPtr = mem::translate(sq_alu_const_cache_ps << 8);
            auto bufferSize = sq_alu_const_buffer_size_ps * 16 * 4;
            bindDataBuffer(bufferPtr, bufferSize, srvs[i]);
         }

         if (mActivePipelineMode == PipelineMode::Buffers) {
            mCmdList->SetGraphicsRootDescriptorTable(1, srvs);
         } else if (mActivePipelineMode == PipelineMode::Geometry) {
            mCmdList->SetGraphicsRootDescriptorTable(2, srvs);
         } else {
            decaf_abort("Impossible pipeline mode...");
         }
      }
   } else {
      decaf_abort("Unexpected pipeline mode encountered");
   }

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
