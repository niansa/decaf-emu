#pragma once

#ifdef DECAF_DX12

#include "decaf_graphics.h"
#include <d3d12.h>

namespace decaf
{

class DX12Driver : public GraphicsDriver
{
public:
   using SwapFunction = std::function<void(ID3D12Resource *, ID3D12Resource *)>;

   virtual ~DX12Driver()
   {
   }

   virtual void initialise(ID3D12Device *device, ID3D12CommandQueue *queue) = 0;
   virtual void getSwapBuffers(ID3D12Resource **tv, ID3D12Resource **drc) = 0;
   virtual void syncPoll(const SwapFunction &swapFunc) = 0;

protected:

};

} // namespace decaf

#endif // DECAF_DX12