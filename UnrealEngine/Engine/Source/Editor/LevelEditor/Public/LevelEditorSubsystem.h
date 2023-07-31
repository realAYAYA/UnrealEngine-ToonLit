// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "IActorEditorContextClient.h"
#include "Engine/EngineTypes.h"

#include "LevelEditorSubsystem.generated.h"

class UTypedElementSelectionSet;
struct FToolMenuContext;
class FEditorModeTools;

/**
* ULevelEditorSubsystem
* Subsystem for exposing Level Editor related functionality to scripts
*/
UCLASS()
class LEVELEDITOR_API ULevelEditorSubsystem : public UEditorSubsystem, public IActorEditorContextClient
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;
	void ExtendQuickActionMenu();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta=(DevelopmentOnly))
	void PilotLevelActor(AActor* ActorToPilot, FName ViewportConfigKey = NAME_None);
	void PilotLevelActor(const FToolMenuContext& InContext);
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void EjectPilotLevelActor(FName ViewportConfigKey = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta=(DevelopmentOnly))
	AActor* GetPilotLevelActor(FName ViewportConfigKey = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void EditorPlaySimulate();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void EditorInvalidateViewports();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void EditorSetGameView(bool bGameView, FName ViewportConfigKey = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	bool EditorGetGameView(FName ViewportConfigKey = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void EditorRequestEndPlay();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	bool IsInPlayInEditor() const;

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	TArray<FName> GetViewportConfigKeys();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	FName GetActiveViewportConfigKey();

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	void SetAllowsCinematicControl(bool bAllow, FName ViewportConfigKey = NAME_None);

	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility", meta = (DevelopmentOnly))
	bool GetAllowsCinematicControl(FName ViewportConfigKey = NAME_None);

	/**
	 * Close the current Persistent Level (without saving it). Create a new blank Level and save it. Load the new created level.
	 * @param	AssetPath		Asset Path of where the level will be saved.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool NewLevel(const FString& AssetPath);

	/**
	 * Close the current Persistent Level (without saving it). Create a new Level base on another level and save it. Load the new created level.
	 * @param	AssetPath				Asset Path of where the level will be saved.
	 *		ie. /Game/MyFolder/MyAsset
	 * @param	TemplateAssetPath		Level to be used as Template.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool NewLevelFromTemplate(const FString& AssetPath, const FString& TemplateAssetPath);

	/**
	 * Close the current Persistent Level (without saving it). Loads the specified level.
	 * @param	AssetPath				Asset Path of the level to be loaded.
	 *		ie. /Game/MyFolder/MyAsset
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool LoadLevel(const FString& AssetPath);

	/**
	 * Saves the specified Level. Must already be saved at lease once to have a valid path.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool SaveCurrentLevel();

	/**
	 * Saves all Level currently loaded by the World Editor.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool SaveAllDirtyLevels();

	/**
	 * Set the current level used by the world editor.
	 * If more than one level shares the same name, the first one encounter of that level name will be used.
	 * @param	LevelName	The name of the Level the actor belongs to (same name as in the ContentBrowser).
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool SetCurrentLevelByName(FName LevelName);

	/**
	* Get the current level used by the world editor.
	* @return	The current level
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	ULevel* GetCurrentLevel();

	/**
	 * Get the level Editor typed element selection set for the current world
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	UTypedElementSelectionSet* GetSelectionSet();

	/**
	 * Build Light Maps and optionally the reflection captures.
	 * @param	Quality	One of the enum LightingBuildQuality value. Default is Quality_Production.
	 * @param	bWithReflectionCaptures	Build the related reflection captures after building the light maps.
	 * @return	True if build was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Level Utility")
	bool BuildLightMaps(ELightingBuildQuality Quality = ELightingBuildQuality::Quality_Production, bool bWithReflectionCaptures = false);

	/**
	 * Gets the global level editor mode manager, if we have one.
	 * The mode manager is not created in commandlet environments, because modes inherently imply user interactions.
	 */
	FEditorModeTools* GetLevelEditorModeManager();

	//~ Begin IActorEditorContextClient interface
	virtual void OnExecuteActorEditorContextAction(UWorld* InWorld, const EActorEditorContextAction& InType, class AActor* InActor = nullptr) {}
	virtual bool GetActorEditorContextDisplayInfo(UWorld* InWorld, FActorEditorContextClientDisplayInfo& OutDiplayInfo) const override;
	virtual bool CanResetContext(UWorld* InWorld) const override { return false; };
	virtual TSharedRef<SWidget> GetActorEditorContextWidget(UWorld* InWorld) const override;
	virtual FOnActorEditorContextClientChanged& GetOnActorEditorContextClientChanged() override { return ActorEditorContextClientChanged; }
	//~ End IActorEditorContextClient interface

private:

	/** Called when a Level is added to a world or remove from a world */
	void OnLevelAddedOrRemoved(ULevel* InLevel, UWorld* InWorld);

	/** Called when the current level changes on a world */
	void OnCurrentLevelChanged(ULevel* InNewLevel, ULevel* InOldLevel, UWorld* InWorld);

	/** Delegate used to notify changes to ActorEditorContextSubsystem */
	FOnActorEditorContextClientChanged ActorEditorContextClientChanged;
};