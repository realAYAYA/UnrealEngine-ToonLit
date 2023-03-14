// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UnrealClient.h"
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/GCObject.h"

class FCanvas;
class ITextureEditorToolkit;
class STextureEditorViewport;
class UTexture2D;
class SSimulcamViewport;
class SSimulcamEditorViewport;

class FSimulcamEditorViewportClient
	: public FViewportClient
{
public:
	/** Constructor */
	FSimulcamEditorViewportClient(const TSharedRef<SSimulcamViewport>& InSimulcamViewport, const TSharedRef<SSimulcamEditorViewport>& InSimulcamEditorViewport, const bool bInWithZoom, const bool bInWithPan);

	/** Begin FViewportClient interface */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	virtual bool InputChar(FViewport* Viewport, int32 ControllerId, TCHAR Character) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) override;
	virtual UWorld* GetWorld() const override { return nullptr; }
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport, int32 X, int32 Y) override;
	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;
	/** End FViewportClient interface */

	/** Returns a string representation of the currently displayed textures resolution */
	FText GetDisplayedResolution() const;

	/** Triggered whenever the viewport is resized */
	void OnViewportResized(FViewport* InViewport, uint32 InParams);

	/** Triggered whenever the underlying texture resource is resized */
	void OnTextureResized();

private:
	FVector2D GetTexturePosition() const;
	
	/** TRUE if right clicking and dragging for panning a texture 2D */
	bool ShouldUseMousePanning(FViewport* Viewport) const;

	bool CanPanHorizontally(FViewport* Viewport, float Direction) const;
	bool CanPanVertically(FViewport* Viewport, float Direction) const;

	/** Zoom under a specific pixel maintaining it aligned to the cursor */
	void ZoomOnPoint(FViewport* Viewport, FIntPoint InPoint, TFunction<void()> ZoomFunction);

	/** Zoom to fit the texture in the viewport */
	void ZoomToFit(FViewport* InViewport);

	/** Zoom Out trying to (smoothly) re-fit the image if there is space on borders */
	void ZoomTowardsFit(FViewport* Viewport);

	TWeakPtr<SSimulcamViewport> SimulcamViewportWeakPtr;
	TWeakPtr<SSimulcamEditorViewport> SimulcamEditorViewportWeakPtr;

	FIntPoint MousePosition;

	FVector2D CurrentTexturePosition = FVector2D(0, 0);

	FIntPoint CurrentViewportSize = FIntPoint(0, 0);

	bool bWithZoom;
	bool bWithPan;
};
