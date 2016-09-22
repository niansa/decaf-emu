#pragma once
#include <cstdint>

#ifdef DECAF_DX12

#include <d3d12.h>

#endif

namespace decaf
{

namespace debugger
{

void
initialise();

#ifndef DECAF_NOGL

void
initialiseUiGL();

void
drawUiGL(uint32_t width, uint32_t height);

#endif // DECAF_NOGL

#ifdef DECAF_DX12

void
initialiseUiDX12(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, D3D12_CPU_DESCRIPTOR_HANDLE texCpuDesc, D3D12_GPU_DESCRIPTOR_HANDLE texGpuDesc);

void
drawUiDX12(uint32_t width, uint32_t height, ID3D12Device *device, ID3D12GraphicsCommandList *cmdList);

#endif

} // namespace debugger

} // namespace decaf
