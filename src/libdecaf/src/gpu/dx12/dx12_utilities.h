#pragma once

#ifdef DECAF_DX12

#include "gpu/latte_enum_sq.h"
#include <d3d12.h>

namespace gpu
{

namespace dx12
{

D3D12_RESOURCE_DIMENSION
getDxSurfDim(latte::SQ_TEX_DIM dim);

DXGI_FORMAT
getDxSurfFormat(latte::SQ_DATA_FORMAT format,
                latte::SQ_NUM_FORMAT numFormat,
                latte::SQ_FORMAT_COMP formatComp,
                uint32_t degamma);

} // namespace dx12

} // namespace gpu

#endif
