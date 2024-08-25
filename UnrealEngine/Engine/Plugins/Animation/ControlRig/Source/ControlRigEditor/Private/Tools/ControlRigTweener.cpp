// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ControlRigTweener.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "ControlRig.h"
#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "Rigs/RigControlHierarchy.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "IKeyArea.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "CurveEditor.h"
#include "CurveModel.h"
#include "Channels/FloatChannelCurveModel.h"
#include "MovieSceneSection.h"
#include "EditorModeManager.h"
#include "EditMode/ControlRigEditMode.h"

#define LOCTEXT_NAMESPACE "ControlRigTweenWidget"

/**
*
* FAnimSliderKeySelection
*
*/

FAnimSliderKeySelection::FContiguousKeys::FContiguousKeys(const TArray<FKeyHandle>& AllKeyHandles, const TArray<FKeyPosition>& AllKeyPositions, const TArray<int32>& InContiguousKeyIndices)
{
	Indices.Reset();
	if (AllKeyHandles.Num() > 0 && AllKeyPositions.Num() == AllKeyHandles.Num() && InContiguousKeyIndices.Num() > 0)
	{
		Indices = InContiguousKeyIndices;
		//Previous
		PreviousIndex = Indices[0] - 1;
		if (PreviousIndex < 0)
		{
			PreviousIndex = INDEX_NONE;
		}		
		//Next
		NextIndex = Indices[Indices.Num() -1] + 1;
		if (NextIndex >= AllKeyHandles.Num())
		{
			NextIndex = INDEX_NONE;
		}
	}
}

FAnimSliderKeySelection::FContiguousKeysArray::FContiguousKeysArray(const TArray<FKeyHandle>& InAllKeyHandles, const TArray<FKeyPosition>& InAllKeyPositions)
{
	AllKeyHandles = InAllKeyHandles;
	AllKeyPositions = InAllKeyPositions;
	KeysArray.Reset();	
}

void FAnimSliderKeySelection::FContiguousKeysArray::Add(const TArray<int32>& InContiguousKeyIndices)
{
	if (AllKeyHandles.Num() > 0 && AllKeyPositions.Num() == AllKeyHandles.Num() && InContiguousKeyIndices.Num() > 0)
	{
		KeysArray.Add(FContiguousKeys(AllKeyHandles, AllKeyPositions, InContiguousKeyIndices));
	}
}

static int32 GetIndex(const TArray<FKeyPosition>& Keys, double Time)
{
	const int32 Index = Algo::LowerBoundBy(Keys, Time, [](const FKeyPosition& Value) { return Value.InputValue; });

	// don't trust precision issues so will double check to make sure the index is correct
	if (Index != INDEX_NONE)
	{
		if (FMath::IsNearlyEqual(Keys[Index].InputValue, Time))
		{
			return Index;
		}
		else if (((Index - 1) >= 0) && FMath::IsNearlyEqual(Keys[Index - 1].InputValue, Time))
		{
			return Index - 1;
		}
		else if (((Index + 1) < Keys.Num()) && FMath::IsNearlyEqual(Keys[Index + 1].InputValue, Time))
		{
			return Index + 1;
		}
	}
	return Index;
}

bool FAnimSliderKeySelection::Setup(const TWeakPtr<ISequencer>& InSequencer)
{
	using namespace UE::Sequencer;

	KeyMap.Reset();
	if (InSequencer.IsValid())
	{
		const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = InSequencer.Pin()->GetViewModel();
		const FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>();
		check(CurveEditorExtension);
		CurveEditor = CurveEditorExtension->GetCurveEditor();
		GetMapOfContiguousKeys();
	}
	return (KeyMap.Num() > 0);
}

bool FAnimSliderObjectSelection::Setup(TWeakPtr<ISequencer>& InSequencer, TWeakPtr<FControlRigEditMode>& InEditMode)
{
	ChannelsArray.Reset();
	const TArray<UControlRig*> ControlRigs = GetControlRigs(InEditMode);
	return Setup(ControlRigs, InSequencer);
}

