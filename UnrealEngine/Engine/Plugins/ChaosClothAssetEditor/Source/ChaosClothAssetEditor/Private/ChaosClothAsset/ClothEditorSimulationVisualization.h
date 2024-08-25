// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/BitArray.h"
#include "Templates/SharedPointer.h"

class FMenuBuilder;
class UChaosClothComponent;
class FPrimitiveDrawInterface;
class FCanvas;
class FSceneView;

namespace UE::Chaos::ClothAsset
{

class FChaosClothAssetEditor3DViewportClient;

class FClothEditorSimulationVisualization
{
public:
	FClothEditorSimulationVisualization();

	void ExtendViewportShowMenu(FMenuBuilder& MenuBuilder, TSharedRef<FChaosClothAssetEditor3DViewportClient> ViewportClient);
	void DebugDrawSimulation(const UChaosClothComponent* ClothComponent, FPrimitiveDrawInterface* PDI);
	void DebugDrawSimulationTexts(const UChaosClothComponent* ClothComponent, FCanvas* Canvas, const FSceneView* SceneView);
	FText GetDisplayString(const UChaosClothComponent* ClothComponent) const;

private:
	/** Return whether or not - given the current enabled options - the simulation should be disabled. */
	bool ShouldDisableSimulation() const;
	/** Show/hide all cloth sections for the specified mesh compoment. */
	void ShowClothSections(UChaosClothComponent* ClothComponent, bool bIsClothSectionsVisible) const;

private:
	/** Flags used to store the checked status for the visualization options. */
	TBitArray<> Flags;
};
} // namespace UE::Chaos::ClothAsset
