// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif

enum EShaderPlatform : uint16;

namespace UE::StereoRenderUtils
{

/*
	* Detect the single-draw stereo shader variant, in order to support usage across different platforms
	*/
class FStereoShaderAspects
{
public:
	/**
	* Determines the stereo aspects of the shader pipeline based on the input shader platform
	* @param Platform	Target shader platform used to determine stereo shader variant
	*/
	RENDERCORE_API FStereoShaderAspects(EShaderPlatform Platform);

	/**
	* Whether instanced stereo rendering is enabled - i.e. using a single instanced drawcall to render to both stereo views.
	* The output is redirected via the viewport index.
	*/
	inline bool IsInstancedStereoEnabled() const { return bInstancedStereoEnabled; }

	/**
	* Whether mobile multiview is enabled - i.e. using VK_KHR_multiview. Another drawcall reduction technique, independent of instanced stereo.
	* Mobile multiview generates view indices to index into texture arrays.
	* Can be internally emulated using instanced stereo if native support is unavailable, by using ISR-generated view indices to index into texture arrays.
	*/
	inline bool IsMobileMultiViewEnabled() const { return bMobileMultiViewEnabled; };

	/**
	* Whether multiviewport rendering is enabled - i.e. using ViewportIndex to index into viewport.
	* Relies on instanced stereo rendering being enabled.
	*/
	inline bool IsInstancedMultiViewportEnabled() const { return bInstancedMultiViewportEnabled; };

private:
	bool bInstancedStereoEnabled : 1;
	bool bMobileMultiViewEnabled : 1;
	bool bInstancedMultiViewportEnabled : 1;

	bool bInstancedStereoNative : 1;
	bool bMobileMultiViewNative : 1;
	bool bMobileMultiViewFallback : 1;

};

RENDERCORE_API void LogISRInit(const UE::StereoRenderUtils::FStereoShaderAspects& Aspects);

RENDERCORE_API void VerifyISRConfig(const UE::StereoRenderUtils::FStereoShaderAspects& Aspects, EShaderPlatform ShaderPlatform);

} // namespace UE::StereoRenderUtils
