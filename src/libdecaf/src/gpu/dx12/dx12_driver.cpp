#ifdef DECAF_DX12

#include "common/d3dx12.h"
#include "decaf_config.h"
#include "dx12_driver.h"
#include "gpu/gpu_commandqueue.h"
#include "gpu/pm4_buffer.h"
#include <d3dcompiler.h>

namespace gpu
{

namespace dx12
{

Driver::Driver()
{

}

void
Driver::initialise(ID3D12Device *device, ID3D12CommandQueue *queue)
{
   mDevice = device;
   mCmdQueue = queue;

   decaf_checkhr(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mWaitFence)));
   mWaitFenceCounter = 1;

   // Create our registers based root signature.
   {
      CD3DX12_DESCRIPTOR_RANGE ranges[2];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 0);
      ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[7];
      rootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[1].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[3].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[4].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[5].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[6].InitAsConstants(4, 13, 0, D3D12_SHADER_VISIBILITY_ALL);

      CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init(
         _countof(rootParameters), rootParameters,
         0, nullptr,
         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

      ComPtr<ID3DBlob> signature;
      ComPtr<ID3DBlob> error;
      decaf_checkhr(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
      decaf_checkhr(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRegRootSignature)));

      if (decaf::config::gpu::debug) {
         mRegRootSignature->SetName(L"Register Root Signature");
      }
   }

   // Create our uniform buffer based root signature.
   {
      // TODO: Handle the fact that we can only have 14 CBV's here, but GX2 exposes all 16
      //        constant buffers to the application.  Hopefully this won't matter...

      CD3DX12_DESCRIPTOR_RANGE ranges[3];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 13, 0, 0);
      ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 0);
      ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[7];
      rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[2].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[4].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[5].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[6].InitAsConstants(4, 13, 0, D3D12_SHADER_VISIBILITY_ALL);

      CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init(
         _countof(rootParameters), rootParameters,
         0, nullptr,
         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

      ComPtr<ID3DBlob> signature;
      ComPtr<ID3DBlob> error;
      decaf_checkhr(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
      decaf_checkhr(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mUniRootSignature)));

      if (decaf::config::gpu::debug) {
         mUniRootSignature->SetName(L"Uniform Root Signature");
      }
   }

   // Create our geometry based root signature.
   {
      CD3DX12_DESCRIPTOR_RANGE ranges[3];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 13, 0, 0);
      ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 0);
      ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 16, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[10];
      rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_GEOMETRY);
      rootParameters[2].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[3].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[4].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_GEOMETRY);
      rootParameters[5].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[6].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_VERTEX);
      rootParameters[7].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_GEOMETRY);
      rootParameters[8].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);
      rootParameters[9].InitAsConstants(4, 13, 0, D3D12_SHADER_VISIBILITY_ALL);

      CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init(
         _countof(rootParameters), rootParameters,
         0, nullptr,
         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS);

      ComPtr<ID3DBlob> signature;
      ComPtr<ID3DBlob> error;
      decaf_checkhr(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
      decaf_checkhr(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mGeomRootSignature)));

      if (decaf::config::gpu::debug) {
         mGeomRootSignature->SetName(L"Geometry Root Signature");
      }
   }

   // Create our copy root signature.
   {
      CD3DX12_DESCRIPTOR_RANGE ranges[1];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[1];
      rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

      D3D12_STATIC_SAMPLER_DESC sampler = {};
      sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
      sampler.MipLODBias = 0;
      sampler.MaxAnisotropy = 0;
      sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
      sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
      sampler.MinLOD = 0.0f;
      sampler.MaxLOD = 0.0f;
      sampler.ShaderRegister = 0;
      sampler.RegisterSpace = 0;
      sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

      CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
      rootSignatureDesc.Init(
         _countof(rootParameters), rootParameters,
         1, &sampler,
         D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
         D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS);

      ComPtr<ID3DBlob> signature;
      ComPtr<ID3DBlob> error;
      decaf_checkhr(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
      decaf_checkhr(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mCopyRootSignature)));

      if (decaf::config::gpu::debug) {
         mCopyRootSignature->SetName(L"Copy Root Signature");
      }
   }

   // Create our copy pipeline state.
   {
      static const char* shaders =
         "struct VS_INPUT\n\
         {\n\
            float2 pos : POSITION;\n\
            float2 uv : TEXCOORD0;\n\
         };\n\
         \n\
         struct PS_INPUT\n\
         {\n\
            float4 pos : SV_POSITION;\n\
            float2 uv  : TEXCOORD0;\n\
         };\n\
         \n\
         PS_INPUT VSMain(VS_INPUT input)\n\
         {\n\
            PS_INPUT output;\n\
            output.pos = float4(input.pos.xy, 0.0f, 1.0f);\n\
            output.uv  = input.uv;\n\
            return output;\n\
         }\n\
         \n\
         SamplerState sampler0 : register(s0);\n\
         Texture2D texture0 : register(t0);\n\
         \n\
         float4 PSMain(PS_INPUT input) : SV_TARGET\n\
         {\n\
            return texture0.Sample(sampler0, input.uv);\n\
         }\n";

      ComPtr<ID3DBlob> errors;
      UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      auto vsRes = D3DCompile(shaders, strlen(shaders), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &mCopyVertexShader, &errors);
      if (FAILED(vsRes)) {
         decaf_abort(fmt::format("D3DCompile Failure:\n{}", (const char*)errors->GetBufferPointer()));
      }
      auto psRes = D3DCompile(shaders, strlen(shaders), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &mCopyPixelShader, &errors);
      if (FAILED(psRes)) {
         decaf_abort(fmt::format("D3DCompile Failure:\n{}", (const char*)errors->GetBufferPointer()));
      }
   }
}

