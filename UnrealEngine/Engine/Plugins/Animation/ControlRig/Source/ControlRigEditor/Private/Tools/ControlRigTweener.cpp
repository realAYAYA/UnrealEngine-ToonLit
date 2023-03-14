// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ControlRigTweener.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "ControlRig.h"
#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModels/EditorViewModel.h"
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
	int32 Index = Algo::LowerBoundBy(Keys, Time, [](const FKeyPosition& Value) { return Value.InputValue; });

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

bool FAnimSliderKeySelection::Setup(TWeakPtr<ISequencer>& InSequencer)
{
	using namespace UE::Sequencer;

	KeyMap.Reset();
	if (InSequencer.IsValid())
	{
		TSharedPtr<FEditorViewModel> SequencerViewModel = InSequencer.Pin()->GetViewModel();
		FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>();
		check(CurveEditorExtension);
		CurveEditor = CurveEditorExtension->GetCurveEditor();
		GetMapOfContiguousKeys();
	}
	return (KeyMap.Num() > 0);
}

bool FAnimSliderObjectSelection::Setup(TWeakPtr<ISequencer>& InSequencer)
{
	ChannelsArray.Reset();
	TArray<UControlRig*> ControlRigs = GetControlRigs();
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
		FCurveModel* Curve = CurveEditor->FindCurve(Pair.Key);

		if (Curve)
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
TArray<UControlRig*> FAnimSliderObjectSelection::GetControlRigs()
{
	TArray<UControlRig*> ControlRigs;
	if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
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
			FFrameNumber FrameNumber = KeyTimes[Index];
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

	FQualifiedFrameTime CurrentTime = Sequencer->GetLocalTime();

	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	//get selected controls and objects
	TArray<FGuid> SelectedGuids;
	Sequencer->GetSelectedObjects(SelectedGuids);

	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		if (SelectedGuids.Num() > 0 && SelectedGuids.Contains(Binding.GetObjectGuid()))
		{
			const TArray<UMovieSceneTrack*>& Tracks = Binding.GetTracks();
			for (UMovieSceneTrack* Track : Tracks)
			{
				if (Track && Track->IsA<UMovieSceneControlRigParameterTrack>() == false)
				{
					const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
					for (UMovieSceneSection* Section : Sections)
					{
						if (Section && Section->IsActive())
						{
							FMovieSceneChannelProxy& Proxy = Section->GetChannelProxy();

							const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();

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
							int ChannelIdx = 0;
							for (ChannelIdx = 0;ChannelIdx < NumFloatChannels; ++ChannelIdx)
							{
								FMovieSceneFloatChannel* FloatChannel = FloatChannels[ChannelIdx];
								SetupChannel(CurrentTime.Time.GetFrame(), KeyTimes, Handles, FloatChannel, nullptr,
									ObjectChannels.KeyBounds[ChannelIdx]);
								if (ObjectChannels.KeyBounds[ChannelIdx].bValid)
								{
									++NumValidChannels;
								}
							}
							for (ChannelIdx = 0; ChannelIdx < NumDoubleChannels; ++ChannelIdx)
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
		UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (Track && Track->GetControlRig() && SelectedControlRigs.Contains(Track->GetControlRig()))
		{
			for (UMovieSceneSection* MovieSection : Track->GetAllSections())
			{
				UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(MovieSection);
				if (Section && Section->IsActive() && Section->GetRange().Contains(CurrentTime.Time.GetFrame()))
				{
					UControlRig* ControlRig = Track->GetControlRig();
					TArray<FRigControlElement*> CurrentControls;
					ControlRig->GetControlsInOrder(CurrentControls);
					URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
					FMovieSceneChannelProxy& Proxy = Section->GetChannelProxy();

					const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();

					FMovieSceneChannelProxy& ChannelProxy = Section->GetChannelProxy();
					TArrayView<FMovieSceneFloatChannel*> Channels = ChannelProxy.GetChannels<FMovieSceneFloatChannel>();
					//reuse these arrays
					TArray<FFrameNumber> KeyTimes;
					TArray<FKeyHandle> Handles;
					for (FRigControlElement* ControlElement : CurrentControls)
					{
						if (ControlRig->GetHierarchy()->IsAnimatable(ControlElement) &&  ControlRig->IsControlSelected(ControlElement->GetName()))
						{
							FAnimSliderObjectSelection::FObjectChannels ObjectChannels;
							ObjectChannels.Section = Section;
							KeyTimes.SetNum(0);
							Handles.SetNum(0);
							FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlElement->GetName());
							if (pChannelIndex == nullptr)
							{
								continue;
							}
							int NumChannels = 0;
							switch (ControlElement->Settings.ControlType)
							{
								case ERigControlType::Float:
								{
									NumChannels = 1;
								}
								case ERigControlType::Vector2D:
								{
									NumChannels = 2;
									break;
								}
								case ERigControlType::Position:
								case ERigControlType::Scale:
								case ERigControlType::Rotator:
								{
									NumChannels = 3;
									break;
								}
								case ERigControlType::TransformNoScale:
								{
									NumChannels = 6;
									break;
								}
								case ERigControlType::Transform:
								case ERigControlType::EulerTransform:
								{
									NumChannels = 9;
									break;
								}
							}
							int32 BoundIndex = 0;
							int32 NumValidChannels = 0;
							ObjectChannels.KeyBounds.SetNum(NumChannels);
							for (int ChannelIdx = pChannelIndex->ChannelIndex; ChannelIdx < (pChannelIndex->ChannelIndex + NumChannels); ++ChannelIdx)
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
		}
	}
	return ChannelsArray.Num() > 0;
}

/*
*
*  FBaseAnimSlider
* 
*/

bool FBaseAnimSlider::Setup(TWeakPtr<ISequencer>& InSequencer)
{
	KeySelection.KeyMap.Reset();
	ObjectSelection.ChannelsArray.Reset();
	if (KeySelection.Setup(InSequencer) == false)
	{
		return ObjectSelection.Setup(InSequencer);
	}
	else
	{
		return true;
	}
	return false;
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
	ISequencer* Sequencer = InSequencer.Pin().Get();

	bool bDidBlend = false;

	if (KeySelection.KeyMap.Num() > 0)
	{
		TArray<FKeyHandle> KeyHandles;
		TArray<FKeyPosition> KeyPositions;
		for (TPair<FCurveModelID, FAnimSliderKeySelection::FContiguousKeysArray>& KeysArray : KeySelection.KeyMap)
		{
			FCurveModel* Curve = KeySelection.CurveEditor->FindCurve(KeysArray.Key);
			if (Curve)
			{
				Curve->Modify();
				for (FAnimSliderKeySelection::FContiguousKeys& Keys : KeysArray.Value.KeysArray)
				{
					const int32 PreviousIndex = Keys.PreviousIndex != INDEX_NONE ? Keys.PreviousIndex : Keys.Indices[0];
					const double PreviousTime = KeysArray.Value.AllKeyPositions[PreviousIndex].InputValue;
					const double  PreviousValue = KeysArray.Value.AllKeyPositions[PreviousIndex].OutputValue;
					const int32 NextIndex = Keys.NextIndex != INDEX_NONE ? Keys.NextIndex : Keys.Indices[Keys.Indices.Num() - 1];
					const double NextTime = KeysArray.Value.AllKeyPositions[NextIndex].InputValue;
					const double  NextValue = KeysArray.Value.AllKeyPositions[NextIndex].OutputValue;

					KeyHandles.Reset();
					KeyPositions.Reset();
					for (int32 Index : Keys.Indices)
					{
						KeyHandles.Add(KeysArray.Value.AllKeyHandles[Index]);
						const double CurrentValue = KeysArray.Value.AllKeyPositions[Index].OutputValue;
						const double CurrentTime = KeysArray.Value.AllKeyPositions[Index].InputValue;

						double NewValue = DoBlend(PreviousTime,PreviousValue, CurrentTime, CurrentValue,
							NextTime,  NextValue, BlendValue);

						 KeyPositions.Add(FKeyPosition(KeysArray.Value.AllKeyPositions[Index].InputValue, NewValue));
					}
					Curve->SetKeyPositions(KeyHandles, KeyPositions);
					bDidBlend = true;
				}
			}
		}
		return bDidBlend;
	}

	FFrameTime  FrameTime = Sequencer->GetLocalTime().Time;
	FFrameRate TickResoultion = Sequencer->GetFocusedTickResolution();

	if(ObjectSelection.ChannelsArray.Num() > 0)
	{ 
		for (const FAnimSliderObjectSelection::FObjectChannels& ObjectChannels : ObjectSelection.ChannelsArray)
		{
			if (ObjectChannels.Section)
			{
				ObjectChannels.Section->Modify();
			}
			for (int Index = 0; Index < ObjectChannels.KeyBounds.Num(); ++Index)
			{
				if (ObjectChannels.KeyBounds[Index].bValid)
				{
					double PreviousValue = ObjectChannels.KeyBounds[Index].PreviousValue;
					double PreviousTime = TickResoultion.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].PreviousFrame));
					double NextValue = ObjectChannels.KeyBounds[Index].NextValue;
					double NextTime = TickResoultion.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].NextFrame));
					double CurrentValue = ObjectChannels.KeyBounds[Index].CurrentValue;
					double CurrentTime = TickResoultion.AsSeconds(FFrameTime(ObjectChannels.KeyBounds[Index].CurrentFrame));
					double NewValue = DoBlend(PreviousTime, PreviousValue, CurrentTime, CurrentValue,
						NextTime, NextValue, BlendValue);
					if (ObjectChannels.KeyBounds[Index].FloatChannel)
					{
						AddKeyToChannel(ObjectChannels.KeyBounds[Index].FloatChannel, FrameTime.GetFrame(), (float)NewValue, Sequencer->GetKeyInterpolation());
					}
					else if (ObjectChannels.KeyBounds[Index].DoubleChannel)
					{
						AddKeyToChannel(ObjectChannels.KeyBounds[Index].DoubleChannel, FrameTime.GetFrame(), NewValue, Sequencer->GetKeyInterpolation());
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

double FControlsToTween::DoBlend(const double PreviousTime, const double PreviousValue, const double CurrentTime, const double CurrentValue,
	const double NextTime, const double NextValue, const double BlendValue)
{
	//clasic tween will move all to same location, not based on current time at all just blend and values
	double NormalizedBlendValue = (BlendValue + 1.0f) * 0.5f;
	double Value = PreviousValue + (NextValue - PreviousValue) * (NormalizedBlendValue);
	return Value;
}

FText FControlsToTween::GetText()
{
	return LOCTEXT("TW", "TW");
}

FText FControlsToTween::GetTooltipText()
{
	return LOCTEXT("TweenControllerTooltip", "Tween between the next and previous keys");
}

bool FControlsToTween::Setup(const TArray<UControlRig*>& SelectedControlRigs, TWeakPtr<ISequencer>& InSequencer)
{
	return ObjectSelection.Setup(SelectedControlRigs, InSequencer);
}

bool FControlsToTween::Setup(TWeakPtr<ISequencer>& InSequencer)
{
	return FBaseAnimSlider::Setup(InSequencer);
}

/*
*
*  PushPull Slider
*
*/


double FPushPullSlider::DoBlend(const double PreviousTime, const double PreviousValue, const double CurrentTime, const double CurrentValue,
	const double NextTime, const double NextValue, const double BlendValue)
{
	const double T = (CurrentTime - PreviousTime) / (NextTime - PreviousTime);
	const double ValueAtT = PreviousValue + T * (NextValue - PreviousValue);
	double NewValue = 0.0;
	if (BlendValue < 0.0)
	{
		NewValue = CurrentValue + (-1.0 * BlendValue) * (ValueAtT - CurrentValue);
	}
	else
	{
		const double AmplifyValueAtT = CurrentValue + (CurrentValue - ValueAtT);
		NewValue = CurrentValue + BlendValue * (AmplifyValueAtT - CurrentValue);
	}
	return NewValue;
}

FText FPushPullSlider::GetText()
{
	return LOCTEXT("PP", "PP");
}

FText FPushPullSlider::GetTooltipText()
{
	return LOCTEXT("PushPullTooltip", "Push or pull the values to the interpolation between the previous and next keys");
}


/*
*
*  Blend To Neighbors Slider
*
*/

double FBlendNeighborSlider::DoBlend(const double PreviousTime, const double PreviousValue, const double CurrentTime, const double CurrentValue,
	const double NextTime, const double NextValue, const double BlendValue)
{
	double NewValue;
	if (BlendValue < 0.0)
	{
		NewValue = CurrentValue + (-1.0 * BlendValue) * (PreviousValue - CurrentValue);
	}
	else
	{
		NewValue = CurrentValue + BlendValue * (NextValue - CurrentValue);
	}
	return NewValue;
}

FText FBlendNeighborSlider::GetText()
{
	return LOCTEXT("BN", "BN");
}

FText FBlendNeighborSlider::GetTooltipText()
{
	return LOCTEXT("BlendToNeighborsSliderTooltip", "Blend to the next or previous values for selected keys or objects");
}

#undef LOCTEXT_NAMESPACE

