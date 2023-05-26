#include "pch.h"

#include "BigPEye.h"
#include "Pimpl.hpp"

Pimpl* gPimpl = nullptr;

void createBigPEye()
{
  gPimpl = new Pimpl();
}

void updateMemory( void const* ptr )
{
  if ( gPimpl )
    gPimpl->updateMemory( ptr );
}

void destroyBigPEye()
{
  delete gPimpl;
  gPimpl = nullptr;
}
