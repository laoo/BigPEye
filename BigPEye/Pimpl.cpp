#include "pch.h"
#include "Pimpl.hpp"
#include "WinImgui.hpp"
#include "Ex.hpp"
#include "dat.h"
#include "fnt.h"

wchar_t gClassName[] = L"EyeWindowClass";

#define V_THROW(x) { HRESULT hr_ = (x); if( FAILED( hr_ ) ) { throw Ex{} << "DXError"; } }
#define L_ERROR std::cerr
#define L_DEBUG std::cout

LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{

  switch ( msg )
  {
  case WM_CREATE:
  {
    Pimpl * pEye = reinterpret_cast<Pimpl *>( reinterpret_cast<LPCREATESTRUCT>( lParam )->lpCreateParams );
    assert( pEye );
    try
    {
      pEye->initialize( hWnd );
    }
    catch ( std::exception const & ex )
    {
      MessageBoxA( nullptr, ex.what(), "Eye Error", MB_OK | MB_ICONERROR );
      PostQuitMessage( 0 );
    }
    SetWindowLongPtr( hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( pEye ) );

    EnableMenuItem( GetSystemMenu( hWnd, FALSE ), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED );
    return 0;
  }
  case WM_CLOSE:
    DestroyWindow( hWnd );
    break;
  case WM_DESTROY:
    if ( Pimpl * pImpl = reinterpret_cast<Pimpl *>( GetWindowLongPtr( hWnd, GWLP_USERDATA ) ) )
    {
      pImpl->mDoLoop.store( false );
    }
    SetWindowLong( hWnd, GWLP_USERDATA, NULL );
    break;
  default:
    if ( Pimpl * pImpl = reinterpret_cast<Pimpl *>( GetWindowLongPtr( hWnd, GWLP_USERDATA ) ) )
    {
      if ( pImpl->win32_WndProcHandler( hWnd, msg, wParam, lParam ) )
        return true;
    }
    return DefWindowProc( hWnd, msg, wParam, lParam );
  }
  return 0;
}


Pimpl::~Pimpl()
{
  mDoLoop.store( false );
  if ( mWndProc.joinable() )
    mWndProc.join();
  if ( mRenderProc.joinable() )
    mRenderProc.join();
}

