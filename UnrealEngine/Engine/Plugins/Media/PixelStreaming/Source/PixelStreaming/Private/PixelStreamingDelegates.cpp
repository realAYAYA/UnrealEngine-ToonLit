// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingDelegates.h"

UPixelStreamingDelegates* UPixelStreamingDelegates::Singleton = nullptr;

UPixelStreamingDelegates* UPixelStreamingDelegates::CreateInstance()
{
	if (Singleton == nullptr)
	{
		Singleton = NewObject<UPixelStreamingDelegates>();
		Singleton->AddToRoot();
		return Singleton;
	}
	return Singleton;
}
