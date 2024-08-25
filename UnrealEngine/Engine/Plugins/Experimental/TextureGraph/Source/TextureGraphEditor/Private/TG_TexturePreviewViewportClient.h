// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/GCObject.h"
#include "UnrealClient.h"
#include "ViewportClient.h"
#include "STG_SelectionPreview.h"

class FCanvas;
class ITextureEditorToolkit;
class STG_TexturePreviewViewport;
class UTexture2D;

class FTG_TexturePreviewViewportClient
	: public FViewportClient
	, public FGCObject
{
public:
	/** Constructor */
	FTG_TexturePreviewViewportClient(TWeakPtr<STG_SelectionPreview> InSelectionPreview, TWeakPtr<STG_TexturePreviewViewport> InTexturePreviewViewport);
	~FTG_TexturePreviewViewportClient();

	/** FViewportClient interface */
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas) override;
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual bool InputGesture(FViewport* Viewport, EGestureEvent GestureType, const FVector2D& GestureDelta, bool bIsDirectionInvertedFromDevice) override;
	virtual UWorld* GetWorld() const override { return nullptr; }
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport, int32 X, int32 Y) override;
	virtual void MouseMove(FViewport* Viewport, int32 X, int32 Y) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FTG_TexturePreviewViewportClient");
	}

	/** Modifies the checkerboard texture's data */
	void ModifyCheckerboardTextureColors();

	/** Returns a string representation of the currently displayed textures resolution */
	FText GetDisplayedResolution() const;

	/** Returns the ratio of the size of the Texture texture to the size of the viewport */
	float GetViewportVerticalScrollBarRatio() const;
	float GetViewportHorizontalScrollBarRatio() const;

	void SetClearColor(const FLinearColor InColor){ ClearColorOverride = InColor;}

private:
	/** Updates the states of the scrollbars */
	void UpdateScrollBars();

	/** Returns the positions of the scrollbars relative to the Texture textures */
	FVector2D GetViewportScrollBarPositions() const;

	/** Destroy the checkerboard texture if one exists */
	void DestroyCheckerboardTexture();

	/** TRUE if right clicking and dragging for panning a texture 2D */
	bool ShouldUseMousePanning(FViewport* Viewport) const;

private:
	/** Pointer back to the Texture editor tool that owns us */
	TWeakPtr<STG_SelectionPreview> SelectionPreviewPtr;

	/** Pointer back to the Texture viewport control that owns us */
	TWeakPtr<STG_TexturePreviewViewport> TG_TexturePreviewViewportPtr;

	/** Checkerboard texture */
	TObjectPtr<UTexture2D> CheckerboardTexture;

	FLinearColor ClearColorOverride = FLinearColor::Black;
};