void Pimpl::initialize( HWND hWnd )
{
  mHWnd = hWnd;

  typedef HRESULT( WINAPI * LPD3D11CREATEDEVICE )( IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT32, CONST D3D_FEATURE_LEVEL *, UINT, UINT32, ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext ** );
  static LPD3D11CREATEDEVICE  s_DynamicD3D11CreateDevice = nullptr;
  HMODULE hModD3D11 = ::LoadLibrary( L"d3d11.dll" );
  if ( hModD3D11 == nullptr )
    throw std::runtime_error{ "DXError" };

  s_DynamicD3D11CreateDevice = (LPD3D11CREATEDEVICE)GetProcAddress( hModD3D11, "D3D11CreateDevice" );


  D3D_FEATURE_LEVEL  featureLevelsRequested = D3D_FEATURE_LEVEL_11_0;
  UINT               numFeatureLevelsRequested = 1;
  D3D_FEATURE_LEVEL  featureLevelsSupported;

  HRESULT hr = s_DynamicD3D11CreateDevice( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
#ifndef NDEBUG
    D3D11_CREATE_DEVICE_DEBUG,
#else
    0,
#endif
    & featureLevelsRequested, numFeatureLevelsRequested, D3D11_SDK_VERSION, mD3DDevice.ReleaseAndGetAddressOf(), &featureLevelsSupported, mImmediateContext.ReleaseAndGetAddressOf() );

  V_THROW( hr );

  DXGI_SWAP_CHAIN_DESC sd{ { 0, 0, { 60, 1 }, DXGI_FORMAT_R8G8B8A8_UNORM }, { 1, 0 }, DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_RENDER_TARGET_OUTPUT, 2, mHWnd, TRUE, DXGI_SWAP_EFFECT_DISCARD, 0 };

  ComPtr<IDXGIDevice> pDXGIDevice;
  V_THROW( mD3DDevice->QueryInterface( __uuidof( IDXGIDevice ), (void **)pDXGIDevice.ReleaseAndGetAddressOf() ) );
  ComPtr<IDXGIAdapter> pDXGIAdapter;
  V_THROW( pDXGIDevice->GetParent( __uuidof( IDXGIAdapter ), (void **)pDXGIAdapter.ReleaseAndGetAddressOf() ) );
  ComPtr<IDXGIFactory> pIDXGIFactory;
  V_THROW( pDXGIAdapter->GetParent( __uuidof( IDXGIFactory ), (void **)pIDXGIFactory.ReleaseAndGetAddressOf() ) );
  ComPtr<IDXGISwapChain> pSwapChain;
  V_THROW( pIDXGIFactory->CreateSwapChain( mD3DDevice.Get(), &sd, pSwapChain.ReleaseAndGetAddressOf() ) );
  mSwapChain = std::move( pSwapChain );

  D3D11_BUFFER_DESC bd{ sizeof( CB ), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
  V_THROW( mD3DDevice->CreateBuffer( &bd, NULL, mCBCB.ReleaseAndGetAddressOf() ) );

  mImgui.reset( new WinImgui{ mHWnd, mD3DDevice, mImmediateContext } );

  prepareHex();

  mRenderProc = std::thread{ [this]()
  {
    renderProc();
  } };
}

void Pimpl::copyHex( uint8_t const * src, uint8_t * dst, size_t width )
{
  for ( size_t y = 0; y < 8; ++y )
  {
    memcpy( dst + y * 16, src + y * font_dat_len / 8, width );
  }
}


void Pimpl::fillHex( uint8_t hex, uint8_t * buf )
{
  char buffer[3];
  sprintf( buffer, "%02x", hex );

  auto hi = fontDesc( (int)buffer[0] );
  auto lo = fontDesc( (int)buffer[1] );

  copyHex( font_dat + hi->x, buf + 8 - hi->xadvance, hi->width );
  copyHex( font_dat + lo->x, buf + 8, lo->width );
}

void Pimpl::prepareHex()
{
  std::array<D3D11_SUBRESOURCE_DATA, 256> initData;
  std::vector<uint8_t> buf;
  buf.resize( 16 * 8 * initData.size() );

  for ( size_t i = 0; i < initData.size(); ++i )
  {
    auto & ini = initData[i];
    ini.pSysMem = buf.data() + i * 16 * 8;
    fillHex( (uint8_t)i, ( uint8_t * )ini.pSysMem );
    ini.SysMemPitch = 16;
    ini.SysMemSlicePitch = 0;
  }

  ComPtr<ID3D11Texture2D> hex;
  D3D11_TEXTURE2D_DESC descsrc{ 16, 8, 1, (uint32_t)initData.size(), DXGI_FORMAT_R8_UNORM, { 1, 0 }, D3D11_USAGE_IMMUTABLE, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
  V_THROW( mD3DDevice->CreateTexture2D( &descsrc, initData.data(), hex.ReleaseAndGetAddressOf() ) );
  V_THROW( mD3DDevice->CreateShaderResourceView( hex.Get(), NULL, mHexSRV.ReleaseAndGetAddressOf() ) );
}

bool Pimpl::win32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
  return mImgui->win32_WndProcHandler( hWnd, msg, wParam, lParam );
}

void Pimpl::renderPreview()
{
  float phase = std::sinf( ( std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::high_resolution_clock::now().time_since_epoch() ).count() % 1000 ) / 1000.0f * 2.0f * 3.14159f ) * 0.5f + 0.5f;

  RECT r;
  ::GetClientRect( mHWnd, &r );

  if ( mEyeWidth != mEye.getRescaledWidth() )
  {
    mEyeWidth = mEye.getRescaledWidth();
    mEyeHeight = mEye.getRescaledHeight();

    {
      D3D11_TEXTURE2D_DESC descsrc{ mEyeWidth, mEyeHeight, 1, 1, DXGI_FORMAT_R8_UINT, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0 };
      V_THROW( mD3DDevice->CreateTexture2D( &descsrc, nullptr, mSource.ReleaseAndGetAddressOf() ) );
      V_THROW( mD3DDevice->CreateShaderResourceView( mSource.Get(), NULL, mSourceSRV.ReleaseAndGetAddressOf() ) );
    }

    {
      D3D11_TEXTURE2D_DESC descsrc{ mEyeWidth, mEyeHeight, 1, 1, DXGI_FORMAT_R8_UINT, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0, 0 };
      V_THROW( mD3DDevice->CreateTexture2D( &descsrc, nullptr, mCopy.ReleaseAndGetAddressOf() ) );
      V_THROW( mD3DDevice->CreateUnorderedAccessView( mCopy.Get(), NULL, mCopyUAV.ReleaseAndGetAddressOf() ) );
    }

    {
      D3D11_TEXTURE2D_DESC descsrc{ mEyeWidth, mEyeHeight, 1, 1, DXGI_FORMAT_R32_SINT, { 1, 0 }, D3D11_USAGE_DEFAULT, D3D11_BIND_UNORDERED_ACCESS, 0, 0 };
      V_THROW( mD3DDevice->CreateTexture2D( &descsrc, nullptr, mLastAccess.ReleaseAndGetAddressOf() ) );
      V_THROW( mD3DDevice->CreateUnorderedAccessView( mLastAccess.Get(), NULL, mLastAccessUAV.ReleaseAndGetAddressOf() ) );
    }

    if ( !mRendererCS )
    {
      char EYEWIDTH[10];
      char EYEHEIGHT[10];

      sprintf( EYEWIDTH, "%d", mEyeWidth );
      sprintf( EYEHEIGHT, "%d", mEyeHeight );

      D3D_SHADER_MACRO shaderMacros[] = {
        "EYEWIDTH", EYEWIDTH,
        "EYEHEIGHT", EYEHEIGHT,
        NULL, NULL
      };

      ComPtr<ID3D10Blob> csBlob;
      ComPtr<ID3D10Blob> csErrBlob;

      char computeShader[] =
#include "render.h"

        D3DCompile( computeShader, strlen( computeShader ), "ComputeShader", shaderMacros, NULL, "main", "cs_5_0", 0, 0, csBlob.ReleaseAndGetAddressOf(), csErrBlob.ReleaseAndGetAddressOf() );
      if ( !csBlob ) // NB: Pass ID3D10Blob* pErrorBlob to D3DCompile() to get error showing in (const char*)pErrorBlob->GetBufferPointer(). Make sure to Release() the blob!
      {
        OutputDebugStringA( (char const *)csErrBlob->GetBufferPointer() );
        throw Ex{} << "cs blob error";
      }

      V_THROW( mD3DDevice->CreateComputeShader( csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, mRendererCS.ReleaseAndGetAddressOf() ) );

      ComPtr<ID3D10Blob> csDisAsm;
      V_THROW( D3DDisassemble( csBlob->GetBufferPointer(), csBlob->GetBufferSize(), D3D_DISASM_ENABLE_INSTRUCTION_OFFSET | D3D_DISASM_INSTRUCTION_ONLY, "Waldek", csDisAsm.ReleaseAndGetAddressOf() ) );
      OutputDebugStringA( (char *)csDisAsm->GetBufferPointer() );
    }
  }


  mSX = (std::max)( 1, mWinWidth / ( mPreviewWidth * 2 ) );
  mSY = (std::max)( 1, mWinHeight / mPreviewHeight );
  int s = (std::min)( mSX, mSY );

  if ( mWinHeight != ( r.bottom - r.top ) || ( mWinWidth != r.right - r.left ) )
  {
    mWinHeight = r.bottom - r.top;
    mWinWidth = r.right - r.left;

    if ( mWinHeight == 0 || mWinWidth == 0 )
    {
      return;
    }

    mSX = (std::max)( 1, mWinWidth / ( mPreviewWidth * 2 ) );
    mSY = (std::max)( 1, mWinHeight / mPreviewHeight );
    s = (std::min)( mSX, mSY );

    mBackBufferUAV.Reset();
    mBackBufferRTV.Reset();

    mSwapChain->ResizeBuffers( 0, mWinWidth, mWinHeight, DXGI_FORMAT_UNKNOWN, 0 );

    ComPtr<ID3D11Texture2D> backBuffer;
    V_THROW( mSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (LPVOID *)backBuffer.ReleaseAndGetAddressOf() ) );
    V_THROW( mD3DDevice->CreateUnorderedAccessView( backBuffer.Get(), nullptr, mBackBufferUAV.ReleaseAndGetAddressOf() ) );
    V_THROW( mD3DDevice->CreateRenderTargetView( backBuffer.Get(), nullptr, mBackBufferRTV.ReleaseAndGetAddressOf() ) );
  }

  auto mapping = mEye.computeMapping( r, 8 );

  uint32_t w = mapping.rightPos - mapping.leftPos;
  uint32_t h = mapping.bottomPos - mapping.topPos;
  uint32_t w16 = w / 16 + ( w % 16 == 0 ? 0 : 1 );
  uint32_t h16 = h / 16 + ( h % 16 == 0 ? 0 : 1 );
  

  mCB = {
      mapping.leftPos,
      mapping.topPos,
      mapping.leftOff,
      mapping.topOff,
      mapping.scaleX,
      mapping.scaleY,
      w, h,
      mapping.scaleX / 32,
      mapping.scaleY / 16,
      16.0f / (float)mapping.scaleX,
      8.0f / (float)mapping.scaleY,
      mCurrentTimestamp - 120,
      1.0f / 120.0f,
      mShowLastAccess
  };

  mImmediateContext->UpdateSubresource( mSource.Get(), 0, NULL, mData.data(), mEyeWidth, 0 );

  mImmediateContext->UpdateSubresource( mCBCB.Get(), 0, NULL, &mCB, 0, 0 );

  mImmediateContext->CSSetConstantBuffers( 0, 1, mCBCB.GetAddressOf() );
  std::array<ID3D11ShaderResourceView* const, 2> srv{ mHexSRV.Get(), mSourceSRV.Get() };
  mImmediateContext->CSSetShaderResources( 0, 2, srv.data() );
  std::array<ID3D11UnorderedAccessView* const, 3> uav{ mBackBufferUAV.Get(), mCopyUAV.Get(), mLastAccessUAV.Get() };
  mImmediateContext->CSSetUnorderedAccessViews( 0, 3, uav.data(), nullptr );
  mImmediateContext->CSSetShader( mRendererCS.Get(), nullptr, 0 );
  float v[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
  mImmediateContext->ClearUnorderedAccessViewFloat( mBackBufferUAV.Get(), v );

  if ( mapping.bottomPos > mapping.topPos )
    mImmediateContext->Dispatch( w16, h16, 1 );

  std::array<ID3D11UnorderedAccessView * const, 3> uavc{};
  mImmediateContext->CSSetUnorderedAccessViews( 0, 3, uavc.data(), nullptr );

  std::array<ID3D11ShaderResourceView * const, 2> srvc{};
  mImmediateContext->CSSetShaderResources( 0, 2, srvc.data() );

}

