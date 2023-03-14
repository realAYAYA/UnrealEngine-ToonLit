// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/MeshMerging.h"
#include "GameFramework/Actor.h"
#include "BodySetupEnums.h"
#include "UVMapSettings.h"
#include "StaticMeshEditorSubsystemHelpers.h"
#include "StaticMeshEditorSubsystem.h"
#include "EditorStaticMeshLibrary.generated.h"

class UStaticMeshComponent;

//  Deprecated as of 5.0, use the struct FStaticMeshReductionSettings in Static Mesh Editor Library Instead
USTRUCT(BlueprintType)
struct FEditorScriptingMeshReductionSettings_Deprecated
{
	GENERATED_BODY()

	FEditorScriptingMeshReductionSettings_Deprecated()
		: PercentTriangles(0.5f)
		, ScreenSize(0.5f)
	{ }

	// Percentage of triangles to keep. Ranges from 0.0 to 1.0: 1.0 = no reduction, 0.0 = no triangles.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float PercentTriangles;

	// ScreenSize to display this LOD. Ranges from 0.0 to 1.0.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ScreenSize;
};

//  Deprecated as of 5.0, use the struct FStaticMeshReductionOptions in Static Mesh Editor Library Instead
USTRUCT(BlueprintType)
struct FEditorScriptingMeshReductionOptions_Deprecated
{
	GENERATED_BODY()

	FEditorScriptingMeshReductionOptions_Deprecated()
		: bAutoComputeLODScreenSize(true)
	{ }

	// If true, the screen sizes at which LODs swap are computed automatically
	// @note that this is displayed as 'Auto Compute LOD Distances' in the UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoComputeLODScreenSize;

	// Array of reduction settings to apply to each new LOD mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FEditorScriptingMeshReductionSettings_Deprecated> ReductionSettings;
};

//  Deprecated as of 5.0, use the enum EScriptCollisionShapeType in Static Mesh Editor Library Instead
/** Types of Collision Construct that are generated **/
UENUM(BlueprintType)
enum class EScriptingCollisionShapeType_Deprecated : uint8
{
	Box,
	Sphere,
	Capsule,
	NDOP10_X,
	NDOP10_Y,
	NDOP10_Z,
	NDOP18,
	NDOP26
};

/**
 * Utility class to altering and analyzing a StaticMesh and use the common functionalities of the Mesh Editor.
 * The editor should not be in play in editor mode.
 */
