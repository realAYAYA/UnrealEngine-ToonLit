// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/CoreMiscDefines.h"

struct UE_DEPRECATED(5.3, "Options are not used anymore to customize the property customization behavior.") GAMEPLAYTAGSEDITOR_API FGameplayTagCustomizationOptions
{
	// If true, any Gameplay Tag Widget created should not offer an 'Add Tag' option 
	bool bForceHideAddTag = false;

	// If true, any created Gameplay Tag Widget created should not offer an 'Add Tag Source' option 
	bool bForceHideAddTagSource = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreTypes.h"
#endif
