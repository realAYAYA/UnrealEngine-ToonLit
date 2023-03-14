// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnalyticsBlueprintLibrary.h"
#include "Engine/EngineTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "USDConversionBlueprintLibrary.generated.h"

class AInstancedFoliageActor;
class UFoliageType;
class ULevel;
class ULevelExporterUSDOptions;

/** Wrapped static conversion functions from the UsdUtilities module, so that they can be used via scripting */
UCLASS(meta=(ScriptName="UsdConversionLibrary"))
class USDEXPORTER_API UUsdConversionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Fully streams in and displays all levels whose names are not in LevelsToIgnore */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static void StreamInRequiredLevels( UWorld* World, const TSet<FString>& LevelsToIgnore );

	/**
	 * If we have the Sequencer open with a level sequence animating the level before export, this function can revert
	 * any actor or component to its unanimated state
	 */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static void RevertSequencerAnimations();

	/**
	 * If we used `ReverseSequencerAnimations` to undo the effect of an opened sequencer before export, this function
	 * can be used to re-apply the sequencer state back to the level after the export is complete
	 */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static void ReapplySequencerAnimations();

	/**
	 * Returns the path name (e.g. "/Game/Maps/MyLevel") of levels that are loaded on `World`.
	 * We use these to revert the `World` to its initial state after we force-stream levels in for exporting
	 */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static TArray<FString> GetLoadedLevelNames( UWorld* World );

	/**
	 * Returns the path name (e.g. "/Game/Maps/MyLevel") of levels that checked to be visible in the editor within `World`.
	 * We use these to revert the `World` to its initial state after we force-stream levels in for exporting
	 */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static TArray<FString> GetVisibleInEditorLevelNames( UWorld* World );

	/** Streams out/hides sublevels that were streamed in before export */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static void StreamOutLevels( UWorld* OwningWorld, const TArray<FString>& LevelNamesToStreamOut, const TArray<FString>& LevelNamesToHide );

	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static TSet<AActor*> GetActorsToConvert( UWorld* World );

	/**
	 * Generates a unique identifier string that involves ObjectToExport's package's persistent guid, the
	 * corresponding file save date and time, and the number of times the package has been dirtied since last being
	 * saved.
	 * Optionally it can also combine that hash with a hash of the export options being used for the export, if
	 * available.
	 * This can be used to track the version of exported assets and levels, to prevent reexporting of actors and
	 * components.
	 */
	UFUNCTION( BlueprintCallable, Category = "World utils" )
	static FString GenerateObjectVersionString( const UObject* ObjectToExport, UObject* ExportOptions );

	UFUNCTION( BlueprintCallable, Category = "Layer utils" )
	static FString MakePathRelativeToLayer( const FString& AnchorLayerPath, const FString& PathToMakeRelative );

	UFUNCTION( BlueprintCallable, Category = "Layer utils" )
	static void InsertSubLayer( const FString& ParentLayerPath, const FString& SubLayerPath, int32 Index = -1 );

	UFUNCTION( BlueprintCallable, Category = "Layer utils" )
	static void AddPayload( const FString& ReferencingStagePath, const FString& ReferencingPrimPath, const FString& TargetStagePath );

	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static FString GetPrimPathForObject( const UObject* ActorOrComponent, const FString& ParentPrimPath = TEXT(""), bool bUseActorFolders = false );

	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static FString GetSchemaNameForComponent( const USceneComponent* Component );

	/**
	 * Wraps AInstancedFoliageActor::GetInstancedFoliageActorForLevel, and allows retrieving the current AInstancedFoliageActor
	 * for a level. Will default to the current editor level if Level is left nullptr.
	 * This function is useful because it's difficult to retrieve this actor otherwise, as it will be filtered from
	 * the results of functions like EditorLevelLibrary.get_all_level_actors()
	 */
	UFUNCTION( BlueprintCallable, Category = "USD Foliage Exporter" )
	static AInstancedFoliageActor* GetInstancedFoliageActorForLevel( bool bCreateIfNone = false, ULevel* Level = nullptr );

	/**
	 * Returns all the different types of UFoliageType assets that a particular AInstancedFoliageActor uses.
	 * This function exists because we want to retrieve all instances of all foliage types on an actor, but we
	 * can't return nested containers from UFUNCTIONs, so users of this API should call this, and then GetInstanceTransforms.
	 */
	UFUNCTION( BlueprintCallable, meta = ( ScriptMethod ), Category = "USD Foliage Exporter" )
	static TArray<UFoliageType*> GetUsedFoliageTypes( AInstancedFoliageActor* Actor );

	/**
	 * Returns the source asset for a UFoliageType.
	 * It can be a UStaticMesh in case we're dealing with a UFoliageType_InstancedStaticMesh, but it can be other types of objects.
	 */
	UFUNCTION( BlueprintCallable, meta = ( ScriptMethod ), Category = "USD Foliage Exporter" )
	static UObject* GetSource( UFoliageType* FoliageType );

	/**
	 * Returns the transforms of all instances of a particular UFoliageType on a given level. If no level is provided all instances will be returned.
	 * Use GetUsedFoliageTypes() to retrieve all foliage types managed by a particular actor.
	 */
	UFUNCTION( BlueprintCallable, meta = ( ScriptMethod ), Category = "USD Foliage Exporter" )
	static TArray<FTransform> GetInstanceTransforms( AInstancedFoliageActor* Actor, UFoliageType* FoliageType, ULevel* InstancesLevel = nullptr );

	/** Retrieves the analytics attributes to send for the provided options object */
	UFUNCTION( BlueprintCallable, Category = "Analytics" )
	static TArray<FAnalyticsEventAttr> GetAnalyticsAttributes( const ULevelExporterUSDOptions* Options );

	/** Defer to the USDClasses module to actually send analytics information */
	UFUNCTION( BlueprintCallable, Category = "Analytics" )
	static void SendAnalytics( const TArray<FAnalyticsEventAttr>& Attrs, const FString& EventName, bool bAutomated, double ElapsedSeconds, double NumberOfFrames, const FString& Extension );

    /**
	 * Removes all the prim specs for Prim on the given Layer.
	 *
	 * This function is useful in case the prim is inside a variant set: In that case, just calling FUsdStage::RemovePrim()
	 * will attempt to remove the "/Root/Example/Child", which wouldn't remove the "/Root{Varset=Var}Example/Child" spec,
	 * meaning the prim may still be left on the stage. Note that it's even possible to have both of those specs at the same time:
	 * for example when we have a prim inside a variant set, but outside of it we have overrides to the same prim. This function
	 * will remove both.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPath - Prim to remove
	 * @param Layer - Layer to remove prim specs from. This can be left with the invalid layer (default) in order to remove all
	 *				  specs from the entire stage's local layer stack.
	 */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static void RemoveAllPrimSpecs( const FString& StageRootLayer, const FString& PrimPath, const FString& TargetLayer );

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage and removes all the prim specs for Prims from their stages.
	 * These cut prims can then be pasted with PastePrims.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPaths - Prims to cut
	 * @return True if we managed to cut
	 */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static bool CutPrims( const FString& StageRootLayer, const TArray<FString>& PrimPaths );

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage.
	 * These copied prims can then be pasted with PastePrims.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPaths - Prims to copy
	 * @return True if we managed to copy
	 */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static bool CopyPrims( const FString& StageRootLayer, const TArray<FString>& PrimPaths );

	/**
	 * Pastes the prims from the clipboard stage as children of ParentPrim.
	 *
	 * The pasted prims may be renamed in order to have valid names for the target location, which is why this function
	 * returns the pasted prim paths.
	 * This function returns just paths instead of actual prims because USD needs to respond to the notices about
	 * the created prim specs before the prims are fully created, which means we wouldn't be able to return the
	 * created prims yet, in case this function was called from within an SdfChangeBlock.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param ParentPrimPath - Prim that will become parent to the pasted prims
	 * @return Paths to the pasted prim specs, after they were added as children of ParentPrim
	 */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static TArray<FString> PastePrims( const FString& StageRootLayer, const FString& ParentPrimPath );

	/** Returns true if we have prims that we can paste within our clipboard stage */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static bool CanPastePrims();

	/** Clears all prims from our clipboard stage */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static void ClearPrimClipboard();

	/**
	 * Duplicates all provided Prims one-by-one, performing the requested DuplicateType.
	 * See the documentation on EUsdDuplicateType for the different operation types.
	 *
	 * The duplicated prims may be renamed in order to have valid names for the target location, which is why this
	 * function returns the pasted prim paths.
	 * This function returns just paths instead of actual prims because USD needs to respond to the notices about
	 * the created prim specs before the prims are fully created, which means we wouldn't be able to return the
	 * created prims yet, in case this function was called from within an SdfChangeBlock.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPaths - Prims to duplicate
	 * @param DuplicateType - Type of prim duplication to perform
	 * @param TargetLayer - Target layer to use when duplicating, if relevant for that duplication type
	 * @return Paths to the duplicated prim specs, after they were added as children of ParentPrim.
	 */
	UFUNCTION( BlueprintCallable, Category = "Prim utils" )
	static TArray<FString> DuplicatePrims(
		const FString& StageRootLayer,
		const TArray<FString>& PrimPaths,
		EUsdDuplicateType DuplicateType,
		const FString& TargetLayer
	);
};
