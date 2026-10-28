/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
#include "d3d11_surface.hpp"
#include "com/com_object.hpp"
#include "d3d11_resource.hpp"
#include <algorithm>
#include <cstring>

namespace dxmt {

D3D11DXGISurface::D3D11DXGISurface(ID3D11Resource *pResource) :
    resource_(pResource),
    subresource_(0),
    surface_from_subresource_(false) {}

D3D11DXGISurface::D3D11DXGISurface(ID3D11Resource *pParentResource, UINT Subresource) :
    resource_(pParentResource),
    subresource_(Subresource),
    surface_from_subresource_(true) {}

ULONG STDMETHODCALLTYPE
D3D11DXGISurface::AddRef() {
  return resource_->AddRef();
}

ULONG STDMETHODCALLTYPE
D3D11DXGISurface::Release() {
  return resource_->Release();
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::QueryInterface(REFIID riid, void **ppvObject) {

  InitReturnPtr(ppvObject);

  // Only a subset of interfaces are available for subresource surfaces
  if (surface_from_subresource_) {
    if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject) ||
        riid == __uuidof(IDXGISurface) || riid == __uuidof(IDXGISurface1) || riid == __uuidof(IDXGISurface2)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  return resource_->QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData) {
  return resource_->GetPrivateData(Name, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::SetPrivateData(REFGUID Name, UINT DataSize, const void *pData) {
  return resource_->SetPrivateData(Name, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) {
  return resource_->SetPrivateDataInterface(Name, pUnknown);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetParent(REFIID riid, void **ppParent) {
  return GetDevice(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDevice(REFIID riid, void **ppDevice) {
  Com<ID3D11Device> device;
  resource_->GetDevice(&device);
  return device->QueryInterface(riid, ppDevice);
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDesc(DXGI_SURFACE_DESC *pDesc) {
  auto d3d11_resource = GetResourceCommon(resource_);

  if (!pDesc)
    return DXGI_ERROR_INVALID_CALL;

  union {
    D3D11_BUFFER_DESC Buffer;
    D3D11_TEXTURE1D_DESC Texture1D;
    D3D11_TEXTURE2D_DESC Texture2D;
    D3D11_TEXTURE3D_DESC Texture3D;
  } desc;
  D3D11_RESOURCE_DIMENSION dimension;
  d3d11_resource->GetDesc(&desc);
  d3d11_resource->GetType(&dimension);

  if (surface_from_subresource_) {
    if (d3d11_resource->texture()) {
      switch (dimension) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        pDesc->Width = std::max(1u, desc.Texture1D.Width >> (subresource_ % desc.Texture1D.MipLevels));
        pDesc->Height = 1U;
        pDesc->Format = desc.Texture1D.Format;
        pDesc->SampleDesc = {1, 0};
        break;
      }
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        pDesc->Width = std::max(1u, desc.Texture2D.Width >> (subresource_ % desc.Texture2D.MipLevels));
        pDesc->Height = std::max(1u, desc.Texture2D.Height >> (subresource_ % desc.Texture2D.MipLevels));
        pDesc->Format = desc.Texture2D.Format;
        pDesc->SampleDesc = desc.Texture2D.SampleDesc;
        break;
      }
      case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
        pDesc->Width = std::max(1u, desc.Texture3D.Width >> (subresource_ % desc.Texture3D.MipLevels));
        pDesc->Height = std::max(1u, desc.Texture3D.Height >> (subresource_ % desc.Texture3D.MipLevels));
        pDesc->Format = desc.Texture3D.Format;
        pDesc->SampleDesc = {1, 0};
        break;
      }
      default:
        return DXGI_ERROR_INVALID_CALL;
      }
      return S_OK;
    } else if (d3d11_resource->buffer()) {
      pDesc->Width = desc.Buffer.ByteWidth;
      pDesc->Height = 1;
      pDesc->Format = DXGI_FORMAT_UNKNOWN;
      pDesc->SampleDesc = {1, 0};
      return S_OK;
    } else {
      return DXGI_ERROR_INVALID_CALL;
    }
  } else if (d3d11_resource->texture()) {
    switch (dimension) {
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      pDesc->Width = desc.Texture1D.Width;
      pDesc->Height = 1U;
      pDesc->Format = desc.Texture1D.Format;
      pDesc->SampleDesc = {1, 0};
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      pDesc->Width = desc.Texture2D.Width;
      pDesc->Height = desc.Texture2D.Height;
      pDesc->Format = desc.Texture2D.Format;
      pDesc->SampleDesc = desc.Texture2D.SampleDesc;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      pDesc->Width = desc.Texture3D.Width;
      pDesc->Height = desc.Texture3D.Height;
      pDesc->Format = desc.Texture3D.Format;
      pDesc->SampleDesc = {1, 0};
      break;
    }
    default:
      return DXGI_ERROR_INVALID_CALL;
    }
    return S_OK;
  } else {
    return DXGI_ERROR_INVALID_CALL;
  }
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags) {
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;

  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  if (pLockedRect) {
    pLockedRect->Pitch = 0;
    pLockedRect->pBits = nullptr;
  }

  D3D11_MAP map_type;

  if (MapFlags & DXGI_MAP_READ && MapFlags & DXGI_MAP_WRITE)
    map_type = D3D11_MAP_READ_WRITE;
  else if (MapFlags & DXGI_MAP_READ)
    map_type = D3D11_MAP_READ;
  else if (MapFlags & DXGI_MAP_WRITE && MapFlags & DXGI_MAP_DISCARD)
    map_type = D3D11_MAP_WRITE_DISCARD;
  else if (MapFlags & DXGI_MAP_WRITE)
    map_type = D3D11_MAP_WRITE;
  else
    return DXGI_ERROR_INVALID_CALL;

  D3D11_MAPPED_SUBRESOURCE sr;
  HRESULT hr = context->Map(resource_, subresource_, map_type, 0, pLockedRect ? &sr : nullptr);

  if (hr != S_OK)
    return hr;

  pLockedRect->Pitch = sr.RowPitch;
  pLockedRect->pBits = reinterpret_cast<unsigned char *>(sr.pData);
  return hr;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::Unmap() {
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;

  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  context->Unmap(resource_, subresource_);
  return S_OK;
}

D3D11DXGISurface::~D3D11DXGISurface() {
  if (mem_dc_)
    DeleteDC(mem_dc_);
  if (bitmap_)
    DeleteObject(bitmap_);
}

bool
D3D11DXGISurface::CreateDIB(UINT width, UINT height) {
  if (mem_dc_)
    DeleteDC(mem_dc_);
  if (bitmap_)
    DeleteObject(bitmap_);
  mem_dc_ = nullptr;
  bitmap_ = nullptr;
  bits_ = nullptr;
  staging_ = nullptr;

  BITMAPINFO info = {};
  info.bmiHeader.biSize = sizeof(info.bmiHeader);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -(LONG)height; // top-down, like the texture
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  mem_dc_ = CreateCompatibleDC(nullptr);
  if (!mem_dc_)
    return false;
  bitmap_ = CreateDIBSection(mem_dc_, &info, DIB_RGB_COLORS, &bits_, nullptr, 0);
  if (!bitmap_) {
    DeleteDC(mem_dc_);
    mem_dc_ = nullptr;
    return false;
  }
  SelectObject(mem_dc_, bitmap_);
  width_ = width;
  height_ = height;
  return true;
}

HRESULT
D3D11DXGISurface::ReadBack(const D3D11_TEXTURE2D_DESC &desc) {
  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;

  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  if (!staging_) {
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.SampleDesc = {1, 0};
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    HRESULT hr = device->CreateTexture2D(&staging_desc, nullptr, &staging_);
    if (FAILED(hr))
      return hr;
  }

  context->CopySubresourceRegion(staging_.ptr(), 0, 0, 0, 0, resource_, subresource_, nullptr);
  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = context->Map(staging_.ptr(), 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr))
    return hr;
  for (UINT y = 0; y < height_; y++)
    std::memcpy(
        static_cast<char *>(bits_) + y * width_ * 4, static_cast<const char *>(mapped.pData) + y * mapped.RowPitch,
        width_ * 4
    );
  context->Unmap(staging_.ptr(), 0);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetDC(BOOL Discard, HDC *phdc) {
  if (!phdc)
    return DXGI_ERROR_INVALID_CALL;
  *phdc = nullptr;

  Com<ID3D11Texture2D> texture;
  if (FAILED(resource_->QueryInterface(IID_PPV_ARGS(&texture))))
    return DXGI_ERROR_INVALID_CALL;

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);

  /* GDI compatible surfaces are B8G8R8A8 */
  if (!(desc.MiscFlags & D3D11_RESOURCE_MISC_GDI_COMPATIBLE) || dc_acquired_ ||
      (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB))
    return DXGI_ERROR_INVALID_CALL;

  if (!bitmap_ || width_ != desc.Width || height_ != desc.Height) {
    if (!CreateDIB(desc.Width, desc.Height))
      return E_FAIL;
  }

  if (!Discard) {
    HRESULT hr = ReadBack(desc);
    if (FAILED(hr))
      return hr;
  }

  dc_acquired_ = true;
  *phdc = mem_dc_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::ReleaseDC(RECT *pDirtyRect) {
  if (!dc_acquired_)
    return DXGI_ERROR_INVALID_CALL;
  dc_acquired_ = false;

  RECT rect = {0, 0, (LONG)width_, (LONG)height_};
  if (pDirtyRect) {
    rect.left = std::max<LONG>(pDirtyRect->left, 0);
    rect.top = std::max<LONG>(pDirtyRect->top, 0);
    rect.right = std::min<LONG>(pDirtyRect->right, width_);
    rect.bottom = std::min<LONG>(pDirtyRect->bottom, height_);
    if (rect.left >= rect.right || rect.top >= rect.bottom)
      return S_OK;
  }

  Com<ID3D11Device> device;
  Com<ID3D11DeviceContext> context;
  resource_->GetDevice(&device);
  device->GetImmediateContext(&context);

  D3D11_BOX box = {(UINT)rect.left, (UINT)rect.top, 0, (UINT)rect.right, (UINT)rect.bottom, 1};
  const UINT pitch = width_ * 4;
  context->UpdateSubresource(
      resource_, subresource_, &box, static_cast<const char *>(bits_) + rect.top * pitch + rect.left * 4, pitch, 0
  );
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
D3D11DXGISurface::GetResource(REFIID riid, void **ppParentResource, UINT *pSubresourceIndex) {
  HRESULT hr;

  if (!ppParentResource)
    return E_POINTER;

  InitReturnPtr(ppParentResource);
  hr = resource_->QueryInterface(riid, ppParentResource);
  if (SUCCEEDED(hr))
    *pSubresourceIndex = subresource_;

  return hr;
}

}; // namespace dxmt