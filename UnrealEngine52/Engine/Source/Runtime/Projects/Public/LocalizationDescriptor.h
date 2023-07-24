// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Serialization/JsonWriter.h"

class FJsonObject;
class FText;

/**
 * Policy by which the localization data associated with a target should be loaded.
 */
namespace ELocalizationTargetDescriptorLoadingPolicy
{
	enum Type
	{
		/** The localization data will never be loaded automatically. */
		Never,
		/** The localization data will always be loaded automatically. */
		Always,
		/** The localization data will only be loaded when running the editor. Use if the target localizes the editor. */
		Editor,
		/** The localization data will only be loaded when running the game. Use if the target localizes your game. */
		Game,
		/** The localization data will only be loaded if the editor is displaying localized property names. */
		PropertyNames,
		/** The localization data will only be loaded if the editor is displaying localized tool tips. */
		ToolTips,
		/** NOTE: If you add a new value, make sure to update the ToString() method below!. */
		Max,
	};

	/**
	 * Converts a string to a ELocalizationTargetDescriptorLoadingPolicy::Type value
	 *
	 * @param	The string to convert to a value
	 * @return	The corresponding value, or 'Max' if the string is not valid.
	 */
	PROJECTS_API ELocalizationTargetDescriptorLoadingPolicy::Type FromString(const TCHAR *Text);

	/**
	 * Returns the name of a localization loading policy.
	 *
	 * @param	The value to convert to a string
	 * @return	The string representation of this enum value
	 */
	PROJECTS_API const TCHAR* ToString(const ELocalizationTargetDescriptorLoadingPolicy::Type Value);
};

/**
 * Description of a localization target.
 */
struct PROJECTS_API FLocalizationTargetDescriptor
{
	/** Name of this target */
	FString Name;

	/** When should the localization data associated with a target should be loaded? */
	ELocalizationTargetDescriptorLoadingPolicy::Type LoadingPolicy;

	/** Normal constructor */
	FLocalizationTargetDescriptor(FString InName = FString(), ELocalizationTargetDescriptorLoadingPolicy::Type InLoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::Never);

	/** Reads a descriptor from the given JSON object */
	bool Read(const FJsonObject& InObject, FText* OutFailReason = nullptr);

	/** Reads a descriptor from the given JSON object */
	bool Read(const FJsonObject& InObject, FText& OutFailReason);

	/** Reads an array of targets from the given JSON object */
	static bool ReadArray(const FJsonObject& InObject, const TCHAR* InName, TArray<FLocalizationTargetDescriptor>& OutTargets, FText* OutFailReason = nullptr);

	/** Reads an array of targets from the given JSON object */
	static bool ReadArray(const FJsonObject& InObject, const TCHAR* InName, TArray<FLocalizationTargetDescriptor>& OutTargets, FText& OutFailReason);

	/** Writes a descriptor to JSON */
	void Write(TJsonWriter<>& Writer) const;

	/** Updates the given json object with values in this descriptor */
	void UpdateJson(FJsonObject& JsonObject) const;

	/** Writes an array of targets to JSON */
	static void WriteArray(TJsonWriter<>& Writer, const TCHAR* ArrayName, const TArray<FLocalizationTargetDescriptor>& Descriptors);

	/** Updates an array of descriptors in the specified JSON field (indexed by name) */
	static void UpdateArray(FJsonObject& JsonObject, const TCHAR* ArrayName, const TArray<FLocalizationTargetDescriptor>& Descriptors);

	/** Returns true if we should load this localization target based upon the current runtime environment */
	bool ShouldLoadLocalizationTarget() const;
};
