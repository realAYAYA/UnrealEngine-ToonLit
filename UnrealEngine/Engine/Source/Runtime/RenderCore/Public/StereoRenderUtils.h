// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIDefinitions.h"

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

}
