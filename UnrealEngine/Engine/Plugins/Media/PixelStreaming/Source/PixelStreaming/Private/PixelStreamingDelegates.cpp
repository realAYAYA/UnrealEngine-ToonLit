// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingDelegates.h"

UPixelStreamingDelegates* UPixelStreamingDelegates::Singleton = nullptr;
bool UPixelStreamingDelegates::bIsExiting = false;

UPixelStreamingDelegates* UPixelStreamingDelegates::CreateInstance()
{
	if (Singleton == nullptr && !bIsExiting)
	{
		Singleton = NewObject<UPixelStreamingDelegates>();
		Singleton->AddToRoot();
		return Singleton;
	}
	return Singleton;
}
