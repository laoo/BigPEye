#include "pch.h"
#include "EyeDesc.hpp"

EyeDesc::EyeDesc() : mMutex{}, mOrigin { -256, -128 }, mWidth{ 3072 }, mScale{ 1 }, mDragedOrigin{}
{
}

uint32_t EyeDesc::getWidth() const
{
  return mWidth;
}

uint32_t EyeDesc::getRescaledWidth() const
{
  return (int)mWidth / 2 / mScale;
}

uint32_t EyeDesc::getRescaledHeight() const
{
  auto w = getRescaledWidth();
  return (2048 * 1024) / w + ((2048 * 1024) % w == 0 ? 0 : 1 );
}

uint32_t EyeDesc::getHeight() const
{
  return mScale * getRescaledHeight();
}

void EyeDesc::updateWidth( int w )
{
  std::scoped_lock l{ mMutex };

  static int sw = w;

  if ( sw == w )
    return;

  sw = w;

  int newWidth2 = mScale * w * 2;

  mOrigin.x = (int)std::round( (double)mOrigin.x * newWidth2 / getWidth() );
  mOrigin.y = (int)std::round( (double)mOrigin.y * getWidth() / newWidth2 );

  mWidth = newWidth2;
}

void EyeDesc::smallUpscale()
{
  std::scoped_lock l{ mMutex };

  int rescaledWidth = getRescaledWidth();

  mScale += 1;

  int oldWidth = mWidth;
  mWidth = 2 * rescaledWidth * mScale;

  updateOrigin( oldWidth );
}

bool EyeDesc::smallDownscale()
{
  std::scoped_lock l{ mMutex };

  int rescaledWidth = getRescaledWidth();

  int newScale = mScale - 1;

  if ( newScale < 1 )
    return false;

  mScale = newScale;
  int oldWidth = mWidth;
  mWidth = 2 * rescaledWidth * newScale;
  updateOrigin( oldWidth );
  return true;
}

void EyeDesc::bigUpscale( uint32_t mult )
{
  std::scoped_lock l{ mMutex };
  
  do
  {
    smallUpscale();
  } while ( mScale % mult != 0 );
}

bool EyeDesc::bigDownscale( uint32_t mult )
{
  std::scoped_lock l{ mMutex };

  do
  {
    if ( mScale <= mult )
      return false;

    if ( !smallDownscale() )
      return false;
  } while ( mScale % mult != 0 );

  return true;
}

uint32_t EyeDesc::getScale() const
{
  return mScale;
}

EyeMapping EyeDesc::computeMapping( RECT const & r, uint32_t mult ) const
{
  std::scoped_lock l{ mMutex };

  EyeMapping m;

  auto origin = getOrigin();

  int mx = ( r.right - r.left ) / 2;
  int my = ( r.bottom - r.top ) / 2;

  int ox = origin.x + mx;
  int oy = origin.y + my;
  int ex = ox + getWidth();
  int ey = oy + getHeight();

  m.leftPos = std::max( ox, (int)r.left );
  m.topPos = std::max( oy, (int)r.top );
  m.rightPos = std::min( ex, (int)r.right );
  m.bottomPos = std::min( ey, (int)r.bottom );
  m.leftOff = m.leftPos - ox;
  m.topOff = m.topPos - oy;
  m.scaleX = mScale * 2;
  m.scaleY = mScale;

  return m;
}


void EyeDesc::updateOrigin( int oldWidth )
{
  double s = (double)getWidth() / oldWidth;

  mOrigin.x = (int)( mOrigin.x * s );
  mOrigin.y = (int)( mOrigin.y * s );
}

Point EyeDesc::getOrigin() const
{
  return mDragedOrigin.value_or( mOrigin );
}

void EyeDesc::drag( Point delta )
{
  std::scoped_lock l{ mMutex };

  mDragedOrigin = { mOrigin.x + delta.x, mOrigin.y + delta.y };

  mDragedOrigin->x = std::min( mDragedOrigin->x, 0 );
  mDragedOrigin->y = std::min( mDragedOrigin->y, 0 );
  mDragedOrigin->x = std::max( mDragedOrigin->x, -(int)getWidth() );
  mDragedOrigin->y = std::max( mDragedOrigin->y, -(int)getHeight() );
}

void EyeDesc::endDrag()
{
  std::scoped_lock l{ mMutex };

  mOrigin = getOrigin();
  mDragedOrigin = {};
}