void
Driver::getSwapBuffers(ID3D12Resource **tv, ID3D12Resource **drc)
{
   *tv = mTvScanBuffers.object.Get();
   *drc = mDrcScanBuffers.object.Get();
}

void Driver::executeBuffer(pm4::Buffer *buffer)
{
   decaf_check(buffer);

   decaf_check(!mFrameData);
   decaf_check(!mCmdList);

   // Allocate some frame data for this buffer
   if (!mFrameDataPool.empty()) {
      mFrameData = mFrameDataPool.back();
      mFrameDataPool.pop_back();

      mFrameData->allocator->Reset();
      mFrameData->cmdList->Reset(mFrameData->allocator.Get(), nullptr);
   } else {
      mFrameData = new FrameData();

      // Create Allocator
      decaf_checkhr(mDevice->CreateCommandAllocator(
         D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mFrameData->allocator)));

      // Create Command List
      decaf_checkhr(mDevice->CreateCommandList(
         0, D3D12_COMMAND_LIST_TYPE_DIRECT, mFrameData->allocator.Get(), nullptr, IID_PPV_ARGS(&mFrameData->cmdList)));

      if (decaf::config::gpu::debug) {
         mFrameData->allocator->SetName(L"GPU7 Command Allocator");
         mFrameData->cmdList->SetName(L"GPU7 Command List");
      }
   }

   // Sanity checks
   decaf_check(mFrameData->fenceValue == 0);
   decaf_check(mFrameData->callbacks.empty());
   decaf_check(mFrameData->uploadBuffers.empty());
   decaf_check(mFrameData->readbackBuffers.empty());

   // Make the command list easier to access
   mCmdList = mFrameData->cmdList.Get();

   // Save the buffer we are processing
   mFrameData->sourceBuffer = buffer;

   // Force descriptor heaps to be set for this command list
   ensureDescriptorHeaps(true);

   // Execute command buffer
   runCommandBuffer(buffer->buffer, buffer->curSize);

   // Just some quick checks
   decaf_check(mFrameData->cmdList.Get() == mCmdList);
   decaf_check(mFrameData->sourceBuffer == buffer);

   // Close the command list in preparation of execution
   decaf_checkhr(mCmdList->Close());

   // Send our local command list to the host GPU
   ID3D12CommandList *cmdLists[] = { mCmdList };
   mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

   // Write our signal so we know when its been completed
   mFrameData->fenceValue = mWaitFenceCounter++;
   mCmdQueue->Signal(mWaitFence.Get(), mFrameData->fenceValue);

   // Save this in the list of stuff we are waiting for
   mFrameWaits.push(mFrameData);

   // Clear up all the state
   mFrameData = nullptr;
   mCmdList = nullptr;
}

