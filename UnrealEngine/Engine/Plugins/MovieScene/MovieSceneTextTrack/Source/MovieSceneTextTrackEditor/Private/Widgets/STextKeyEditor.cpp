// Copyright Epic Games, Inc. All Rights Reserved.

#include "STextKeyEditor.h"
#include "IKeyArea.h"
#include "ISequencer.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MovieSceneTimeHelpers.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "STextKeyEditor"

namespace UE::MovieScene
{

void STextKeyEditor::Construct(const FArguments& InArgs, FTextKeyEditorParams&& InParams)
{
	Params = MoveTemp(InParams);

	ChildSlot
	[
		SNew(SEditableTextBox)
		.MinDesiredWidth(10.f)
		.SelectAllTextWhenFocused(true)
		.Text(this, &STextKeyEditor::GetText)
		.OnTextCommitted(this, &STextKeyEditor::OnTextCommitted)
	];
}

FText STextKeyEditor::GetText() const
{
	ISequencer* Sequencer = Params.WeakSequencer.Pin().Get();
	UMovieSceneSection* Section = Params.WeakSection.Get();
	FMovieSceneTextChannel* Channel = Params.ChannelHandle.Get();

	if (!Channel || !Sequencer || !Section)
	{
		return FText::GetEmpty();
	}

	const FFrameTime CurrentTime = UE::MovieScene::ClampToDiscreteRange(Sequencer->GetLocalTime().Time, Section->GetRange());

	FText Text;

	// If we have no keys and no default, key with the external value if it exists
	if (!UE::MovieScene::EvaluateChannel(Section, Channel, CurrentTime, Text))
	{
		FTrackInstancePropertyBindings* PropertyBindings = Params.WeakPropertyBindings.Pin().Get();
		for (TWeakObjectPtr<> WeakObject : Sequencer->FindBoundObjects(Params.ObjectBindingID, Sequencer->GetFocusedTemplateID()))
		{
			if (UObject* Object = WeakObject.Get())
			{
				TOptional<FText> ExternalValue = Params.OnGetExternalValue(*Object, PropertyBindings);
				if (ExternalValue.IsSet())
				{
					return ExternalValue.GetValue();
				}
			}
		}
	}

	return Text;
}

void STextKeyEditor::OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	using namespace UE::MovieScene;
	using namespace UE::Sequencer;

	UMovieSceneSection* Section = Params.WeakSection.Get();
	if (!Section)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetTextKey", "Set Text Key Value"));

	Section->SetFlags(RF_Transactional);

	FMovieSceneTextChannel* Channel  = Params.ChannelHandle.Get();
	TSharedPtr<ISequencer> Sequencer = Params.WeakSequencer.Pin();

	if (!Section->TryModify() || !Channel || !Sequencer.IsValid())
	{
		return;
	}

	const bool bAutoSetTrackDefaults = Sequencer->GetAutoSetTrackDefaults();
	const FKeySelection& KeySelection = Sequencer->GetViewModel()->GetSelection()->KeySelection;
	if (KeySelection.Num())
	{
		for (FKeyHandle Key : KeySelection)
		{
			// Make sure we only manipulate the values of the channel with the same channel type we're editing
			TSharedPtr<FChannelModel> ChannelModel = KeySelection.GetModelForKey(Key);

			const bool bMatchingChannelType = ChannelModel.IsValid()
				&& ChannelModel->GetKeyArea()
				&& ChannelModel->GetKeyArea()->GetChannel().GetChannelTypeName() == Params.ChannelHandle.GetChannelTypeName();

			if (bMatchingChannelType)
			{
				UMovieSceneSection* ModelSection = ChannelModel->GetSection();
				if (ModelSection && ModelSection->TryModify())
				{
					AssignValue(reinterpret_cast<FMovieSceneTextChannel*>(ChannelModel->GetChannel()), Key, InText);
				}
			}
		}
	}
	else
	{
		const FFrameNumber CurrentTime = Sequencer->GetLocalTime().Time.FloorToFrame();

		EMovieSceneKeyInterpolation Interpolation = GetInterpolationMode(Channel, CurrentTime, Sequencer->GetKeyInterpolation());

		TArray<FKeyHandle> KeysAtCurrentTime;
		Channel->GetKeys(TRange<FFrameNumber>(CurrentTime), nullptr, &KeysAtCurrentTime);

		if (KeysAtCurrentTime.Num() > 0)
		{
			AssignValue(Channel, KeysAtCurrentTime[0], InText);
		}
		else
		{
			bool bHasAnyKeys = Channel->GetNumKeys() != 0;

			if (bHasAnyKeys || bAutoSetTrackDefaults == false)
			{
				// When auto setting track defaults are disabled, add a key even when it's empty so that the changed
				// value is saved and is propagated to the property.
				AddKeyToChannel(Channel, CurrentTime, InText, Interpolation);
				bHasAnyKeys = Channel->GetNumKeys() != 0;
			}

			if (bHasAnyKeys)
			{
				TRange<FFrameNumber> KeyRange     = TRange<FFrameNumber>(CurrentTime);
				TRange<FFrameNumber> SectionRange = Section->GetRange();

				if (!SectionRange.Contains(KeyRange))
				{
					Section->SetRange(TRange<FFrameNumber>::Hull(KeyRange, SectionRange));
				}
			}
		}
	}

	// Always update the default value when auto-set default values is enabled so that the last changes
	// are always saved to the track.
	if (bAutoSetTrackDefaults)
	{
		SetChannelDefault(Channel, InText);
	}

	// need to tell channel change happened (float will call AutoSetTangents())
	Channel->PostEditChange();

	const FMovieSceneChannelMetaData* ChannelMetaData = Params.ChannelHandle.GetMetaData();
	Sequencer->OnChannelChanged().Broadcast(ChannelMetaData, Section);
	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChangedRefreshImmediately);
}

}
#undef LOCTEXT_NAMESPACE
