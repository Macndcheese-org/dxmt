#pragma once
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"

namespace dxmt {

class D3D11DXGISurface : public IDXGISurface2 {

public:
  D3D11DXGISurface(ID3D11Resource *pResource);

  D3D11DXGISurface(ID3D11Resource *pParentResource, UINT Subresource);

  ~D3D11DXGISurface();

  ULONG STDMETHODCALLTYPE AddRef();

  ULONG STDMETHODCALLTYPE Release();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize, void *pData);

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void *pData);

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown);

  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent);

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **ppDevice);

  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SURFACE_DESC *pDesc);

  HRESULT STDMETHODCALLTYPE Map(DXGI_MAPPED_RECT *pLockedRect, UINT MapFlags);

  HRESULT STDMETHODCALLTYPE Unmap();

  HRESULT STDMETHODCALLTYPE GetDC(BOOL Discard, HDC *phdc);

  HRESULT STDMETHODCALLTYPE ReleaseDC(RECT *pDirtyRect);

  HRESULT STDMETHODCALLTYPE GetResource(REFIID riid, void **ppParentResource, UINT *pSubresourceIndex);

private:
  bool CreateDIB(UINT width, UINT height);

  HRESULT ReadBack(const D3D11_TEXTURE2D_DESC &desc);

  ID3D11Resource *resource_;
  UINT subresource_;
  bool surface_from_subresource_;

  /* GetDC(): a device context on a system memory bitmap, which is filled from the texture and written back to it */
  HDC mem_dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  void *bits_ = nullptr;
  UINT width_ = 0;
  UINT height_ = 0;
  bool dc_acquired_ = false;
  Com<ID3D11Texture2D> staging_;
};

} // namespace dxmt