void
Driver::syncPoll(const SwapFunction &swapFunc)
{
   if (mRunState == RunState::None) {
      mRunState = RunState::Running;
   }

   mSwapFunc = swapFunc;

   while (true) {
      checkSyncObjects();

      auto buffer = gpu::tryUnqueueCommandBuffer();
      if (!buffer) {
         break;
      }

      executeBuffer(buffer);
   }
}

void
Driver::ensureDescriptorHeaps(bool forceSet)
{
   // Release the heaps to be free'd if we are not comfortable with the number
   //  descriptor items remaining in any of the heaps.

   if (mRtvHeap && mRtvHeap->usedItems >= mRtvHeap->maxItems / 4 * 3) {
      mFrameData->rtvHeaps.push_back(mRtvHeap);
      mRtvHeap = nullptr;
   }

   if (mSamplerHeap && mSamplerHeap->usedItems >= mSamplerHeap->maxItems / 4 * 3) {
      mFrameData->samplerHeaps.push_back(mSamplerHeap);
      mSamplerHeap = nullptr;
   }

   if (mSrvHeap && mSrvHeap->usedItems >= mSrvHeap->maxItems / 4 * 3) {
      mFrameData->srvHeaps.push_back(mSrvHeap);
      mSrvHeap = nullptr;
   }

   bool needSetHeaps = false;

   // Allocate an RTV heap to use if we need to
   if (!mRtvHeap) {
      if (!mRtvFrameHeapPool.empty()) {
         mRtvHeap = mRtvFrameHeapPool.back();
         mRtvFrameHeapPool.pop_back();
      } else {
         mRtvHeap = new FrameHeap();
         mRtvHeap->maxItems = 1000;
         mRtvHeap->usedItems = 0;

         D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
         heapDesc.NumDescriptors = mRtvHeap->maxItems;
         heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
         heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
         decaf_checkhr(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mRtvHeap->heap)));

         if (decaf::config::gpu::debug) {
            mRtvHeap->heap->SetName(L"GPU7 RTV Heap");
         }

         mRtvHeap->descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      }
   }

   // Allocate a sampler heap to use if we need to
   if (!mSamplerHeap) {
      if (!mSamplerFrameHeapPool.empty()) {
         mSamplerHeap = mSamplerFrameHeapPool.back();
         mSamplerFrameHeapPool.pop_back();
      } else {
         mSamplerHeap = new FrameHeap();
         mSamplerHeap->maxItems = 2048;
         mSamplerHeap->usedItems = 0;

         D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
         heapDesc.NumDescriptors = mSamplerHeap->maxItems;
         heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
         heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         decaf_checkhr(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSamplerHeap->heap)));

         if (decaf::config::gpu::debug) {
            mSamplerHeap->heap->SetName(L"GPU7 Sampler Heap");
         }

         mSamplerHeap->descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
      }

      needSetHeaps = true;
   }

   // Allocate an SRV heap to use if we need to
   if (!mSrvHeap) {
      if (!mSrvFrameHeapPool.empty()) {
         mSrvHeap = mSrvFrameHeapPool.back();
         mSrvFrameHeapPool.pop_back();
      } else {
         mSrvHeap = new FrameHeap();
         mSrvHeap->maxItems = 10000;
         mSrvHeap->usedItems = 0;

         D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
         heapDesc.NumDescriptors = mSrvHeap->maxItems;
         heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
         heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         decaf_checkhr(mDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mSrvHeap->heap)));

         if (decaf::config::gpu::debug) {
            mSrvHeap->heap->SetName(L"GPU7 SRV Heap");
         }

         mSrvHeap->descriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      }

      needSetHeaps = true;
   }

   if (needSetHeaps || forceSet) {
      // Set our GPU-visible heaps up
      ID3D12DescriptorHeap *heaps[] = { mSrvHeap->heap.Get(), mSamplerHeap->heap.Get() };
      mCmdList->SetDescriptorHeaps(_countof(heaps), heaps);
   }
}

