// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/MeshMerging.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StaticMeshEditorSubsystem.h"

#include "EditorLevelLibrary.generated.h"

USTRUCT(BlueprintType)
struct FEditorScriptingJoinStaticMeshActorsOptions_Deprecated
{
	GENERATED_BODY()

	FEditorScriptingJoinStaticMeshActorsOptions_Deprecated()
		: bDestroySourceActors(true)
		, bRenameComponentsFromSource(true)
	{ }

	// Destroy the provided Actors after the operation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bDestroySourceActors;

	// Name of the new spawned Actor to replace the provided Actors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FString NewActorLabel;

	// Rename StaticMeshComponents based on source Actor's name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRenameComponentsFromSource;
};

USTRUCT(BlueprintType)
struct FEditorScriptingMergeStaticMeshActorsOptions_Deprecated : public FEditorScriptingJoinStaticMeshActorsOptions_Deprecated
{
	GENERATED_BODY()

	FEditorScriptingMergeStaticMeshActorsOptions_Deprecated()
		: bSpawnMergedActor(true)
	{ }

	// Spawn the new merged actors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSpawnMergedActor;

	// The package path you want to save to. ie: /Game/MyFolder
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FString BasePackageName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FMeshMergingSettings MeshMergingSettings;
};

USTRUCT(BlueprintType)
struct FEditorScriptingCreateProxyMeshActorOptions_Deprecated : public FEditorScriptingJoinStaticMeshActorsOptions_Deprecated
{
	GENERATED_BODY()

	FEditorScriptingCreateProxyMeshActorOptions_Deprecated()
		: bSpawnMergedActor(true)
	{ }

	// Spawn the new merged actors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSpawnMergedActor;

	// The package path you want to save to. ie: /Game/MyFolder
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FString BasePackageName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FMeshProxySettings MeshProxySettings;
};

/**
 * Utility class to do most of the common functionalities in the World Editor.
 * The editor should not be in play in editor mode.
 */
