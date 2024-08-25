// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EditorUndoClient.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
class FText;
class SAvaEaseCurveEditor;
class SAvaEaseCurvePreset;
class UCurveBase;
class UToolMenu;
struct FKeyHandle;
struct FRichCurve;

class SAvaEaseCurveTool
	: public SCompoundWidget
	, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SAvaEaseCurveTool)
		: _ToolMode(FAvaEaseCurveTool::EMode::DualKeyEdit)
		, _ToolOperation(FAvaEaseCurveTool::EOperation::InOut)
	{}
		SLATE_ATTRIBUTE(FAvaEaseCurveTool::EMode, ToolMode)
		SLATE_ATTRIBUTE(FAvaEaseCurveTool::EOperation, ToolOperation)
		SLATE_ARGUMENT(FAvaEaseCurveTangents, InitialTangents)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAvaEaseCurveTool>& InEaseCurveTool);
	
	void SetTangents(const FAvaEaseCurveTangents& InTangents, FAvaEaseCurveTool::EOperation InOperation,
		const bool bInSetEaseCurve, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents) const;

	float GetStartTangent() const;
	float GetStartTangentWeight() const;
	float GetEndTangent() const;
	float GetEndTangentWeight() const;

	FKeyHandle GetSelectedKeyHandle() const;

	void ZoomToFit() const;

protected:
	TSharedRef<SWidget> ConstructCurveEditorPanel();
	TSharedRef<SWidget> ConstructInputBoxes();
	TSharedRef<SWidget> ConstructTangentNumBox(const FText& InLabel
		, const FText& InToolTip
		, const TAttribute<float>& InValue
		, const SNumericEntryBox<float>::FOnValueChanged& InOnValueChanged
		, const TOptional<float>& InMinSliderValue
		, const TOptional<float>& InMaxSliderValue) const;

	void HandleEditorTangentsChanged(const FAvaEaseCurveTangents& InTangents) const;

	void OnStartTangentSpinBoxChanged(const float InNewValue) const;
	void OnStartTangentWeightSpinBoxChanged(const float InNewValue) const;
	void OnEndTangentSpinBoxChanged(const float InNewValue) const;
	void OnEndTangentWeightSpinBoxChanged(const float InNewValue) const;

	void OnPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const;
	void OnQuickPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const;

	void BindCommands();

	TSharedRef<SWidget> CreateContextMenuContent();
	void MakeContextMenuSettings(UToolMenu* const InToolMenu);

	void UndoAction();
	void RedoAction();

	void OnEditorDragStart() const;
	void OnEditorDragEnd() const;

	FText GetStartText() const;
	FText GetStartTooltipText() const;
	FText GetEndText() const;
	FText GetEndTooltipText() const;

	void ResetToDefaultPresets();

	//~ Begin SWidget
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<FAvaEaseCurveTool> EaseCurveTool;

	TAttribute<FAvaEaseCurveTool::EMode> ToolMode;
	TAttribute<FAvaEaseCurveTool::EOperation> ToolOperation;

	TSharedPtr<SAvaEaseCurveEditor> CurveEaseEditorWidget;
	TSharedPtr<SAvaEaseCurvePreset> CurvePresetWidget;

	float CurrentGraphSize = 200.f;
};
