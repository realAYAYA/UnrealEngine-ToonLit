// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

struct GAMEPLAYTAGSEDITOR_API FGameplayTagCustomizationOptions
{
	// If true, any Gameplay Tag Widget created should not offer an 'Add Tag' option 
	bool bForceHideAddTag = false;

	// If true, any created Gameplay Tag Widget created should not offer an 'Add Tag Source' option 
	bool bForceHideAddTagSource = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreTypes.h"
#endif
