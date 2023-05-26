#pragma once

struct Point
{
  int x;
  int y;
};

struct EyeMapping
{
  int leftPos;
  int topPos;
  int rightPos;
  int bottomPos;
  int leftOff;
  int topOff;
  int scaleX;
  int scaleY;
};


class EyeDesc
{
  mutable std::recursive_mutex mMutex;
  Point mOrigin;
  uint32_t mWidth;
  uint32_t mScale;
  std::optional<Point> mDragedOrigin;

public:


  EyeDesc();

  uint32_t getWidth() const;
  uint32_t getHeight() const;
  Point getOrigin() const;

  uint32_t getRescaledWidth() const;
  uint32_t getRescaledHeight() const;

  uint32_t getScale() const;

  EyeMapping computeMapping( RECT const& r, uint32_t mult ) const;


  void drag( Point delta );
  void endDrag();

  void updateWidth( int w );

  void smallUpscale();
  bool smallDownscale();

  void bigUpscale( uint32_t mult );
  bool bigDownscale( uint32_t mult );

private:
  void updateOrigin( int oldWidth );

};