void FAnimSliderKeySelection::GetMapOfContiguousKeys()
{
	TArray<FKeyHandle> AllKeyHandles;
	TArray<FKeyPosition> AllKeyPositions;

	TArray<FKeyHandle> SelectedKeyHandles;
	TArray<FKeyPosition> SelectedKeyPositions;

	const TMap<FCurveModelID, FKeyHandleSet>& SelectionKeyMap = CurveEditor->Selection.GetAll();

	TArray<int32> ContiguousKeyIndices;

	for (const TPair<FCurveModelID, FKeyHandleSet>& Pair : SelectionKeyMap)
	{
		if (const FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key))
		{
			AllKeyHandles.Reset();
			Curve->GetKeys(*CurveEditor, TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), TNumericLimits<double>::Lowest(), TNumericLimits<double>::Max(), AllKeyHandles);
			AllKeyPositions.SetNum(AllKeyHandles.Num());
			Curve->GetKeyPositions(AllKeyHandles, AllKeyPositions);
			SelectedKeyHandles.Reset(Pair.Value.Num());
			SelectedKeyHandles.Append(Pair.Value.AsArray().GetData(), Pair.Value.Num());

			// Get all the selected keys
			SelectedKeyPositions.SetNum(SelectedKeyHandles.Num());
			Curve->GetKeyPositions(SelectedKeyHandles, SelectedKeyPositions);

			ContiguousKeyIndices.Reset();
			FContiguousKeysArray& KeyArray = KeyMap.Add(Pair.Key, FContiguousKeysArray(AllKeyHandles, AllKeyPositions));

			TArray<int32> SelectedIndices;
			for (int32 Index = 0; Index < SelectedKeyPositions.Num(); ++Index)
			{
				int32 AllKeysIndex = GetIndex(AllKeyPositions, SelectedKeyPositions[Index].InputValue);
				SelectedIndices.Add(AllKeysIndex);
			}
			SelectedIndices.Sort();
			for(int32 AllKeysIndex: SelectedIndices)
			{
				if (ContiguousKeyIndices.Num() > 0) //see if this key is next to the previous one
				{
					if (ContiguousKeyIndices[ContiguousKeyIndices.Num() - 1] + 1 == AllKeysIndex)
					{
						ContiguousKeyIndices.Add(AllKeysIndex);
					}
					else //not contiguous, so need to add to the OutKeyMap
					{
						//now start new set with the new index
						KeyArray.Add(ContiguousKeyIndices);
						ContiguousKeyIndices.Reset();
						ContiguousKeyIndices.Add(AllKeysIndex);
					}
				}
				else//first key in this set so just add
				{
					ContiguousKeyIndices.Add(AllKeysIndex);
				}
			}
			if (ContiguousKeyIndices.Num() > 0)
			{
				KeyArray.Add(ContiguousKeyIndices);
				ContiguousKeyIndices.Reset();
			}
		}
	}
}

/**
*
* FAnimSliderObjectSelection
*
*/
TArray<UControlRig*> FAnimSliderObjectSelection::GetControlRigs(TWeakPtr<FControlRigEditMode>& InEditMode)
{
	TArray<UControlRig*> ControlRigs;
	if (const FControlRigEditMode* EditMode = InEditMode.Pin().Get())
	{
		TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
		EditMode->GetAllSelectedControls(SelectedControls);
		for (TPair<UControlRig*, TArray<FRigElementKey>>& Selected : SelectedControls)
		{
			ControlRigs.Add(Selected.Key);
		}
	}
	return ControlRigs;
}