void Pimpl::eyeConfig( ImGuiIO & io )
{
  static std::optional<ImVec2> dragDelta = std::nullopt;

  ImGui::Begin( "Eye config" );
  bool hovered = ImGui::IsWindowHovered( ImGuiHoveredFlags_ChildWindows | ImGuiHoveredFlags_AllowWhenBlockedByPopup );

  Point mousePos{ (int)io.MousePos.x, (int)io.MousePos.y };
  Point origin = mEye.getOrigin();
  auto scale = mEye.getScale();

  static constexpr std::array<int,16> widthTable = {
    128,
    256,
    320,
    512,
    640,
    768,
    1024,
    1152,
    1280,
    1536,
    1792,
    1920,
    2048,
    4096,
    8192,
    16384
  };

  auto it = std::find( widthTable.cbegin(), widthTable.cend(), mEye.getRescaledWidth() );
  int w = it == widthTable.cend() ? 9 : (int)std::distance( widthTable.cbegin(), it );

  std::stringstream ss;
  ss << mEyeWidth << "x" << mEyeHeight;
  auto str = ss.str();
  ImGui::Checkbox( "Last Access", &mShowLastAccess ); ImGui::SameLine();
  if ( ImGui::Button( "Clear" ) )
  {
    std::scoped_lock<std::mutex> l{ mMutex };
    uint32_t v[4] = { 0, 0, 0, 0 };
    mImmediateContext->ClearUnorderedAccessViewUint( mLastAccessUAV.Get(), v );
    mCurrentTimestamp = 0;
  }

  ImGui::SliderInt( "dimension", &w, 0, 15, str.c_str() );
  hovered |= ImGui::IsItemActive();
  mEye.updateWidth( widthTable[w] );

  if ( io.MouseWheel > 0 )
  {
    if ( io.KeyShift )
    {
      mEye.bigUpscale( 8 );
    }
    else
    {
      mEye.smallUpscale();
    }
  }
  else if ( io.MouseWheel < 0 )
  {
    if ( io.KeyShift )
    {
      mEye.bigDownscale( 8 );
    }
    else
    {
      mEye.smallDownscale();
    }
  }

  if ( !hovered )
  {
    if ( ImGui::IsMouseDragging( ImGuiMouseButton_Left, 1.0f ) )
    {
      dragDelta = ImGui::GetMouseDragDelta( ImGuiMouseButton_Left, 1.0f );
      mEye.drag( { (int)dragDelta->x, (int)dragDelta->y } );
    }
    else if ( dragDelta.has_value() )
    {
      mEye.endDrag();
    }
  }

  ImGui::End();

}

