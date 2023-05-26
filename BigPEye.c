//__BIGPEMU_SCRIPT_MODULE__
//__BIGPEMU_META_DESC__		"BigPEye"
//__BIGPEMU_META_AUTHOR__	"laoo"

#include "bigpcrt.h"

static uint8_t jagBuffer[2 * 1024 * 1024];

static int sOnLoadEvent = -1;
static int sOnUnloadEvent = -1;
static int sOnVideoEvent = -1;

typedef void( * TCreateBigPEye )( );
typedef void( * TUpdateMemory )( uint64_t ptr );
typedef void( * TDestroyBigPEye )( );

uint64_t createBigPEyeAddr = 0;
uint64_t updateMemoryAddr = 0;
uint64_t destroyBigPEyeAddr = 0;
uint64_t modHandle = 0;

static uint32_t on_sw_loaded(const int eventHandle, void *pEventData)
{
	modHandle = bigpemu_nativemod_load( "BigPEye" );
	if ( modHandle )
	{
		createBigPEyeAddr = bigpemu_nativemod_getfuncaddr( modHandle, "createBigPEye" );
		updateMemoryAddr = bigpemu_nativemod_getfuncaddr( modHandle, "updateMemory" );
		destroyBigPEyeAddr = bigpemu_nativemod_getfuncaddr( modHandle, "destroyBigPEye" );

		if ( createBigPEyeAddr )
		{
			TCreateBigPEye pLocalFuncPtr;
			bigpemu_nativemod_call( pLocalFuncPtr, modHandle, createBigPEyeAddr );
		}
		else
		{
			printf( "Error getting function address from BigPEye.dll\n" );
		}
	}
	else
	{
		printf( "Error loading BigPEye.dll\n" );
	}
	return 0;
}

static uint32_t on_sw_unloaded(const int eventHandle, void *pEventData)
{
	if ( destroyBigPEyeAddr && modHandle )
	{
		TDestroyBigPEye pLocalFuncPtr;
		bigpemu_nativemod_call( pLocalFuncPtr, modHandle, destroyBigPEyeAddr );
		destroyBigPEyeAddr = 0;
	}

	bigpemu_nativemod_free( modHandle );
	modHandle = 0;
	return 0;
}

static uint32_t on_video_frame(const int eventHandle, void *pEventData)
{
	if ( updateMemoryAddr && modHandle )
	{
		bigpemu_jag_sysmemread( jagBuffer, 0, 2 * 1024 * 1024 );
		TUpdateMemory pLocalFuncPtr;
		const uint64_t memPtr = bigpemu_process_memory_alias( jagBuffer );
		bigpemu_nativemod_call( pLocalFuncPtr, modHandle, updateMemoryAddr, memPtr );
	}
}

void bigp_init()
{
	void *pMod = bigpemu_get_module_handle();
	
	sOnLoadEvent = bigpemu_register_event_sw_loaded(pMod, on_sw_loaded);
	sOnUnloadEvent = bigpemu_register_event_sw_unloaded(pMod, on_sw_unloaded);
	sOnVideoEvent = bigpemu_register_event_video_thread_frame(pMod, on_video_frame);
}

void bigp_shutdown()
{
	void *pMod = bigpemu_get_module_handle();
	bigpemu_unregister_event(pMod, sOnLoadEvent);
	bigpemu_unregister_event(pMod, sOnUnloadEvent);
	bigpemu_unregister_event(pMod, sOnVideoEvent);
}
