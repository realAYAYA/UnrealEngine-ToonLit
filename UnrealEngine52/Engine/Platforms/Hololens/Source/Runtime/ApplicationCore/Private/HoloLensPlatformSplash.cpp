// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensPlatformSplash.h"
#include "HoloLensPlatformApplicationMisc.h"

void FHoloLensSplash::Show()
{
	//@todo.HoloLens: Implement me
	FHoloLensPlatformApplicationMisc::PumpMessages(true);
}

void FHoloLensSplash::Hide()
{
	//@todo.HoloLens: Implement me
	FHoloLensPlatformApplicationMisc::PumpMessages(true);
}

void FHoloLensSplash::SetSplashText( const SplashTextType::Type InType, const TCHAR* InText )
{
	//@todo.HoloLens: Implement me
}