void Pimpl::renderGui()
{
  ImGuiIO & io = ImGui::GetIO();

  //temporary
  if ( io.KeysDown[VK_RIGHT] )
  {
    mOffsetX = (std::min)( (int)mWorkspaceWidth / 4 - 40, mOffsetX + 1 );
  }
  else if ( io.KeysDown[VK_LEFT] )
  {
    mOffsetX = (std::max)( 0, mOffsetX - 1 );
  }
  else if ( io.KeysDown[VK_DOWN] )
  {
    mOffsetY = (std::min)( (int)mWorkspaceHeight / 8 - 24, mOffsetY + 1 );
  }
  else if ( io.KeysDown[VK_UP] )
  {
    mOffsetY = (std::max)( 0, mOffsetY - 1 );
  }

  mImgui->dx11_NewFrame();
  mImgui->win32_NewFrame();

  ImGui::NewFrame();

  //ImGui::ShowDemoWindow();

  eyeConfig( io );

  int x = ( (int)io.MousePos.x - mCB.destorgX + mCB.offsetX ) / mCB.scaleX;
  int y = ( (int)io.MousePos.y - mCB.destorgY + mCB.offsetY ) / mCB.scaleY;


  if ( x >= 0 && x < (int)mEyeWidth && y >= 0 && y < (int)mEyeHeight )
  {
    int adr = x + y * mEyeWidth;

    char buf[20];
    sprintf( buf, "%04x:%02x", adr, mData[adr] );
    ImGui::SetTooltip( buf );
  }

  ImGui::Render();

  mImmediateContext->OMSetRenderTargets( 1, mBackBufferRTV.GetAddressOf(), nullptr );
  mImgui->dx11_RenderDrawData( ImGui::GetDrawData() );

  std::array<ID3D11RenderTargetView * const, 1> rtv{};
  mImmediateContext->OMSetRenderTargets( 1, rtv.data(), nullptr );

  mSwapChain->Present( 1, 0 );

  mCurrentTimestamp += 1;
}

