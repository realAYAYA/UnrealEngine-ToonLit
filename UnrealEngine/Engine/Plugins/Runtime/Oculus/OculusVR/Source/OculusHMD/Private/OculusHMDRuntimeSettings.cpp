// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMDRuntimeSettings.h"

//////////////////////////////////////////////////////////////////////////
// UDEPRECATED_UOculusHMDRuntimeSettings

#include "OculusHMD_Settings.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

UDEPRECATED_UOculusHMDRuntimeSettings::UDEPRECATED_UOculusHMDRuntimeSettings(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
, bAutoEnabled(true)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	// FSettings is the sole source of truth for Oculus default settings
	OculusHMD::FSettings DefaultSettings; 
	bSupportsDash = DefaultSettings.Flags.bSupportsDash;
	bCompositesDepth = DefaultSettings.Flags.bCompositeDepth;
	bHQDistortion = DefaultSettings.Flags.bHQDistortion;
	CPULevel = DefaultSettings.CPULevel;
	GPULevel = DefaultSettings.GPULevel;
	PixelDensityMin = DefaultSettings.PixelDensityMin;
	PixelDensityMax = DefaultSettings.PixelDensityMax;
	bFocusAware = DefaultSettings.Flags.bFocusAware;
	XrApi = DefaultSettings.XrApi;
	ColorSpace = DefaultSettings.ColorSpace;
	bRequiresSystemKeyboard = DefaultSettings.Flags.bRequiresSystemKeyboard;
	HandTrackingSupport = DefaultSettings.HandTrackingSupport;
	HandTrackingFrequency = DefaultSettings.HandTrackingFrequency;
	bPhaseSync = DefaultSettings.bPhaseSync;

#else
	// Some set of reasonable defaults, since blueprints are still available on non-Oculus platforms.
	bSupportsDash = false;
	bCompositesDepth = false;
	bHQDistortion = false;
	CPULevel = 2;
	GPULevel = 3;
	PixelDensityMin = 0.5f;
	PixelDensityMax = 1.0f;
	bFocusAware = true;
    XrApi = EOculusXrApi::LegacyOVRPlugin;
	bLateLatching = false;
	bPhaseSync = false;
	ColorSpace = EOculusColorSpace::Rift_CV1;
	bRequiresSystemKeyboard = false;
	HandTrackingSupport = EHandTrackingSupport::ControllersOnly;
	HandTrackingFrequency = EHandTrackingFrequency::Low;
#endif

	LoadFromIni();
}

#if WITH_EDITOR
bool UDEPRECATED_UOculusHMDRuntimeSettings::CanEditChange(const FProperty* InProperty) const
{
	bool bIsEditable = Super::CanEditChange(InProperty);
	if (bIsEditable && InProperty)
	{
		const FName PropertyName = InProperty->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UDEPRECATED_UOculusHMDRuntimeSettings, bFocusAware))
		{
			bIsEditable = false;
		}
	}

	return bIsEditable;
}
#endif // WITH_EDITOR

void UDEPRECATED_UOculusHMDRuntimeSettings::LoadFromIni()
{
	const TCHAR* OculusSettings = TEXT("Oculus.Settings");
	bool v;
	float f;
	FVector vec;

	if (GConfig->GetFloat(OculusSettings, TEXT("PixelDensityMax"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		PixelDensityMax = f;
	}
	if (GConfig->GetFloat(OculusSettings, TEXT("PixelDensityMin"), f, GEngineIni))
	{
		check(!FMath::IsNaN(f));
		PixelDensityMin = f;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bHQDistortion"), v, GEngineIni))
	{
		bHQDistortion = v;
	}
	if (GConfig->GetBool(OculusSettings, TEXT("bCompositeDepth"), v, GEngineIni))
	{
		bCompositesDepth = v;
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
