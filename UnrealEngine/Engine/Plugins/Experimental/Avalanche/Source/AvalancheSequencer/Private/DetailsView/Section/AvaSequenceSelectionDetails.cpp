// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceSelectionDetails.h"
#include "AvaSequencer.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"
#include "DetailsView/SAvaKeyFrameEdit.h"
#include "IKeyArea.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "SKeyEditInterface.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "AvaSequenceSelectionDetails"

namespace UE::AvaSequencer::Private
{
	FKeyEditData GetKeyEditData(const UE::Sequencer::FKeySelection& InKeySelection)
	{
		if (InKeySelection.Num() == 1)
		{
			for (const FKeyHandle Key : InKeySelection)
			{
				if (const TSharedPtr<UE::Sequencer::FChannelModel> Channel = InKeySelection.GetModelForKey(Key))
				{
					FKeyEditData KeyEditData;
					KeyEditData.KeyStruct     = Channel->GetKeyArea()->GetKeyStruct(Key);
					KeyEditData.OwningSection = Channel->GetSection();
					return KeyEditData;
				}
			}
		}
		else
		{
			TArray<FKeyHandle> KeyHandles;
			UMovieSceneSection* CommonSection = nullptr;
			for (FKeyHandle Key : InKeySelection)
			{
				TSharedPtr<UE::Sequencer::FChannelModel> Channel = InKeySelection.GetModelForKey(Key);
				if (Channel.IsValid())
				{
					KeyHandles.Add(Key);
					if (!CommonSection)
					{
						CommonSection = Channel->GetSection();
					}
					else if (CommonSection != Channel->GetSection())
					{
						CommonSection = nullptr;
						break;
					}
				}
			}

			if (CommonSection)
			{
				FKeyEditData KeyEditData;
				KeyEditData.KeyStruct     = CommonSection->GetKeyStruct(KeyHandles);
				KeyEditData.OwningSection = CommonSection;
				return KeyEditData;
			}
		}

		return FKeyEditData();
	}

	TSharedPtr<UE::Sequencer::FSequencerSelection> GetSelection(const ISequencer& InSequencer)
	{
		const TSharedPtr<UE::Sequencer::FSequencerEditorViewModel> ViewModel = InSequencer.GetViewModel();
		if (!ViewModel.IsValid())
		{
			return nullptr;
		}

		return ViewModel->GetSelection();
	}
}

FName FAvaSequenceSelectionDetails::GetSectionName() const
{
	return TEXT("Selection");
}

FText FAvaSequenceSelectionDetails::GetSectionDisplayName() const
{
	return LOCTEXT("SelectionLabel", "Selection");
}

TSharedRef<SWidget> FAvaSequenceSelectionDetails::CreateContentWidget(const TSharedRef<FAvaSequencer>& InAvaSequencer)
{
	AvaSequencerWeak = InAvaSequencer;

	ContentBorder = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("NoBorder"));

	const ISequencer& Sequencer = InAvaSequencer->GetSequencer().Get();

	if (const TSharedPtr<UE::Sequencer::FSequencerSelection> SequencerSelection = UE::AvaSequencer::Private::GetSelection(Sequencer))
	{
		SequencerSelection->KeySelection.OnChanged.AddSP(this, &FAvaSequenceSelectionDetails::OnSequencerSelectionChanged);
		SequencerSelection->TrackArea.OnChanged.AddSP(this, &FAvaSequenceSelectionDetails::OnSequencerSelectionChanged);
		SequencerSelection->Outliner.OnChanged.AddSP(this, &FAvaSequenceSelectionDetails::OnSequencerSelectionChanged);

		OnSequencerSelectionChanged();
	}

	return ContentBorder.ToSharedRef();
}

