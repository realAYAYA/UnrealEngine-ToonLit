// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ReadOnlyCVarCache.h: Cache of read-only console variables used by the renderer
=============================================================================*/

#pragma once
#include "RHIShaderPlatform.h"

struct FReadOnlyCVARCache
{
	static void Initialize();
		
	static inline bool AllowStaticLighting()
	{
		checkSlow(bInitialized);
		#ifdef PROJECT_CVAR_ALLOW_STATIC_LIGHTING
			return PROJECT_CVAR_ALLOW_STATIC_LIGHTING;
		#else	
			return bAllowStaticLighting;
		#endif	
	}

	static inline bool EnablePointLightShadows()
	{
		return bEnablePointLightShadows;
	}
	
	static inline bool EnableStationarySkylight()
	{
		return bEnableStationarySkylight;
	}
	
	static inline bool EnableLowQualityLightmaps()
	{
		return bEnableLowQualityLightmaps;
	}

	static inline bool SupportSkyAtmosphere()
	{
		return bSupportSkyAtmosphere;
	}

	// Mobile specific
	static inline bool MobileHDR()
	{
		checkSlow(bInitialized);
		#ifdef PROJECT_CVAR_MOBILE_HDR
			return PROJECT_CVAR_MOBILE_HDR;
		#else
			return bMobileHDR;
		#endif	
	}

	static inline bool MobileSupportsGPUScene()
	{
		checkSlow(bInitialized);
		#ifdef PROJECT_CVAR_MOBILE_SUPPORTS_GPUSCENE
			return PROJECT_CVAR_MOBILE_SUPPORTS_GPUSCENE;
		#else
			return bMobileSupportsGPUScene;
		#endif
	}

	static inline bool MobileAllowMovableDirectionalLights()
	{
		return bMobileAllowMovableDirectionalLights;
	}

	static inline bool MobileAllowDistanceFieldShadows()
	{
		return bMobileAllowDistanceFieldShadows;
	}
	
	static inline bool MobileEnableStaticAndCSMShadowReceivers()
	{
		return bMobileEnableStaticAndCSMShadowReceivers;
	}

	static inline bool MobileEnableMovableLightCSMShaderCulling()
	{
		return bMobileEnableMovableLightCSMShaderCulling;
	}
	
	static inline int32 MobileSkyLightPermutation()
	{
		return MobileSkyLightPermutationValue;
	}

	static inline bool MobileEnableNoPrecomputedLightingCSMShader()
	{
		return bMobileEnableNoPrecomputedLightingCSMShader;
	}

	static inline int32 MobileEarlyZPass(EShaderPlatform Platform)
	{
		#if WITH_EDITOR
			return MobileEarlyZPassIniValue(Platform);
		#else
			return MobileEarlyZPassValue;
		#endif
	}
	
	static inline int32 MobileForwardLocalLights(EShaderPlatform Platform)
	{
		#if WITH_EDITOR
			return MobileForwardLocalLightsIniValue(Platform);
		#elif defined PROJECT_CVAR_MOBILE_FORWARD_LOCALLIGHTS
			return PROJECT_CVAR_MOBILE_FORWARD_LOCALLIGHTS;
		#else
			return MobileForwardLocalLightsValue;
		#endif
	}
	
	static inline bool MobileDeferredShading(EShaderPlatform Platform)
	{
		#if WITH_EDITOR
			return MobileDeferredShadingIniValue(Platform);
		#elif defined PROJECT_CVAR_MOBILE_DEFERRED_SHADING
			return PROJECT_CVAR_MOBILE_DEFERRED_SHADING;
		#else
			return bMobileDeferredShadingValue;
		#endif
	}

	static inline bool MobileEnableMovableSpotlightsShadow(EShaderPlatform Platform)
	{
		#if WITH_EDITOR
			return MobileEnableMovableSpotlightsShadowIniValue(Platform);
		#else
			return bMobileEnableMovableSpotlightsShadowValue;
		#endif
	}

private:
	RENDERCORE_API static bool bInitialized;

	RENDERCORE_API static bool bAllowStaticLighting;
	RENDERCORE_API static bool bEnablePointLightShadows;
	RENDERCORE_API static bool bEnableStationarySkylight;
	RENDERCORE_API static bool bEnableLowQualityLightmaps;
	RENDERCORE_API static bool bSupportSkyAtmosphere;

	// Mobile specific
	RENDERCORE_API static bool bMobileHDR;
	RENDERCORE_API static bool bMobileAllowMovableDirectionalLights;
	RENDERCORE_API static bool bMobileAllowDistanceFieldShadows;
	RENDERCORE_API static bool bMobileEnableStaticAndCSMShadowReceivers;
	RENDERCORE_API static bool bMobileEnableMovableLightCSMShaderCulling;
	RENDERCORE_API static bool bMobileSupportsGPUScene;
	RENDERCORE_API static int32 MobileSkyLightPermutationValue;
	RENDERCORE_API static int32 MobileEarlyZPassValue;
	RENDERCORE_API static int32 MobileForwardLocalLightsValue;
	RENDERCORE_API static bool bMobileEnableNoPrecomputedLightingCSMShader;
	RENDERCORE_API static bool bMobileDeferredShadingValue;
	RENDERCORE_API static bool bMobileEnableMovableSpotlightsShadowValue;

private:
	RENDERCORE_API static int32 MobileEarlyZPassIniValue(EShaderPlatform Platform);
	RENDERCORE_API static int32 MobileForwardLocalLightsIniValue(EShaderPlatform Platform);
	RENDERCORE_API static bool MobileDeferredShadingIniValue(EShaderPlatform Platform);
	RENDERCORE_API static bool MobileEnableMovableSpotlightsShadowIniValue(EShaderPlatform Platform);
};

