// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaKeyFrameEdit.h"
#include "AvaSequencer.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveTool.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Widgets/Layout/SScrollBox.h"

using namespace UE::Sequencer;

#define LOCTEXT_NAMESPACE "SAvaKeyFrameEdit"

namespace UE::AvaSequencer::Private
{
	/** Utility widget to auto update horizontal alignment. */
	class SDynamicHAlign : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDynamicHAlign)
			: _HAlign(HAlign_Fill)
		{}
			SLATE_ATTRIBUTE(EHorizontalAlignment, HAlign)
			SLATE_ARGUMENT(TSharedPtr<SWidget>, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			SetCanTick(true);

			HAlign = InArgs._HAlign;

			if (InArgs._Content.IsValid())
			{
				ChildSlot
					.HAlign(HAlign.Get())
					[
						InArgs._Content.ToSharedRef()
					];
			}
		}

	protected:
		//~ Begin SWidget
		virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
		{
			SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

			if (ChildSlot.GetHorizontalAlignment() != HAlign.Get())
			{
				ChildSlot.SetHorizontalAlignment(HAlign.Get());
			}
		}
		//~ End SWidget

		TAttribute<EHorizontalAlignment> HAlign;
	};
}

void SAvaKeyFrameEdit::Construct(const FArguments& InArgs, const TSharedRef<FAvaSequencer>& InSequencer)
{
	AvaSequencerWeak = InSequencer;

	KeyEditData = InArgs._KeyEditData;

	const TSharedRef<ISequencer> Sequencer = InSequencer->GetSequencer();

	if (const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = Sequencer->GetViewModel())
	{
		SequencerSelectionWeak = SequencerViewModel->GetSelection();
	}

	if (!AvaSequencerWeak.IsValid())
	{
		return;
	}

	const TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();

	ChildSlot
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			.AutoSize()
			.HAlign(HAlign_Fill)
			.Padding(1.f, 2.f, 2.f, 3.f)
			[
				/** Horizontal alignment is polled every frame from settings.
				Fill will stretch the ease curve graph editor horizontally in the details panel.
				Left/Right/Center will maintain 1:1 ratio. */
				SNew(UE::AvaSequencer::Private::SDynamicHAlign)
				.HAlign_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGraphHAlign)
				.Content(InSequencer->GetEaseCurveTool()->GenerateWidget())
			]
			+ SScrollBox::Slot()
			.AutoSize()
			.HAlign(HAlign_Fill)
			[
				SNew(SKeyEditInterface, AvaSequencer->GetSequencer())
				.EditData(KeyEditData)
			]
		];
}

#undef LOCTEXT_NAMESPACE
