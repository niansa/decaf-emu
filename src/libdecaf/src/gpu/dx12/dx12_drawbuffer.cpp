#ifdef DECAF_DX12

#include "dx12_driver.h"
#include "gpu/gpu_utilities.h"

namespace gpu
{

namespace dx12
{

SurfaceTexture *
Driver::getColorBuffer(latte::CB_COLORN_BASE cb_color_base,
                       latte::CB_COLORN_SIZE cb_color_size,
                       latte::CB_COLORN_INFO cb_color_info,
                       bool discardData)
{
   auto baseAddress = (cb_color_base.BASE_256B() << 8) & 0xFFFFF800;
   auto pitch_tile_max = cb_color_size.PITCH_TILE_MAX();
   auto slice_tile_max = cb_color_size.SLICE_TILE_MAX();

   auto pitch = static_cast<uint32_t>((pitch_tile_max + 1) * latte::MicroTileWidth);
   auto height = static_cast<uint32_t>(((slice_tile_max + 1) * (latte::MicroTileWidth * latte::MicroTileHeight)) / pitch);

   auto format = getColorBufferDataFormat(cb_color_info.FORMAT(), cb_color_info.NUMBER_TYPE());
   auto tileMode = getArrayModeTileMode(cb_color_info.ARRAY_MODE());

   auto buffer = getSurfaceBuffer(
      baseAddress, latte::SQ_TEX_DIM::DIM_2D,
      format.format, format.numFormat, format.formatComp, format.degamma,
      pitch, pitch, height, 1, 0, false, tileMode, discardData);

   return buffer->active;
}

SurfaceTexture *
Driver::getDepthBuffer(latte::DB_DEPTH_BASE db_depth_base,
                       latte::DB_DEPTH_SIZE db_depth_size,
                       latte::DB_DEPTH_INFO db_depth_info,
                       bool discardData)
{
   auto baseAddress = (db_depth_base.BASE_256B() << 8) & 0xFFFFF800;
   auto pitch_tile_max = db_depth_size.PITCH_TILE_MAX();
   auto slice_tile_max = db_depth_size.SLICE_TILE_MAX();

   auto pitch = static_cast<uint32_t>((pitch_tile_max + 1) * latte::MicroTileWidth);
   auto height = static_cast<uint32_t>(((slice_tile_max + 1) * (latte::MicroTileWidth * latte::MicroTileHeight)) / pitch);

   auto format = getDepthBufferDataFormat(db_depth_info.FORMAT());
   auto tileMode = getArrayModeTileMode(db_depth_info.ARRAY_MODE());

   auto buffer = getSurfaceBuffer(
      baseAddress, latte::SQ_TEX_DIM::DIM_2D,
      format.format, format.numFormat, format.formatComp, format.degamma,
      pitch, pitch, height, 1, 0, true, tileMode, discardData);

   return buffer->active;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
