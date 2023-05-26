R"(
Texture2DArray<unorm float> thex : register( t0 );
Texture2D<uint> src : register( t1 );

RWTexture2D<unorm float4> dst : register( u0 );
RWTexture2D<uint> srcCopy : register( u1 );
RWTexture2D<int> lastUpdate : register( u2 );

cbuffer cb : register( b0 )
{
  int2 destorg;
  int2 offset;
  int2 scale;
  uint2 size;
  int2 soff;
  float2 sxy;
  int time;
  float fadeOut;
  int showLastAccess;
};

[numthreads( 16, 16, 1 )]
void main( uint3 DT : SV_DispatchThreadID, uint3 G : SV_GroupID )
{
  if ( DT.x >= size.x || DT.y >= size.y )
    return;

  int2 idx = ( DT + offset ) / scale;
  int2 rem = ( DT + offset ) % scale;

  if ( src[idx] != srcCopy[idx] )
  {
    lastUpdate[idx] = time + 120;
  }

  srcCopy[idx] = src[idx];

  float diff = showLastAccess ? lastUpdate[idx] != 0 : ( lastUpdate[idx] - time ) * fadeOut;

  float4 color = saturate( float4(diff,0,0,1) );
  float hpix = 0;

  if ( scale.y >= 8 )
  {
    uint hidx = src[idx];

    if ( scale.y % 8 == 0 )
    {
      int s = scale.y / 8;
      hpix = thex[int3( rem / s, hidx)];
    }
    else
    {
      float2 rem = ( ( DT + offset ) % scale - soff ) * sxy;
      int2 irem;
      float2 frem = modf(rem, irem);
      float d00 = thex[int3( irem + int2(0,0), hidx)];
      float d01 = thex[int3( irem + int2(0,1), hidx)];
      float d10 = thex[int3( irem + int2(1,0), hidx)];
      float d11 = thex[int3( irem + int2(1,1), hidx)];
      float d0 = lerp( d00, d01, frem.y); 
      float d1 = lerp( d10, d11, frem.y); 
      hpix = lerp( d0, d1, frem.x);
    }

    dst[destorg + DT.xy] = color + float4( hpix, hpix, hpix, 1 );
  }
  else
  {
    dst[destorg + DT.xy]  = color;
  }
}
)";
