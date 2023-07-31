// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "Engine/EngineBaseTypes.h"
#include "UObject/GCObject.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UTexture;

struct FGeometry;
struct FKey;
struct FPointerEvent;

class SSimulcamEditorViewport;

/** Delegate to be executed when the viewport area is clicked */
DECLARE_DELEGATE_TwoParams(FSimulcamViewportClickedEventHandler, const FGeometry& MyGeometry, const FPointerEvent& PointerEvent);

/** Delegate to be executed when the viewport area receives a key event */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FSimulcamViewportInputKeyEventHandler, const FKey& Key, const EInputEvent& Event);

/**
 * UI to display the provided UTexture and respond to user input.
 */
class SSimulcamViewport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimulcamViewport)
	: _WithZoom(true), _WithPan(true)
	{}
	SLATE_EVENT(FSimulcamViewportClickedEventHandler, OnSimulcamViewportClicked)
	SLATE_EVENT(FSimulcamViewportInputKeyEventHandler, OnSimulcamViewportInputKey)
	SLATE_ATTRIBUTE(bool, WithZoom)
	SLATE_ATTRIBUTE(bool, WithPan)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InTexture The source texture to render if any
	 */
	void Construct(const FArguments& InArgs, UTexture* InTexture);

	/** Returns the current camera texture */
	UTexture* GetTexture() const;

	/** Is the camera texture valid ? */
	bool HasValidTextureResource() const;

	/** Called whenever the viewport is clicked */
	void OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent) { OnSimulcamViewportClicked.ExecuteIfBound(MyGeometry, PointerEvent); }

	/** Called when the viewport receives input key presses*/
	bool OnViewportInputKey(const FKey& Key, const EInputEvent& Event);

private:

	/** Delegate to be executed when the viewport area is clicked */
	FSimulcamViewportClickedEventHandler OnSimulcamViewportClicked;

	/** Delegate to be executed when the viewport receives input key presses */
	FSimulcamViewportInputKeyEventHandler OnSimulcamViewportInputKey;

	TSharedPtr<SSimulcamEditorViewport> TextureViewport;

	TStrongObjectPtr<UTexture> Texture;
};

