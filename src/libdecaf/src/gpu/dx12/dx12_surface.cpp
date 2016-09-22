#ifdef DECAF_DX12

#include "common/decaf_assert.h"
#include "decaf_config.h"
#include "dx12_driver.h"
#include "dx12_utilities.h"
#include "gpu/gpu_datahash.h"
#include "gpu/gpu_tiling.h"
#include "gpu/gpu_utilities.h"

namespace gpu
{

namespace dx12
{

SurfaceTexture *
Driver::createSurfaceTexture(latte::SQ_TEX_DIM dim,
                             latte::SQ_DATA_FORMAT format,
                             latte::SQ_NUM_FORMAT numFormat,
                             latte::SQ_FORMAT_COMP formatComp,
                             uint32_t degamma,
                             uint32_t width,
                             uint32_t height,
                             uint32_t depth,
                             uint32_t samples)
{
   auto dxDim = getDxSurfDim(dim);
   auto dxFormat = getDxSurfFormat(format, numFormat, formatComp, degamma);

   auto tex = new SurfaceTexture();

   D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;

   switch (format) {
   case latte::SQ_DATA_FORMAT::FMT_8:
   case latte::SQ_DATA_FORMAT::FMT_4_4:
   case latte::SQ_DATA_FORMAT::FMT_3_3_2:
   case latte::SQ_DATA_FORMAT::FMT_16:
   case latte::SQ_DATA_FORMAT::FMT_16_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_8_8:
   case latte::SQ_DATA_FORMAT::FMT_5_6_5:
   case latte::SQ_DATA_FORMAT::FMT_6_5_5:
   case latte::SQ_DATA_FORMAT::FMT_1_5_5_5:
   case latte::SQ_DATA_FORMAT::FMT_4_4_4_4:
   case latte::SQ_DATA_FORMAT::FMT_5_5_5_1:
   case latte::SQ_DATA_FORMAT::FMT_32:
   case latte::SQ_DATA_FORMAT::FMT_32_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_16_16:
   case latte::SQ_DATA_FORMAT::FMT_16_16_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_10_11_11:
   case latte::SQ_DATA_FORMAT::FMT_10_11_11_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_11_11_10:
   case latte::SQ_DATA_FORMAT::FMT_11_11_10_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_2_10_10_10:
   case latte::SQ_DATA_FORMAT::FMT_8_8_8_8:
   case latte::SQ_DATA_FORMAT::FMT_10_10_10_2:
   case latte::SQ_DATA_FORMAT::FMT_32_32:
   case latte::SQ_DATA_FORMAT::FMT_32_32_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_16_16_16_16:
   case latte::SQ_DATA_FORMAT::FMT_16_16_16_16_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_32_32_32_32:
   case latte::SQ_DATA_FORMAT::FMT_32_32_32_32_FLOAT:
   //case latte::SQ_DATA_FORMAT::FMT_1:
   //case latte::SQ_DATA_FORMAT::FMT_GB_GR:
   //case latte::SQ_DATA_FORMAT::FMT_BG_RG:
   //case latte::SQ_DATA_FORMAT::FMT_32_AS_8:
   //case latte::SQ_DATA_FORMAT::FMT_32_AS_8_8:
   //case latte::SQ_DATA_FORMAT::FMT_5_9_9_9_SHAREDEXP:
   case latte::SQ_DATA_FORMAT::FMT_8_8_8:
   case latte::SQ_DATA_FORMAT::FMT_16_16_16:
   case latte::SQ_DATA_FORMAT::FMT_16_16_16_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_32_32_32:
   case latte::SQ_DATA_FORMAT::FMT_32_32_32_FLOAT:
      resFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
      break;
   case latte::SQ_DATA_FORMAT::FMT_8_24:
   case latte::SQ_DATA_FORMAT::FMT_8_24_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_24_8:
   case latte::SQ_DATA_FORMAT::FMT_24_8_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_X24_8_32_FLOAT:
   case latte::SQ_DATA_FORMAT::FMT_BC1:
   case latte::SQ_DATA_FORMAT::FMT_BC2:
   case latte::SQ_DATA_FORMAT::FMT_BC3:
   case latte::SQ_DATA_FORMAT::FMT_BC4:
   case latte::SQ_DATA_FORMAT::FMT_BC5:
      // Cannot be render targets
      break;
   default:
      decaf_abort("Unexpected texture format");
   }

   // This logic is a bit strange, and only required for DX12...
   auto createWidth = width;
   auto createHeight = height;
   if (format >= SQ_DATA_FORMAT::FMT_BC1 && format <= SQ_DATA_FORMAT::FMT_BC5) {
      createWidth = align_up(createWidth, 4);
      createHeight = align_up(createHeight, 4);
   }

   // Describe and create a Texture2D.
   D3D12_RESOURCE_DESC textureDesc = {};
   textureDesc.Dimension = dxDim;
   textureDesc.Width = createWidth;
   textureDesc.Height = createHeight;
   textureDesc.DepthOrArraySize = depth;
   textureDesc.MipLevels = 1;
   textureDesc.Format = dxFormat;
   textureDesc.SampleDesc.Count = 1;
   textureDesc.SampleDesc.Quality = 0;
   textureDesc.Flags = resFlags;

   decaf_checkhr(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr,
      IID_PPV_ARGS(&tex->object)));