UCLASS()
class EDITORSCRIPTINGUTILITIES_API UEditorLevelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Find all loaded Actors in the world editor. Exclude actor that are pending kill, in PIE, PreviewEditor, ...
	 * @return	List of found Actors
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static TArray<class AActor*> GetAllLevelActors();

	/**
	 * Find all loaded ActorComponent own by an actor in the world editor. Exclude actor that are pending kill, in PIE, PreviewEditor, ...
	 * @return	List of found ActorComponent
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static TArray<class UActorComponent*> GetAllLevelActorsComponents();

	/**
	 * Find all loaded Actors that are selected in the world editor. Exclude actor that are pending kill, in PIE, PreviewEditor, ...
	 * @param	ActorClass	Actor Class to find.
	 * @return	List of found Actors
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static TArray<class AActor*> GetSelectedLevelActors();

	/**
	 * Clear the current world editor selection and select the provided actors. Exclude actor that are pending kill, in PIE, PreviewEditor, ...
	 * @param	ActorsToSelect	Actor that should be selected in the world editor.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static void SetSelectedLevelActors(const TArray<class AActor*>& ActorsToSelect);

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta=(DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static void PilotLevelActor(AActor* ActorToPilot);

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta=(DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static void EjectPilotLevelActor();

#if WITH_EDITOR

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static void EditorPlaySimulate();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	static void EditorEndPlay();

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static void EditorInvalidateViewports();

	/**
	 * Replaces the selected Actors with the same number of a different kind of Actor using the specified factory to spawn the new Actors
	 * note that only Location, Rotation, Drawscale, Drawscale3D, Tag, and Group are copied from the old Actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	static void ReplaceSelectedActors(const FString& InAssetPath);

#endif

	/**
	 * Gets information about the camera position for the primary level editor viewport.  In non-editor builds, these will be zeroed
	 *
	 * @param	CameraLocation	(out) Current location of the level editing viewport camera, or zero if none found
	 * @param	CameraRotation	(out) Current rotation of the level editing viewport camera, or zero if none found
	 * @return	Whether or not we were able to get a camera for a level editing viewport
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem")
	UFUNCTION(BlueprintPure, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem"))
	static bool GetLevelViewportCameraInfo(FVector& CameraLocation, FRotator& CameraRotation);

	/**
	* Sets information about the camera position for the primary level editor viewport.
	*
	* @param	CameraLocation	Location the camera will be moved to.
	* @param	CameraRotation	Rotation the camera will be set to.
	*/
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem"))
	static void SetLevelViewportCameraInfo(FVector CameraLocation, FRotator CameraRotation);

	// Remove all actors from the selection set
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static void ClearActorSelectionSet();

	// Selects nothing in the editor (another way to clear the selection)
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static void SelectNothing();

	// Set the selection state for the selected actor
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static void SetActorSelectionState(AActor* Actor, bool bShouldBeSelected);

	/**
	* Attempts to find the actor specified by PathToActor in the current editor world
	* @param	PathToActor	The path to the actor (e.g. PersistentLevel.PlayerStart)
	* @return	A reference to the actor, or none if it wasn't found
	*/
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintPure, Category = "Development|Editor", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static AActor* GetActorReference(FString PathToActor);

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly, DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static void EditorSetGameView(bool bGameView);

	/**
	 * Create an actor and place it in the world editor. The Actor can be created from a Factory, Archetype, Blueprint, Class or an Asset.
	 * The actor will be created in the current level and will be selected.
	 * @param	ObjectToUse		Asset to attempt to use for an actor to place.
	 * @param	Location		Location of the new actor.
	 * @return	The created actor.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (KeyWords = "Transient"))
	static AActor* SpawnActorFromObject(class UObject* ObjectToUse, FVector Location, FRotator Rotation = FRotator::ZeroRotator, bool bTransient = false);

	/**
	 * Create an actor and place it in the world editor. Can be created from a Blueprint or a Class.
	 * The actor will be created in the current level and will be selected.
	 * @param	ActorClass		Asset to attempt to use for an actor to place.
	 * @param	Location		Location of the new actor.
	 * @return	The created actor.
	 */
    UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeterminesOutputType = "ActorClass", KeyWords = "Transient"))
	static AActor* SpawnActorFromClass(TSubclassOf<class AActor> ActorClass, FVector Location, FRotator Rotation = FRotator::ZeroRotator, bool bTransient = false);

	/**
	 * Destroy the actor from the world editor. Notify the Editor that the actor got destroyed.
	 * @param	ToDestroyActor	Actor to destroy.
	 * @return	True if the operation succeePlugin is deprecated - Use ds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static bool DestroyActor(class AActor* ActorToDestroy);

	/**
	 * Find the World in the world editor. It can then be used as WorldContext by other libraries like GameplayStatics.
	 * @return	The World used by the world editor.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem"))
	static UWorld* GetEditorWorld();

	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Unreal Editor Subsystem"))
	static UWorld* GetGameWorld();
	
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	static TArray<UWorld*> GetPIEWorlds(bool bIncludeDedicatedServer);

public:
	/**
	 * Close the current Persistent Level (without saving it). Create a new blank Level and save it. Load the new created level.
	 * @param	AssetPath		Asset Path of where the level will be saved.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool NewLevel(const FString& AssetPath);

	/**
	 * Close the current Persistent Level (without saving it). Create a new Level base on another level and save it. Load the new created level.
	 * @param	AssetPath				Asset Path of where the level will be saved.
	 *		ie. /Game/MyFolder/MyAsset
	 * @param	TemplateAssetPath		Level to be used as Template.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool NewLevelFromTemplate(const FString& AssetPath, const FString& TemplateAssetPath);

	/**
	 * Close the current Persistent Level (without saving it). Loads the specified level.
	 * @param	AssetPath				Asset Path of the level to be loaded.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool LoadLevel(const FString& AssetPath);

	/**
	 * Saves the specified Level. Must already be saved at lease once to have a valid path.
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool SaveCurrentLevel();

	/**
	 * Saves all Level currently loaded by the World Editor.
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool SaveAllDirtyLevels();

	/**
	 * Set the current level used by the world editor.
	 * If more than one level shares the same name, the first one encounter of that level name will be used.
	 * @param	LevelName	The name of the Level the actor belongs to (same name as in the ContentBrowser).
	 * @return	True if the operation succeeds.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Level Editor Subsystem"))
	static bool SetCurrentLevelByName(FName LevelName);

public:
	/**
	 * Find the references of the material MaterialToReplaced on all the MeshComponents provided and replace it by NewMaterial.
	 * @param	MeshComponents			List of MeshComponent to search from.
	 * @param	MaterialToBeReplaced	Material we want to replace.
	 * @param	NewMaterial				Material to replace MaterialToBeReplaced by.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void ReplaceMeshComponentsMaterials(const TArray<class UMeshComponent*>& MeshComponents, class UMaterialInterface* MaterialToBeReplaced, class UMaterialInterface* NewMaterial);

	/**
	 * Find the references of the material MaterialToReplaced on all the MeshComponents of all the Actors provided and replace it by NewMaterial.
	 * @param	Actors					List of Actors to search from.
	 * @param	MaterialToBeReplaced	Material we want to replace.
	 * @param	NewMaterial				Material to replace MaterialToBeReplaced by.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void ReplaceMeshComponentsMaterialsOnActors(const TArray<class AActor*>& Actors, class UMaterialInterface* MaterialToBeReplaced, class UMaterialInterface* NewMaterial);

	/**
	 * Find the references of the mesh MeshToBeReplaced on all the MeshComponents provided and replace it by NewMesh.
	 * The editor should not be in play in editor mode.
	 * @param	MeshComponents			List of MeshComponent to search from.
	 * @param	MeshToBeReplaced		Mesh we want to replace.
	 * @param	NewMesh					Mesh to replace MeshToBeReplaced by.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void ReplaceMeshComponentsMeshes(const TArray<class UStaticMeshComponent*>& MeshComponents, class UStaticMesh* MeshToBeReplaced, class UStaticMesh* NewMesh);

	/**
	 * Find the references of the mesh MeshToBeReplaced on all the MeshComponents of all the Actors provided and replace it by NewMesh.
	 * @param	Actors					List of Actors to search from.
	 * @param	MeshToBeReplaced		Mesh we want to replace.
	 * @param	NewMesh					Mesh to replace MeshToBeReplaced by.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static void ReplaceMeshComponentsMeshesOnActors(const TArray<class AActor*>& Actors, class UStaticMesh* MeshToBeReplaced, class UStaticMesh* NewMesh);

	/**
	 * Replace in the level all Actors provided with a new actor of type ActorClass. Destroy all Actors provided.
	 * @param	Actors					List of Actors to replace.
	 * @param	ActorClass				Class/Blueprint of the new actor that will be spawn.
	 * @param	StaticMeshPackagePath	If the list contains Brushes and it is requested to change them to StaticMesh, StaticMeshPackagePath is the package path to where the StaticMesh will be created. ie. /Game/MyFolder/
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeterminesOutputType = "ActorClass", DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Editor Actor Utilities Subsystem"))
	static TArray<class AActor*> ConvertActors(const TArray<class AActor*>& Actors, TSubclassOf<class AActor> ActorClass, const FString& StaticMeshPackagePath);

public:
	/**
	 * Create a new Actor in the level that contains a duplicate of all the Actors Static Meshes Component.
	 * The ActorsToJoin need to be in the same Level.
	 * This will have a low impact on performance but may help the edition by grouping the meshes under a single Actor.
	 * @param	ActorsToJoin			List of Actors to join.
	 * @param	JoinOptions				Options on how to join the actors.
	 * @return The new created actor.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static class AActor* JoinStaticMeshActors(const TArray<class AStaticMeshActor*>& ActorsToJoin, const FEditorScriptingJoinStaticMeshActorsOptions_Deprecated& JoinOptions);

	/**
	 * Merge the meshes into a unique mesh with the provided StaticMeshActors. There are multiple options on how to merge the meshes and their materials.
	 * The ActorsToMerge need to be in the same Level.
	 * This may have a high impact on performance depending of the MeshMergingSettings options.
	 * @param	ActorsToMerge			List of Actors to merge.
	 * @param	MergeOptions			Options on how to merge the actors.
	 * @param	OutMergedActor			The new created actor, if requested.
	 * @return	if the operation is successful.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static bool MergeStaticMeshActors(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingMergeStaticMeshActorsOptions_Deprecated& MergeOptions, class AStaticMeshActor*& OutMergedActor);

	/**
	 * Build a proxy mesh actor that can replace a set of mesh actors.
	 * @param   ActorsToMerge  List of actors to build a proxy for.
	 * @param   MergeOptions
	 * @param   OutMergedActor generated actor if requested
	 * @return  Success of the proxy creation
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	static bool CreateProxyMeshActor(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingCreateProxyMeshActorOptions_Deprecated& MergeOptions, class AStaticMeshActor*& OutMergedActor);



	public:

	// The functions below are BP exposed copies of functions that use deprecated structs, updated to the new structs in StaticMeshEditorSubsytem
	// The old structs redirect to the new ones, so this makes blueprints that use the old structs still work
	// The old functions are still available as an overload, which makes old code that uses them compatible

	/**
	 * Create a new Actor in the level that contains a duplicate of all the Actors Static Meshes Component.
	 * The ActorsToJoin need to be in the same Level.
	 * This will have a low impact on performance but may help the edition by grouping the meshes under a single Actor.
	 * @param	ActorsToJoin			List of Actors to join.
	 * @param	JoinOptions				Options on how to join the actors.
	 * @return The new created actor.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static class AActor* JoinStaticMeshActors(const TArray<class AStaticMeshActor*>& ActorsToJoin, const FJoinStaticMeshActorsOptions& JoinOptions);

	/**
	 * Merge the meshes into a unique mesh with the provided StaticMeshActors. There are multiple options on how to merge the meshes and their materials.
	 * The ActorsToMerge need to be in the same Level.
	 * This may have a high impact on performance depending of the MeshMergingSettings options.
	 * @param	ActorsToMerge			List of Actors to merge.
	 * @param	MergeOptions			Options on how to merge the actors.
	 * @param	OutMergedActor			The new created actor, if requested.
	 * @return	if the operation is successful.
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool MergeStaticMeshActors(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FMergeStaticMeshActorsOptions& MergeOptions, class AStaticMeshActor*& OutMergedActor);

	/**
	 * Build a proxy mesh actor that can replace a set of mesh actors.
	 * @param   ActorsToMerge  List of actors to build a proxy for.
	 * @param   MergeOptions
	 * @param   OutMergedActor generated actor if requested
	 * @return  Success of the proxy creation
	 */
	UE_DEPRECATED(5.0, "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem")
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Dataprep", meta = (DeprecatedFunction, DeprecationMessage = "The Editor Scripting Utilities Plugin is deprecated - Use the function in Static Mesh Editor Subsystem"))
	static bool CreateProxyMeshActor(const TArray<class AStaticMeshActor*>& ActorsToMerge, const FCreateProxyMeshActorOptions& MergeOptions, class AStaticMeshActor*& OutMergedActor);
};