void
Driver::checkSyncObjects()
{
   // Early out with nothing waiting
   if (mFrameWaits.empty()) {
      return;
   }

   // Grab the most current fence state
   UINT64 fenceValue = mWaitFence->GetCompletedValue();

   while (!mFrameWaits.empty()) {
      // Grab the top of the sync wait queue
      auto frameData = mFrameWaits.front();

      // See if we've completed this one yet.
      if (fenceValue < frameData->fenceValue) {
         break;
      }

      // Execute any callbacks that were waiting
      for (auto &func : frameData->callbacks) {
         func();
      }
      frameData->callbacks.clear();

      // Retire the buffer
      gpu::retireCommandBuffer(frameData->sourceBuffer);
      frameData->sourceBuffer = nullptr;

      // Free the heaps that were attached
      for (auto &heap : frameData->rtvHeaps) {
         heap->usedItems = 0;
         mRtvFrameHeapPool.push_back(heap);
      }
      frameData->rtvHeaps.clear();

      for (auto &heap : frameData->samplerHeaps) {
         heap->usedItems = 0;
         mSamplerFrameHeapPool.push_back(heap);
      }
      frameData->samplerHeaps.clear();

      for (auto &heap : frameData->srvHeaps) {
         heap->usedItems = 0;
         mSrvFrameHeapPool.push_back(heap);
      }
      frameData->srvHeaps.clear();

      // Free any temporary resources we had
      for (auto &res : frameData->uploadBuffers) {
         mUploadBufferPool.push_back(res);
      }
      frameData->uploadBuffers.clear();

      for (auto &res : frameData->readbackBuffers) {
         mReadbackBufferPool.push_back(res);
      }
      frameData->readbackBuffers.clear();

      // Clear some extra state
      frameData->fenceValue = 0;

      // Save this FrameData back to the pool
      mFrameDataPool.push_back(frameData);

      // Remove this from the wait list
      mFrameWaits.pop();
   }
}

static DescriptorsHandle
allocateFromHeap(FrameHeap *frameHeap, uint32_t numDescs)
{
   decaf_check(frameHeap->usedItems + numDescs <= frameHeap->maxItems);

   auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(frameHeap->heap->GetCPUDescriptorHandleForHeapStart())
      .Offset(frameHeap->usedItems, frameHeap->descriptorSize);
   auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(frameHeap->heap->GetGPUDescriptorHandleForHeapStart())
      .Offset(frameHeap->usedItems, frameHeap->descriptorSize);

   frameHeap->usedItems += numDescs;

   return DescriptorsHandle(numDescs, frameHeap->descriptorSize, cpuHandle, gpuHandle);
}

DescriptorsHandle
Driver::allocateRtvs(uint32_t numRtvs)
{
   return allocateFromHeap(mRtvHeap, numRtvs);
}

DescriptorsHandle
Driver::allocateSamplers(uint32_t numSamplers)
{
   return allocateFromHeap(mSamplerHeap, numSamplers);
}

DescriptorsHandle
Driver::allocateSrvs(uint32_t numSrvs)
{
   return allocateFromHeap(mSrvHeap, numSrvs);
}

