#ifdef DECAF_DX12

#include "common/decaf_assert.h"
#include "decaf_config.h"
#include "dx12_driver.h"
#include "modules/gx2/gx2_event.h"

namespace gpu
{

namespace dx12
{

void
Driver::decafSetBuffer(const pm4::DecafSetBuffer &data)
{
   ensureDescriptorHeaps();

   auto chain = data.isTv ? &mTvScanBuffers : &mDrcScanBuffers;

   if (chain->object) {
      chain->object.Reset();
   }

   // Describe and create a Texture2D.
   D3D12_RESOURCE_DESC textureDesc = {};
   textureDesc.MipLevels = 1;
   textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   textureDesc.Width = data.width;
   textureDesc.Height = data.height;
   textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
   textureDesc.DepthOrArraySize = 1;
   textureDesc.SampleDesc.Count = 1;
   textureDesc.SampleDesc.Quality = 0;
   textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

   decaf_checkhr(mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
      D3D12_HEAP_FLAG_NONE,
      &textureDesc,
      D3D12_RESOURCE_STATE_RENDER_TARGET,
      nullptr,
      IID_PPV_ARGS(&chain->object)));

   chain->width = data.width;
   chain->height = data.height;

   auto rtvHandle = allocateRtvs(1);
   mDevice->CreateRenderTargetView(chain->object.Get(), NULL, rtvHandle);

   const float clearColor[] = { 0.7f, 0.3f, 0.3f, 1.0f };
   mCmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

