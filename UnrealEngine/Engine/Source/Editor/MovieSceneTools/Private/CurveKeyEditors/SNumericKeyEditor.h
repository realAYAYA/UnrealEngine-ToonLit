// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"
#include "Editor.h"
#include "CurveKeyEditors/SequencerKeyEditor.h"
#include "Widgets/Input/SSpinBox.h"
#include "Styling/AppStyle.h"
#include "CurveKeyEditors/SequencerKeyEditor.h"

#define LOCTEXT_NAMESPACE "NumericKeyEditor"

template<typename T>
struct SNonThrottledSpinBox : SSpinBox<T>
{
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SSpinBox<T>::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (Reply.IsEventHandled())
		{
			Reply.PreventThrottling();
		}
		return Reply;
	}
};

/**
 * A widget for editing a curve representing integer keys.
 */
template<typename ChannelType, typename NumericType>
class SNumericKeyEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNumericKeyEditor){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSequencerKeyEditor<ChannelType, NumericType>& InKeyEditor)
	{
		KeyEditor = InKeyEditor;
		ChildSlot
		[
			SNew(SNonThrottledSpinBox<NumericType>)
			.Style(&FAppStyle::GetWidgetStyle<FSpinBoxStyle>("Sequencer.HyperlinkSpinBox"))
			.Font(FAppStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont"))
			.MinValue(TOptional<NumericType>())
			.MaxValue(TOptional<NumericType>())
			.MaxSliderValue(TOptional<NumericType>())
			.MinSliderValue(TOptional<NumericType>())
			.Value_Raw(&KeyEditor, &decltype(KeyEditor)::GetCurrentValue)
			.OnValueChanged(this, &SNumericKeyEditor::OnValueChanged)
			.OnValueCommitted(this, &SNumericKeyEditor::OnValueCommitted)
			.OnBeginSliderMovement(this, &SNumericKeyEditor::OnBeginSliderMovement)
			.OnEndSliderMovement(this, &SNumericKeyEditor::OnEndSliderMovement)
		];
	}

private:

	void OnBeginSliderMovement()
	{
		GEditor->BeginTransaction(LOCTEXT("SetNumericKey", "Set Key Value"));
	}

	void OnEndSliderMovement(NumericType Value)
	{
		if (GEditor->IsTransactionActive())
		{
			KeyEditor.SetValue(Value);
			GEditor->EndTransaction();
		}
	}

	void OnValueChanged(NumericType Value)
	{
		KeyEditor.SetValueWithNotify(Value, EMovieSceneDataChangeType::TrackValueChanged);
	}

	void OnValueCommitted(NumericType Value, ETextCommit::Type CommitInfo)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			const FScopedTransaction Transaction( LOCTEXT("SetNumericKey", "Set Key Value") );
			KeyEditor.SetValueWithNotify(Value, EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
		}
	}

private:

	TSequencerKeyEditor<ChannelType, NumericType> KeyEditor;
};

#undef LOCTEXT_NAMESPACE