ID3D12Resource *
Driver::allocateUploadBuffer(UINT64 size)
{
   FrameResource *buffer = nullptr;

   {
      // Find the best-fit buffer
      auto bestIter = mUploadBufferPool.end();
      for (auto i = mUploadBufferPool.begin(); i != mUploadBufferPool.end(); ++i) {
         auto buffer = *i;

         if (buffer->size >= size) {
            if (bestIter == mUploadBufferPool.end()) {
               bestIter = i;
            }

            if (buffer->size < (*bestIter)->size) {
               bestIter = i;
            }
         }
      }

      if (bestIter != mUploadBufferPool.end()) {
         mUploadBufferPool.erase(bestIter);
         buffer = *bestIter;
      }
   }

   if (!buffer) {
      buffer = new FrameResource();

      // Create the upload resource
      decaf_checkhr(mDevice->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
         D3D12_HEAP_FLAG_NONE,
         &CD3DX12_RESOURCE_DESC::Buffer(size),
         D3D12_RESOURCE_STATE_GENERIC_READ,
         nullptr,
         IID_PPV_ARGS(&buffer->object)));

      // Record the size, just because
      buffer->size = size;
   }

   // Save it for cleanup later
   mFrameData->uploadBuffers.push_back(buffer);

   return buffer->object.Get();
}

ID3D12Resource *
Driver::allocateReadbackBuffer(UINT64 size)
{
   FrameResource *buffer = nullptr;

   {
      // Find the best-fit buffer
      auto bestIter = mReadbackBufferPool.end();
      for (auto i = mReadbackBufferPool.begin(); i != mReadbackBufferPool.end(); ++i) {
         auto buffer = *i;

         if (buffer->size >= size) {
            if (bestIter == mReadbackBufferPool.end()) {
               bestIter = i;
            }

            if (buffer->size < (*bestIter)->size) {
               bestIter = i;
            }
         }
      }

      if (bestIter != mReadbackBufferPool.end()) {
         mReadbackBufferPool.erase(bestIter);
         buffer = *bestIter;
      }
   }

   if (!buffer) {
      buffer = new FrameResource();

      // Create the readback resource
      decaf_checkhr(mDevice->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
         D3D12_HEAP_FLAG_NONE,
         &CD3DX12_RESOURCE_DESC::Buffer(size),
         D3D12_RESOURCE_STATE_COPY_DEST,
         nullptr,
         IID_PPV_ARGS(&buffer->object)));

      // Record the size, just because
      buffer->size = size;
   }

   // Save it for cleanup later
   mFrameData->readbackBuffers.push_back(buffer);

   return buffer->object.Get();
}