   if (decaf::config::gpu::debug) {
      if (data.isTv) {
         chain->object->SetName(L"TV ScanBuffer");
      } else {
         chain->object->SetName(L"DRC ScanBuffer");
      }
   }
}

enum
{
   SCANTARGET_TV = 1,
   SCANTARGET_DRC = 4,
};

void
Driver::decafCopyColorToScan(const pm4::DecafCopyColorToScan &data)
{
   ensureDescriptorHeaps();

   auto buffer = getColorBuffer(data.cb_color_base, data.cb_color_size, data.cb_color_info, false);
   ScanBufferChain *target = nullptr;

   if (data.scanTarget == SCANTARGET_TV) {
      target = &mTvScanBuffers;
   } else if (data.scanTarget == SCANTARGET_DRC) {
      target = &mDrcScanBuffers;
   } else {
      decaf_abort("decafCopyColorToScan called for unknown scanTarget");
   }

   transitionSurfaceTexture(buffer, D3D12_RESOURCE_STATE_COPY_SOURCE);

   blitTextureRegion(
      CD3DX12_TEXTURE_COPY_LOCATION(target->object.Get(), 0),
      CD3DX12_BOX(0, 0, 0, target->width, target->height, 1),
      CD3DX12_TEXTURE_COPY_LOCATION(buffer->object.Get(), 0),
      CD3DX12_BOX(0, 0, 0, buffer->width, buffer->height, 1));
}

void
Driver::decafSwapBuffers(const pm4::DecafSwapBuffers &data)
{
   static const auto weight = 0.9;

   mFrameData->callbacks.push_back([=]() {
      gx2::internal::onFlip();

      auto now = std::chrono::system_clock::now();

      if (mLastSwap.time_since_epoch().count()) {
         mAverageFrameTime = weight * mAverageFrameTime + (1.0 - weight) * (now - mLastSwap);
      }

      mLastSwap = now;

      if (mSwapFunc) {
         mSwapFunc(mTvScanBuffers.object.Get(), mDrcScanBuffers.object.Get());
      }
   });
}

void
Driver::decafCapSyncRegisters(const pm4::DecafCapSyncRegisters &data)
{
}

void
Driver::decafSetSwapInterval(const pm4::DecafSetSwapInterval &data)
{
}

void
Driver::decafDebugMarker(const pm4::DecafDebugMarker &data)
{
}

void
Driver::decafOSScreenFlip(const pm4::DecafOSScreenFlip &data)
{
}

void
Driver::decafCopySurface(const pm4::DecafCopySurface &data)
{
}

void
Driver::surfaceSync(const pm4::SurfaceSync &data)
{
}

void
Driver::memWrite(const pm4::MemWrite &data)
{
}

void
Driver::eventWrite(const pm4::EventWrite &data)
{
}

void
Driver::eventWriteEOP(const pm4::EventWriteEOP &data)
{
}

void
Driver::pfpSyncMe(const pm4::PfpSyncMe &data)
{
}

template<bool IsRects, typename IndexType>
static void
unpackQuadRectList(uint32_t count,
                   const IndexType *src,
                   IndexType *dst)
{
   // Unpack quad indices into triangle indices
   if (src) {
      for (auto i = 0u; i < count / 4; ++i) {
         auto index_0 = *src++;
         auto index_1 = *src++;
         auto index_2 = *src++;
         auto index_3 = *src++;

         *(dst++) = index_0;
         *(dst++) = index_1;
         *(dst++) = index_2;

         if (!IsRects) {
            *(dst++) = index_0;
            *(dst++) = index_2;
            *(dst++) = index_3;
         } else {
            // Rectangles use a different winding order apparently...
            *(dst++) = index_2;
            *(dst++) = index_1;
            *(dst++) = index_3;
         }
      }
   } else {
      auto index_0 = 0u;
      auto index_1 = 1u;
      auto index_2 = 2u;
      auto index_3 = 3u;

      for (auto i = 0u; i < count / 4; ++i) {
         auto index = i * 4;
         *(dst++) = index_0 + index;
         *(dst++) = index_1 + index;
         *(dst++) = index_2 + index;

         if (!IsRects) {
            *(dst++) = index_0 + index;
            *(dst++) = index_2 + index;
            *(dst++) = index_3 + index;
         } else {
            // Rectangles use a different winding order apparently...
            *(dst++) = index_2 + index;
            *(dst++) = index_1 + index;
            *(dst++) = index_3 + index;
         }
      }
   }
}

void
Driver::drawGenericIndexed(uint32_t numIndices, void *indices)
{
   // These are up here because it is imperative they persist for the whole method
   std::vector<uint8_t> swappedIndices;
   std::vector<uint8_t> quadIndices;

   auto vgt_dma_index_type = getRegister<latte::VGT_DMA_INDEX_TYPE>(latte::Register::VGT_DMA_INDEX_TYPE);

   auto indexType = vgt_dma_index_type.INDEX_TYPE();

   if (!indices) {
      // If there were no indexes passed, we force the index type to 32-bit
      //  for auto-generation to avoid running out of indices...
      indexType = latte::VGT_INDEX_TYPE::INDEX_32;
   }

   uint32_t indexBytes;
   if (indexType == latte::VGT_INDEX_TYPE::INDEX_16) {
      indexBytes = numIndices * sizeof(uint16_t);
   } else if (indexType == latte::VGT_INDEX_TYPE::INDEX_32) {
      indexBytes = numIndices * sizeof(uint32_t);
   } else {
      decaf_abort("Unexpected index type");
   }

   if (indices) {
      auto swapMode = vgt_dma_index_type.SWAP_MODE();

      if (swapMode == latte::VGT_DMA_SWAP::SWAP_16_BIT) {
         swappedIndices.resize(indexBytes);

         auto swapSrc = reinterpret_cast<uint16_t *>(indices);
         auto swapDest = reinterpret_cast<uint16_t *>(swappedIndices.data());
         for (auto i = 0; i < indexBytes / sizeof(uint16_t); ++i) {
            swapDest[i] = byte_swap(swapSrc[i]);
         }

         indices = swappedIndices.data();
      } else if (vgt_dma_index_type.SWAP_MODE() == latte::VGT_DMA_SWAP::SWAP_32_BIT) {
         swappedIndices.resize(indexBytes);

         auto swapSrc = reinterpret_cast<uint32_t *>(indices);
         auto swapDest = reinterpret_cast<uint32_t *>(swappedIndices.data());
         for (auto i = 0; i < indexBytes / sizeof(uint32_t); ++i) {
            swapDest[i] = byte_swap(swapSrc[i]);
         }

         indices = swappedIndices.data();
      } else if (vgt_dma_index_type.SWAP_MODE() == latte::VGT_DMA_SWAP::NONE) {
         // Nothing to do here!
      } else {
         decaf_abort(fmt::format("Unimplemented vgt_dma_index_type.SWAP_MODE {}", vgt_dma_index_type.SWAP_MODE()));
      }
   }

   auto vgt_primitive_type = getRegister<latte::VGT_PRIMITIVE_TYPE>(latte::Register::VGT_PRIMITIVE_TYPE);
   auto primType = vgt_primitive_type.PRIM_TYPE();


   // Handle the screen space bit...
   if (primType == latte::VGT_DI_PRIMITIVE_TYPE::RECTLIST) {
      decaf_check(mActiveVertexShader->desc.isScreenSpace);

      auto cb_color_base = getRegister<latte::CB_COLORN_BASE>(latte::Register::CB_COLOR0_BASE);
      auto cb_color_size = getRegister<latte::CB_COLORN_SIZE>(latte::Register::CB_COLOR0_SIZE);
      auto cb_color_info = getRegister<latte::CB_COLORN_INFO>(latte::Register::CB_COLOR0_INFO);

      float screenMul[] = { 2, 2 };

      if (cb_color_base.BASE_256B()) {
         auto surface = getColorBuffer(cb_color_base, cb_color_size, cb_color_info, false);
         screenMul[0] = static_cast<float>(surface->width) / 2;
         screenMul[1] = static_cast<float>(surface->height) / 2;
      }

	  mCmdList->SetGraphicsRoot32BitConstants(getDescriptorIndex(DescriptorType::SystemData), 2, &screenMul, 0);
   }


   // Maybe adjust the type
   if (primType == latte::VGT_DI_PRIMITIVE_TYPE::RECTLIST) {
      quadIndices.resize(indexBytes / 4 * 6);

      if (indexType == latte::VGT_INDEX_TYPE::INDEX_16) {
         unpackQuadRectList<true>(numIndices,
            reinterpret_cast<uint16_t*>(indices),
            reinterpret_cast<uint16_t*>(quadIndices.data()));
      } else if (indexType == latte::VGT_INDEX_TYPE::INDEX_32) {
         unpackQuadRectList<true>(numIndices,
            reinterpret_cast<uint32_t*>(indices),
            reinterpret_cast<uint32_t*>(quadIndices.data()));
      } else {
         decaf_abort("Unexpected index type");
      }

      primType = latte::VGT_DI_PRIMITIVE_TYPE::TRILIST;
      indices = quadIndices.data();
      numIndices = numIndices / 4 * 6;
      indexBytes = indexBytes / 4 * 6;
   } else if (primType == latte::VGT_DI_PRIMITIVE_TYPE::QUADLIST) {
      quadIndices.resize(indexBytes / 4 * 6);

      if (indexType == latte::VGT_INDEX_TYPE::INDEX_16) {
         unpackQuadRectList<false>(numIndices,
            reinterpret_cast<uint16_t*>(indices),
            reinterpret_cast<uint16_t*>(quadIndices.data()));
      } else if (indexType == latte::VGT_INDEX_TYPE::INDEX_32) {
         unpackQuadRectList<false>(numIndices,
            reinterpret_cast<uint32_t*>(indices),
            reinterpret_cast<uint32_t*>(quadIndices.data()));
      } else {
         decaf_abort("Unexpected index type");
      }

      primType = latte::VGT_DI_PRIMITIVE_TYPE::TRILIST;
      indices = quadIndices.data();
      numIndices = numIndices / 4 * 6;
      indexBytes = indexBytes / 4 * 6;
   }

   if (indices) {
      auto indexUploadBuffer = allocateUploadBuffer(indexBytes);

      static const CD3DX12_RANGE EmptyRange = { 0, 0 };
      void *indicesPtr;
      indexUploadBuffer->Map(0, &EmptyRange, &indicesPtr);

      memcpy(indicesPtr, indices, indexBytes);

      indexUploadBuffer->Unmap(0, nullptr);

      D3D12_INDEX_BUFFER_VIEW idxview;
      idxview.BufferLocation = indexUploadBuffer->GetGPUVirtualAddress();
      idxview.SizeInBytes = indexBytes;

      if (indexType == latte::VGT_INDEX_TYPE::INDEX_16) {
         idxview.Format = DXGI_FORMAT_R16_UINT;
      } else if (indexType == latte::VGT_INDEX_TYPE::INDEX_32) {
         idxview.Format = DXGI_FORMAT_R32_UINT;
      } else {
         decaf_abort("Unexpected index type");
      }

      mCmdList->IASetIndexBuffer(&idxview);
   } else {
      mCmdList->IASetIndexBuffer(nullptr);
   }


   switch (primType) {
   case latte::VGT_DI_PRIMITIVE_TYPE::POINTLIST:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::LINELIST:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::LINESTRIP:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::TRILIST:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::TRISTRIP:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::LINELIST_ADJ:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::LINESTRIP_ADJ:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::TRILIST_ADJ:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::TRISTRIP_ADJ:
      mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ);
      break;
   case latte::VGT_DI_PRIMITIVE_TYPE::RECTLIST:
   case latte::VGT_DI_PRIMITIVE_TYPE::QUADLIST:
      // This should have been adjusted above...
      decaf_abort("Impossibly found RECTLIST or QUADLIST primitives");
   //case latte::VGT_DI_PRIMITIVE_TYPE::NONE:
   //case latte::VGT_DI_PRIMITIVE_TYPE::QUADSTRIP:
   //case latte::VGT_DI_PRIMITIVE_TYPE::TRI_WITH_WFLAGS:
   //case latte::VGT_DI_PRIMITIVE_TYPE::LINELOOP:
   //case latte::VGT_DI_PRIMITIVE_TYPE::TRIFAN:
   //case latte::VGT_DI_PRIMITIVE_TYPE::POLYGON:
   //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V0:
   //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V1:
   //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V2:
   //case latte::VGT_DI_PRIMITIVE_TYPE::COPY_RECT_LIST_2D_V3:
   //case latte::VGT_DI_PRIMITIVE_TYPE::FILL_RECT_LIST_2D:
   //case latte::VGT_DI_PRIMITIVE_TYPE::LINE_STRIP_2D:
   //case latte::VGT_DI_PRIMITIVE_TYPE::TRI_STRIP_2D:
   default:
      decaf_abort("Unexpected primitive type");
   }

