#pragma once


#ifdef BIGPEYE_EXPORTS
#define BIGPEYE_API extern "C" __declspec(dllexport)
#else
#define BIGPEYE_API extern "C" __declspec(dllimport)
#endif

typedef void( __cdecl* TCreateBigPEye )();
typedef void( __cdecl* TUpdateMemory )( void const* ptr );
typedef void( __cdecl* TDestroyBigPEye )();

BIGPEYE_API void createBigPEye();
BIGPEYE_API void updateMemory( void const* ptr );
BIGPEYE_API void destroyBigPEye();