void
Driver::blitTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dest,
                          const D3D12_BOX &destRegion,
                          const D3D12_TEXTURE_COPY_LOCATION &source,
                          const D3D12_BOX &sourceRegion)
{
   auto destDesc = dest.pResource->GetDesc();
   auto sourceDesc = source.pResource->GetDesc();

   auto &pipelineState = mCopyPipelineStates[destDesc.Format];
   if (!pipelineState) {
      D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
      {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
      };

      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
      psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
      psoDesc.pRootSignature = mCopyRootSignature.Get();
      psoDesc.VS = CD3DX12_SHADER_BYTECODE(mCopyVertexShader.Get());
      psoDesc.PS = CD3DX12_SHADER_BYTECODE(mCopyPixelShader.Get());
      psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.SampleMask = UINT_MAX;
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      psoDesc.NumRenderTargets = 1;
      psoDesc.RTVFormats[0] = destDesc.Format;
      psoDesc.SampleDesc.Count = 1;
      decaf_checkhr(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
   }

   // We currently only support copying a single layer
   decaf_check(destRegion.front == 0 && destRegion.back == 1);
   decaf_check(sourceRegion.front == 0 && sourceRegion.back == 1);

   D3D12_VIEWPORT viewport;
   viewport.TopLeftX = static_cast<FLOAT>(destRegion.left);
   viewport.TopLeftY = static_cast<FLOAT>(destRegion.top);
   viewport.Width = static_cast<FLOAT>(destRegion.right - destRegion.left);
   viewport.Height = static_cast<FLOAT>(destRegion.bottom - destRegion.top);
   viewport.MinDepth = 0.0f;
   viewport.MaxDepth = 1.0f;

   CD3DX12_RECT scissorRect;
   scissorRect.left = destRegion.left;
   scissorRect.top = destRegion.top;
   scissorRect.right = destRegion.right;
   scissorRect.bottom = destRegion.bottom;

   auto sourceLeft = static_cast<float>(sourceRegion.left);
   auto sourceTop = static_cast<float>(sourceRegion.top);
   auto sourceRight = static_cast<float>(sourceRegion.right);
   auto sourceBottom = static_cast<float>(sourceRegion.bottom);
   auto sourceWidth = static_cast<float>(sourceDesc.Width);
   auto sourceHeight = static_cast<float>(sourceDesc.Height);

   // This is a piss-poor implementation, but there is an NVIDIA driver bug that causes
   //  drawing with no vertex buffers assigned to draw nothing.  WARP works, but NVIDIA
   //  does not.  For now lets use a vertex buffer we constantly update...
   float verts[] = {
      -1.0f,  1.0f,  sourceLeft / sourceWidth,  sourceTop / sourceHeight,
       1.0f,  1.0f,  sourceRight / sourceWidth, sourceTop / sourceHeight,
      -1.0f, -1.0f,  sourceLeft / sourceWidth,  sourceBottom / sourceHeight,
       1.0f, -1.0f,  sourceRight / sourceWidth, sourceBottom / sourceHeight
   };

   static const CD3DX12_RANGE EmptyRange(0, 0);
   auto vertBuffer = allocateUploadBuffer(16 * sizeof(float));
   void *vertBufferPtr;
   vertBuffer->Map(0, &EmptyRange, &vertBufferPtr);
   memcpy(vertBufferPtr, verts, 16 * sizeof(float));
   vertBuffer->Unmap(0, nullptr);

   D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
   vertexBufferView.BufferLocation = vertBuffer->GetGPUVirtualAddress();
   vertexBufferView.StrideInBytes = 4 * sizeof(float);
   vertexBufferView.SizeInBytes = 4 * vertexBufferView.StrideInBytes;

   auto sourceSrv = allocateSrvs(1);
   mDevice->CreateShaderResourceView(source.pResource, nullptr, sourceSrv);

   auto destRtv = allocateRtvs(1);
   mDevice->CreateRenderTargetView(dest.pResource, nullptr, destRtv);
   D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = destRtv;

   mCmdList->SetGraphicsRootSignature(mCopyRootSignature.Get());
   mCmdList->SetPipelineState(pipelineState.Get());
   mCmdList->SetGraphicsRootDescriptorTable(0, sourceSrv);
   mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);
   mCmdList->IASetVertexBuffers(0, 1, &vertexBufferView);
   mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   mCmdList->RSSetViewports(1, &viewport);
   mCmdList->RSSetScissorRects(1, &scissorRect);
   mCmdList->DrawInstanced(4, 1, 0, 0);
}

void
Driver::run()
{
   if (mRunState != RunState::None) {
      return;
   }

   mRunState = RunState::Running;

   while (mRunState == RunState::Running) {
      pm4::Buffer *buffer;

      if (mFrameWaits.empty()) {
         buffer = gpu::unqueueCommandBuffer();
      } else {
         buffer = gpu::tryUnqueueCommandBuffer();
      }

      checkSyncObjects();

      if (buffer) {
         executeBuffer(buffer);
      }
   }
}

void
Driver::stop()
{
   mRunState = RunState::Stopped;

   // Wake the GPU thread
   gpu::awaken();
}

float
Driver::getAverageFPS()
{
   // TODO: This is not thread safe...
   static const auto second = std::chrono::duration_cast<duration_system_clock>(std::chrono::seconds{ 1 }).count();
   return static_cast<float>(second / mAverageFrameTime.count());
}

void
Driver::notifyCpuFlush(void *ptr, uint32_t size)
{
}

void
Driver::notifyGpuFlush(void *ptr, uint32_t size)
{
}

void
Driver::applyRegister(latte::Register reg)
{
   // DX12 does not perform direct application of any registers
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
