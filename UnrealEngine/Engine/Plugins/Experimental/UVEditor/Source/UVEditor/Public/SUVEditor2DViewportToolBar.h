// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SViewportToolBar.h"
#include "Styling/SlateTypes.h"

class FExtender;
class FUICommandList;
class SUVEditor2DViewport;
class FUVEditor2DViewportClient;

/**
 * Toolbar that shows up at the top of the 2d viewport (has gizmo controls)
 */
class SUVEditor2DViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SUVEditor2DViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
		SLATE_ARGUMENT(TSharedPtr<FUVEditor2DViewportClient>, Viewport2DClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeSelectionToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeGizmoToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeTransformToolBar(const TSharedPtr< FExtender > InExtenders);

	TSharedRef<SWidget> BuildLocationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const;
	TSharedRef<SWidget> BuildRotationGridCheckBoxList(FName InExtentionHook, const FText& InHeading, const TArray<float>& InGridSizes) const;


	/** Grid snap label callbacks */
	FText GetLocationGridLabel() const;
	FText GetRotationGridLabel() const;	
	FText GetScaleGridLabel() const;

	/** GridSnap menu construction callbacks */
	TSharedRef<SWidget> FillLocationGridSnapMenu();
	TSharedRef<SWidget> FillRotationGridSnapMenu();
	TSharedRef<SWidget> FillScaleGridSnapMenu();

	/** Grid Snap checked state callbacks */
	ECheckBoxState IsLocationGridSnapChecked() const;
	ECheckBoxState IsRotationGridSnapChecked() const;
	ECheckBoxState IsScaleGridSnapChecked() const;

	/** Grid snap toggle handlers */
	void HandleToggleLocationGridSnap(ECheckBoxState InState);
	void HandleToggleRotationGridSnap(ECheckBoxState InState);	
	void HandleToggleScaleGridSnap(ECheckBoxState InState);

	/** Command list */
	TSharedPtr<FUICommandList> CommandList;

	/** Client - For connecting snap controls */
	TSharedPtr<FUVEditor2DViewportClient> Viewport2DClient;

	/** Current Snap Indices */
	int32 CurLocationSnapIndex;
	int32 CurRotationSnapIndex;
	int32 CurScaleSnapIndex;
};
