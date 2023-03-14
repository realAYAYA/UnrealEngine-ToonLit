// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImgMediaSettings)


/* UImgMediaSettings structors
 *****************************************************************************/

UImgMediaSettings::UImgMediaSettings()
	: DefaultFrameRate(24, 1)
	, CacheBehindPercentage(25)
	, CacheSizeGB(1.0f)
	, CacheThreads(2)
	, CacheThreadStackSizeKB(128)
	, GlobalCacheSizeGB(1.0f)
	, UseGlobalCache(true)
	, ExrDecoderThreads(0)
	, DefaultProxy(TEXT("proxy"))
	, UseDefaultProxy(false)
{ }

void UImgMediaSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ValidateSettings();
}

#if WITH_EDITOR
void UImgMediaSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	ValidateSettings();

	SettingsChangedDelegate.Broadcast(this);
}

UImgMediaSettings::FOnImgMediaSettingsChanged& UImgMediaSettings::OnSettingsChanged()
{
	return SettingsChangedDelegate;
}

UImgMediaSettings::FOnImgMediaSettingsChanged UImgMediaSettings::SettingsChangedDelegate;
#endif

void UImgMediaSettings::ValidateSettings()
{
	// Global cache size cannot be smaller than the cache size.
	if (GlobalCacheSizeGB < CacheSizeGB)
	{
		GlobalCacheSizeGB = CacheSizeGB;
	}
}

