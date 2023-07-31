// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

// Plugin-wide log categories
#if UE_BUILD_SHIPPING
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShare,    Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareObject, Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareObjectProxy, Warning, Warning);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareWorldSubsystem,  Warning, Warning);

#else
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShare,    Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareObject, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareObjectProxy, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTextureShareWorldSubsystem,  Log, All);

#endif