void FAnimSliderObjectSelection::SetupChannel(FFrameNumber CurrentFrame, TArray<FFrameNumber>& KeyTimes, TArray<FKeyHandle>& Handles, 
	FMovieSceneFloatChannel* FloatChannel, FMovieSceneDoubleChannel* DoubleChannel,
	FChannelKeyBounds& KeyBounds)
{
	KeyBounds.FloatChannel = FloatChannel;
	KeyBounds.DoubleChannel = DoubleChannel;
	KeyBounds.CurrentFrame = CurrentFrame;
	KeyBounds.PreviousIndex = INDEX_NONE;
	KeyBounds.NextIndex = INDEX_NONE;
	KeyTimes.SetNum(0);
	Handles.SetNum(0);
	if (FloatChannel)
	{
		FloatChannel->GetKeys(TRange<FFrameNumber>(), &KeyTimes, &Handles);
		float OutValue;
		FloatChannel->Evaluate(FFrameTime(CurrentFrame), OutValue);
		KeyBounds.CurrentValue = OutValue;
	}
	else if (DoubleChannel)
	{
		DoubleChannel->GetKeys(TRange<FFrameNumber>(), &KeyTimes, &Handles);
		double OutValue;
		DoubleChannel->Evaluate(FFrameTime(CurrentFrame), OutValue);
		KeyBounds.CurrentValue = OutValue;
	}

	if (KeyTimes.Num() > 0)
	{
		TArrayView<const FMovieSceneFloatValue> FloatValues;
		TArrayView<const FMovieSceneDoubleValue> DoubleValues;
		if (FloatChannel)
		{
			FloatValues = FloatChannel->GetValues();
		}
		else if (DoubleChannel)
		{
			DoubleValues = DoubleChannel->GetValues();

		}
		for (int32 Index = 0; Index < KeyTimes.Num(); Index++)
		{
			const FFrameNumber FrameNumber = KeyTimes[Index];
			if (FrameNumber < CurrentFrame || (FrameNumber == CurrentFrame && KeyBounds.PreviousIndex == INDEX_NONE))
			{
				KeyBounds.PreviousIndex = Index;
				KeyBounds.PreviousFrame = FrameNumber;
				if (FloatChannel)
				{
					KeyBounds.PreviousValue = FloatValues[Index].Value;
				}
				else if (DoubleChannel)
				{
					KeyBounds.PreviousValue = DoubleValues[Index].Value;
				}
			}
			else if (FrameNumber > CurrentFrame || (FrameNumber == CurrentFrame && Index == KeyTimes.Num() -1))
			{
				KeyBounds.NextIndex = Index;
				KeyBounds.NextFrame = FrameNumber;
				if (FloatChannel)
				{
					KeyBounds.NextValue = FloatValues[Index].Value;
				}
				else if (DoubleChannel)
				{
					KeyBounds.NextValue = DoubleValues[Index].Value;
				}
				break;
			}
		}
	}
	KeyBounds.bValid = (KeyBounds.PreviousIndex != INDEX_NONE && KeyBounds.NextIndex != INDEX_NONE
		&& KeyBounds.PreviousIndex != KeyBounds.NextIndex) ? true : false;
}

