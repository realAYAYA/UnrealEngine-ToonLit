// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreMinimal.h"

/** Utility functions for dealing with localized package names */
struct FPackageLocalizationUtil
{
	/**
	 * Converts a localized version of a package path to the source version (by removing /L10N/<code> from the package path, if present)
	 * Note: This does not check whether the source package exists
	 *
	 * @param InLocalized	Path to the localized package.
	 * @param OutSource		Path to the source package.
	 *
	 * @returns True if the conversion happened, false otherwise
	 */
	static COREUOBJECT_API bool ConvertLocalizedToSource(const FString& InLocalized, FString& OutSource);

	/**
	 * Converts a source version of a package path to the localized version for the given culture (by adding /L10N/<code> to the package path)
	 * Note: This does not check whether the source package exists
	 *
	 * @param InSource		Path to the source package.
	 * @param InCulture		Culture code to use.
	 * @param OutLocalized	Path to the localized package.
	 *
	 * @returns True if the conversion happened, false otherwise
	 */
	static COREUOBJECT_API bool ConvertSourceToLocalized(const FString& InSource, const FString& InCulture, FString& OutLocalized);

	/**
	 * Given a package path, get the localized root package for the given culture (eg, if given "/Game/MyFolder/MyAsset" and a culture of "fr", this would return "/Game/L10N/fr")
	 *
	 * @param InPath		Package path to use.
	 * @param InCulture		Culture code to use (if any).
	 * @param OutLocalized	Localized package root.
	 *
	 * @returns True if the conversion happened, false otherwise
	 */
	static COREUOBJECT_API bool GetLocalizedRoot(const FString& InPath, const FString& InCulture, FString& OutLocalized);
};
