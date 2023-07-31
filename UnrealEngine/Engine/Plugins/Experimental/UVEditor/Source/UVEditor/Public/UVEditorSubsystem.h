// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorSubsystem.h"
#include "ToolTargets/ToolTarget.h" //FToolTargetTypeRequirements

#include "GeometryBase.h"

#include "UVEditorSubsystem.generated.h"

PREDECLARE_GEOMETRY(class FAssetDynamicMeshTargetComboInterface);

class UToolTargetManager;
class UUVEditor;
class UToolTarget;

/**
 * The subsystem is meant to hold any UV editor operations that are not UI related (those are
 * handled by the toolkit); however, in our case, most of our operations are wrapped up inside
 * tools and the UV mode. 
 * Instead, the subsystem deals with some global UV asset editor things- it manages existing
 * instances (we can't rely on the asset editor subsystem for this because the UV editor is
 * not the primary editor for meshes), and figures out what we can launch the editor on.
 */
UCLASS()
class UVEDITOR_API UUVEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Checks that all of the objects are valid targets for a UV editor session. */
	virtual bool AreObjectsValidTargets(const TArray<UObject*>& InObjects) const;

	/** 
	 * Tries to build the core targets that provide meshes for UV tools to work on.
	 */
	virtual void BuildTargets(const TArray<TObjectPtr<UObject>>& ObjectsIn, 
		const FToolTargetTypeRequirements& TargetRequirements,
		TArray<TObjectPtr<UToolTarget>>& TargetsOut);

	/**
	 * Either brings to the front an existing UV editor instance that is editing one of
	 * these objects, if one exists, or starts up a new instance editing all of these 
	 * objects.
	 */
	virtual void StartUVEditor(TArray<TObjectPtr<UObject>> ObjectsToEdit);

	/** 
	 * Needs to be called when a UV editor instance is closed so that the subsystem knows
	 * to create a new one for these objects if they are opened again.
	 */
	virtual void NotifyThatUVEditorClosed(TArray<TObjectPtr<UObject>> ObjectsItWasEditing);

protected:

	/**
	 * Used to let the subsystem figure out whether targets are valid. New factories should be
	 * added here in Initialize() as they are developed.
	 */
	UPROPERTY()
	TObjectPtr<UToolTargetManager> ToolTargetManager = nullptr;

	TMap<TObjectPtr<UObject>, TObjectPtr<UUVEditor>> OpenedEditorInstances;
};