bool FAnimSliderObjectSelection::Setup(const TArray<UControlRig*>& SelectedControlRigs, TWeakPtr<ISequencer>& InSequencer)
{
	ChannelsArray.Reset();
	if (InSequencer.IsValid() == false)
	{
		return false;
	}
	ISequencer* Sequencer = InSequencer.Pin().Get();

	const FQualifiedFrameTime CurrentTime = Sequencer->GetLocalTime();
	const UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	//get selected controls and objects
	TArray<FGuid> SelectedGuids;
	Sequencer->GetSelectedObjects(SelectedGuids);

	TArray<UMovieSceneControlRigParameterSection*> HandledSections;
	auto SetupControlRigTrackChannels = [this, &CurrentTime, &HandledSections](const UMovieSceneControlRigParameterTrack* Track)
	{
		check(Track);		
		for (UMovieSceneSection* MovieSection : Track->GetAllSections())
		{
			UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(MovieSection);
			if (Section && Section->IsActive() && Section->GetRange().Contains(CurrentTime.Time.GetFrame()) && !HandledSections.Contains(Section))
			{
				HandledSections.Add(Section);				
				UControlRig* ControlRig = Track->GetControlRig();
				TArray<FRigControlElement*> CurrentControls;
				ControlRig->GetControlsInOrder(CurrentControls);

				FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
				TArrayView<FMovieSceneFloatChannel*> Channels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
				//reuse these arrays
				TArray<FFrameNumber> KeyTimes;
				TArray<FKeyHandle> Handles;
				for (const FRigControlElement* ControlElement : CurrentControls)
				{
					if (ControlRig->GetHierarchy()->IsAnimatable(ControlElement) &&  ControlRig->IsControlSelected(ControlElement->GetFName()))
					{
						FAnimSliderObjectSelection::FObjectChannels ObjectChannels;
						ObjectChannels.Section = Section;
						KeyTimes.SetNum(0);
						Handles.SetNum(0);
						const FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetFName());
						if (pChannelIndex == nullptr)
						{
							continue;
						}

						const int32 NumChannels = [&ControlElement]()
						{
							switch (ControlElement->Settings.ControlType)
							{
								case ERigControlType::Float:
								case ERigControlType::ScaleFloat:
								{
									return 1;
								}
								case ERigControlType::Vector2D:
								{
									return 2;
								}
								case ERigControlType::Position:
								case ERigControlType::Scale:
								case ERigControlType::Rotator:
								{
									return 3;
								}
								case ERigControlType::TransformNoScale:
								{
									return 6;
								}
								case ERigControlType::Transform:
								case ERigControlType::EulerTransform:
								{
									return 9;
								}
							default:
									return 0;
							}
						}();
					
						int32 BoundIndex = 0;
						int32 NumValidChannels = 0;
						ObjectChannels.KeyBounds.SetNum(NumChannels);
						for (int32 ChannelIdx = pChannelIndex->ChannelIndex; ChannelIdx < (pChannelIndex->ChannelIndex + NumChannels); ++ChannelIdx)
						{
							FMovieSceneFloatChannel* Channel = Channels[ChannelIdx];
							SetupChannel(CurrentTime.Time.GetFrame(), KeyTimes, Handles, Channel, nullptr,
							ObjectChannels.KeyBounds[BoundIndex]);
							if (ObjectChannels.KeyBounds[BoundIndex].bValid)
							{
								++NumValidChannels;
							}
							++BoundIndex;
						}
						if (NumValidChannels > 0)
						{
							ChannelsArray.Add(ObjectChannels);
						}
					}
				}
			}
		}
	};
	
	// Handle MovieScene bindings
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		if (SelectedGuids.Num() > 0 && SelectedGuids.Contains(Binding.GetObjectGuid()))
		{
			const TArray<UMovieSceneTrack*>& Tracks = Binding.GetTracks();
			for (const UMovieSceneTrack* Track : Tracks)
			{
				if (Track && Track->IsA<UMovieSceneControlRigParameterTrack>() == false)
				{
					const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
					for (UMovieSceneSection* Section : Sections)
					{
						if (Section && Section->IsActive())
						{
							FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
							TArrayView<FMovieSceneFloatChannel*> FloatChannels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
							TArrayView<FMovieSceneDoubleChannel*> DoubleChannels = ChannelProxy.GetChannels<FMovieSceneDoubleChannel>();

							//reuse these arrays
							TArray<FFrameNumber> KeyTimes;
							TArray<FKeyHandle> Handles;
							FAnimSliderObjectSelection::FObjectChannels ObjectChannels;
							ObjectChannels.Section = Section;
							KeyTimes.SetNum(0);
							Handles.SetNum(0);
							const int32 NumFloatChannels = FloatChannels.Num();
							const int32 NumDoubleChannels = DoubleChannels.Num();

							ObjectChannels.KeyBounds.SetNum(NumFloatChannels + NumDoubleChannels);
							int32 NumValidChannels = 0;
							for (int32 ChannelIdx = 0;ChannelIdx < NumFloatChannels; ++ChannelIdx)
							{
								FMovieSceneFloatChannel* FloatChannel = FloatChannels[ChannelIdx];
								SetupChannel(CurrentTime.Time.GetFrame(), KeyTimes, Handles, FloatChannel, nullptr,
									ObjectChannels.KeyBounds[ChannelIdx]);
								if (ObjectChannels.KeyBounds[ChannelIdx].bValid)
								{
									++NumValidChannels;
								}
							}
							for (int32 ChannelIdx = 0; ChannelIdx < NumDoubleChannels; ++ChannelIdx)
							{
								FMovieSceneDoubleChannel* DoubleChannel = DoubleChannels[ChannelIdx];
								SetupChannel(CurrentTime.Time.GetFrame(), KeyTimes, Handles, nullptr, DoubleChannel,
									ObjectChannels.KeyBounds[ChannelIdx]);
								if (ObjectChannels.KeyBounds[ChannelIdx].bValid)
								{
									++NumValidChannels;
								}
							}
							if (NumValidChannels > 0)
							{
								ChannelsArray.Add(ObjectChannels);
							}
						}
					}
				}
			}
		}
		const UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig() && SelectedControlRigs.Contains(Track->GetControlRig()))
		{
			SetupControlRigTrackChannels(Track);
		}
	}
	
	// Handle movie tracks in general (for non-binding, USkeleton, ControlRig tracks)
	for (const UMovieSceneTrack* MovieTrack : MovieScene->GetTracks())
	{
		if (const UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieTrack))
		{
			if (Track && Track->GetControlRig() && SelectedControlRigs.Contains(Track->GetControlRig()))
			{
				SetupControlRigTrackChannels(Track);
			}
		}
	}
	
	return ChannelsArray.Num() > 0;
}

