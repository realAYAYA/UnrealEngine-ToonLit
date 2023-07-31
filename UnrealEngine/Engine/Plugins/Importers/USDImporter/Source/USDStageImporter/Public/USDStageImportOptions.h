// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealUSDWrapper.h"
#include "USDStageOptions.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Factories/MaterialImportHelpers.h"
#include "GroomAssetInterpolation.h"
#include "UObject/ObjectMacros.h"

#include "USDStageImportOptions.generated.h"

struct FAnalyticsEventAttribute;

UENUM(BlueprintType)
enum class EReplaceActorPolicy : uint8
{
	/** Spawn new actors and components alongside the existing ones */
	Append,

	/** Replaces existing actors and components with new ones */
	Replace,

	/** Update transforms on existing actors but do not replace actors or components */
	UpdateTransform,

	/** Ignore any conflicting new assets and components, keeping the old ones */
	Ignore,
};

UENUM(BlueprintType)
enum class EReplaceAssetPolicy : uint8
{
	/** Create new assets with numbered suffixes */
	Append,

	/** Replaces existing asset with new asset */
	Replace,

	/** Ignores the new asset and keeps the existing asset */
	Ignore,
};

UCLASS(config = EditorPerProjectUserSettings)
class USDSTAGEIMPORTER_API UUsdStageImportOptions : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "DataToImport", meta = (DisplayName = "Actors"))
	bool bImportActors;

	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "DataToImport", meta = (DisplayName = "Geometry"))
	bool bImportGeometry;

	/** Whether to try importing UAnimSequence skeletal animation assets for each encountered UsdSkelAnimQuery */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "DataToImport", meta = (EditCondition = bImportGeometry, DisplayName = "Skeletal Animations"))
	bool bImportSkeletalAnimations;

	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "DataToImport", meta = (DisplayName = "LevelSequences" ) )
	bool bImportLevelSequences;

	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "DataToImport", meta = (DisplayName = "Materials & Textures"))
	bool bImportMaterials;

	/**
	 * List of paths of prims to import (e.g. ["/Root/MyBox", "/Root/OtherPrim"]).
	 * Importing a prim will import its entire subtree.
	 * If this list contains the root prim path the entire stage will be imported (default value).
	 */
	UPROPERTY( BlueprintReadWrite, EditAnywhere, Category = "Prims to Import" )
	TArray<FString> PrimsToImport = TArray<FString>{ TEXT( "/" ) };

	/** Only import prims with these specific purposes from the USD file */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category= "USD options", meta = (Bitmask, BitmaskEnum="/Script/UnrealUSDWrapper.EUsdPurpose"))
	int32 PurposesToImport;

	/** Try enabling Nanite for static meshes that are generated with at least this many triangles */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "USD options", meta = ( NoSpinbox = "true", UIMin = "0", ClampMin = "0" ) )
	int32 NaniteTriangleThreshold;

	/** Specifies which set of shaders to use when parsing USD materials, in addition to the universal render context. */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category= "USD options")
	FName RenderContextToImport;

	/** Specifies which material purpose to use when parsing USD material bindings, in addition to the "allPurpose" fallback */
	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "USD options" )
	FName MaterialPurpose;

	// Describes what to add to the root bone animation within generated AnimSequences, if anything
	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "USD options", meta = ( EditCondition = bImportSkeletalAnimations ) )
	EUsdRootMotionHandling RootMotionHandling = EUsdRootMotionHandling::NoAdditionalRootMotion;

	/** Whether to use the specified StageOptions instead of the stage's own settings */
	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "USD options" )
	bool bOverrideStageOptions;

	/** Custom StageOptions to use for the stage */
	UPROPERTY( EditAnywhere, config, BlueprintReadWrite, Category = "USD options", meta = ( EditCondition = bOverrideStageOptions ) )
	FUsdStageOptions StageOptions;


	/** Groom group interpolation settings */
	UPROPERTY(EditAnywhere, config, BlueprintReadWrite, Category = "Groom")
	TArray<FHairGroupsInterpolation> GroomInterpolationSettings;


	/** What should happen when imported actors and components try to overwrite existing actors and components */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "Collision", meta=(EditCondition=bImportActors))
	EReplaceActorPolicy ExistingActorPolicy;

	/** What should happen when imported assets try to overwrite existing assets */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "Collision")
	EReplaceAssetPolicy ExistingAssetPolicy;



	/**
	 * When enabled, assets will be imported into a content folder structure according to their prim path. When disabled,
	 * assets are imported into content folders according to asset type (e.g. 'Materials', 'StaticMeshes', etc).
	 */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "Processing")
	bool bPrimPathFolderStructure;

	/**
	 * Whether to try to combine individual assets and components of the same type on a kind-per-kind basis,
	 * like multiple Mesh prims into a single Static Mesh
	 */
	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "Processing", meta = ( Bitmask, BitmaskEnum = "/Script/UnrealUSDWrapper.EUsdDefaultKind" ) )
	int32 KindsToCollapse;

	/**
	 * If enabled, when multiple mesh prims are collapsed into a single static mesh, identical material slots are merged into one slot.
	 * Otherwise, material slots are simply appended to the list.
	 */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category = "Processing")
	bool bMergeIdenticalMaterialSlots;

	/**
	 * If true, will cause us to collapse any point instancer prim into a single static mesh and static mesh component.
	 * If false, will cause us to use HierarchicalInstancedStaticMeshComponents to replicate the instancing behavior.
	 * Point instancers inside other point instancer prototypes are *always* collapsed into the prototype's static mesh.
	 */
	UPROPERTY( BlueprintReadWrite, config, EditAnywhere, Category = "Processing" )
	bool bCollapseTopLevelPointInstancers;

	/** When true, if a prim has a "LOD" variant set with variants named "LOD0", "LOD1", etc. where each contains a UsdGeomMesh, the importer will attempt to parse the meshes as separate LODs of a single UStaticMesh. When false, only the selected variant will be parsed as LOD0 of the UStaticMesh.  */
	UPROPERTY(BlueprintReadWrite, config, EditAnywhere, Category="Processing", meta=(DisplayName="Interpret LOD variant sets", EditCondition=bImportGeometry) )
	bool bInterpretLODs;

public:
	void EnableActorImport(bool bEnable);
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};

namespace UsdUtils
{
	USDSTAGEIMPORTER_API void AddAnalyticsAttributes(
		const UUsdStageImportOptions& Options,
		TArray< FAnalyticsEventAttribute >& InOutAttributes
	);
}
