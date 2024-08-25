#ifndef _EasyBlendSDKPlatforms_H_
#define _EasyBlendSDKPlatforms_H_

// The following ifdef block is the standard way of creating macros
// which make exporting from a DLL simpler. All files within this
// DLL are compiled with the EasyBlendSDK_EXPORTS symbol defined on
// the command line. this symbol should not be defined on any
// project that uses this DLL. This way any other project whose
// source files include this file see EasyBlendSDK_API functions as
// being imported from a DLL, whereas this DLL sees symbols defined
// with this macro as being exported.

#if  !defined(_EASYBLENDSDK_LINUX) && !defined(_EASYBLENDSDK_STATIC) 
  #ifdef MESHSDK_EXPORTS
  #  define EasyBlendSDK_API extern "C" __declspec(dllexport)
  #else
  #  define EasyBlendSDK_API extern "C" __declspec(dllimport)
  #endif /* ifdef MESHSDK_EXPORTS */
#else
  #  define EasyBlendSDK_API extern "C"
#endif /* ifndef _EASYBLENDSDK_LINUX */


// methods for deprecated functions
#if  !defined(_EASYBLENDSDK_LINUX)
#  define EASYBLENDSDK_DEPRECATED_CALL(txt,func)       \
          __declspec(deprecated(txt)) func
#else
#  define EASYBLENDSDK_DEPRECATED_CALL(txt,func)        \
          func __attribute__ ((deprecated))
#endif /* ifndef _EASYBLENDSDK_LINUX */

#if defined(_WIN32) || defined(WIN32)
	#define EASYBLENDSDK_GRAPHICS_API_DX12

	//EasyBlend VK and OGL is not currently supported by nDisplay.
	//#define EASYBLENDSDK_GRAPHICS_API_OGL
	//#define EASYBLENDSDK_GRAPHICS_API_VK
#endif

#endif // _EasyBlendSDKPlatforms_H_
