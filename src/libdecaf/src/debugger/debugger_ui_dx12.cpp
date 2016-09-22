#ifdef DECAF_DX12

#include "common/d3dx12.h"
#include "common/decaf_assert.h"
#include "debugger/debugger_ui.h"
#include "decaf.h"
#include "decaf_debugger.h"
#include <d3dcompiler.h>
#include <imgui.h>
#include <wrl.h>

using namespace Microsoft::WRL;

#define decaf_checkhr(op) \
   { HRESULT hr = (op); if (FAILED(hr)) decaf_abort(fmt::format("{}\nfailed with status {:08X}", ##op, (uint32_t)hr)); }

namespace decaf
{

namespace debugger
{

struct DebugDrawData
{
   ComPtr<ID3D12RootSignature> rootSignature;
   ComPtr<ID3D12PipelineState> pipelineState;
   ComPtr<ID3D12Resource> texture;
   ComPtr<ID3D12Resource> textureUpload;
   ComPtr<ID3D12Resource> vertexBuffer;
   ComPtr<ID3D12Resource> indexBuffer;
   INT vertexBufferSize = 0;
   INT indexBufferSize = 0;
   D3D12_CPU_DESCRIPTOR_HANDLE textureSrvCpuDescHandle = {};
   D3D12_GPU_DESCRIPTOR_HANDLE textureSrvGpuDescHandle = {};
};

static DebugDrawData
sDebugDrawData;

void
initialiseUiDX12(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, D3D12_CPU_DESCRIPTOR_HANDLE texCpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE texGpuDesc)
{
   auto &data = sDebugDrawData;
   ImGuiIO& io = ImGui::GetIO();

   auto configPath = makeConfigPath("imgui.ini");
   ::debugger::ui::initialise(configPath);

   data.textureSrvCpuDescHandle = texCpuDesc;
   data.textureSrvGpuDescHandle = texGpuDesc;

   // Create the root signature.
   {
      CD3DX12_DESCRIPTOR_RANGE ranges[1];
      ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

      CD3DX12_ROOT_PARAMETER rootParameters[2];
      rootParameters[0].InitAsConstants(16, 0, 0);
      rootParameters[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

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
      decaf_checkhr(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&data.rootSignature)));
   }

   // Create the pipeline state.
   {
      static const char* shaders =
         "cbuffer vertexBuffer : register(b0) \n\
         {\n\
            float4x4 ProjectionMatrix; \n\
         };\n\
         struct VS_INPUT\n\
         {\n\
            float2 pos : POSITION;\n\
            float2 uv  : TEXCOORD0;\n\
            float4 col : COLOR0;\n\
         };\n\
         \n\
         struct PS_INPUT\n\
         {\n\
            float4 pos : SV_POSITION;\n\
            float4 col : COLOR0;\n\
            float2 uv  : TEXCOORD0;\n\
         };\n\
         \n\
         PS_INPUT VSMain(VS_INPUT input)\n\
         {\n\
         PS_INPUT output;\n\
         output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\n\
         output.col = input.col;\n\
         output.uv  = input.uv;\n\
         return output;\n\
         }\n\
         \n\
         SamplerState sampler0 : register(s0);\n\
         Texture2D texture0 : register(t0);\n\
         \n\
         float4 PSMain(PS_INPUT input) : SV_TARGET\n\
         {\n\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv);\n\
            return out_col;\n\
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

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
      D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
      {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, OFFSETOF(ImDrawVert, pos), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, OFFSETOF(ImDrawVert, uv), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, OFFSETOF(ImDrawVert, col), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
      };
#undef OFFSETOF

      D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
      psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
      psoDesc.pRootSignature = data.rootSignature.Get();
      psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
      psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
      psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
      psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
      psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
      psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
      psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
      psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
      psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
      psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
      psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

      psoDesc.DepthStencilState.DepthEnable = FALSE;
      psoDesc.DepthStencilState.StencilEnable = FALSE;
      psoDesc.SampleMask = UINT_MAX;
      psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      psoDesc.NumRenderTargets = 1;
      psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
      psoDesc.SampleDesc.Count = 1;
      decaf_checkhr(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&data.pipelineState)));
   }

   // Upload the font texture
   {
      unsigned char* pixels;
      int width, height;
      io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

      // Describe and create a Texture2D.
      D3D12_RESOURCE_DESC textureDesc = {};
      textureDesc.MipLevels = 1;
      textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      textureDesc.Width = width;
      textureDesc.Height = height;
      textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
      textureDesc.DepthOrArraySize = 1;
      textureDesc.SampleDesc.Count = 1;
      textureDesc.SampleDesc.Quality = 0;
      textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

      decaf_checkhr(device->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
         D3D12_HEAP_FLAG_NONE,
         &textureDesc,
         D3D12_RESOURCE_STATE_COPY_DEST,
         nullptr,
         IID_PPV_ARGS(&data.texture)));

      const UINT64 uploadBufferSize = GetRequiredIntermediateSize(data.texture.Get(), 0, 1);

      // Create the GPU upload buffer.
      decaf_checkhr(device->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
         D3D12_HEAP_FLAG_NONE,
         &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
         D3D12_RESOURCE_STATE_GENERIC_READ,
         nullptr,
         IID_PPV_ARGS(&data.textureUpload)));

      D3D12_SUBRESOURCE_DATA textureData = {};
      textureData.pData = pixels;
      textureData.RowPitch = width * 4;
      textureData.SlicePitch = textureData.RowPitch * height;

      UpdateSubresources(cmdList, data.texture.Get(), data.textureUpload.Get(), 0, 0, 1, &textureData);
      cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(data.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

      // Create texture view
      device->CreateShaderResourceView(data.texture.Get(), NULL, data.textureSrvCpuDescHandle);

      // Store our identifier
      static_assert(sizeof(void*) >= sizeof(data.textureSrvGpuDescHandle.ptr), "Can't pack descriptor handle into TexID");
      ImGui::GetIO().Fonts->TexID = (void*)data.textureSrvGpuDescHandle.ptr;
   }
}

void
drawUiDX12(uint32_t width, uint32_t height, ID3D12Device *device, ID3D12GraphicsCommandList *cmdList)
{
   auto &data = sDebugDrawData;
   ImGuiIO& io = ImGui::GetIO();

   // Destroy the upload heap when we draw the first time...
   if (data.textureUpload) {
      data.textureUpload.Reset();
   }

   // Update some per-frame state information
   io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
   io.DeltaTime = 1.0f / 60.0f;
   ::debugger::ui::updateInput();

   // Start the frame
   ImGui::NewFrame();

   ::debugger::ui::draw();

   ImGui::Render();

   auto drawData = ImGui::GetDrawData();
   int fbWidth = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
   int fbHeight = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
   drawData->ScaleClipRects(io.DisplayFramebufferScale);

   if (!data.vertexBuffer || data.vertexBufferSize < drawData->TotalVtxCount) {
      if (data.vertexBuffer) {
         data.vertexBuffer.Reset();
      }

      data.vertexBufferSize = drawData->TotalVtxCount + 5000;
      decaf_checkhr(device->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
         D3D12_HEAP_FLAG_NONE,
         &CD3DX12_RESOURCE_DESC::Buffer(data.vertexBufferSize * sizeof(ImDrawVert)),
         D3D12_RESOURCE_STATE_GENERIC_READ,
         nullptr,
         IID_PPV_ARGS(&data.vertexBuffer)));
   }

   if (!data.indexBuffer || data.indexBufferSize < drawData->TotalIdxCount) {
      if (data.indexBuffer) {
         data.indexBuffer.Reset();
      }

      data.indexBufferSize = drawData->TotalIdxCount + 10000;
      decaf_checkhr(device->CreateCommittedResource(
         &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
         D3D12_HEAP_FLAG_NONE,
         &CD3DX12_RESOURCE_DESC::Buffer(data.indexBufferSize * sizeof(ImDrawIdx)),
         D3D12_RESOURCE_STATE_GENERIC_READ,
         nullptr,
         IID_PPV_ARGS(&data.indexBuffer)));
   }

   static const CD3DX12_RANGE EmptyRange(0, 0);

   void *vtxData = nullptr;
   decaf_checkhr(data.vertexBuffer->Map(0, &EmptyRange, &vtxData));

   void *idxData = nullptr;
   decaf_checkhr(data.indexBuffer->Map(0, &EmptyRange, &idxData));

   auto vtxDataPtr = reinterpret_cast<ImDrawVert*>(vtxData);
   auto idxDataPtr = reinterpret_cast<ImDrawIdx*>(idxData);

   for (auto i = 0; i < drawData->CmdListsCount; ++i) {
      const ImDrawList *imDrawList = drawData->CmdLists[i];
      memcpy(vtxDataPtr, &imDrawList->VtxBuffer[0], imDrawList->VtxBuffer.size() * sizeof(ImDrawVert));
      memcpy(idxDataPtr, &imDrawList->IdxBuffer[0], imDrawList->IdxBuffer.size() * sizeof(ImDrawIdx));
      vtxDataPtr += imDrawList->VtxBuffer.size();
      idxDataPtr += imDrawList->IdxBuffer.size();
   }

   data.vertexBuffer->Unmap(0, &EmptyRange);
   data.indexBuffer->Unmap(0, &EmptyRange);

   // Setup orthographic projection matrix into our constant buffer
   FLOAT proj_matrix[4][4] = {};
   {
      const float L = 0.0f;
      const float R = ImGui::GetIO().DisplaySize.x;
      const float B = ImGui::GetIO().DisplaySize.y;
      const float T = 0.0f;
      const float mvp[4][4] =
      {
         { 2.0f / (R - L),   0.0f,           0.0f,       0.0f },
         { 0.0f,         2.0f / (T - B),     0.0f,       0.0f },
         { 0.0f,         0.0f,           0.5f,       0.0f },
         { (R + L) / (L - R),  (T + B) / (B - T),    0.5f,       1.0f },
      };
      memcpy(proj_matrix, mvp, sizeof(mvp));
   }

   D3D12_VIEWPORT viewport = {
      0.0f, 0.0f,
      (float)fbWidth, (float)fbHeight,
      0.0f, 1.0f
   };

   // Bind shader and vertex buffers
   D3D12_VERTEX_BUFFER_VIEW vbv = {};
   vbv.BufferLocation = data.vertexBuffer->GetGPUVirtualAddress();
   vbv.SizeInBytes = data.vertexBufferSize * sizeof(ImDrawVert);
   vbv.StrideInBytes = sizeof(ImDrawVert);

   D3D12_INDEX_BUFFER_VIEW ibv = {};
   ibv.BufferLocation = data.indexBuffer->GetGPUVirtualAddress();
   ibv.SizeInBytes = data.indexBufferSize * sizeof(ImDrawIdx);
   ibv.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

   FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };

   cmdList->SetPipelineState(data.pipelineState.Get());
   cmdList->SetGraphicsRootSignature(data.rootSignature.Get());
   cmdList->SetGraphicsRoot32BitConstants(0, 16, &proj_matrix, 0);
   cmdList->IASetIndexBuffer(&ibv);
   cmdList->IASetVertexBuffers(0, 1, &vbv);
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->RSSetViewports(1, &viewport);
   cmdList->OMSetBlendFactor(blendFactor);

   // Render command lists
   int vtxOffset = 0;
   int idxOffset = 0;
   for (int n = 0; n < drawData->CmdListsCount; n++)
   {
      const ImDrawList* imCmdList = drawData->CmdLists[n];
      for (int cmd_i = 0; cmd_i < imCmdList->CmdBuffer.size(); cmd_i++)
      {
         const ImDrawCmd* pcmd = &imCmdList->CmdBuffer[cmd_i];
         if (pcmd->UserCallback)
         {
            pcmd->UserCallback(imCmdList, pcmd);
         } else
         {
            D3D12_RECT scissorRect = { (LONG)pcmd->ClipRect.x, (LONG)pcmd->ClipRect.y, (LONG)pcmd->ClipRect.z, (LONG)pcmd->ClipRect.w };

            D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = {};
            srvHandle.ptr = (UINT64)pcmd->TextureId;

            cmdList->SetGraphicsRootDescriptorTable(1, srvHandle);
            cmdList->RSSetScissorRects(1, &scissorRect);
            cmdList->DrawIndexedInstanced(pcmd->ElemCount, 1, idxOffset, vtxOffset, 0);
         }
         idxOffset += pcmd->ElemCount;
      }
      vtxOffset += imCmdList->VtxBuffer.size();
   }

}

} // namespace debugger

} // namespace decaf

#endif // DECAF_DX12
