// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "USDMetadata.h"

#include "AnalyticsBlueprintLibrary.h"
#include "Engine/EngineTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "USDConversionBlueprintLibrary.generated.h"

class AInstancedFoliageActor;
class UFoliageType;
class ULevel;
class ULevelExporterUSDOptions;
class UUsdAssetUserData;
struct FMatrix2D;
struct FMatrix3D;

/** Wrapped static conversion functions from the UsdUtilities module, so that they can be used via scripting */
UCLASS(meta = (ScriptName = "UsdConversionLibrary"))
class USDEXPORTER_API UUsdConversionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns how many total Unreal levels (persistent + all sublevels) will be exported if we consider LevelsToIgnore */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static int32 GetNumLevelsToExport(UWorld* World, const TSet<FString>& LevelsToIgnore);

	/** Fully streams in and displays all levels whose names are not in LevelsToIgnore */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static void StreamInRequiredLevels(UWorld* World, const TSet<FString>& LevelsToIgnore);

	/**
	 * If we have the Sequencer open with a level sequence animating the level before export, this function can revert
	 * any actor or component to its unanimated state
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static void RevertSequencerAnimations();

	/**
	 * If we used `ReverseSequencerAnimations` to undo the effect of an opened sequencer before export, this function
	 * can be used to re-apply the sequencer state back to the level after the export is complete
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static void ReapplySequencerAnimations();

	/**
	 * Returns the path name (e.g. "/Game/Maps/MyLevel") of levels that are loaded on `World`.
	 * We use these to revert the `World` to its initial state after we force-stream levels in for exporting
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static TArray<FString> GetLoadedLevelNames(UWorld* World);

	/**
	 * Returns the path name (e.g. "/Game/Maps/MyLevel") of levels that checked to be visible in the editor within `World`.
	 * We use these to revert the `World` to its initial state after we force-stream levels in for exporting
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static TArray<FString> GetVisibleInEditorLevelNames(UWorld* World);

	/** Streams out/hides sublevels that were streamed in before export */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static void StreamOutLevels(UWorld* OwningWorld, const TArray<FString>& LevelNamesToStreamOut, const TArray<FString>& LevelNamesToHide);

	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static TSet<AActor*> GetActorsToConvert(UWorld* World);

	/**
	 * Generates a unique identifier string that involves ObjectToExport's package's persistent guid, the
	 * corresponding file save date and time, and the number of times the package has been dirtied since last being
	 * saved.
	 * Optionally it can also combine that hash with a hash of the export options being used for the export, if
	 * available.
	 * This can be used to track the version of exported assets and levels, to prevent reexporting of actors and
	 * components.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static FString GenerateObjectVersionString(const UObject* ObjectToExport, UObject* ExportOptions);

	/** Checks whether we can create a USD Layer with "TargetFilePath" as identifier and export to it */
	UFUNCTION(BlueprintCallable, Category = "USD|World utils")
	static bool CanExportToLayer(const FString& TargetFilePath);

	UFUNCTION(BlueprintCallable, Category = "USD|Layer utils")
	static FString MakePathRelativeToLayer(const FString& AnchorLayerPath, const FString& PathToMakeRelative);

	UFUNCTION(BlueprintCallable, Category = "USD|Layer utils")
	static void InsertSubLayer(const FString& ParentLayerPath, const FString& SubLayerPath, int32 Index = -1);

	UFUNCTION(BlueprintCallable, Category = "USD|Layer utils")
	static void AddReference(const FString& ReferencingStagePath, const FString& ReferencingPrimPath, const FString& TargetStagePath);

	UFUNCTION(BlueprintCallable, Category = "USD|Layer utils")
	static void AddPayload(const FString& ReferencingStagePath, const FString& ReferencingPrimPath, const FString& TargetStagePath);

	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static FString GetPrimPathForObject(const UObject* ActorOrComponent, const FString& ParentPrimPath = TEXT(""), bool bUseActorFolders = false);

	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static FString GetSchemaNameForComponent(const USceneComponent* Component);

	/**
	 * Wraps AInstancedFoliageActor::GetInstancedFoliageActorForLevel, and allows retrieving the current AInstancedFoliageActor
	 * for a level. Will default to the current editor level if Level is left nullptr.
	 * This function is useful because it's difficult to retrieve this actor otherwise, as it will be filtered from
	 * the results of functions like EditorLevelLibrary.get_all_level_actors()
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|Foliage Exporter")
	static AInstancedFoliageActor* GetInstancedFoliageActorForLevel(bool bCreateIfNone = false, ULevel* Level = nullptr);

	/**
	 * Returns all the different types of UFoliageType assets that a particular AInstancedFoliageActor uses.
	 * This function exists because we want to retrieve all instances of all foliage types on an actor, but we
	 * can't return nested containers from UFUNCTIONs, so users of this API should call this, and then GetInstanceTransforms.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod), Category = "USD|Foliage Exporter")
	static TArray<UFoliageType*> GetUsedFoliageTypes(AInstancedFoliageActor* Actor);

	/**
	 * Returns the source asset for a UFoliageType.
	 * It can be a UStaticMesh in case we're dealing with a UFoliageType_InstancedStaticMesh, but it can be other types of objects.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod), Category = "USD|Foliage Exporter")
	static UObject* GetSource(UFoliageType* FoliageType);

	/**
	 * Returns the transforms of all instances of a particular UFoliageType on a given level. If no level is provided all instances will be returned.
	 * Use GetUsedFoliageTypes() to retrieve all foliage types managed by a particular actor.
	 */
	UFUNCTION(BlueprintCallable, meta = (ScriptMethod), Category = "USD|Foliage Exporter")
	static TArray<FTransform> GetInstanceTransforms(AInstancedFoliageActor* Actor, UFoliageType* FoliageType, ULevel* InstancesLevel = nullptr);

	/** Retrieves the analytics attributes to send for the provided options object */
	UFUNCTION(BlueprintCallable, Category = "USD|Analytics")
	static TArray<FAnalyticsEventAttr> GetAnalyticsAttributes(const ULevelExporterUSDOptions* Options);

	/** Defer to the USDClasses module to actually send analytics information */
	UFUNCTION(BlueprintCallable, Category = "USD|Analytics")
	static void SendAnalytics(
		const TArray<FAnalyticsEventAttr>& Attrs,
		const FString& EventName,
		bool bAutomated,
		double ElapsedSeconds,
		double NumberOfFrames,
		const FString& Extension
	);

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
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static void RemoveAllPrimSpecs(const FString& StageRootLayer, const FString& PrimPath, const FString& TargetLayer);

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage and removes all the prim specs for Prims from their stages.
	 * These cut prims can then be pasted with PastePrims.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPaths - Prims to cut
	 * @return True if we managed to cut
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static bool CutPrims(const FString& StageRootLayer, const TArray<FString>& PrimPaths);

	/**
	 * Copies flattened versions of the input prims onto the clipboard stage.
	 * These copied prims can then be pasted with PastePrims.
	 *
	 * @param StageRootLayer - Path to the root layer of the stage from which we should fetch the Prims
	 * @param PrimPaths - Prims to copy
	 * @return True if we managed to copy
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static bool CopyPrims(const FString& StageRootLayer, const TArray<FString>& PrimPaths);

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
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static TArray<FString> PastePrims(const FString& StageRootLayer, const FString& ParentPrimPath);

	/** Returns true if we have prims that we can paste within our clipboard stage */
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static bool CanPastePrims();

	/** Clears all prims from our clipboard stage */
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
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
	UFUNCTION(BlueprintCallable, Category = "USD|Prim utils")
	static TArray<FString> DuplicatePrims(
		const FString& StageRootLayer,
		const TArray<FString>& PrimPaths,
		EUsdDuplicateType DuplicateType,
		const FString& TargetLayer
	);

	/* Retrieve the first instance of UUsdAssetUserData contained on the Object, if any */
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static UUsdAssetUserData* GetUsdAssetUserData(UObject* Object);

	/*
	 * Sets AssetUserData as the single UUsdAssetUserData on the Object, overwriting an existing one if encountered.
	 * Returns true if it managed to add AssetUserData to Object.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static bool SetUsdAssetUserData(UObject* Object, UUsdAssetUserData* AssetUserData);

	/*
	 * Utilities that make it easier to get/set metadata fields without having to manipulate the nested struct instances directly.
	 * It will create the struct entries automatically if needed, and overwrite existing entries with the same key if needed.
	 *
	 * If the AssetUserData contains exactly one entry for StageIdentifier and one entry for PrimPath, you can omit those arguments
	 * and that single entry will be used. If there are more or less than exactly one entry for StageIdentifier or for PrimPath however,
	 * you must specify which one to use, and failing to do so will cause the functions to return false and emit a warning.
	 *
	 * It is possible to get these functions to automatically trigger pre/post property changed events by providing "true" for
	 * bTriggerPropertyChangeEvents, which is useful as it is not trivial to trigger those from Python/Blueprint given how the
	 * metadata is stored inside nested structs and maps. If these AssetUserData belong to generated transient assets when opening
	 * stages, emitting property change events causes those edits to be immediately written out to the opened USD Stage.
	 *
	 * Returns true if it managed to set the new key-value pair.
	 */
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static bool SetMetadataField(
		UUsdAssetUserData* AssetUserData,
		const FString& Key,
		const FString& Value,
		const FString& ValueTypeName,
		const FString& StageIdentifier = "",
		const FString& PrimPath = "",
		bool bTriggerPropertyChangeEvents = true
	);
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static bool ClearMetadataField(
		UUsdAssetUserData* AssetUserData,
		const FString& Key,
		const FString& StageIdentifier = "",
		const FString& PrimPath = "",
		bool bTriggerPropertyChangeEvents = true
	);
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static bool HasMetadataField(
		UUsdAssetUserData* AssetUserData,
		const FString& Key,
		const FString& StageIdentifier = "",
		const FString& PrimPath = ""
	);
	UFUNCTION(BlueprintCallable, Category = "USD|Metadata utils")
	static FUsdMetadataValue GetMetadataField(
		UUsdAssetUserData* AssetUserData,
		const FString& Key,
		const FString& StageIdentifier = "",
		const FString& PrimPath = ""
	);

