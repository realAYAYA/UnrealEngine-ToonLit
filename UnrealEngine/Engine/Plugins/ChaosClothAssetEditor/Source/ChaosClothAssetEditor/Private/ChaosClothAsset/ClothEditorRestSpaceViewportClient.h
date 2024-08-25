// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Behaviors/2DViewportBehaviorTargets.h" // FEditor2DScrollBehaviorTarget, FEditor2DMouseWheelZoomBehaviorTarget
#include "InputBehaviorSet.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothPatternVertexType.h"

class UInputBehaviorSet;
class UPointLightComponent;

namespace UE::Chaos::ClothAsset
{

class CHAOSCLOTHASSETEDITOR_API FChaosClothEditorRestSpaceViewportClient : public FEditorViewportClient, public IInputBehaviorSource
{
public:

	FChaosClothEditorRestSpaceViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene = nullptr, const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);

	virtual ~FChaosClothEditorRestSpaceViewportClient() = default;

	// IInputBehaviorSource
	virtual const UInputBehaviorSet* GetInputBehaviors() const override;

	// FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

	virtual bool ShouldOrbitCamera() const override;

	void SetConstructionViewMode(EClothPatternVertexType InViewMode);
	EClothPatternVertexType GetConstructionViewMode() const;

	void SetEditorViewportWidget(TWeakPtr<SEditorViewport> InEditorViewportWidget);
	void SetToolCommandList(TWeakPtr<FUICommandList> ToolCommandList);

	float GetCameraPointLightIntensity() const;
	void SetCameraPointLightIntensity(float Intensity);

private:

	virtual void Tick(float DeltaSeconds) override;

	TObjectPtr<UPointLightComponent> CameraPointLight;

	EClothPatternVertexType ConstructionViewMode = EClothPatternVertexType::Sim2D;

	TObjectPtr<UInputBehaviorSet> BehaviorSet;

	TArray<TObjectPtr<UInputBehavior>> BehaviorsFor2DMode;

	TUniquePtr<FEditor2DScrollBehaviorTarget> ScrollBehaviorTarget;
	TUniquePtr<FEditor2DMouseWheelZoomBehaviorTarget> ZoomBehaviorTarget;

	TWeakPtr<FUICommandList> ToolCommandList;

	// Saved view transform for the currently inactive view mode (i.e. store the 3D camera here while in 2D mode and vice-versa)
	FViewportCameraTransform SavedInactiveViewTransform;
};
} // namespace UE::Chaos::ClothAsset
