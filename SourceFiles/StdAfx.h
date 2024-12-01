




#pragma once



#ifndef _WIN32 
  #include "brx_platform_linux.h"
#endif 



#define STRICT

#ifndef WINVER
#define WINVER 0x0501
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif




#if defined(_WIN32) && defined(_DEBUG) && !defined(BRX_BCAD_DEBUG)
  #define _DEBUG_WAS_DEFINED
  #undef _DEBUG
  #define NDEBUG
  #pragma message ("     Compiling MFC / STL / ATL header files in release mode.")
#endif



#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>



#ifdef _WIN32 

  #ifdef BRX_APP 

    #import "axbricscaddb1.tlb" no_registry
    #import "axbricscadapp1.tlb" no_registry

  #endif 

#endif 



#include "arxHeaders.h"




#ifdef _DEBUG_WAS_DEFINED
  #undef NDEBUG
  #define _DEBUG
  #undef _DEBUG_WAS_DEFINED
#endif



#include "brx_version.h"
