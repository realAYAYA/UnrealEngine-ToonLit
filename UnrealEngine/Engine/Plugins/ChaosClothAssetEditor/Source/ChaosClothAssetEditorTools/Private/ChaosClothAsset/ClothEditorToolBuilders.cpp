// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditorToolBuilders.h"
#include "ChaosClothAsset/ClothPatternVertexType.h"
#include "TargetInterfaces/PrimitiveComponentBackedTarget.h"
#include "ChaosClothAsset/ClothEditorContextObject.h"
#include "ToolContextInterfaces.h"
#include "ToolTargetManager.h"
#include "ContextObjectStore.h"

// Tools
#include "ClothMeshSelectionTool.h"
#include "ClothTransferSkinWeightsTool.h"
#include "ClothWeightMapPaintTool.h"


// ------------------- Weight Map Paint Tool -------------------

void UClothEditorWeightMapPaintToolBuilder::GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Render);
}

UMeshSurfacePointTool* UClothEditorWeightMapPaintToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UClothEditorWeightMapPaintTool* PaintTool = NewObject<UClothEditorWeightMapPaintTool>(SceneState.ToolManager);
	PaintTool->SetWorld(SceneState.World);

	if (UClothEditorContextObject* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		PaintTool->SetClothEditorContextObject(ContextObject);
	}

	return PaintTool;
}

// ------------------- Selection Tool -------------------

void UClothMeshSelectionToolBuilder::GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim2D);
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Render);
}

const FToolTargetTypeRequirements& UClothMeshSelectionToolBuilder::GetTargetRequirements() const
{
	static FToolTargetTypeRequirements TypeRequirements(UPrimitiveComponentBackedTarget::StaticClass());
	return TypeRequirements;
}

bool UClothMeshSelectionToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return (SceneState.TargetManager->CountSelectedAndTargetable(SceneState, GetTargetRequirements()) == 1);
}

UInteractiveTool* UClothMeshSelectionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClothMeshSelectionTool* const NewTool = NewObject<UClothMeshSelectionTool>(SceneState.ToolManager);

	UToolTarget* const Target = SceneState.TargetManager->BuildFirstSelectedTargetable(SceneState, GetTargetRequirements());
	NewTool->SetTarget(Target);
	NewTool->SetWorld(SceneState.World);

	if (UClothEditorContextObject* const ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		NewTool->SetClothEditorContextObject(ContextObject);
	}

	return NewTool;
}


// ------------------- Skin Weight Transfer Tool -------------------

void UClothTransferSkinWeightsToolBuilder::GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const
{
	Modes.Add(UE::Chaos::ClothAsset::EClothPatternVertexType::Sim3D);
}

USingleSelectionMeshEditingTool* UClothTransferSkinWeightsToolBuilder::CreateNewTool(const FToolBuilderState& SceneState) const
{
	UClothTransferSkinWeightsTool* NewTool = NewObject<UClothTransferSkinWeightsTool>(SceneState.ToolManager);

	if (UClothEditorContextObject* ContextObject = SceneState.ToolManager->GetContextObjectStore()->FindContext<UClothEditorContextObject>())
	{
		NewTool->SetClothEditorContextObject(ContextObject);
	}

	return NewTool;
}



namespace UE::Chaos::ClothAsset
{
	void GetClothEditorToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
	{
		ToolCDOs.Add(GetMutableDefault<UClothEditorWeightMapPaintTool>());
		ToolCDOs.Add(GetMutableDefault<UClothTransferSkinWeightsTool>());
		ToolCDOs.Add(GetMutableDefault<UClothMeshSelectionTool>());
	}
}
