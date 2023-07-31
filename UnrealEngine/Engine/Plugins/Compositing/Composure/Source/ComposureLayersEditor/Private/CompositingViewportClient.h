// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "UObject/WeakObjectPtr.h"
#include "TickableEditorObject.h"

class FCanvas;
class FCompositingViewport;
class UEditorCompElementContainer;

class FCompositingViewportClient : public FEditorViewportClient, public FTickableEditorObject
{
public:
	 FCompositingViewportClient(TWeakObjectPtr<UEditorCompElementContainer> CompElements);
	 virtual ~FCompositingViewportClient();

	 bool IsDrawing() const;

	//~ Begin FViewportClient interface
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override;
	virtual bool ProcessScreenShots(FViewport* Viewport) override;
	//~ End FViewportClient interface

	//~ Begin FEditorViewportClient interface
	virtual bool WantsDrawWhenAppIsHidden() const override;
	//~ End FEditorViewportClient Interface

	//~ Begin FTickableEditorObject interface
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableEditorObject interface

private:
	bool InternalIsVisible() const;

	TSharedPtr<FCompositingViewport> CompositingViewport;
	TWeakObjectPtr<UEditorCompElementContainer> ElementsContainerPtr;
	bool bIsDrawing = false;
};
