#ifdef DECAF_DX12

#include "clilog.h"
#include "common/d3dx12.h"
#include "common/decaf_assert.h"
#include "config.h"
#include "decafsdl_dx12.h"
#include <d3dcompiler.h>
#include <SDL.h>
#include <SDL_syswm.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#define decaf_checkhr(op) \
   { HRESULT hr = (op); if (FAILED(hr)) decaf_abort(fmt::format("{}\nfailed with status {:08X}", #op, (uint32_t)hr)); }

DecafSDLDX12::DecafSDLDX12()
{
}

DecafSDLDX12::~DecafSDLDX12()
{
}

void findHardwareAdapter(IDXGIFactory2* pFactory, IDXGIAdapter1** ppAdapter)
{
   ComPtr<IDXGIAdapter1> adapter;
   *ppAdapter = nullptr;

   for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
   {
      DXGI_ADAPTER_DESC1 desc;
      adapter->GetDesc1(&desc);

      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      {
         // Don't select the Basic Render Driver adapter.
         // If you want a software adapter, pass in "/warp" on the command line.
         continue;
      }

      // Check to see if the adapter supports Direct3D 12, but don't create the
      // actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
      {
         break;
      }
   }

   *ppAdapter = adapter.Detach();
}

bool
DecafSDLDX12::initialise(int width, int height)
{
   // Create window
   mWindow = SDL_CreateWindow("Decaf",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      width, height,
      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

   if (!mWindow) {
      gCliLog->error("Failed to create DX12 SDL window");
      return false;
   }

   // Try and trigger debug mode on the D3D context
   if (decaf::config::gpu::debug) {
      ComPtr<ID3D12Debug> debugController;
      if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
      {
         debugController->EnableDebugLayer();
      }
   }

   // Create our factory
   ComPtr<IDXGIFactory4> factory;
   decaf_checkhr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

   // Create our device
   if (0) {
      ComPtr<IDXGIAdapter> warpAdapter;
      decaf_checkhr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

      decaf_checkhr(D3D12CreateDevice(
         warpAdapter.Get(),
         D3D_FEATURE_LEVEL_11_0,
         IID_PPV_ARGS(&mDevice)
      ));
   } else {
      ComPtr<IDXGIAdapter1> hardwareAdapter;
      findHardwareAdapter(factory.Get(), &hardwareAdapter);

      decaf_checkhr(D3D12CreateDevice(
         hardwareAdapter.Get(),
         D3D_FEATURE_LEVEL_11_0,
         IID_PPV_ARGS(&mDevice)
      ));
   }

   // Create a command queue
   D3D12_COMMAND_QUEUE_DESC queueDesc = {};
   queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
   queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

   decaf_checkhr(mDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCmdQueue)));

   // Create a swap chain to render to
   int clientWidth, clientHeight;
   SDL_GetWindowSize(mWindow, &clientWidth, &clientHeight);

   SDL_SysWMinfo wndInfo;
   SDL_VERSION(&wndInfo.version);
   SDL_GetWindowWMInfo(mWindow, &wndInfo);
   decaf_check(wndInfo.subsystem == SDL_SYSWM_WINDOWS);

   DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
   swapChainDesc.BufferCount = SwapBufferCount;
   swapChainDesc.Width = clientWidth;
   swapChainDesc.Height = clientHeight;
   swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   swapChainDesc.SampleDesc.Count = 1;

   ComPtr<IDXGISwapChain1> swapChain;
   decaf_checkhr(factory->CreateSwapChainForHwnd(
      mCmdQueue.Get(),
      wndInfo.info.win.window,
      &swapChainDesc,
      nullptr,
      nullptr,
      &swapChain
   ));

   decaf_checkhr(swapChain.As(&mSwapChain));

   // Disable Alt-Enter triggering full screen
   decaf_checkhr(factory->MakeWindowAssociation(wndInfo.info.win.window, DXGI_MWA_NO_ALT_ENTER));

   // Create descriptor heaps.
   {
      // Describe and create a render target view (RTV) descriptor heap.
      D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
      rtvHeapDesc.NumDescriptors = SwapBufferCount;
      rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      decaf_checkhr(mDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

      // Describe and create a shader resource view (SRV) heap for the texture.
      D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
      srvHeapDesc.NumDescriptors = 3;
      srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      decaf_checkhr(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

      mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
      mSrvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   }

   // Create frame resources.
   {
      CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

      // Create a RTV for each frame.
      for (UINT n = 0; n < SwapBufferCount; n++)
      {
         decaf_checkhr(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
         mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
         rtvHandle.Offset(1, mRtvDescriptorSize);
      }
   }

   // Create a command allocator
   decaf_checkhr(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCmdAllocator)));

   // Create an empty root signature.
   {
      CD3DX12_DESCRIPTOR_RANGE ranges[1];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[1];
      rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

      D3D12_STATIC_SAMPLER_DESC sampler = { };
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
      decaf_checkhr(mDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
   }

   // Create the pipeline state.
   {
      static const char* shaders =
         "struct VS_INPUT\n\
         {\n\
            float2 pos : POSITION;\n\
            float2 uv  : TEXCOORD0;\n\
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
            output.pos = float4(input.pos.xy, 0.f, 1.f);\n\
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
      ComPtr<ID3DBlob> vertexShader;
      ComPtr<ID3DBlob> pixelShader;
      UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
      auto vsRes = D3DCompile(shaders, strlen(shaders), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errors);
      if (FAILED(vsRes)) {
         decaf_abort(fmt::format("D3DCompile Failure:\n{}", (const char*)errors->GetBufferPointer()));
      }
      auto psRes = D3DCompile(shaders, strlen(shaders), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errors);
      if (FAILED(psRes)) {
         decaf_abort(fmt::format("D3DCompile Failure:\n{}", (const char*)errors->GetBufferPointer()));
      }

      D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
      {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
      };

      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
      psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
      psoDesc.pRootSignature = mRootSignature.Get();
      psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
      psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
      psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
      psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
      psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.SampleMask = UINT_MAX;
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      psoDesc.NumRenderTargets = 1;
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
      psoDesc.SampleDesc.Count = 1;
      decaf_checkhr(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineState)));
   }

   // Create the command list.
   decaf_checkhr(mDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAllocator.Get(), mPipelineState.Get(), IID_PPV_ARGS(&mCmdList)));

   // Set up the vertex buffer we draw the screens with
   static const FLOAT vertices[] = {
      -1.0f,  -1.0f,   0.0f, 1.0f,
      1.0f,  -1.0f,   1.0f, 1.0f,
      1.0f, 1.0f,   1.0f, 0.0f,

      1.0f, 1.0f,   1.0f, 0.0f,
      -1.0f, 1.0f,   0.0f, 0.0f,
      -1.0f,  -1.0f,   0.0f, 1.0f,
   };

   auto vtxDataSize = 4 * 6 * sizeof(float);
   decaf_checkhr(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(vtxDataSize),
      D3D12_RESOURCE_STATE_GENERIC_READ,
      nullptr,
      IID_PPV_ARGS(&mVertexBuffer)));

   static const auto EmptyRange = CD3DX12_RANGE(0, 0);
   void *vtxData = nullptr;
   mVertexBuffer->Map(0, &EmptyRange, &vtxData);
   memcpy(vtxData, vertices, vtxDataSize);
   mVertexBuffer->Unmap(0, nullptr);

   // Set up any needed resources
   CD3DX12_CPU_DESCRIPTOR_HANDLE texCpuDesc(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
   CD3DX12_GPU_DESCRIPTOR_HANDLE texGpuDesc(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
   texCpuDesc.Offset(2, mSrvDescriptorSize);
   texGpuDesc.Offset(2, mSrvDescriptorSize);
   decaf::debugger::initialiseUiDX12(mDevice.Get(), mCmdList.Get(), texCpuDesc, texGpuDesc);

   // Close the command list
   decaf_checkhr(mCmdList->Close());

   // Execute the list to perform all initialisation stuff
   ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
   mCmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

   // Create synchronization objects.
   {
      decaf_checkhr(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
      mFenceValue = 1;

      // Create an event handle to use for frame synchronization.
      mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
      decaf_check(mFenceEvent);
   }

   // Wait for initialisation to complete
   waitForPreviousFrame();

   // Setup decaf driver
   auto dxDriver = decaf::createDX12Driver();
   decaf_check(dxDriver);
   mDecafDriver = reinterpret_cast<decaf::DX12Driver*>(dxDriver);

   // Initialise the driver
   mDecafDriver->initialise(mDevice.Get(), mCmdQueue.Get());

   // Start graphics thread
   if (!config::gpu::force_sync) {
      mGraphicsThread = std::thread {[this]() {
         mDecafDriver->run();
      } };
   }

   return true;
}

void DecafSDLDX12::waitForPreviousFrame()
{
   // Signal and increment the fence value.
   const UINT64 fence = mFenceValue;
   decaf_checkhr(mCmdQueue->Signal(mFence.Get(), fence));
   mFenceValue++;

   // Wait until the previous frame is finished.
   if (mFence->GetCompletedValue() < fence)
   {
      decaf_checkhr(mFence->SetEventOnCompletion(fence, mFenceEvent));
      WaitForSingleObject(mFenceEvent, INFINITE);
   }

   mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

void
DecafSDLDX12::shutdown()
{
}

void
DecafSDLDX12::windowResized()
{
   decaf_checkhr(mCmdAllocator->Reset());

   // Create a RTV for each frame.
   for (UINT n = 0; n < SwapBufferCount; n++)
   {
      mRenderTargets[n].Reset();
   }

   int width, height;
   SDL_GetWindowSize(mWindow, &width, &height);
   decaf_checkhr(mSwapChain->ResizeBuffers(SwapBufferCount, width, height, DXGI_FORMAT_UNKNOWN, 0));

   // Create frame resources.
   {
      CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

      // Create a RTV for each frame.
      for (UINT n = 0; n < SwapBufferCount; n++)
      {
         decaf_checkhr(mSwapChain->GetBuffer(n, IID_PPV_ARGS(&mRenderTargets[n])));
         mDevice->CreateRenderTargetView(mRenderTargets[n].Get(), nullptr, rtvHandle);
         rtvHandle.Offset(1, mRtvDescriptorSize);
      }
   }

   // Reset the frame index
   mFrameIndex = mSwapChain->GetCurrentBackBufferIndex();
}

void
DecafSDLDX12::drawScanBuffer(Viewport &vp, ID3D12Resource *texture, uint32_t bufferIdx)
{
   auto texCpuDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvHeap->GetCPUDescriptorHandleForHeapStart())
      .Offset(bufferIdx, mSrvDescriptorSize);
   auto texGpuDesc = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart())
      .Offset(bufferIdx, mSrvDescriptorSize);

   mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
      texture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

   mDevice->CreateShaderResourceView(texture, NULL, texCpuDesc);

   D3D12_VIEWPORT viewport;
   viewport.TopLeftX = vp.x;
   viewport.TopLeftY = vp.y;
   viewport.Width = vp.width;
   viewport.Height = vp.height;
   viewport.MinDepth = 0.0f;
   viewport.MaxDepth = 1.0f;

   D3D12_VERTEX_BUFFER_VIEW vbv;
   vbv.BufferLocation = mVertexBuffer->GetGPUVirtualAddress();
   vbv.StrideInBytes = 4 * sizeof(float);
   vbv.SizeInBytes = 6 * vbv.StrideInBytes;

   mCmdList->RSSetViewports(1, &viewport);
   mCmdList->SetGraphicsRootDescriptorTable(0, texGpuDesc);
   mCmdList->IASetVertexBuffers(0, 1, &vbv);
   mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   mCmdList->DrawInstanced(6, 1, 0, 0);

   mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
      texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));
}

