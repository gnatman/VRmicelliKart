#include "gfx_direct3d11_vr.h"

namespace Fast {

GfxDirect3D11VR::GfxDirect3D11VR() : GfxRenderingAPIDX11() {
}

void GfxDirect3D11VR::BindExternalRenderTarget(ID3D11RenderTargetView* rtv) {
#ifdef _WIN32
    if (mFrameBuffers.empty()) return;

    if (rtv) {
        mContext->OMSetRenderTargets(1, &rtv, mFrameBuffers[0].depth_stencil_view.Get());
    } else {
        // Fallback to default backbuffer if needed
        auto& fb = mFrameBuffers[0];
        mContext->OMSetRenderTargets(1, fb.render_target_view.GetAddressOf(), fb.depth_stencil_view.Get());
    }
#endif
}

}
