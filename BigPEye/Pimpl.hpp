#pragma once

class WinImgui;

#include "EyeDesc.hpp"

class Pimpl
{
public:
  Pimpl();
  ~Pimpl();

  void updateMemory( void const* ptr);

private:

  struct CB
  {
    int32_t destorgX;
    int32_t destorgY;
    int32_t offsetX;
    int32_t offsetY;
    int32_t scaleX;
    int32_t scaleY;
    uint32_t groupsX;
    uint32_t groupsY;
    int32_t soffx;
    int32_t soffy;
    float sx16;
    float sx8;
    int32_t currentTick;
    float fadeOut;
    int32_t lastAccessMask;
    uint32_t fill3;
  };

private:

  friend LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
  void initialize( HWND hWnd );
  bool win32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
  void renderPreview();
  void renderGui();

  void wndProc();
  void renderProc();
  void eyeConfig( ImGuiIO & io );
  void prepareHex();
  void fillHex( uint8_t hex, uint8_t * buf );
  void copyHex( uint8_t const * src, uint8_t * dst, size_t width );

private:
  EyeDesc mEye;
  std::atomic_bool mDoLoop;
  std::atomic<HWND> mHWnd;
  std::thread mWndProc;
  std::thread mRenderProc;
  ComPtr<ID3D11Device>                    mD3DDevice;
  ComPtr<ID3D11DeviceContext>             mImmediateContext;
  ComPtr<IDXGISwapChain>                  mSwapChain;
  ComPtr<ID3D11ComputeShader>             mRendererCS;
  ComPtr<ID3D11Buffer>                    mCBCB;
  ComPtr<ID3D11UnorderedAccessView>       mBackBufferUAV;
  ComPtr<ID3D11RenderTargetView>          mBackBufferRTV;
  ComPtr<ID3D11ShaderResourceView>        mHexSRV;
  ComPtr<ID3D11Texture2D>                 mSource;
  ComPtr<ID3D11ShaderResourceView>        mSourceSRV;
  ComPtr<ID3D11Texture2D>                 mCopy;
  ComPtr<ID3D11UnorderedAccessView>       mCopyUAV;
  ComPtr<ID3D11Texture2D>                 mLastAccess;
  ComPtr<ID3D11UnorderedAccessView>       mLastAccessUAV;

  CB mCB;
  uint32_t mWorkspaceWidth;
  uint32_t mWorkspaceHeight;
  int mWinWidth;
  int mWinHeight;
  int mOffsetX;
  int mOffsetY;
  int mSX;
  int mSY;
  uint32_t mEyeWidth;
  uint32_t mEyeHeight;
  uint32_t mErrorMask;
  std::unique_ptr<WinImgui> mImgui;
  std::vector<uint8_t> mData;
  int32_t mCurrentTimestamp;
  bool mShowLastAccess;
  mutable std::mutex mMutex;

  int mPreviewWidth;
  int mPreviewHeight;

};