   if (mActivePixelShader->desc.alphaRefFunc != latte::REF_FUNC::ALWAYS) {
	   float alphaRef = getRegister<float>(latte::Register::SX_ALPHA_REF);
	   mCmdList->SetGraphicsRoot32BitConstants(getDescriptorIndex(DescriptorType::SystemData), 1, &alphaRef, 2);
   }

   auto vgt_dma_num_instances = getRegister<latte::VGT_DMA_NUM_INSTANCES>(latte::Register::VGT_DMA_NUM_INSTANCES);
   auto sq_vtx_base_vtx_loc = getRegister<latte::SQ_VTX_BASE_VTX_LOC>(latte::Register::SQ_VTX_BASE_VTX_LOC);
   auto sq_vtx_start_inst_loc = getRegister<latte::SQ_VTX_START_INST_LOC>(latte::Register::SQ_VTX_START_INST_LOC);

   auto baseVertex = sq_vtx_base_vtx_loc.OFFSET();
   auto numInstances = vgt_dma_num_instances.NUM_INSTANCES();
   auto baseInstance = sq_vtx_start_inst_loc.OFFSET();

   if (indices) {
      mCmdList->DrawIndexedInstanced(numIndices, numInstances, 0, baseVertex, baseInstance);
   } else {
      mCmdList->DrawInstanced(numIndices, numInstances, baseVertex, baseInstance);
   }
}

