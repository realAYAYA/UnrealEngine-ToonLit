// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencerStagger.h"
#include "AvaSequencer.h"
#include "Commands/AvaSequencerCommands.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "ISequencer.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/LayerBarModel.h"
#include "Misc/FrameTime.h"
#include "SAvaSequencerStaggerSettings.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "AvaSequencerStagger"

FAvaSequencerStagger::FStaggerElement::FStaggerElement(UE::Sequencer::TViewModelPtr<UE::Sequencer::FLayerBarModel> InLayerBarModel)
	: LayerBarModel(MoveTemp(InLayerBarModel))
{
}

void FAvaSequencerStagger::FStaggerElement::ComputeRange()
{
	ComputedRange = LayerBarModel->ComputeRange();
}

void FAvaSequencerStagger::MapAction(const TSharedRef<FUICommandList>& InCommandList)
{
	InCommandList->MapAction(FAvaSequencerCommands::Get().StaggerLayerBars
		, FExecuteAction::CreateSP(this, &FAvaSequencerStagger::Execute)
		, FCanExecuteAction::CreateSP(this, &FAvaSequencerStagger::CanExecute));
}

bool FAvaSequencerStagger::CanExecute() const
{
	return GatherStaggerElements().Num() > 1;
}

void FAvaSequencerStagger::Execute()
{
	using namespace UE::Sequencer;

	TArray<FStaggerElement> StaggerElements = GatherStaggerElements();
	if (StaggerElements.Num() <= 1)
	{
		return;
	}

	FAvaSequencerStaggerSettings Settings;
	if (!GetSettings(Settings))
	{
		return;
	}

	TSharedRef<ISequencer> Sequencer = Owner.GetSequencer();

	Settings.Shift = ConvertFrameTime(Settings.Shift
		, Sequencer->GetFocusedDisplayRate()
		, Sequencer->GetFocusedTickResolution()).FrameNumber;

	Stagger(StaggerElements, Settings);
}

TArray<FAvaSequencerStagger::FStaggerElement> FAvaSequencerStagger::GatherStaggerElements() const
{
	using namespace UE::Sequencer;

	TSharedPtr<FSequencerSelection> SequencerSelection = GetSequencerSelection();
	if (!SequencerSelection.IsValid())
	{
		return TArray<FStaggerElement>();
	}

	// Gather all the Layer Bar Models Selected
	TArray<TViewModelPtr<FLayerBarModel>> LayerBarModels;
	LayerBarModels.Reserve(SequencerSelection->Outliner.Num());

	for (FViewModelPtr ViewModel : SequencerSelection->Outliner)
	{
		if (!ViewModel.IsValid())
		{
			continue;
		}

		TSharedPtr<ITrackAreaExtension> TrackArea = ViewModel.ImplicitCast();
		if (!TrackArea.IsValid())
		{
			continue;
		}

		for (const FViewModelPtr& TrackAreaModel : TrackArea->GetTopLevelChildTrackAreaModels())
		{
			if (TViewModelPtr<FLayerBarModel> LayerBarModel = TrackAreaModel.ImplicitCast())
			{
				LayerBarModels.Add(LayerBarModel);
			}
		}
	}

	// Remove all Layer Models that have a Descendant that is Selected
	LayerBarModels.RemoveAll([SequencerSelection](const TViewModelPtr<FLayerBarModel>& InLayerBarModel)
		{
			TViewModelPtr<IOutlinerExtension> OutlinerExtension = InLayerBarModel->GetLinkedOutlinerItem();
			if (!OutlinerExtension.IsValid())
			{
				return false;
			}

			constexpr bool bIncludeThis = false;
			for (TViewModelPtr<IOutlinerExtension> ChildOutlinerExtension : OutlinerExtension.AsModel()->GetDescendantsOfType<IOutlinerExtension>(bIncludeThis))
			{
				if (SequencerSelection->Outliner.IsSelected(ChildOutlinerExtension))
				{
					return true;
				}
			}
			return false;
		});

	TArray<FStaggerElement> StaggerElements;
	StaggerElements.Append(MoveTemp(LayerBarModels));
	return StaggerElements;
}

bool FAvaSequencerStagger::GetSettings(FAvaSequencerStaggerSettings& OutSettings)
{
	TSharedRef<SAvaSequencerStaggerSettings> SettingsWidget = SNew(SAvaSequencerStaggerSettings);

	TSharedRef<SWindow> SettingsWindow =
		SNew(SWindow)
		.Title(LOCTEXT("StaggerSettings", "Stagger Settings"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.Content()
		[
			SettingsWidget
		];

	TSharedPtr<SWidget> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(Owner.GetSequencer()->GetSequencerWidget());
	FSlateApplication::Get().AddModalWindow(SettingsWindow, ParentWindow);

	if (SettingsWidget->GetReturnType() == EAppReturnType::Ok)
	{
		OutSettings = SettingsWidget->GetSettings();
		return true;
	}

	return false;
}

void FAvaSequencerStagger::Stagger(TArrayView<FStaggerElement> InStaggerElements, const FAvaSequencerStaggerSettings& InSettings)
{
	if (InStaggerElements.Num() <= 1)
	{
		return;
	}

	// Compute Range for Each Element to Stagger
	for (FStaggerElement& Element : InStaggerElements)
	{
		Element.ComputeRange();
	}

	FScopedTransaction Transaction(LOCTEXT("Stagger", "Stagger"));

	// Find Earliest Stagger Point
	FFrameNumber NextStaggerPoint = InStaggerElements[0].ComputedRange.GetLowerBoundValue();

	switch (InSettings.StartPosition)
	{
	case FAvaSequencerStaggerSettings::EStartPosition::FirstSelected:
		break;
	case FAvaSequencerStaggerSettings::EStartPosition::FirstInTimeline:
		for (const FStaggerElement& Element : InStaggerElements)
		{
			const FFrameNumber& ElementLowerBound = Element.ComputedRange.GetLowerBoundValue();
			if (ElementLowerBound < NextStaggerPoint)
			{
				NextStaggerPoint = ElementLowerBound;
			}
		}
		break;
	}

	for (const FStaggerElement& Element : InStaggerElements)
	{
		Element.LayerBarModel->Offset(NextStaggerPoint - Element.ComputedRange.GetLowerBoundValue());

		switch (InSettings.OperationPoint)
		{
		case FAvaSequencerStaggerSettings::EOperationPoint::Start:
			NextStaggerPoint += InSettings.Shift.FrameNumber;
			break;
		case FAvaSequencerStaggerSettings::EOperationPoint::End:
			NextStaggerPoint += Element.ComputedRange.Size<FFrameNumber>() + InSettings.Shift.FrameNumber;
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE
