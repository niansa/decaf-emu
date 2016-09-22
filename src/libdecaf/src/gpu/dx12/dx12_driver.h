#pragma once

#ifdef DECAF_DX12

#include "common/d3dx12.h"
#include "common/decaf_assert.h"
#include "libdecaf/decaf_graphics.h"
#include "libdecaf/decaf_dx12.h"
#include "gpu/gpu_datahash.h"
#include "gpu/hlsl2/hlsl2_translate.h"
#include "gpu/pm4_buffer.h"
#include "gpu/pm4_processor.h"
#include <list>
#include <queue>
#include <unordered_map>
#include <vector>
#include <wrl.h>

using namespace Microsoft::WRL;

namespace gpu
{

namespace dx12
{

enum class DescriptorType : uint32_t
{
   VertexRegs,
   PixelRegs,
   VertexUniforms,
   GeometryUniforms,
   PixelUniforms,
   VertexTextures,
   GeometryTextures,
   PixelTextures,
   VertexSamplers,
   GeometrySamplers,
   PixelSamplers,
   SystemData
};

struct Resource
{
   uint32_t cpuMemStart;
   uint32_t cpuMemEnd;
   DataHash cpuHash;
};

struct ScanBufferChain
{
   ComPtr<ID3D12Resource> object = nullptr;

   uint32_t width;
   uint32_t height;

   struct {

   } dbgInfo;
};

struct VertexShader
{
   ComPtr<ID3DBlob> object = nullptr;
   gpu::VertexShaderDesc desc;
   hlsl2::VertexShader data;
};

struct GeometryShader
{
   ComPtr<ID3DBlob> object = nullptr;
   gpu::GeometryShaderDesc desc;
   hlsl2::GeometryShader data;
};

struct PixelShader
{
   ComPtr<ID3DBlob> object = nullptr;
   gpu::PixelShaderDesc desc;
   hlsl2::PixelShader data;
};

enum class PipelineMode : uint32_t
{
   Invalid,
   Registers,
   Buffers,
   Geometry
};

struct Pipeline
{
   ComPtr<ID3D12PipelineState> object = nullptr;
   D3D12_GRAPHICS_PIPELINE_STATE_DESC data;
   PipelineMode mode;
   VertexShader *vertexShader;
   GeometryShader *geometryShader;
   PixelShader *pixelShader;
};

struct SurfaceTexture : Resource
{
   ComPtr<ID3D12Resource> object = nullptr;
   uint32_t width = 0;
   uint32_t height = 0;
   uint32_t depth = 0;

   D3D12_RESOURCE_STATES resourceState;

   SurfaceTexture *next = nullptr;

   struct
   {
      latte::SQ_TEX_DIM dim;
      latte::SQ_DATA_FORMAT format;
      latte::SQ_NUM_FORMAT numFormat;
      latte::SQ_FORMAT_COMP formatComp;
      uint32_t degamma;
   } dbgInfo;
};

struct SurfaceObject
{
   SurfaceTexture *active = nullptr;
   SurfaceTexture *master = nullptr;

   bool needUpload = true;

   struct
   {
      latte::SQ_TEX_DIM dim;
      uint32_t pitch;
      uint32_t height;
      uint32_t depth;
   } dbgInfo;
};

struct BufferObject
{
   ComPtr<ID3D12Resource> object = nullptr;
};

struct BufferView
{
   ID3D12Resource *object = nullptr;
   uint32_t offset = 0;
   uint32_t size = 0;
};

class DescriptorsHandle
{
public:
   DescriptorsHandle(UINT descriptorCount,
                     UINT descriptorSize,
                     D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                     D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
      : mIndex(0), mDescriptorCount(descriptorCount), mDescriptorSize(descriptorSize),
      mCpuHandle(cpuHandle), mGpuHandle(gpuHandle)
   {
   }

   DescriptorsHandle operator[](size_t index)
   {
      DescriptorsHandle out = *this;
      out.mIndex += static_cast<UINT>(index);
      decaf_check(out.mIndex < out.mDescriptorCount);
      return out;
   }

   DescriptorsHandle & operator++()
   {
      mIndex++;
      decaf_check(mIndex < mDescriptorCount);
      return *this;
   }

   operator D3D12_CPU_DESCRIPTOR_HANDLE() const
   {
      return CD3DX12_CPU_DESCRIPTOR_HANDLE(mCpuHandle).Offset(mIndex, mDescriptorSize);
   }

   operator D3D12_GPU_DESCRIPTOR_HANDLE() const
   {
      return CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuHandle).Offset(mIndex, mDescriptorSize);
   }

private:
   UINT mIndex;
   UINT mDescriptorCount;
   UINT mDescriptorSize;
   CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
   CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuHandle;

};

struct FrameHeap
{
   ComPtr<ID3D12DescriptorHeap> heap = nullptr;

   UINT descriptorSize = 0;
   UINT maxItems = 0;
   UINT usedItems = 0;
};

struct FrameResource
{
   ComPtr<ID3D12Resource> object = nullptr;
   UINT64 size = 0;
};

struct FrameData
{
   ComPtr<ID3D12CommandAllocator> allocator = nullptr;
   ComPtr<ID3D12GraphicsCommandList> cmdList = nullptr;

   pm4::Buffer *sourceBuffer = nullptr;
   UINT64 fenceValue = 0;
   std::vector<std::function<void()>> callbacks;
   FrameHeap *rtvHeap;
   FrameHeap *samplerHeap;
   FrameHeap *srvHeap;
   std::vector<FrameResource *> uploadBuffers;
   std::vector<FrameResource *> readbackBuffers;
};

class Driver : public decaf::DX12Driver, public Pm4Processor
{
public:
   Driver();
   virtual ~Driver() = default;

   void
   initialise(ID3D12Device *device, ID3D12CommandQueue *queue) override;

   void
   getSwapBuffers(ID3D12Resource **tv, ID3D12Resource **drc) override;

   void
   syncPoll(const SwapFunction &swapFunc) override;

   void run() override;
   void stop() override;
   float getAverageFPS() override;

   void notifyCpuFlush(void *ptr, uint32_t size) override;
   void notifyGpuFlush(void *ptr, uint32_t size) override;

private:
   void decafSetBuffer(const pm4::DecafSetBuffer &data) override;
   void decafCopyColorToScan(const pm4::DecafCopyColorToScan &data) override;
   void decafSwapBuffers(const pm4::DecafSwapBuffers &data) override;
   void decafCapSyncRegisters(const pm4::DecafCapSyncRegisters &data) override;
   void decafClearColor(const pm4::DecafClearColor &data) override;
   void decafClearDepthStencil(const pm4::DecafClearDepthStencil &data) override;
   void decafDebugMarker(const pm4::DecafDebugMarker &data) override;
   void decafOSScreenFlip(const pm4::DecafOSScreenFlip &data) override;
   void decafCopySurface(const pm4::DecafCopySurface &data) override;
   void decafSetSwapInterval(const pm4::DecafSetSwapInterval &data) override;
   void drawIndexAuto(const pm4::DrawIndexAuto &data) override;
   void drawIndex2(const pm4::DrawIndex2 &data) override;
   void drawIndexImmd(const pm4::DrawIndexImmd &data) override;
   void memWrite(const pm4::MemWrite &data) override;
   void eventWrite(const pm4::EventWrite &data) override;
   void eventWriteEOP(const pm4::EventWriteEOP &data) override;
   void pfpSyncMe(const pm4::PfpSyncMe &data) override;
   void streamOutBaseUpdate(const pm4::StreamOutBaseUpdate &data) override;
   void streamOutBufferUpdate(const pm4::StreamOutBufferUpdate &data) override;
   void surfaceSync(const pm4::SurfaceSync &data) override;
   void applyRegister(latte::Register reg) override;

   void executeBuffer(pm4::Buffer *buffer);
   void checkSyncObjects();

   void ensureDescriptorHeaps(bool forceSet = false);

   bool checkReadyDraw();
   bool checkActiveRootSignature();
   bool checkActivePipeline();
   bool checkActiveRenderTargets();
   bool checkActiveUniforms();
   bool checkActiveVertexBuffers();
   bool checkActiveSamplers();
   bool checkActiveTextures();
   bool checkActiveView();

   // Per frame stuff...
   DescriptorsHandle
   allocateRtvs(uint32_t numRtvs);

   DescriptorsHandle
   allocateSamplers(uint32_t numSamplers);

   DescriptorsHandle
   allocateSrvs(uint32_t numSrvs);

   ID3D12Resource *
   allocateUploadBuffer(UINT64 size);

   ID3D12Resource *
   allocateReadbackBuffer(UINT64 size);

   SurfaceTexture *
   createSurfaceTexture(latte::SQ_TEX_DIM dim,
                        latte::SQ_DATA_FORMAT format,
                        latte::SQ_NUM_FORMAT numFormat,
                        latte::SQ_FORMAT_COMP formatComp,
                        uint32_t degamma,
                        uint32_t width,
                        uint32_t height,
                        uint32_t depth,
                        uint32_t samples);

   void
   copySurfaceTexture(SurfaceTexture* dest,
                      SurfaceTexture *source,
                      latte::SQ_TEX_DIM dim);

   void
   uploadSurfaceTexture(SurfaceTexture *buffer,
                        ppcaddr_t baseAddress,
                        uint32_t swizzle,
                        latte::SQ_TEX_DIM dim,
                        latte::SQ_DATA_FORMAT format,
                        latte::SQ_NUM_FORMAT numFormat,
                        latte::SQ_FORMAT_COMP formatComp,
                        uint32_t degamma,
                        uint32_t pitch,
                        uint32_t width,
                        uint32_t height,
                        uint32_t depth,
                        uint32_t samples,
                        bool isDepthBuffer,
                        latte::SQ_TILE_MODE tileMode);

   SurfaceObject *
   getSurfaceBuffer(ppcaddr_t baseAddress,
                    latte::SQ_TEX_DIM dim,
                    latte::SQ_DATA_FORMAT format,
                    latte::SQ_NUM_FORMAT numFormat,
                    latte::SQ_FORMAT_COMP formatComp,
                    uint32_t degamma,
                    uint32_t pitch,
                    uint32_t width,
                    uint32_t height,
                    uint32_t depth,
                    uint32_t samples,
                    bool isDepthBuffer,
                    latte::SQ_TILE_MODE tileMode,
                    bool discardData);

   void
   transitionSurfaceTexture(SurfaceTexture *surface,
                            D3D12_RESOURCE_STATES state);

   SurfaceTexture *
   getColorBuffer(latte::CB_COLORN_BASE cb_color_base,
                  latte::CB_COLORN_SIZE cb_color_size,
                  latte::CB_COLORN_INFO cb_color_info,
                  bool discardData);

   SurfaceTexture *
   getDepthBuffer(latte::DB_DEPTH_BASE db_depth_base,
                  latte::DB_DEPTH_SIZE db_depth_size,
                  latte::DB_DEPTH_INFO db_depth_info,
                  bool discardData);

   void
   blitTextureRegion(const D3D12_TEXTURE_COPY_LOCATION &dest,
                     const D3D12_BOX &destRegion,
                     const D3D12_TEXTURE_COPY_LOCATION &source,
                     const D3D12_BOX &sourceRegion);

   uint32_t
   getDescriptorIndex(DescriptorType descriptorType);

   BufferView
   getDataBuffer(void *data, uint32_t size);

   bool
   bindDataBuffer(void *data, uint32_t size, const DescriptorsHandle &handle);

   void
   drawGenericIndexed(uint32_t numIndices, void *indices);

   bool
   checkActiveVertexShader();

   bool
   checkActiveGeometryShader();

   bool
   checkActivePixelShader();

   // Variables
   enum class RunState
   {
      None,
      Running,
      Stopped
   };

   volatile RunState mRunState = RunState::None;
   SwapFunction mSwapFunc;

   PipelineMode mActivePipelineMode = PipelineMode::Invalid;
   ID3D12RootSignature *mActiveRootSignature = nullptr;
   VertexShader *mActiveVertexShader = nullptr;
   GeometryShader *mActiveGeometryShader = nullptr;
   PixelShader *mActivePixelShader = nullptr;
   Pipeline *mActivePipeline = nullptr;

   ID3D12Device *mDevice = nullptr;
   ID3D12CommandQueue *mCmdQueue = nullptr;

   ComPtr<ID3D12RootSignature> mRegRootSignature;
   ComPtr<ID3D12RootSignature> mUniRootSignature;
   ComPtr<ID3D12RootSignature> mGeomRootSignature;

   ComPtr<ID3D12RootSignature> mCopyRootSignature;
   ComPtr<ID3DBlob> mCopyVertexShader;
   ComPtr<ID3DBlob> mCopyPixelShader;
   ComPtr<ID3D12PipelineState> mCopyPipelineStates[128];

   std::vector<FrameData *> mFrameDataPool;
   std::vector<FrameHeap *> mSrvFrameHeapPool;
   std::vector<FrameHeap *> mRtvFrameHeapPool;
   std::vector<FrameHeap *> mSamplerFrameHeapPool;
   std::list<FrameResource *> mUploadBufferPool;
   std::list<FrameResource *> mReadbackBufferPool;

   ComPtr<ID3D12Fence> mWaitFence = nullptr;
   UINT64 mWaitFenceCounter = 0;
   std::queue<FrameData*> mFrameWaits;

   FrameData *mFrameData = nullptr;
   ID3D12GraphicsCommandList *mCmdList = nullptr;
   FrameHeap *mRtvHeap = nullptr;
   FrameHeap *mSrvHeap = nullptr;
   FrameHeap *mSamplerHeap = nullptr;

   ScanBufferChain mTvScanBuffers;
   ScanBufferChain mDrcScanBuffers;
   std::unordered_map<uint64_t, VertexShader> mVertexShaders;
   std::unordered_map<uint64_t, GeometryShader> mGeometryShaders;
   std::unordered_map<uint64_t, PixelShader> mPixelShaders;
   std::unordered_map<uint64_t, Pipeline> mPipelines;
   std::unordered_map<uint64_t, SurfaceObject> mSurfaces;

   using duration_system_clock = std::chrono::duration<double, std::chrono::system_clock::period>;
   using duration_ms = std::chrono::duration<double, std::chrono::milliseconds::period>;
   std::chrono::time_point<std::chrono::system_clock> mLastSwap;
   duration_system_clock mAverageFrameTime;

};

} // namespace dx12

} // namespace gpu

#define decaf_checkhr(op) \
   { HRESULT hr = (op); if (FAILED(hr)) decaf_abort(fmt::format("{}\nfailed with status {:08X}", #op, (uint32_t)hr)); }

#endif // DECAF_DX12