void
Driver::drawIndexAuto(const pm4::DrawIndexAuto &data)
{
   ensureDescriptorHeaps();

   if (!checkReadyDraw()) {
      return;
   }

   drawGenericIndexed(data.count, nullptr);
}

void
Driver::drawIndex2(const pm4::DrawIndex2 &data)
{
   ensureDescriptorHeaps();

   if (!checkReadyDraw()) {
      return;
   }

   drawGenericIndexed(data.count, data.addr.get());
}

void
Driver::drawIndexImmd(const pm4::DrawIndexImmd &data)
{
   ensureDescriptorHeaps();

   if (!checkReadyDraw()) {
      return;
   }

   drawGenericIndexed(data.count, data.indices.data());
}

void
Driver::streamOutBaseUpdate(const pm4::StreamOutBaseUpdate &data)
{
}

void
Driver::streamOutBufferUpdate(const pm4::StreamOutBufferUpdate &data)
{
}

void
Driver::decafClearColor(const pm4::DecafClearColor &data)
{
   ensureDescriptorHeaps();

   // Find our colorbuffer to clear
   auto buffer = getColorBuffer(data.cb_color_base, data.cb_color_size, data.cb_color_info, true);

   transitionSurfaceTexture(buffer, D3D12_RESOURCE_STATE_RENDER_TARGET);

   auto rtvHandle = allocateRtvs(1);
   mDevice->CreateRenderTargetView(buffer->object.Get(), NULL, rtvHandle);

   float colors[] = {
      data.red,
      data.green,
      data.blue,
      data.alpha
   };
   mCmdList->ClearRenderTargetView(rtvHandle, colors, 0, nullptr);
}

void
Driver::decafClearDepthStencil(const pm4::DecafClearDepthStencil &data)
{

}

} // namespace dx12

} // namespace gpu

#endif // DECAF_DX12