public:	   // Stringify functions
	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsBool(bool Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uchar"))
	static FString StringifyAsUChar(uint8 Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uint"))
	static FString StringifyAsUInt(int32 Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt64(int64 Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uint64"))
	static FString StringifyAsUInt64(int64 Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf(float Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat(float Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble(double Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_timecode"))
	static FString StringifyAsTimeCode(double Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsString(const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsToken(const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsAssetPath(const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix2d(const FMatrix2D& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix3d(const FMatrix3D& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix4d(const FMatrix& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuatd(const FQuat& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuatf(const FQuat& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuath(const FQuat& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble2(const FVector2D& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat2(const FVector2D& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf2(const FVector2D& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt2(const FIntPoint& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble3(const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat3(const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf3(const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt3(const FIntVector& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble4(const FVector4& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat4(const FVector4& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf4(const FVector4& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt4(const FIntVector4& Value);

public:	   // Stringify array functions
	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsBoolArray(const TArray<bool>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uchar_array"))
	static FString StringifyAsUCharArray(const TArray<uint8>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsIntArray(const TArray<int32>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uint_array"))
	static FString StringifyAsUIntArray(const TArray<int32>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt64Array(const TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_uint64_array"))
	static FString StringifyAsUInt64Array(const TArray<int64>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalfArray(const TArray<float>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloatArray(const TArray<float>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDoubleArray(const TArray<double>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "stringify_as_timecode_array"))
	static FString StringifyAsTimeCodeArray(const TArray<double>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsStringArray(const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsTokenArray(const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsAssetPathArray(const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsListOpTokens(const TArray<FString>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix2dArray(const TArray<FMatrix2D>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix3dArray(const TArray<FMatrix3D>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsMatrix4dArray(const TArray<FMatrix>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuatdArray(const TArray<FQuat>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuatfArray(const TArray<FQuat>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsQuathArray(const TArray<FQuat>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble2Array(const TArray<FVector2D>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat2Array(const TArray<FVector2D>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf2Array(const TArray<FVector2D>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt2Array(const TArray<FIntPoint>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble3Array(const TArray<FVector>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat3Array(const TArray<FVector>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf3Array(const TArray<FVector>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt3Array(const TArray<FIntVector>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsDouble4Array(const TArray<FVector4>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsFloat4Array(const TArray<FVector4>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsHalf4Array(const TArray<FVector4>& Value);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString StringifyAsInt4Array(const TArray<FIntVector4>& Value);

public:	   // Unstringify functions
	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static bool UnstringifyAsBool(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uchar"))
	static uint8 UnstringifyAsUChar(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static int32 UnstringifyAsInt(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uint"))
	static int32 UnstringifyAsUInt(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static int64 UnstringifyAsInt64(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uint64"))
	static int64 UnstringifyAsUInt64(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static float UnstringifyAsHalf(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static float UnstringifyAsFloat(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static double UnstringifyAsDouble(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_timecode"))
	static double UnstringifyAsTimeCode(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString UnstringifyAsString(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString UnstringifyAsToken(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FString UnstringifyAsAssetPath(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FMatrix2D UnstringifyAsMatrix2d(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FMatrix3D UnstringifyAsMatrix3d(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FMatrix UnstringifyAsMatrix4d(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FQuat UnstringifyAsQuatd(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FQuat UnstringifyAsQuatf(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FQuat UnstringifyAsQuath(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector2D UnstringifyAsDouble2(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector2D UnstringifyAsFloat2(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector2D UnstringifyAsHalf2(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FIntPoint UnstringifyAsInt2(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector UnstringifyAsDouble3(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector UnstringifyAsFloat3(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector UnstringifyAsHalf3(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FIntVector UnstringifyAsInt3(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector4 UnstringifyAsDouble4(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector4 UnstringifyAsFloat4(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FVector4 UnstringifyAsHalf4(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static FIntVector4 UnstringifyAsInt4(const FString& String);

public:	   // Unstringify array functions
	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<bool> UnstringifyAsBoolArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uchar_array"))
	static TArray<uint8> UnstringifyAsUCharArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<int32> UnstringifyAsIntArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uint_array"))
	static TArray<int32> UnstringifyAsUIntArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<int64> UnstringifyAsInt64Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_uint64_array"))
	static TArray<int64> UnstringifyAsUInt64Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<float> UnstringifyAsHalfArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<float> UnstringifyAsFloatArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<double> UnstringifyAsDoubleArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils", Meta = (ScriptName = "unstringify_as_timecode_array"))
	static TArray<double> UnstringifyAsTimeCodeArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FString> UnstringifyAsStringArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FString> UnstringifyAsTokenArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FString> UnstringifyAsAssetPathArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FString> UnstringifyAsListOpTokens(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FMatrix2D> UnstringifyAsMatrix2dArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FMatrix3D> UnstringifyAsMatrix3dArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FMatrix> UnstringifyAsMatrix4dArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FQuat> UnstringifyAsQuatdArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FQuat> UnstringifyAsQuatfArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FQuat> UnstringifyAsQuathArray(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector2D> UnstringifyAsDouble2Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector2D> UnstringifyAsFloat2Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector2D> UnstringifyAsHalf2Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FIntPoint> UnstringifyAsInt2Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector> UnstringifyAsDouble3Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector> UnstringifyAsFloat3Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector> UnstringifyAsHalf3Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FIntVector> UnstringifyAsInt3Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector4> UnstringifyAsDouble4Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector4> UnstringifyAsFloat4Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FVector4> UnstringifyAsHalf4Array(const FString& String);

	UFUNCTION(BlueprintCallable, Category = "USD|Stringify utils")
	static TArray<FIntVector4> UnstringifyAsInt4Array(const FString& String);
};
