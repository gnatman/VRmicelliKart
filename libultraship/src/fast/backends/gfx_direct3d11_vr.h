#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "fast/backends/gfx_direct3d_common.h"

namespace Fast {
class GfxDirect3D11VR : public GfxRenderingAPIDX11 {
public:
    GfxDirect3D11VR();
    void BindExternalRenderTarget(ID3D11RenderTargetView* rtv);
    ID3D11Device* GetDevice() { return mDevice.Get(); }
};
}