/*
*
*  FBaseAnimSlider
* 
*/

bool FBaseAnimSlider::Setup(TWeakPtr<ISequencer>& InSequencer, TWeakPtr<FControlRigEditMode>& InEditMode)
{
	KeySelection.KeyMap.Reset();
	ObjectSelection.ChannelsArray.Reset();
	if (KeySelection.Setup(InSequencer) == false)
	{
		return ObjectSelection.Setup(InSequencer, InEditMode);
	}
	return true;
}

/*
*
*  FBasicBlendSlider
*
*/

bool FBasicBlendSlider::Blend(TWeakPtr<ISequencer>& InSequencer, const double BlendValue)
{
	if (InSequencer.IsValid() == false)
	{
		return false;
	}
	const ISequencer* Sequencer = InSequencer.Pin().Get();

	bool bDidBlend = false;

	if (KeySelection.KeyMap.Num() > 0)
	{
		TArray<FKeyHandle> KeyHandles;
		TArray<FKeyPosition> KeyPositions;
		for (TPair<FCurveModelID, FAnimSliderKeySelection::FContiguousKeysArray>& KeysArray : KeySelection.KeyMap)
		{
			if (FCurveModel* Curve = KeySelection.CurveEditor->FindCurve(KeysArray.Key))
			{
				Curve->Modify();
				for (FAnimSliderKeySelection::FContiguousKeys& Keys : KeysArray.Value.KeysArray)
				{
					const int32 PreviousIndex = Keys.PreviousIndex != INDEX_NONE ? Keys.PreviousIndex : Keys.Indices[0];
					const double PreviousTime = KeysArray.Value.AllKeyPositions[PreviousIndex].InputValue;
					const double PreviousValue = KeysArray.Value.AllKeyPositions[PreviousIndex].OutputValue;
					const int32 NextIndex = Keys.NextIndex != INDEX_NONE ? Keys.NextIndex : Keys.Indices[Keys.Indices.Num() - 1];
					const double NextTime = KeysArray.Value.AllKeyPositions[NextIndex].InputValue;
					const double NextValue = KeysArray.Value.AllKeyPositions[NextIndex].OutputValue;

					const int32 NumIndices = Keys.Indices.Num();
					KeyHandles.SetNum(NumIndices);
					KeyPositions.SetNum(NumIndices);
					const double FirstValue = KeysArray.Value.AllKeyPositions[Keys.Indices[0]].OutputValue;
					const double LastValue = KeysArray.Value.AllKeyPositions[Keys.Indices[Keys.Indices.Num() -1]].OutputValue;
					FBlendStruct BlendStruct(KeysArray.Value.AllKeyPositions, Keys.Indices);
					int32 CurrentIndex = 0;
					for (int32 Index = 0; Index < NumIndices; ++Index)
					{
						const int32& KeyIndex = Keys.Indices[Index];
						KeyHandles[Index] = KeysArray.Value.AllKeyHandles[KeyIndex];
						const double CurrentValue = KeysArray.Value.AllKeyPositions[KeyIndex].OutputValue;
						const double CurrentTime = KeysArray.Value.AllKeyPositions[KeyIndex].InputValue;
						BlendStruct.SetValues(PreviousTime, PreviousValue, CurrentTime, CurrentValue,
							NextTime, NextValue, BlendValue, FirstValue, LastValue, CurrentIndex);
						++CurrentIndex;
						double NewValue = DoBlend(BlendStruct);
						KeyPositions[Index] = FKeyPosition(KeysArray.Value.AllKeyPositions[KeyIndex].InputValue, NewValue);
					}
					Curve->SetKeyPositions(KeyHandles, KeyPositions);
					bDidBlend = true;
				}
			}
		}
		return bDidBlend;
	}

	const FFrameTime FrameTime = Sequencer->GetLocalTime().Time;
	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

	if(ObjectSelection.ChannelsArray.Num() > 0)
	{ 
		//empty key array for the blend struct
		const TArray<FKeyPosition> AllKeyPositions;
		const TArray <int32> Indices;
		const int32 CurrentIndex = -1;
		FBlendStruct BlendStruct(AllKeyPositions, Indices);

		for (const FAnimSliderObjectSelection::FObjectChannels& ObjectChannels : ObjectSelection.ChannelsArray)
		{
			if (ObjectChannels.Section)
			{
				ObjectChannels.Section->Modify();
			}
			for (int32 Index = 0; Index < ObjectChannels.KeyBounds.Num(); ++Index)
			{
				if (ObjectChannels.KeyBounds[Index].bValid)
				{
					const double PreviousValue = ObjectChannels.KeyBounds[Index].PreviousValue;
					const double PreviousTime = TickResolution.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].PreviousFrame));
					const double NextValue = ObjectChannels.KeyBounds[Index].NextValue;
					const double NextTime = TickResolution.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].NextFrame));
					const double CurrentValue = ObjectChannels.KeyBounds[Index].CurrentValue;
					const double CurrentTime = TickResolution.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].CurrentFrame));
					BlendStruct.SetValues(PreviousTime, PreviousValue, CurrentTime, CurrentValue,
						NextTime, NextValue, BlendValue, CurrentValue, CurrentValue, CurrentIndex);
					const double NewValue = DoBlend(BlendStruct);
					using namespace UE::MovieScene;
					if (ObjectChannels.KeyBounds[Index].FloatChannel)
					{
						const EMovieSceneKeyInterpolation KeyInterpolation = GetInterpolationMode(ObjectChannels.KeyBounds[Index].FloatChannel, FrameTime.GetFrame(), Sequencer->GetKeyInterpolation());
						AddKeyToChannel(ObjectChannels.KeyBounds[Index].FloatChannel, FrameTime.GetFrame(), (float)NewValue, KeyInterpolation);
					}
					else if (ObjectChannels.KeyBounds[Index].DoubleChannel)
					{
						const EMovieSceneKeyInterpolation KeyInterpolation = GetInterpolationMode(ObjectChannels.KeyBounds[Index].DoubleChannel, FrameTime.GetFrame(), Sequencer->GetKeyInterpolation());
						AddKeyToChannel(ObjectChannels.KeyBounds[Index].DoubleChannel, FrameTime.GetFrame(), NewValue, KeyInterpolation);
					}
					bDidBlend = true;
				}
			}
		}
	}
	return bDidBlend;
}

