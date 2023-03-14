// Copyright Epic Games, Inc. All Rights Reserved.

#include "Profile/MediaProfileBlueprintLibrary.h"

#include "Features/IModularFeatures.h"
#include "Profile/IMediaProfileManager.h"


UMediaProfile* UMediaProfileBlueprintLibrary::GetMediaProfile()
{
	return IMediaProfileManager::Get().GetCurrentMediaProfile();
}


void UMediaProfileBlueprintLibrary::SetMediaProfile(UMediaProfile* MediaProfile)
{
	return IMediaProfileManager::Get().SetCurrentMediaProfile(MediaProfile);
}


TArray<UProxyMediaSource*> UMediaProfileBlueprintLibrary::GetAllMediaSourceProxy()
{
	return IMediaProfileManager::Get().GetAllMediaSourceProxy();
}


TArray<UProxyMediaOutput*> UMediaProfileBlueprintLibrary::GetAllMediaOutputProxy()
{
	return IMediaProfileManager::Get().GetAllMediaOutputProxy();
}