void FAvaSequenceSelectionDetails::OnSequencerSelectionChanged()
{
	const TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	if (!AvaSequencer.IsValid())
	{
		ContentBorder->ClearContent();
		return;
	}

	const TSharedRef<ISequencer> Sequencer = AvaSequencer->GetSequencer();

	const TSharedPtr<UE::Sequencer::FSequencerSelection> SequencerSelection = UE::AvaSequencer::Private::GetSelection(*Sequencer);
	if (!SequencerSelection.IsValid())
	{
		ContentBorder->ClearContent();
		return;
	}

	/**
	 * @TODO: This does not check for other selections being made.
	 * Ex. Selecting both a key and a section will always show the key details.
	 * 1) Should we disallow details for a key selection if there is also a section selection?
	 * 2) Should we show all the details for the all the selections in the details view?
	 */

	if (SequencerSelection->KeySelection.Num() > 0)
	{
		const FKeyEditData KeyEditData = UE::AvaSequencer::Private::GetKeyEditData(SequencerSelection->KeySelection);
		if (KeyEditData.KeyStruct.IsValid())
		{
			const TSharedRef<SAvaKeyFrameEdit> KeyFrameEditor = SNew(SAvaKeyFrameEdit, AvaSequencer.ToSharedRef())
				.KeyEditData(this, &FAvaSequenceSelectionDetails::GetKeyEditData);
			ContentBorder->SetContent(KeyFrameEditor);
		}
		else
		{
			ContentBorder->SetContent(CreateHintText(LOCTEXT("InvalidKeyCombination", "Selected keys must belong to the same section.")));
		}

		return;
	}

	TArray<UObject*> ObjectsToView;

	TArray<UMovieSceneSection*> MovieSceneSectionObjects;
	Sequencer->GetSelectedSections(MovieSceneSectionObjects);

	if (MovieSceneSectionObjects.Num() > 0)
	{
		ObjectsToView.Append(MovieSceneSectionObjects);
	}
	else
	{
		TArray<UMovieSceneTrack*> MovieSceneTrackObjects;
		Sequencer->GetSelectedTracks(MovieSceneTrackObjects);

		if (MovieSceneTrackObjects.Num() > 0)
		{
			ObjectsToView.Append(MovieSceneTrackObjects);
		}
	}

	if (ObjectsToView.Num() > 0)
	{
		FCustomDetailsViewArgs CustomDetailsViewArgs;
		CustomDetailsViewArgs.IndentAmount = 0.f;
		CustomDetailsViewArgs.bShowCategories = true;
		CustomDetailsViewArgs.bAllowGlobalExtensions = true;
		CustomDetailsViewArgs.bDefaultItemsExpanded = true;

		DetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(CustomDetailsViewArgs);
		DetailsView->SetObjects(ObjectsToView);

		ContentBorder->SetContent(DetailsView.ToSharedRef());

		return;
	}

	ContentBorder->SetContent(CreateHintText(LOCTEXT("NoSelection", "Select an object to view details.")));
}

TSharedRef<SWidget> FAvaSequenceSelectionDetails::CreateHintText(const FText& InMessage)
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(2.0f, 24.0f, 2.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(InMessage)
			.TextStyle(FAppStyle::Get(), "HintText")
		];
}

FKeyEditData FAvaSequenceSelectionDetails::GetKeyEditData() const
{
	const TSharedPtr<FAvaSequencer> AvaSequencer = AvaSequencerWeak.Pin();
	if (!AvaSequencer.IsValid())
	{
		ContentBorder->ClearContent();
		return FKeyEditData();
	}

	const TSharedRef<ISequencer> Sequencer = AvaSequencer->GetSequencer();

	const TSharedPtr<UE::Sequencer::FSequencerSelection> SequencerSelection = UE::AvaSequencer::Private::GetSelection(*Sequencer);
	if (!SequencerSelection.IsValid())
	{
		ContentBorder->ClearContent();
		return FKeyEditData();
	}

	return UE::AvaSequencer::Private::GetKeyEditData(SequencerSelection->KeySelection);
}

#undef LOCTEXT_NAMESPACE