void Pimpl::updateMemory( void const* ptr )
{
  memcpy( mData.data(), ptr, 2 * 1024 * 1024 );
}


Pimpl::Pimpl() : mEye{}, mWinWidth{}, mWinHeight{}, mPreviewWidth{ 2048 }, mPreviewHeight{ 1024 }, mDoLoop{ true }, mEyeWidth{}, mCurrentTimestamp{}, mShowLastAccess {}, mMutex{}
{
  constexpr size_t size = 2048 * 1024 + 1024;
  mData.resize( size );

  mWndProc = std::thread{ [this]()
  {
    wndProc();
  } };
}

void Pimpl::wndProc()
{
  auto hInstance = GetModuleHandle( nullptr );

  WNDCLASSEX wc{ sizeof( WNDCLASSEX ), 0, WndProc, 0, 0, hInstance, nullptr, LoadCursor( nullptr, IDC_ARROW ), (HBRUSH)( COLOR_WINDOW + 1 ), nullptr, gClassName, nullptr };

  if ( !RegisterClassEx( &wc ) )
  {
    mDoLoop.store( false );
    return;
  }

  auto hwnd = CreateWindowEx( WS_EX_DLGMODALFRAME, gClassName, L"BigPEye (preliminary)", WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 980, 619, nullptr, nullptr, hInstance, reinterpret_cast<LPVOID>( this ));

  ShowWindow( hwnd, SW_SHOW );
  UpdateWindow( hwnd );

  MSG msg{};

  while ( mDoLoop.load() )
  {
    while ( PeekMessage( &msg, mHWnd, 0, 0, PM_REMOVE ) )
    {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
    }

    std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
  }
}

void Pimpl::renderProc()
{
  while ( mDoLoop.load() )
  {
    renderPreview();
    renderGui();
  }
}
