// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "Physics/CollisionPropertySets.h"
#include "SimpleCollisionEditorTool.generated.h"

class UPreviewGeometry;
class UCollisionPrimitivesMechanic;
class USimpleCollisionEditorTool;

UCLASS()
class MESHMODELINGTOOLSEXP_API USimpleCollisionEditorToolBuilder : public USingleSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};

UENUM()
enum class ESimpleCollisionEditorToolAction : uint8
{
	NoAction,
	AddSphere,
	AddBox,
	AddCapsule,
	Duplicate,
	DeleteSelected,
	DeleteAll
};

UCLASS()
class MESHMODELINGTOOLSEXP_API USimpleCollisionEditorToolActionProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<USimpleCollisionEditorTool> ParentTool;
	void Initialize(USimpleCollisionEditorTool* ParentToolIn) { ParentTool = ParentToolIn; }
	void PostAction(ESimpleCollisionEditorToolAction Action);

	/** Duplicate all selected simple collision shapes */
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Duplicate"))
	void Duplicate()
	{
		PostAction(ESimpleCollisionEditorToolAction::Duplicate);
	}

	/** Remove currently selected simple collision shapes from the mesh */
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Delete Selected"))
	void Delete()
	{
		PostAction(ESimpleCollisionEditorToolAction::DeleteSelected);
	}

	/** Remove all current simple collision shapes from the mesh */
	UFUNCTION(CallInEditor, Category = Actions, meta = (DisplayName = "Delete All"))
	void DeleteAll()
	{
		PostAction(ESimpleCollisionEditorToolAction::DeleteAll);
	}

	/** Add a new simple sphere collision shape */
	UFUNCTION(CallInEditor, Category = "Add Shapes", meta = (DisplayName = "Add Sphere"))
		void AddSphere()
	{
		PostAction(ESimpleCollisionEditorToolAction::AddSphere);
	}

	/** Add a new simple box collision shape */
	UFUNCTION(CallInEditor, Category = "Add Shapes", meta = (DisplayName = "Add Box"))
		void AddBox()
	{
		PostAction(ESimpleCollisionEditorToolAction::AddBox);
	}

	/** Add a new simple capsule collision shape */
	UFUNCTION(CallInEditor, Category = "Add Shapes", meta = (DisplayName = "Add Capsule"))
		void AddCapsule()
	{
		PostAction(ESimpleCollisionEditorToolAction::AddCapsule);
	}

};

/**
 * Simple Collision Editing tool for updating the simple collision geometry on meshes
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USimpleCollisionEditorTool : public USingleSelectionMeshEditingTool, public IInteractiveToolManageGeometrySelectionAPI
{
	GENERATED_BODY()
public:
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override { return true; }

	// IInteractiveToolManageGeometrySelectionAPI -- this tool won't update external geometry selection or change selection-relevant mesh IDs
	virtual bool IsInputSelectionValidOnOutput() override
	{
		return true;
	}

protected:

	UPROPERTY()
	TObjectPtr<USimpleCollisionEditorToolActionProperties> ActionProperties;
	
	ESimpleCollisionEditorToolAction PendingAction = ESimpleCollisionEditorToolAction::NoAction;
	void RequestAction(ESimpleCollisionEditorToolAction Action);
	void ApplyAction(ESimpleCollisionEditorToolAction Action);

	friend USimpleCollisionEditorToolActionProperties;

protected:
	TSharedPtr<FPhysicsDataCollection> PhysicsInfos;
	TObjectPtr<UCollisionPrimitivesMechanic> CollisionPrimitivesMechanic;
	void InitializeObjectProperties(const FPhysicsDataCollection& PhysicsData, UPhysicsObjectToolPropertySet* PropSet);
};
