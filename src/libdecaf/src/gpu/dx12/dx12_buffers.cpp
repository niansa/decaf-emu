#ifdef DECAF_DX12

#include "common/log.h"
#include "dx12_driver.h"

namespace gpu
{

namespace dx12
{

BufferView
Driver::getDataBuffer(void *data, uint32_t size)
{
   // This is neccessary due to the alignment requirements of DX12
   auto alignedSize = align_up(size, 256);

   auto uploadBuffer = allocateUploadBuffer(alignedSize);

   static const CD3DX12_RANGE EmptyRange = { 0, 0 };
   void *uploadData = nullptr;
   uploadBuffer->Map(0, &EmptyRange, &uploadData);

   memcpy(uploadData, data, size);

   uploadBuffer->Unmap(0, nullptr);

   BufferView out;
   out.object = uploadBuffer;
   out.offset = 0;
   out.size = alignedSize;
   return out;
}

bool
Driver::bindDataBuffer(void *data, uint32_t size, const DescriptorsHandle &handle)
{
   D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc;

   if (data && size > 0) {
      auto view = getDataBuffer(data, size);

      viewDesc.BufferLocation = view.object->GetGPUVirtualAddress() + view.offset;
      viewDesc.SizeInBytes = view.size;
   } else {
      viewDesc.BufferLocation = 0;
      viewDesc.SizeInBytes = 0;
   }

   mDevice->CreateConstantBufferView(&viewDesc, handle);

   return true;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