/*
*
*  FControlsToTween
*
*/

double FControlsToTween::DoBlend(const FBlendStruct& BlendStruct)
{
	//clasic tween will move all to same location, not based on current time at all just blend and values
	const double NormalizedBlendValue = (BlendStruct.BlendValue + 1.0f) * 0.5f;
	const double Value = BlendStruct.PreviousValue + (BlendStruct.NextValue - BlendStruct.PreviousValue) * (NormalizedBlendValue);
	return Value;
}

FText FControlsToTween::GetText() const
{
	return LOCTEXT("TW", "TW");
}

FText FControlsToTween::GetTooltipText() const
{
	return LOCTEXT("TweenControllerTooltip", "Tween between the next and previous keys");
}

bool FControlsToTween::Setup(const TArray<UControlRig*>& SelectedControlRigs, TWeakPtr<ISequencer>& InSequencer)
{
	return ObjectSelection.Setup(SelectedControlRigs, InSequencer);
}

bool FControlsToTween::Setup(TWeakPtr<ISequencer>& InSequencer, TWeakPtr<FControlRigEditMode>& InEditMode)
{
	return FBaseAnimSlider::Setup(InSequencer, InEditMode);
}

/*
*
*  PushPull Slider
*
*/


double FPushPullSlider::DoBlend(const FBlendStruct& BlendStruct)
{
	if (FMath::IsNearlyEqual(BlendStruct.NextTime, BlendStruct.PreviousTime))
	{
		return BlendStruct.CurrentValue;
	}
	const double T = (BlendStruct.CurrentTime - BlendStruct.PreviousTime) / (BlendStruct.NextTime - BlendStruct.PreviousTime);
	const double ValueAtT = BlendStruct.PreviousValue + T * (BlendStruct.NextValue - BlendStruct.PreviousValue);
	double NewValue;
	if (BlendStruct.BlendValue < 0.0)
	{
		NewValue = BlendStruct.CurrentValue + (-1.0 * BlendStruct.BlendValue) * (ValueAtT - BlendStruct.CurrentValue);
	}
	else
	{
		const double AmplifyValueAtT = BlendStruct.CurrentValue + (BlendStruct.CurrentValue - ValueAtT);
		NewValue = BlendStruct.CurrentValue + BlendStruct.BlendValue * (AmplifyValueAtT - BlendStruct.CurrentValue);
	}
	return NewValue;
}