   if (decaf::config::gpu::debug) {
      tex->object->SetName(L"Texture Surface @ 0x????????");
   }

   tex->resourceState = D3D12_RESOURCE_STATE_COPY_DEST;
   tex->width = width;
   tex->height = height;
   tex->depth = depth;

   if (decaf::config::gpu::debug) {
      tex->dbgInfo.dim = dim;
      tex->dbgInfo.format = format;
      tex->dbgInfo.numFormat = numFormat;
      tex->dbgInfo.degamma = degamma;
      tex->dbgInfo.formatComp = formatComp;
   }

   return tex;
}

void
Driver::copySurfaceTexture(SurfaceTexture* dest,
                           SurfaceTexture *source,
                           latte::SQ_TEX_DIM dim)
{
   auto copyWidth = std::min(dest->width, source->width);
   auto copyHeight = std::min(dest->height, source->height);
   auto copyDepth = std::min(dest->depth, source->depth);

   decaf_check(copyWidth > 0);
   decaf_check(copyHeight > 0);
   decaf_check(copyDepth > 0);

   transitionSurfaceTexture(dest, D3D12_RESOURCE_STATE_COPY_DEST);
   transitionSurfaceTexture(source, D3D12_RESOURCE_STATE_COPY_SOURCE);

   mCmdList->CopyTextureRegion(
      &CD3DX12_TEXTURE_COPY_LOCATION(dest->object.Get(), 0),
      0, 0, 0,
      &CD3DX12_TEXTURE_COPY_LOCATION(source->object.Get(), 0),
      &CD3DX12_BOX(0, 0, 0, copyWidth, copyHeight, copyDepth));
}

void
Driver::uploadSurfaceTexture(SurfaceTexture *buffer,
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
                             latte::SQ_TILE_MODE tileMode)
{
   auto imagePtr = make_virtual_ptr<uint8_t>(baseAddress);
   auto bpp = getDataFormatBitsPerElement(format);
   auto srcWidth = width;
   auto srcHeight = height;
   auto srcPitch = pitch;
   auto uploadWidth = width;
   auto uploadHeight = height;
   auto uploadPitch = width;
   auto uploadDepth = depth;

   if (format >= latte::SQ_DATA_FORMAT::FMT_BC1 && format <= latte::SQ_DATA_FORMAT::FMT_BC5) {
      srcWidth = (srcWidth + 3) / 4;
      srcHeight = (srcHeight + 3) / 4;
      srcPitch = srcPitch / 4;
      uploadWidth = srcWidth * 4;
      uploadHeight = srcHeight * 4;
      uploadPitch = uploadWidth / 4;
   }

   if (dim == latte::SQ_TEX_DIM::DIM_CUBEMAP) {
      uploadDepth *= 6;
   }

   auto srcImageSize = srcPitch * srcHeight * uploadDepth * bpp / 8;
   auto dstImageSize = srcWidth * srcHeight * uploadDepth * bpp / 8;

   auto newHash = DataHash::hash(imagePtr, srcImageSize);
   if (buffer->cpuHash != newHash) {
      buffer->cpuHash = newHash;

      std::vector<uint8_t> untiledImage, untiledMipmap;
      untiledImage.resize(dstImageSize);

      // Untile
      gpu::convertFromTiled(
         untiledImage.data(),
         uploadPitch,
         imagePtr,
         tileMode,
         swizzle,
         srcPitch,
         srcWidth,
         srcHeight,
         uploadDepth,
         0,
         isDepthBuffer,
         bpp);

      // TODO: I expect that the subresource count here needs to be modified for... something?
      const UINT64 uploadBufferSize = GetRequiredIntermediateSize(buffer->object.Get(), 0, 1);
      auto uploadBuffer = allocateUploadBuffer(uploadBufferSize);

      D3D12_SUBRESOURCE_DATA textureData = {};
      textureData.pData = untiledImage.data();
      textureData.RowPitch = uploadPitch * bpp / 8;
      textureData.SlicePitch = untiledImage.size();

      transitionSurfaceTexture(buffer, D3D12_RESOURCE_STATE_COPY_DEST);
      UpdateSubresources(mCmdList, buffer->object.Get(), uploadBuffer, 0, 0, 1, &textureData);
   }
}

SurfaceObject *
Driver::getSurfaceBuffer(ppcaddr_t baseAddress,
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
                         bool discardData)
{
   decaf_check(baseAddress);
   decaf_check(width);
   decaf_check(height);
   decaf_check(depth);
   decaf_check(width <= 8192);
   decaf_check(height <= 8192);

   auto swizzle = baseAddress & 0xFFF;

   // Align the base address according to the GPU logic
   if (tileMode >= latte::SQ_TILE_MODE::TILED_2D_THIN1) {
      baseAddress &= ~(0x800 - 1);
   } else {
      baseAddress &= ~(0x100 - 1);
   }

   // The size key is selected based on which level the dims
   //  are compatible across.  Note that at some point, we may
   //  need to make format not be part of the key as well...
   auto surfaceKey = static_cast<uint64_t>(baseAddress) << 32;
   surfaceKey ^= format << 22 ^ numFormat << 28 ^ formatComp << 30;

   auto dimDimensions = getTexDimDimensions(dim);
   if (dimDimensions == 1) {
      // Nothing to save
   } else if (dimDimensions == 2) {
      surfaceKey ^= pitch;
   } else if (dimDimensions == 3) {
      surfaceKey ^= pitch ^ height << 16;
   } else {
      decaf_abort("Unexpected dim dimensions.");
   }

   auto &buffer = mSurfaces[surfaceKey];

   if (buffer.active &&
      buffer.active->width == width &&
      buffer.active->height == height &&
      buffer.active->depth == depth)
   {
      // We are already the active surface
   } else {
      SurfaceTexture *foundSurface = nullptr;
      SurfaceTexture *newMaster = nullptr;
      SurfaceTexture *newSurface = nullptr;

      if (!buffer.master) {
         // We are the first user of this surface, lets quickly set it up and
         //  allocate a host surface to use

         // Let's track some other useful information
         buffer.dbgInfo.dim = dim;
         buffer.dbgInfo.pitch = dimDimensions >= 2 ? pitch : 0;
         buffer.dbgInfo.height = dimDimensions >= 3 ? height : 0;
         buffer.dbgInfo.depth = 0;

         // Create our new surface
         auto newSurf = createSurfaceTexture(dim, format, numFormat, formatComp, degamma, width, height, depth, samples);

         // Set this new surface up as the newly active one
         buffer.active = newSurf;
         buffer.master = newSurf;

         // Mark this as the found surface
         foundSurface = newSurf;
      }

      if (!foundSurface) {
         // First lets check to see if we already have a surface created
         for (auto surf = buffer.master; surf != nullptr; surf = surf->next) {
            if (surf->width == width &&
               surf->height == height &&
               surf->depth == depth) {
               foundSurface = surf;
               break;
            }
         }
      }

      if (!foundSurface) {
         // Lets check if we need to build a new master surface
         auto masterWidth = width;
         auto masterHeight = height;
         auto masterDepth = depth;

         // It shouldn't be possible to not already have a master surface configured
         decaf_check(buffer.master);

         masterWidth = std::max(masterWidth, buffer.master->width);
         masterHeight = std::max(masterHeight, buffer.master->height);
         masterDepth = std::max(masterDepth, buffer.master->depth);

         if (buffer.master->width < masterWidth || buffer.master->height < masterHeight || buffer.master->depth < masterDepth) {
            newMaster = createSurfaceTexture(dim, format, numFormat, formatComp, degamma, masterWidth, masterHeight, masterDepth, samples);

            // Check if the new master we just made matches our size perfectly.
            if (width == masterWidth && height == masterHeight && depth == masterDepth) {
               foundSurface = newMaster;
            }
         }
      }

      if (!foundSurface) {
         // Lets finally just build our perfect surface...
         foundSurface = createSurfaceTexture(dim, format, numFormat, formatComp, degamma, width, height, depth, samples);
         newSurface = foundSurface;
      }

      // If the active surface is not the master surface, we first need
      //  to copy that surface up to the master
      if (buffer.active != buffer.master) {
         if (!discardData) {
            copySurfaceTexture(buffer.master, buffer.active, dim);
         }

         buffer.active = buffer.master;
      }

      // If we allocated a new master, we need to copy the old master to
      //  the new one.  Note that this can cause a HostSurface which only
      //  was acting as a surface to be orphaned for later GC.
      if (newMaster) {
         copySurfaceTexture(newMaster, buffer.active, dim);

         buffer.active = newMaster;
      }

      // Check to see if we have finally became the active surface, if we
      //   have not, we need to copy one last time... Lolcopies
      if (buffer.active != foundSurface) {
         if (!discardData) {
            copySurfaceTexture(foundSurface, buffer.active, dim);
         }

         buffer.active = foundSurface;
      }

      // If we have a new master surface, we need to put it at the top
      //  of the list (so its actually in the .master slot...)
      if (newMaster) {
         newMaster->next = buffer.master;
         buffer.master = newMaster;
      }

      // Other surfaces are just inserted after the master surface.
      if (newSurface) {
         newSurface->next = buffer.master->next;
         buffer.master->next = newSurface;
      }

      // Update the memory bounds to reflect this usage of the texture data
      // TODO: Yup...
   }

   if (buffer.needUpload) {
      if (!discardData) {
         uploadSurfaceTexture(
            buffer.active,
            baseAddress, swizzle, dim,
            format, numFormat, formatComp, degamma,
            pitch, width, height, depth,
            samples, isDepthBuffer, tileMode);
      }

      buffer.needUpload = false;
   }

   return &buffer;
}

void
Driver::transitionSurfaceTexture(SurfaceTexture *surface,
                                 D3D12_RESOURCE_STATES state)
{
   if (surface->resourceState == state) {
      // Already there, nothing to do!
      return;
   }

   mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
      surface->object.Get(), surface->resourceState, state));
   surface->resourceState = state;
}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
