// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SingleSelectionTool.h"
#include "InteractiveToolBuilder.h"
#include "SingleSelectionMeshEditingTool.generated.h"

class USingleSelectionMeshEditingTool;
class UDEPRECATED_PersistentMeshSelection;

/**
 * USingleSelectionMeshEditingToolBuilder is a base tool builder for single
 * selection tools that define a common set of ToolTarget interfaces required
 * for editing meshes.
 */
UCLASS(Transient, Abstract)
class MODELINGCOMPONENTS_API USingleSelectionMeshEditingToolBuilder : public UInteractiveToolWithToolTargetsBuilder
{
	GENERATED_BODY()

public:
	/** @return true if a single mesh source can be found in the active selection */
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance initialized with selected mesh source */
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	/** @return new Tool instance. Override this in subclasses to build a different Tool class type */
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const PURE_VIRTUAL(USingleSelectionMeshEditingToolBuilder::CreateNewTool, return nullptr; );

	/** Called by BuildTool to configure the Tool with the input MeshSource based on the SceneState */
	virtual void InitializeNewTool(USingleSelectionMeshEditingTool* Tool, const FToolBuilderState& SceneState) const;

	/** @return true if this Tool would like access to an available Input Selection object */
	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	virtual bool WantsInputSelectionIfAvailable() const { return false; }

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};


/**
 * Single Selection Mesh Editing tool base class.
 */
UCLASS()
class MODELINGCOMPONENTS_API USingleSelectionMeshEditingTool : public USingleSelectionTool
{
	GENERATED_BODY()
public:
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnShutdown(EToolShutdownType ShutdownType);

	virtual void SetWorld(UWorld* World);
	virtual UWorld* GetTargetWorld();

protected:
	UPROPERTY()
	TWeakObjectPtr<UWorld> TargetWorld = nullptr;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	//
	// Mesh Selection support
	//

public:
	/**
	 * Set a Selection for the Tool to use. This should be called before tool Setup() (ie in the ToolBuilder) 
	 * to allow the Tool to configure it's behavior based on the Selection (which may or may-not exist).
	 * If the Tool requires a Selection, this needs to be handled at the Builder level.
	 */
	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	virtual void SetInputSelection(const UDEPRECATED_PersistentMeshSelection* SelectionIn)
	{
		InputSelection_DEPRECATED = SelectionIn;
	}

	/** @return true if an InputSelection is available */
	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	virtual bool HasInputSelection() const
	{
		return InputSelection_DEPRECATED != nullptr;
	}

	/** @return the input Selection, or nullptr if one was not configured */
	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	virtual const UDEPRECATED_PersistentMeshSelection* GetInputSelection() const
	{
		return InputSelection_DEPRECATED;
	}

protected:

	/**
	 * (optional) Mesh Selection provided on Tool Input. This should never be modified after the Tool's Setup() is called
	 */
	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	UPROPERTY()
	TObjectPtr<const UDEPRECATED_PersistentMeshSelection> InputSelection_DEPRECATED = nullptr;

PRAGMA_ENABLE_DEPRECATION_WARNINGS
};
