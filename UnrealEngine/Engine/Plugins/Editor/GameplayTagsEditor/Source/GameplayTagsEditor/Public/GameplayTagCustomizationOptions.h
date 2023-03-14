// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct GAMEPLAYTAGSEDITOR_API FGameplayTagCustomizationOptions
{
	// If true, any Gameplay Tag Widget created should not offer an 'Add Tag' option 
	bool bForceHideAddTag = false;

	// If true, any created Gameplay Tag Widget created should not offer an 'Add Tag Source' option 
	bool bForceHideAddTagSource = false;
};