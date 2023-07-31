// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if  PLATFORM_IOS && !PLATFORM_TVOS
#include "IOS/IOSPlatformWebAuth.h"
#elif PLATFORM_ANDROID
#include "Android/AndroidPlatformWebAuth.h"
#else
#include "NullPlatformWebAuth.h"
#endif