FText FPushPullSlider::GetText() const
{
	return LOCTEXT("PP", "PP");
}

FText FPushPullSlider::GetTooltipText() const
{
	return LOCTEXT("PushPullTooltip", "Push or pull the values to the interpolation between the previous and next keys");
}


/*
*
*  Blend To Neighbors Slider
*
*/

double FBlendNeighborSlider::DoBlend(const FBlendStruct& BlendStruct)
{
	double NewValue = BlendStruct.CurrentValue;
	if (BlendStruct.BlendValue < 0.0)
	{
		NewValue = BlendStruct.CurrentValue + (-1.0 * BlendStruct.BlendValue) * (BlendStruct.PreviousValue - BlendStruct.CurrentValue);
	}
	else
	{
		NewValue = BlendStruct.CurrentValue + BlendStruct.BlendValue * (BlendStruct.NextValue - BlendStruct.CurrentValue);
	}
	return NewValue;
}

FText FBlendNeighborSlider::GetText() const
{
	return LOCTEXT("BN", "BN");
}

FText FBlendNeighborSlider::GetTooltipText() const
{
	return LOCTEXT("BlendToNeighborsSliderTooltip", "Blend to the next or previous values for selected keys or objects");
}

/*
*
*  Blend Relative Slider
*
*/

double FBlendRelativeSlider::DoBlend(const FBlendStruct& BlendStruct)
{
	double NewValue = BlendStruct.CurrentValue;
	if (BlendStruct.BlendValue < 0.0)
	{
		NewValue = BlendStruct.CurrentValue + (-1.0 * BlendStruct.BlendValue) * (BlendStruct.PreviousValue - BlendStruct.FirstValue);
	}
	else
	{
		NewValue = BlendStruct.CurrentValue + BlendStruct.BlendValue * (BlendStruct.NextValue - BlendStruct.LastValue);
	}
	return NewValue;
}

FText FBlendRelativeSlider::GetText() const
{
	return LOCTEXT("BR", "BR");
}