void
DecafSDLDX12::drawScanBuffers(Viewport &tv, ID3D12Resource *tvBuffer, Viewport &drc, ID3D12Resource *drcBuffer)
{
   // Grab window information
   int width, height;
   SDL_GetWindowSize(mWindow, &width, &height);

   // Reset the basic stuff
   decaf_checkhr(mCmdAllocator->Reset());
   decaf_checkhr(mCmdList->Reset(mCmdAllocator.Get(), mPipelineState.Get()));

   mCmdList->SetPipelineState(mPipelineState.Get());

   mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

   CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), mFrameIndex, mRtvDescriptorSize);

   // Record commands.
   const float clearColor[] = { 0.6f, 0.2f, 0.2f, 1.0f };
   mCmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

   mCmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, NULL);

   if (tvBuffer || drcBuffer) {
      ID3D12DescriptorHeap* ppHeaps[] = { mSrvHeap.Get() };
      mCmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

      mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

      CD3DX12_RECT scissorRect(0, 0, width, height);
      mCmdList->RSSetScissorRects(1, &scissorRect);

      // TODO: Technically, we are generating these coordinates upside down, and then
      //  'correcting' it here.  We should probably generate these accurately, and then
      //  flip them for OpenGL, which is the API with the unintuitive origin.
      Viewport adjTv = tv;
      adjTv.y = height - (adjTv.y + adjTv.height);
      Viewport adjDrc = drc;
      adjDrc.y = height - (adjDrc.y + adjDrc.height);

      auto drawTV = adjTv.width > 0 && adjTv.height > 0;
      auto drawDRC = adjDrc.width > 0 && adjDrc.height > 0;

      if (tvBuffer && drawTV) {
         drawScanBuffer(adjTv, tvBuffer, 0);
      }

      if (drcBuffer && drawDRC) {
         drawScanBuffer(adjDrc, drcBuffer, 1);
      }
   }

   // Draw UI
   decaf::debugger::drawUiDX12(width, height, mDevice.Get(), mCmdList.Get());

   // Indicate that the back buffer will now be used to present.
   mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRenderTargets[mFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

   decaf_checkhr(mCmdList->Close());

   // Execute the command list.
   ID3D12CommandList* ppCommandLists[] = { mCmdList.Get() };
   mCmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

   // Present the frame.
   decaf_checkhr(mSwapChain->Present(1, 0));

   // Wait for completion
   waitForPreviousFrame();
}

void
DecafSDLDX12::renderFrame(Viewport &tv, Viewport &drc)
{
   if (!config::gpu::force_sync) {
      ID3D12Resource *tvBuffer = 0;
      ID3D12Resource *drcBuffer = 0;
      mDecafDriver->getSwapBuffers(&tvBuffer, &drcBuffer);
      drawScanBuffers(tv, tvBuffer, drc, drcBuffer);
   } else {
      mDecafDriver->syncPoll([&](ID3D12Resource *tvBuffer, ID3D12Resource *drcBuffer) {
         drawScanBuffers(tv, tvBuffer, drc, drcBuffer);
      });
   }
}

decaf::GraphicsDriver *
DecafSDLDX12::getDecafDriver()
{
   return mDecafDriver;
}

#endif // DECAF_DX12
