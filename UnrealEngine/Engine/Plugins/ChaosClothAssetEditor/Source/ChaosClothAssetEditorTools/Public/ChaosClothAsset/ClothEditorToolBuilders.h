// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/Interface.h"
#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "ClothEditorToolBuilders.generated.h"

namespace UE::Chaos::ClothAsset
{
	enum class EClothPatternVertexType : uint8;
}


UINTERFACE(MinimalAPI)
class UChaosClothAssetEditorToolBuilder : public UInterface
{
	GENERATED_BODY()
};


class IChaosClothAssetEditorToolBuilder
{
	GENERATED_BODY()

public:

	/** Returns all Construction View modes that this tool can operate in. The first element should be the preferred mode to switch to if necessary. */
	virtual void GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const = 0;

	/** Returns whether or not view can be set to wireframe when this tool is active.. */
	virtual bool CanSetConstructionViewWireframeActive() const { return true; }
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothEditorWeightMapPaintToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder, public IChaosClothAssetEditorToolBuilder
{
	GENERATED_BODY()

private:
	virtual void GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const override;
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
	virtual bool CanSetConstructionViewWireframeActive() const { return false; }
};


UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothMeshSelectionToolBuilder : public UInteractiveToolWithToolTargetsBuilder, public IChaosClothAssetEditorToolBuilder
{
	GENERATED_BODY()

private:

	virtual void GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const override;
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
	virtual bool CanSetConstructionViewWireframeActive() const { return false; }
};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsToolBuilder : public USingleSelectionMeshEditingToolBuilder, public IChaosClothAssetEditorToolBuilder
{
	GENERATED_BODY()

private:

	virtual void GetSupportedViewModes(TArray<UE::Chaos::ClothAsset::EClothPatternVertexType>& Modes) const override;
	virtual USingleSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

};


namespace UE::Chaos::ClothAsset
{
	// Provide a list of Tool default objects for us in TInteractiveToolCommands::RegisterCommands()
	void CHAOSCLOTHASSETEDITORTOOLS_API GetClothEditorToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs);
}