FText FBlendRelativeSlider::GetTooltipText() const
{
	return LOCTEXT("BlendRelativeSliderTooltip", "Blend relative to the next or previous value for selected keys or objects");
}

/*
*
*  Blend To Ease Slider
*
*/
namespace BlendToEase
{ 
static float ExpIn(float InTime)
{
	return FMath::Pow(2, 10 * (InTime - 1.f));
}
static float ExpOut(float InTime)
{
	return 1.f - ExpIn(1.f - InTime);
}

static float SCurve(float X, float Slope, float Width, float Height, float XShift, float YShift)
{
	if (X > (XShift + Width))
	{
		return(Height + YShift);
	}
	else if (X < XShift)
	{
		return YShift;
	}

	float Val = Height * (FMath::Pow((X - XShift), Slope) / (FMath::Pow(X - XShift, Slope) + FMath::Pow((Width - (X - XShift)), Slope))) + YShift;
	return Val;
}
}

double FBlendToEaseSlider::DoBlend(const FBlendStruct& BlendStruct)
{
	if (FMath::IsNearlyEqual(BlendStruct.NextTime, BlendStruct.PreviousTime))
	{
		return BlendStruct.CurrentValue;
	}
	const double Source = BlendStruct.CurrentValue;
	const double FullTimeDiff = BlendStruct.NextTime - BlendStruct.PreviousTime;
	const double AbsValue = FMath::Abs(BlendStruct.BlendValue);
	const double X = BlendStruct.CurrentTime - BlendStruct.PreviousTime;
	const double Ratio = X / FullTimeDiff;
	double Shift = 0.0, Delta = 0.0, Base = 0.0;
	if (BlendStruct.BlendValue > 0)
	{
		Shift = -1.0;
		Delta = BlendStruct.NextValue - Source;
		Base = Source;
	}
	else
	{
		Shift = 0.0;
		Delta = Source - BlendStruct.PreviousValue;
		Base = BlendStruct.PreviousValue;
	}
	const double Slope = 5.0 * AbsValue;
	const double EaseY = BlendToEase::SCurve(Ratio, Slope, 2.0, 2.0, Shift, Shift);
	const double NewValue = Base + (Delta * EaseY);
	
	return NewValue;
}

FText FBlendToEaseSlider::GetText() const
{
	return LOCTEXT("BE", "BE");
}

FText FBlendToEaseSlider::GetTooltipText() const
{
	return LOCTEXT("BlendToEaseTooltip", "Blend with an ease falloff to the next or previous value for selected keys or objects");
}

/*
*
* Smooth/Rough Slider
*
*/

double FSmoothRoughSlider::DoBlend(const FBlendStruct& BlendStruct)
{
	double PrevVal = BlendStruct.CurrentIndex > 0 ? BlendStruct.AllKeyPositions[BlendStruct.Indices[BlendStruct.CurrentIndex - 1]].OutputValue : BlendStruct.PreviousValue;
	double CurVal = BlendStruct.CurrentValue;
	double NextVal = BlendStruct.CurrentIndex < BlendStruct.Indices.Num() - 1 ? BlendStruct.AllKeyPositions[BlendStruct.Indices[BlendStruct.CurrentIndex + 1]].OutputValue : BlendStruct.NextValue;
	double NewValue = (PrevVal * 0.25 + CurVal * 0.5 + NextVal * 0.25);
	if (BlendStruct.BlendValue < 0.0)
	{
		NewValue = BlendStruct.CurrentValue + (-1.0 * BlendStruct.BlendValue) * (NewValue - BlendStruct.CurrentValue);
	}
	else
	{
		NewValue = BlendStruct.CurrentValue + BlendStruct.BlendValue * (BlendStruct.CurrentValue - NewValue);
	}
	return NewValue;
}

FText FSmoothRoughSlider::GetText() const
{
	return LOCTEXT("SR", "SR");
}

FText FSmoothRoughSlider::GetTooltipText() const
{
	return LOCTEXT("SmoothRoughTooltip", "Make selected keys smooth or rough");
}

#undef LOCTEXT_NAMESPACE

