// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "Behaviors/2DViewportBehaviorTargets.h" // FEditor2DScrollBehaviorTarget, FEditor2DMouseWheelZoomBehaviorTarget
#include "InputBehaviorSet.h"

class UInputBehaviorSet;

class CHAOSCLOTHASSETEDITOR_API FChaosClothEditorRestSpaceViewportClient : public FEditorViewportClient
{
public:

	FChaosClothEditorRestSpaceViewportClient(FEditorModeTools* InModeTools, FPreviewScene* InPreviewScene = nullptr, const TWeakPtr<SEditorViewport>& InEditorViewportWidget = nullptr);

	virtual ~FChaosClothEditorRestSpaceViewportClient() = default;

	// FEditorViewportClient
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;

	virtual bool ShouldOrbitCamera() const override;

	void Set2DMode(bool In2DMode);

	void SetEditorViewportWidget(TWeakPtr<SEditorViewport> InEditorViewportWidget);
	void SetToolCommandList(TWeakPtr<FUICommandList> ToolCommandList);

private:

	bool b2DMode = false;

	TWeakPtr<FUICommandList> ToolCommandList;
};

