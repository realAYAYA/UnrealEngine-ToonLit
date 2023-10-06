// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "LevelEditorViewport.h"

class FChaosVDScene;

/** Client viewport class used for to handle a Chaos Visual Debugger world Interaction/Rendering.
 * It re-routes interaction events to our Chaos VD scene
 */
class FChaosVDPlaybackViewportClient : public FEditorViewportClient
{
public:
	FChaosVDPlaybackViewportClient();
	virtual ~FChaosVDPlaybackViewportClient() override;

	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;

	void SetScene(TWeakPtr<FChaosVDScene> InScene);

	virtual UWorld* GetWorld() const override { return CVDWorld; };

private:

	void HandleObjectFocused(UObject* FocusedObject);

public:
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;

private:
	FDelegateHandle ObjectFocusedDelegateHandle;
	UWorld* CVDWorld;
	TWeakPtr<FChaosVDScene> CVDScene;
};
