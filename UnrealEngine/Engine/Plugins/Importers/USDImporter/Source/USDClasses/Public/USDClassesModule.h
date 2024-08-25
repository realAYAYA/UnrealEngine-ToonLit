// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Modules/ModuleInterface.h"

class UMaterialInstanceConstant;
class UMaterialInstanceDynamic;
struct FAnalyticsEventAttribute;

enum class EUsdReferenceMaterialProperties : uint8
{
	None = 0,
	Translucent = 1,
	VT = 2,
	TwoSided = 4
};
ENUM_CLASS_FLAGS(EUsdReferenceMaterialProperties)

class IUsdClassesModule : public IModuleInterface
{
public:
	/** Updates all plugInfo.json to point their LibraryPaths to TargetDllFolder */
	USDCLASSES_API static void UpdatePlugInfoFiles(const FString& PluginDirectory, const FString& TargetDllFolder);

	/**
	 * Sends analytics about a USD operation
	 * @param InAttributes - Additional analytics events attributes to send, along with new ones collected within this function
	 * @param EventName - Name of the analytics event (e.g. "Export.StaticMesh", so that the full event name is "Engine.Usage.USD.Export.StaticMesh")
	 * @param bAutomated - If the operation was automated (e.g. came from a Python script)
	 * @param ElapsedSeconds - How long the operation took in seconds
	 * @param NumberOfFrames - Number of time codes in the exported/imported/opened stage
	 * @param Extension - Extension of the main USD file opened/emitted/imported (e.g. "usda" or "usd")
	 */
	USDCLASSES_API static void SendAnalytics(
		TArray<FAnalyticsEventAttribute>&& InAttributes,
		const FString& EventName,
		bool bAutomated,
		double ElapsedSeconds,
		double NumberOfFrames,
		const FString& Extension
	);
	USDCLASSES_API static void SendAnalytics(TArray<FAnalyticsEventAttribute>&& InAttributes, const FString& EventName);

	/**
	 * Updates HashToUpdate with the Object's package's persistent guid, the corresponding file save
	 * date and time, and the number of times the package has been dirtied since last being saved.
	 * This can be used to track the version of exported assets and levels, to prevent unnecessary re-exports.
	 */
	USDCLASSES_API static bool HashObjectPackage(const UObject* Object, FSHA1& HashToUpdate);

	/** Returns a world that could be suitably described as "the current world". (e.g. when in PIE, the PIE world) */
	USDCLASSES_API static UWorld* GetCurrentWorld(bool bEditorWorldsOnly = false);

	/**
	 * Returns the set of assets that this object depends on (e.g. when given a material, will return its textures.
	 * When given a mesh, will return materials, etc.)
	 */
	USDCLASSES_API static TSet<UObject*> GetAssetDependencies(UObject* Asset);

	// Adapted from ObjectTools as it is within an Editor-only module
	USDCLASSES_API static FString SanitizeObjectName(const FString& InObjectName);

	/** Describes the type of vertex color/DisplayColor material that we would need in order to render a prim's displayColor data as intended */
	struct USDCLASSES_API FDisplayColorMaterial
	{
		bool bHasOpacity = false;
		bool bIsDoubleSided = false;

		FString ToString();
		static TOptional<FDisplayColorMaterial> FromString(const FString& DisplayColorString);
	};

	USDCLASSES_API static const FSoftObjectPath* GetReferenceMaterialPath(const FDisplayColorMaterial& DisplayColorDescription);

	USDCLASSES_API static UMaterialInstanceDynamic* CreateDisplayColorMaterialInstanceDynamic(const FDisplayColorMaterial& DisplayColorDescription);
	USDCLASSES_API static UMaterialInstanceConstant* CreateDisplayColorMaterialInstanceConstant(const FDisplayColorMaterial& DisplayColorDescription);
};
