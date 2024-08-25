// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "ClothMeshSelectionTool.generated.h"

class UPolygonSelectionMechanic;
class UClothEditorContextObject;
class UPreviewMesh;

namespace UE::Geometry
{
	class FGroupTopology;
	struct FGroupTopologySelection;
}

UENUM()
enum class EClothMeshSelectionToolActions
{
	NoAction,

	ImportFromCollection,
	ImportSecondaryFromCollection,
	TogglePrimarySecondary,

	GrowSelection,
	ShrinkSelection,
	FloodSelection
};


UCLASS()
class UClothMeshSelectionMechanic : public UPolygonSelectionMechanic
{
	GENERATED_BODY()

private:

	virtual bool UpdateSelection(const FRay& WorldRay, FVector3d& LocalHitPositionOut, FVector3d& LocalHitNormalOut) override;
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothMeshSelectionToolActions :  public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	TWeakObjectPtr<UClothMeshSelectionTool> ParentTool;

	void Initialize(UClothMeshSelectionTool* ParentToolIn) { ParentTool = ParentToolIn; }

	void PostAction(EClothMeshSelectionToolActions Action);

	UFUNCTION(CallInEditor, Category = Operations)
	void ImportFromCollection()
	{
		PostAction(EClothMeshSelectionToolActions::ImportFromCollection);
	}

	UFUNCTION(CallInEditor, Category = Operations)
	void ImportSecondaryFromCollection()
	{
		PostAction(EClothMeshSelectionToolActions::ImportSecondaryFromCollection);
	}

	UFUNCTION(CallInEditor, Category = Operations)
	void TogglePrimarySecondary()
	{
		PostAction(EClothMeshSelectionToolActions::TogglePrimarySecondary);
	}

	UFUNCTION(CallInEditor, Category = Selection)
	void GrowSelection()
	{
		PostAction(EClothMeshSelectionToolActions::GrowSelection);
	}

	UFUNCTION(CallInEditor, Category = Selection)
	void ShrinkSelection()
	{
		PostAction(EClothMeshSelectionToolActions::ShrinkSelection);
	}

	UFUNCTION(CallInEditor, Category = Selection)
	void FloodSelection()
	{
		PostAction(EClothMeshSelectionToolActions::FloodSelection);
	}

};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothMeshSelectionToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Transient, Category = Name, meta = (DisplayName = "Name", TransientToolProperty))
	FString Name;

	/**
	* Whether the user is editing the primary or secondary selection set. Stored as a property here so that the tool will remember what the
	* user was doing the last time the tool shut down.
	*/
	UPROPERTY()
	bool bSecondarySelection = false;

	UPROPERTY(EditAnywhere, Category = Visualization, meta = (DisplayName = "Show Vertices"))
	bool bShowVertices = false;
	
	UPROPERTY(EditAnywhere, Category = Visualization, meta = (DisplayName = "Show Edges"))
	bool bShowEdges = false;

private:

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothMeshSelectionTool : public USingleSelectionMeshEditingTool
{
	GENERATED_BODY()

private:

	friend class UClothMeshSelectionToolBuilder;

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	// IInteractiveToolCameraFocusAPI implementation
	virtual FBox GetWorldSpaceFocusBox() override;

	void SetClothEditorContextObject(TObjectPtr<UClothEditorContextObject> InClothEditorContextObject);
	bool GetSelectedNodeInfo(FString& OutMapName, UE::Geometry::FGroupTopologySelection& OutSelection);
	void UpdateSelectedNode();

	UPROPERTY()
	TObjectPtr<UClothMeshSelectionToolProperties> ToolProperties;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanic> SelectionMechanic = nullptr;

	UPROPERTY()
	TObjectPtr<UClothEditorContextObject> ClothEditorContextObject = nullptr;

	TUniquePtr<UE::Geometry::FGroupTopology> Topology;

	bool bAnyChangeMade = false;

	bool bHasNonManifoldMapping = false;
	TArray<int32> DynamicMeshToSelection;
	TArray<TArray<int32>> SelectionToDynamicMesh;

	//
	// Action support
	//

public:
	virtual void RequestAction(EClothMeshSelectionToolActions ActionType);

	UPROPERTY()
	TObjectPtr<UClothMeshSelectionToolActions> ActionsProps;

private:
	bool bHavePendingAction = false;
	EClothMeshSelectionToolActions PendingAction;
	virtual void ApplyAction(EClothMeshSelectionToolActions ActionType);

	void ImportFromCollection(bool bImportFromSecondarySet);
	void TogglePrimarySecondaryAction();
	void UpdatePrimarySecondaryMessage();

	void GrowSelection();
	void ShrinkSelection();
	void FloodSelection();
};