UCLASS(deprecated)
class EDITORSCRIPTINGUTILITIES_API UDEPRECATED_EditorStaticMeshLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Remove then add LODs on a static mesh.
	 * The static mesh must have at least LOD 0.
	 * The LOD 0 of the static mesh is kept after removal.
	 * The build settings of LOD 0 will be applied to all subsequent LODs.
	 * @param	StaticMesh				Mesh to process.
	 * @param	ReductionOptions		Options on how to generate LODs on the mesh.
	 * @param	bApplyChanges				Indicates if change must be notified.
	 * @return the number of LODs generated on the input mesh.
	 * An negative value indicates that the reduction could not be performed. See log for explanation.
	 * No action will be performed if ReductionOptions.ReductionSettings is empty
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static int32 SetLodsWithNotification(UStaticMesh* StaticMesh, const FEditorScriptingMeshReductionOptions_Deprecated& ReductionOptions, bool bApplyChanges );

	/**
	 * Same as SetLodsWithNotification but changes are applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static int32 SetLods(UStaticMesh* StaticMesh, const FEditorScriptingMeshReductionOptions_Deprecated& ReductionOptions)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetLods(StaticMesh, ConvertReductionOptions(ReductionOptions)) : -1;
	}

	/**
	 * Copy the reduction options with the specified LOD reduction settings.
	 * @param StaticMesh - Mesh to process.
	 * @param LodIndex - The LOD we get the reduction settings.
	 * @param OutReductionOptions - The reduction settings where we copy the reduction options.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void GetLodReductionSettings(const UStaticMesh* StaticMesh, const int32 LodIndex, FMeshReductionSettings& OutReductionOptions);

	/**
	 * Set the LOD reduction for the specified LOD index.
	 * @param StaticMesh - Mesh to process.
	 * @param LodIndex - The LOD we will apply the reduction settings.
	 * @param ReductionOptions - The reduction settings we want to apply to the LOD.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void SetLodReductionSettings(UStaticMesh* StaticMesh, const int32 LodIndex, const FMeshReductionSettings& ReductionOptions);

	/**
	 * Copy the build options with the specified LOD build settings.
	 * @param StaticMesh - Mesh to process.
	 * @param LodIndex - The LOD we get the reduction settings.
	 * @param OutBuildOptions - The build settings where we copy the build options.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void GetLodBuildSettings(const UStaticMesh* StaticMesh, const int32 LodIndex, FMeshBuildSettings& OutBuildOptions);

	/**
	 * Set the LOD build options for the specified LOD index.
	 * @param StaticMesh - Mesh to process.
	 * @param LodIndex - The LOD we will apply the build settings.
	 * @param BuildOptions - The build settings we want to apply to the LOD.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void SetLodBuildSettings(UStaticMesh* StaticMesh, const int32 LodIndex, const FMeshBuildSettings& BuildOptions);

	/**
	 * Import or re-import a LOD into the specified base mesh. If the LOD do not exist it will import it and add it to the base static mesh. If the LOD already exist it will re-import the specified LOD.
	 * 
	 * @param BaseStaticMesh: The static mesh we import or re-import a LOD.
	 * @param LODIndex: The index of the LOD to import or re-import. Valid value should be between 0 and the base static mesh LOD number. Invalid value will return INDEX_NONE
	 * @param SourceFilename: The fbx source filename. If we are re-importing an existing LOD, it can be empty in this case it will use the last import file. Otherwise it must be an existing fbx file.
	 *
	 * @return the index of the LOD that was imported or re-imported. Will return INDEX_NONE if anything goes bad.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 ImportLOD(UStaticMesh* BaseStaticMesh, const int32 LODIndex, const FString& SourceFilename);

	/**
	 * Re-import all the custom LODs present in the specified static mesh.
	 *
	 * @param StaticMesh: is the static mesh we import or re-import all LODs.
	 *
	 * @return true if re-import all LODs works, false otherwise see log for explanation.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Library")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool ReimportAllCustomLODs(UStaticMesh* StaticMesh);

	/**
	 * Adds or create a LOD at DestinationLodIndex using the geometry from SourceStaticMesh SourceLodIndex
	 * @param	DestinationStaticMesh		The static mesh to set the LOD in.
	 * @param	DestinationLodIndex			The index of the LOD to set.
	 * @param	SourceStaticMesh			The static mesh to get the LOD from.
	 * @param	SourceLodIndex				The index of the LOD to get.
	 * @param	bReuseExistingMaterialSlots	If true, sections from SourceStaticMesh will be remapped to match the material slots of DestinationStaticMesh
											when they have the same material assigned. If false, all material slots of SourceStaticMesh will be appended in DestinationStaticMesh.
	 * @return	The index of the LOD that was set. It can be different than DestinationLodIndex if it wasn't a valid index.
	 *			A negative value indicates that the LOD was not set. See log for explanation.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 SetLodFromStaticMesh(UStaticMesh* DestinationStaticMesh, int32 DestinationLodIndex, UStaticMesh* SourceStaticMesh, int32 SourceLodIndex, bool bReuseExistingMaterialSlots);

	/**
	 * Get number of LODs present on a static mesh.
	 * @param	StaticMesh				Mesh to process.
	 * @return the number of LODs present on the input mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetLodCount(UStaticMesh* StaticMesh);

	/**
	 * Remove LODs on a static mesh except LOD 0.
	 * @param	StaticMesh			Mesh to remove LOD from.
	 * @return A boolean indicating if the removal was successful, true, or not.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool RemoveLods(UStaticMesh* StaticMesh);

	/**
	 * Get an array of LOD screen sizes for evaluation.
	 * @param	StaticMesh			Mesh to process.
	 * @return array of LOD screen sizes.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static TArray<float> GetLodScreenSizes(UStaticMesh* StaticMesh);

public:
	/**
	 * Add simple collisions to a static mesh.
	 * This method replicates what is done when invoking menu entries "Collision > Add [...] Simplified Collision" in the Mesh Editor.
	 * @param	StaticMesh			Mesh to generate simple collision for.
	 * @param	ShapeType			Options on which simple collision to add to the mesh.
	 * @param	bApplyChanges		Indicates if changes must be apply or not.
	 * @return An integer indicating the index of the collision newly created.
	 * A negative value indicates the addition failed.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static int32 AddSimpleCollisionsWithNotification(UStaticMesh* StaticMesh, const EScriptingCollisionShapeType_Deprecated ShapeType, bool bApplyChanges);

	/**
	 * Same as AddSimpleCollisionsWithNotification but changes are automatically applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static int32 AddSimpleCollisions(UStaticMesh* StaticMesh, const EScriptingCollisionShapeType_Deprecated ShapeType)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->AddSimpleCollisions(StaticMesh, ConvertCollisionShape(ShapeType)) : INDEX_NONE;
	}

	/**
	 * Get number of simple collisions present on a static mesh.
	 * @param	StaticMesh				Mesh to query on.
	 * @return An integer representing the number of simple collisions on the input static mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetSimpleCollisionCount(UStaticMesh* StaticMesh);

	/**
	 * Get the Collision Trace behavior of a static mesh
	 * @param	StaticMesh				Mesh to query on.
	 * @return the Collision Trace behavior.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static TEnumAsByte<ECollisionTraceFlag> GetCollisionComplexity(UStaticMesh* StaticMesh);

	/**
	 * Get number of convex collisions present on a static mesh.
	 * @param	StaticMesh				Mesh to query on.
	 * @return An integer representing the number of convex collisions on the input static mesh.
	 * An negative value indicates that the command could not be executed. See log for explanation.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetConvexCollisionCount(UStaticMesh* StaticMesh);

	/**
	 * Add a convex collision to a static mesh.
	 * Any existing collisions will be removed from the static mesh.
	 * This method replicates what is done when invoking menu entry "Collision > Auto Convex Collision" in the Mesh Editor.
	 * @param	StaticMesh				Static mesh to add convex collision on.
	 * @param	HullCount				Maximum number of convex pieces that will be created. Must be positive.
	 * @param	MaxHullVerts			Maximum number of vertices allowed for any generated convex hull.
	 * @param	HullPrecision			Number of voxels to use when generating collision. Must be positive.
	 * @param	bApplyChanges			Indicates if changes must be apply or not.
	 * @return A boolean indicating if the addition was successful or not.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool SetConvexDecompositionCollisionsWithNotification(UStaticMesh* StaticMesh, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision, bool bApplyChanges);

	/**
	 * Compute convex collisions for a set of static meshes.
	 * Any existing collisions will be removed from the static meshes.
	 * This method replicates what is done when invoking menu entry "Collision > Auto Convex Collision" in the Mesh Editor.
	 * @param	StaticMeshes			Set of Static mesh to add convex collision on.
	 * @param	HullCount				Maximum number of convex pieces that will be created. Must be positive.
	 * @param	MaxHullVerts			Maximum number of vertices allowed for any generated convex hull.
	 * @param	HullPrecision			Number of voxels to use when generating collision. Must be positive.
	 * @param	bApplyChanges			Indicates if changes must be apply or not.
	 * @return A boolean indicating if the addition was successful or not.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool BulkSetConvexDecompositionCollisionsWithNotification(const TArray<UStaticMesh*>& StaticMeshes, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision, bool bApplyChanges);

	/**
	 * Same as SetConvexDecompositionCollisionsWithNotification but changes are automatically applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool SetConvexDecompositionCollisions(UStaticMesh* StaticMesh, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetConvexDecompositionCollisions(StaticMesh, HullCount, MaxHullVerts, HullPrecision) : false;
	}

	/**
	 * Same as SetConvexDecompositionCollisionsWithNotification but changes are automatically applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool BulkSetConvexDecompositionCollisions(const TArray<UStaticMesh*>& StaticMeshes, int32 HullCount, int32 MaxHullVerts, int32 HullPrecision)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->BulkSetConvexDecompositionCollisions(StaticMeshes, HullCount, MaxHullVerts, HullPrecision) : false;
	}

	/**
	 * Remove collisions from a static mesh.
	 * This method replicates what is done when invoking menu entries "Collision > Remove Collision" in the Mesh Editor.
	 * @param	StaticMesh			Static mesh to remove collisions from.
	 * @param	bApplyChanges		Indicates if changes must be apply or not.
	 * @return A boolean indicating if the removal was successful or not.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool RemoveCollisionsWithNotification(UStaticMesh* StaticMesh, bool bApplyChanges);

	/**
	 * Same as RemoveCollisionsWithNotification but changes are applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool RemoveCollisions(UStaticMesh* StaticMesh)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->RemoveCollisions(StaticMesh) : false;
	}

	/**
	 * Enables/disables mesh section collision for a specific LOD.
	 * @param	StaticMesh			Static mesh to Enables/disables collisions from.
	 * @param	bCollisionEnabled	If the collision is enabled or not.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void EnableSectionCollision(UStaticMesh* StaticMesh, bool bCollisionEnabled, int32 LODIndex, int32 SectionIndex);

	/**
	 * Checks if a specific LOD mesh section has collision.
	 * @param	StaticMesh			Static mesh to remove collisions from.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 * @return True is the collision is enabled for the specified LOD of the StaticMesh section.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool IsSectionCollisionEnabled(UStaticMesh* StaticMesh, int32 LODIndex, int32 SectionIndex);

public:

	/**
	 * Enables/disables mesh section shadow casting for a specific LOD.
	 * @param	StaticMesh			Static mesh to Enables/disables shadow casting from.
	 * @param	bCastShadow			If the section should cast shadow.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	SectionIndex		Index of the StaticMesh Section.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void EnableSectionCastShadow(UStaticMesh* StaticMesh, bool bCastShadow, int32 LODIndex, int32 SectionIndex);

	/** Check whether a static mesh has vertex colors */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool HasVertexColors(UStaticMesh* StaticMesh);

	/** Check whether a static mesh component has vertex colors */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool HasInstanceVertexColors(UStaticMeshComponent* StaticMeshComponent);

	/** Set Generate Lightmap UVs for StaticMesh */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta=(ScriptName="SetGenerateLightmapUv", DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool SetGenerateLightmapUVs(UStaticMesh* StaticMesh, bool bGenerateLightmapUVs);

	/** Get number of StaticMesh verts for an LOD */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetNumberVerts(UStaticMesh* StaticMesh, int32 LODIndex);

	/** Get number of Materials for a StaticMesh */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetNumberMaterials(UStaticMesh* StaticMesh);

	/** Sets StaticMeshFlag bAllowCPUAccess  */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void SetAllowCPUAccess(UStaticMesh* StaticMesh, bool bAllowCPUAccess);

public:

	/**
	 * Returns the number of UV channels for the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh to query.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @return the number of UV channels.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 GetNumUVChannels(UStaticMesh* StaticMesh, int32 LODIndex);

	/**
	 * Adds an empty UV channel at the end of the existing channels on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to add a UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @return true if a UV channel was added.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool AddUVChannel(UStaticMesh* StaticMesh, int32 LODIndex);

	/**
	 * Inserts an empty UV channel at the specified channel index on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to insert a UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Index where to insert the UV channel.
	 * @return true if a UV channel was added.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool InsertUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex);

	/**
	 * Removes the UV channel at the specified channel index on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to remove the UV channel.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Index where to remove the UV channel.
	 * @return true if the UV channel was removed.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool RemoveUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex);

	/**
	 * Generates planar UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Tiling				The UV tiling to use to generate the UV mapping.
	 * @return true if the UV mapping was generated.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool GeneratePlanarUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling);

	/**
	 * Generates cylindrical UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Tiling				The UV tiling to use to generate the UV mapping.
	 * @return true if the UV mapping was generated.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool GenerateCylindricalUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector2D& Tiling);

	/**
	 * Generates box UV mapping in the specified UV channel on the given LOD of a StaticMesh.
	 * @param	StaticMesh			Static mesh on which to generate the UV mapping.
	 * @param	LODIndex			Index of the StaticMesh LOD.
	 * @param	UVChannelIndex		Channel where to save the UV mapping.
	 * @param	Position			Position of the center of the projection gizmo.
	 * @param	Orientation			Rotation to apply to the projection gizmo.
	 * @param	Size				The size of the box projection gizmo.
	 * @return true if the UV mapping was generated.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool GenerateBoxUVChannel(UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannelIndex, const FVector& Position, const FRotator& Orientation, const FVector& Size);

	// Converts the deprecated FEditorScriptingMeshReductionOptions to the new FStaticMeshReductionOptions
	static FStaticMeshReductionOptions ConvertReductionOptions(const FEditorScriptingMeshReductionOptions_Deprecated& ReductionOptions);

	// Converts the deprecated EScriptingCollisionShapeType_Deprecated to the new EScriptCollisionShapeType
	static EScriptCollisionShapeType ConvertCollisionShape(const EScriptingCollisionShapeType_Deprecated& CollisionShape);



	public:

	// The functions below are BP exposed copies of functions that use deprecated structs, updated to the new structs in StaticMeshEditorSubsytem
	// The old structs redirect to the new ones, so this makes blueprints that use the old structs still work
	// The old functions are still available as an overload, which makes old code that uses them compatible

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 SetLodsWithNotification(UStaticMesh* StaticMesh, const FStaticMeshReductionOptions& ReductionOptions, bool bApplyChanges );

	/**
	 * Same as SetLodsWithNotification but changes are applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 SetLods(UStaticMesh* StaticMesh, const FStaticMeshReductionOptions& ReductionOptions)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->SetLods(StaticMesh, ReductionOptions) : -1;
	}

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 AddSimpleCollisionsWithNotification(UStaticMesh* StaticMesh, const EScriptCollisionShapeType ShapeType, bool bApplyChanges);

	/**
	 * Same as AddSimpleCollisionsWithNotification but changes are automatically applied.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | StaticMesh", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static int32 AddSimpleCollisions(UStaticMesh* StaticMesh, const EScriptCollisionShapeType ShapeType)
	{
		UStaticMeshEditorSubsystem* StaticMeshEditorSubsystem = GEditor->GetEditorSubsystem<UStaticMeshEditorSubsystem>();

		return StaticMeshEditorSubsystem ? StaticMeshEditorSubsystem->AddSimpleCollisions(StaticMesh, ShapeType) : INDEX_NONE;
	}

};
