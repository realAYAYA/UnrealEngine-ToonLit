// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class FObjectPreSaveContext;
class UObject;
class UVCamOutputProviderBase;
class UVCamModifier;
struct FModifierStackEntry;

template<typename T>
struct TObjectPtr;

namespace UE::VCamCore::CookingUtils::Private
{
#if WITH_EDITOR
	/** Removes every entry that is either 1. null or 2. does not satisfy CanIncludeInCookedGame. */
	void RemoveUnsupportedOutputProviders(TArray<TObjectPtr<UVCamOutputProviderBase>>& OutputProviders, const FObjectPreSaveContext& SaveContext);
	
	/** Removes every entry that is either 1. null or 2. does not satisfy CanIncludeInCookedGame. */
	void RemoveUnsupportedModifiers(TArray<FModifierStackEntry>& OutputProviders, const FObjectPreSaveContext& SaveContext);

	/**
	 * Determines whether it is safe to package this object, which is usually a UVCamOutputProvider or UVCamModifier.
	 * 
	 * This function does two things:
	 *  - Check whether Object returns true from IsEditorOnly() (optimization)
	 *  - Check whether the first native (parent) class is in a runtime module that supported by the target platform.
	 *
	 * Editor-only Blueprint UVCamModifiers can inherit from UEditorOnlyVCamModifier to be skipped.
	 *
	 *  @return Whether to include this object in a build.
	 */
	bool CanIncludeInCookedGame(UObject& Object, const FObjectPreSaveContext& SaveContext);
#endif
};
