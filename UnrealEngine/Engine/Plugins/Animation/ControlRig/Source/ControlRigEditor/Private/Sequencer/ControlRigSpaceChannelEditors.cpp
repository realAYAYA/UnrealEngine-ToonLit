// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigSpaceChannelEditors.h"
#include "ConstraintChannelHelper.inl"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceEditor.h"
#include "MovieSceneEventUtils.h"
#include "Channels/MovieSceneChannelHandle.h"
#include "Sections/MovieSceneEventSectionBase.h"
#include "ISequencerChannelInterface.h"
#include "Widgets/SNullWidget.h"
#include "IKeyArea.h"
#include "ISequencer.h"
#include "SequencerSettings.h"
#include "MovieSceneCommonHelpers.h"
#include "GameFramework/Actor.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "MVVM/Views/KeyDrawParams.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannelEditorData.h"
#include "Channels/DoubleChannelCurveModel.h"
#include "Channels/FloatChannelCurveModel.h"
#include "Channels/IntegerChannelCurveModel.h"
#include "Channels/BoolChannelCurveModel.h"
#include "PropertyCustomizationHelpers.h"
#include "MovieSceneObjectBindingIDCustomization.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "SSocketChooser.h"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "ISequencerModule.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "Tools/ControlRigSnapper.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "MovieScene.h"
#include "Sequencer/MovieSceneControlRigSpaceChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneObjectBindingID.h"
#include "ControlRig.h"
#include "IControlRigObjectBinding.h"
#include "ControlRigSpaceChannelCurveModel.h"
#include "ScopedTransaction.h"
#include "IControlRigEditorModule.h"
#include "TimerManager.h"
#include "CommonMovieSceneTools.h"
#include "SequencerSectionPainter.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "CurveEditorSettings.h"
#include "TimeToPixel.h"

#define LOCTEXT_NAMESPACE "ControlRigEditMode"

//flag used when we add or delete a space channel. When we do so we handle the compensation in those functions since we need to 
//compensate based upon the previous spaces, not the new space. So we set that this flag to be true and then
//do not compensate via the FControlRigParameterTrackEditor setting a key.
static bool bDoNotCompensate = false;

static FKeyHandle SequencerOpenSpaceSwitchDialog(UControlRig* ControlRig, TArray<FRigElementKey> SelectedControls,ISequencer* Sequencer, FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneSection* SectionToKey, FFrameNumber Time)
{
	FKeyHandle Handle = FKeyHandle::Invalid();
	if (ControlRig == nullptr || Sequencer == nullptr)
	{
		return Handle;
	}

	TSharedRef<SRigSpacePickerWidget> PickerWidget =
	SNew(SRigSpacePickerWidget)
	.Hierarchy(ControlRig->GetHierarchy())
	.Controls(SelectedControls)
	.Title(LOCTEXT("PickSpace", "Pick Space"))
	.AllowDelete(false)
	.AllowReorder(false)
	.AllowAdd(false)
	.GetControlCustomization_Lambda([ControlRig](URigHierarchy*, const FRigElementKey& InControlKey)
	{
		return ControlRig->GetControlCustomization(InControlKey);
	})
	.OnActiveSpaceChanged_Lambda([&Handle,ControlRig,Sequencer,Channel,SectionToKey,Time,SelectedControls](URigHierarchy* RigHierarchy, const FRigElementKey& ControlKey, const FRigElementKey& SpaceKey)
	{
			
		Handle = FControlRigSpaceChannelHelpers::SequencerKeyControlRigSpaceChannel(ControlRig, Sequencer, Channel, SectionToKey, Time, RigHierarchy, ControlKey, SpaceKey);

	})
	.OnSpaceListChanged_Lambda([SelectedControls, ControlRig](URigHierarchy* InHierarchy, const FRigElementKey& InControlKey, const TArray<FRigElementKey>& InSpaceList)
	{
		check(SelectedControls.Contains(InControlKey));
				
		// update the settings in the control element
		if (FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InControlKey))
		{
			FScopedTransaction Transaction(LOCTEXT("ControlChangeAvailableSpaces", "Edit Available Spaces"));

			InHierarchy->Modify();

			FRigControlElementCustomization ControlCustomization = *ControlRig->GetControlCustomization(InControlKey);
			ControlCustomization.AvailableSpaces = InSpaceList;
			ControlCustomization.RemovedSpaces.Reset();

			// remember  the elements which are in the asset's available list but removed by the user
			for (const FRigElementKey& AvailableSpace : ControlElement->Settings.Customization.AvailableSpaces)
			{
				if (!ControlCustomization.AvailableSpaces.Contains(AvailableSpace))
				{
					ControlCustomization.RemovedSpaces.Add(AvailableSpace);
				}
			}

			ControlRig->SetControlCustomization(InControlKey, ControlCustomization);
			InHierarchy->Notify(ERigHierarchyNotification::ControlSettingChanged, ControlElement);
		}

	});
	// todo: implement GetAdditionalSpacesDelegate to pull spaces from sequencer

	FReply Reply = PickerWidget->OpenDialog(true);
	if (Reply.IsEventHandled())
	{
		return Handle;
	}
	return FKeyHandle::Invalid();
}

FKeyHandle AddOrUpdateKey(FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneSection* SectionToKey, FFrameNumber Time, ISequencer& Sequencer, const FGuid& InObjectBindingID, FTrackInstancePropertyBindings* PropertyBindings)
{
	UE_LOG(LogControlRigEditor, Log, TEXT("We don't support adding keys to the Space Channel via the + key. Please use the Space Channel Area in the Animation Panel"));

	FKeyHandle Handle = FKeyHandle::Invalid();

	/**
	if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
	{
		if (UControlRig* ControlRig = Section->GetControlRig())
		{
			FName ControlName = Section->FindControlNameFromSpaceChannel(Channel);
			if (ControlName != NAME_None)
			{
				if (FRigControlElement* Control = ControlRig->FindControl(ControlName))
				{
					TArray<FRigElementKey> Controls;
					FRigElementKey ControlKey = Control->GetKey();
					Controls.Add(ControlKey);
					FMovieSceneControlRigSpaceBaseKey ExistingValue, Value;
					using namespace UE::MovieScene;
					EvaluateChannel(Channel, Time, ExistingValue);
					Value = ExistingValue;
					URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
					Handle = SequencerOpenSpaceSwitchDialog(ControlRig, Controls, &Sequencer, Channel, SectionToKey, Time);
				}
			}
		}
	}
	*/
	return Handle;
}

bool CanCreateKeyEditor(const FMovieSceneControlRigSpaceChannel* Channel)
{
	return false; //mz todoo maybe change
}
TSharedRef<SWidget> CreateKeyEditor(const TMovieSceneChannelHandle<FMovieSceneControlRigSpaceChannel>& Channel, UMovieSceneSection* Section, const FGuid& InObjectBindingID, TWeakPtr<FTrackInstancePropertyBindings> PropertyBindings, TWeakPtr<ISequencer> InSequencer)
{
	return SNullWidget::NullWidget;
}



/*******************************************************************
*
* FControlRigSpaceChannelHelpers
*
**********************************************************************/


FSpaceChannelAndSection FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(UControlRig* ControlRig, FName ControlName, ISequencer* Sequencer, bool bCreateIfNeeded)
{
	FSpaceChannelAndSection SpaceChannelAndSection;
	SpaceChannelAndSection.SpaceChannel = nullptr;
	SpaceChannelAndSection.SectionToKey = nullptr;
	if (ControlRig == nullptr || Sequencer == nullptr)
	{
		return SpaceChannelAndSection;
	}
	UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!MovieScene)
	{
		return SpaceChannelAndSection;
	}
	const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
	bool bRecreateCurves = false;
	TArray<TPair<UControlRig*, FName>> ControlRigPairsToReselect;
	for (const FMovieSceneBinding& Binding : Bindings)
	{
		UMovieSceneControlRigParameterTrack* ControlRigParameterTrack = Cast<UMovieSceneControlRigParameterTrack>(MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid(), NAME_None));
		if (ControlRigParameterTrack && ControlRigParameterTrack->GetControlRig() == ControlRig)
		{
			UMovieSceneControlRigParameterSection* ActiveSection = Cast<UMovieSceneControlRigParameterSection>(ControlRigParameterTrack->GetSectionToKey());
			if (ActiveSection)
			{
				ActiveSection->Modify();
				ControlRig->Modify();
				SpaceChannelAndSection.SectionToKey = ActiveSection;
				FSpaceControlNameAndChannel* NameAndChannel = ActiveSection->GetSpaceChannel(ControlName);
				if (NameAndChannel)
				{
					SpaceChannelAndSection.SpaceChannel = &NameAndChannel->SpaceCurve;
				}
				else if (bCreateIfNeeded)
				{
					if (ControlRig->IsControlSelected(ControlName))
					{
						TPair<UControlRig*, FName> Pair;
						Pair.Key = ControlRig;
						Pair.Value = ControlName;
						ControlRigPairsToReselect.Add(Pair);
					}
					ActiveSection->AddSpaceChannel(ControlName, true /*ReconstructChannelProxy*/);
					NameAndChannel = ActiveSection->GetSpaceChannel(ControlName);
					if (NameAndChannel)
					{
						SpaceChannelAndSection.SpaceChannel = &NameAndChannel->SpaceCurve;
						bRecreateCurves = true;
					}
				}
			}
			break;
		}
	}
	if (bRecreateCurves)
	{
		Sequencer->RecreateCurveEditor(); //this will require the curve editor to get recreated so the ordering is correct
		for (TPair<UControlRig*, FName>& Pair : ControlRigPairsToReselect)
		{

			GEditor->GetTimerManager()->SetTimerForNextTick([Pair]()
				{
					Pair.Key->SelectControl(Pair.Value);
				});

		}
	}
	return SpaceChannelAndSection;
}



/*
*  Situations to handle\
*  0) New space sames as current space do nothing
*  1) First space switch, so no existing space keys, if at start frame, just create space key, if not at start frame create current(parent) at start frame then new
*  2) Creating space that was the same as the one just before the current time. In that case we need to delete the current space key and the previous(-1) transform keys
*  3) Creating different space than previous or current, at (-1) curren ttime set space key as current, and set transforms in that space, then at current time set new space and new transforms
*  4) For 1-3 we always compensate any transform keys to the new space
*/

FKeyHandle FControlRigSpaceChannelHelpers::SequencerKeyControlRigSpaceChannel(UControlRig* ControlRig, ISequencer* Sequencer, FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneSection* SectionToKey, FFrameNumber Time, URigHierarchy* RigHierarchy, const FRigElementKey& ControlKey, const FRigElementKey& SpaceKey)
{
	FKeyHandle Handle = FKeyHandle::Invalid();
	if (bDoNotCompensate == true || ControlRig == nullptr || Sequencer == nullptr || Sequencer->GetFocusedMovieSceneSequence() == nullptr || Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene() == nullptr)
	{
		return Handle;
	}

	TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);

	URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
	FMovieSceneControlRigSpaceBaseKey ExistingValue, PreviousValue;
	using namespace UE::MovieScene;
	EvaluateChannel(Channel, Time, ExistingValue);
	EvaluateChannel(Channel, Time -1, PreviousValue);

	TArray<FRigElementKey> BeforeKeys, AfterKeys;
	Channel->GetUniqueSpaceList(&BeforeKeys);

	FMovieSceneControlRigSpaceBaseKey Value = ExistingValue;

	if (SpaceKey == RigHierarchy->GetWorldSpaceReferenceKey())
	{
		Value.SpaceType = EMovieSceneControlRigSpaceType::World;
		Value.ControlRigElement = URigHierarchy::GetWorldSpaceReferenceKey();
	}
	else if (SpaceKey == RigHierarchy->GetDefaultParentKey())
	{
		Value.SpaceType = EMovieSceneControlRigSpaceType::Parent;
		Value.ControlRigElement = RigHierarchy->GetFirstParent(ControlKey);
	}
	else
	{
		FRigElementKey DefaultParent = RigHierarchy->GetFirstParent(ControlKey);
		if (DefaultParent == SpaceKey)
		{
			Value.SpaceType = EMovieSceneControlRigSpaceType::Parent;
			Value.ControlRigElement = DefaultParent;
		}
		else  //support all types
		{
			Value.SpaceType = EMovieSceneControlRigSpaceType::ControlRig;
			Value.ControlRigElement = SpaceKey;
		}
	}

	//we only key if the value is different.
	if (Value != ExistingValue)
	{
		TArray<FFrameNumber> Frames;
		Frames.Add(Time);
		
		TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = Channel->GetData();
		bool bSetPreviousKey = true;
		//find all of the times in the space after this time we now need to compensate for
		TSortedMap<FFrameNumber, FFrameNumber> ExtraFrames;
		FControlRigSpaceChannelHelpers::GetFramesInThisSpaceAfterThisTime(ControlRig, ControlKey.Name, ExistingValue,
			Channel, SectionToKey, Time, ExtraFrames);
		if (ExtraFrames.Num() > 0)
		{
			for (const TPair<FFrameNumber, FFrameNumber>& Frame : ExtraFrames)
			{
				Frames.Add(Frame.Value);
			}
		}
		SectionToKey->Modify();
		//if we have no keys need to set key for current space at start frame, unless setting key at start time, where then don't do previous compensation
		if (Channel->GetNumKeys() == 0 && Sequencer->GetFocusedMovieSceneSequence() && Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene())
		{
			FFrameNumber StartFrame = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
			if (StartFrame != Time)
			{
				FMovieSceneControlRigSpaceBaseKey Original = ExistingValue;
				ChannelInterface.AddKey(StartFrame, Forward<FMovieSceneControlRigSpaceBaseKey>(Original));
			}
			else
			{
				bSetPreviousKey = false;
			}
		}

		TArray<FTransform> ControlRigParentWorldTransforms;
		ControlRigParentWorldTransforms.SetNum(Frames.Num());
		for (FTransform& WorldTransform : ControlRigParentWorldTransforms)
		{
			WorldTransform  = FTransform::Identity;
		}
		TArray<FTransform> ControlWorldTransforms;
		FControlRigSnapper Snapper;
		Snapper.GetControlRigControlTransforms(Sequencer, ControlRig, ControlKey.Name, Frames, ControlRigParentWorldTransforms, ControlWorldTransforms);

		// store tangents to keep the animation when switching 
		TArray<FMovieSceneTangentData> Tangents;
		int32 ChannelIndex = 0, NumChannels = 0;
		UMovieSceneControlRigParameterSection* ControlRigSection = Cast<UMovieSceneControlRigParameterSection>(SectionToKey);
		if (ControlRigSection)
		{
			FChannelMapInfo* pChannelIndex = nullptr;
			FRigControlElement* ControlElement = nullptr;
			Tie(ControlElement, pChannelIndex) = GetControlAndChannelInfo(ControlRig, ControlRigSection, ControlKey.Name);

			if (pChannelIndex && ControlElement)
			{
				// get the number of float channels to treat
				NumChannels = GetNumFloatChannels(ControlElement->Settings.ControlType);
				if (NumChannels > 0)
				{
					ChannelIndex = pChannelIndex->ChannelIndex;
					EvaluateTangentAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels,ControlRigSection, Time, Tangents);
				}
			}
		}
		
		// if current Value not the same as the previous add new space key
		if (PreviousValue != Value)
		{
			SectionToKey->Modify();
			int32 ExistingIndex = ChannelInterface.FindKey(Time);
			if (ExistingIndex != INDEX_NONE)
			{
				Handle = ChannelInterface.GetHandle(ExistingIndex);
				using namespace UE::MovieScene;
				AssignValue(Channel, Handle, Forward<FMovieSceneControlRigSpaceBaseKey>(Value));
			}
			else
			{
				ExistingIndex = ChannelInterface.AddKey(Time, Forward<FMovieSceneControlRigSpaceBaseKey>(Value));
				Handle = ChannelInterface.GetHandle(ExistingIndex);
			}
		}
		else //same as previous
		{
			if (ControlRigSection)
			{
				ControlRigSection->Modify();
				TArray<FFrameNumber> OurKeyTimes;
				TArray<FKeyHandle> OurKeyHandles;
				TRange<FFrameNumber> CurrentFrameRange;
				CurrentFrameRange.SetLowerBound(TRangeBound<FFrameNumber>(Time));
				CurrentFrameRange.SetUpperBound(TRangeBound<FFrameNumber>(Time));
				ChannelInterface.GetKeys(CurrentFrameRange, &OurKeyTimes, &OurKeyHandles);
				if (OurKeyHandles.Num() > 0)
				{
					ChannelInterface.DeleteKeys(OurKeyHandles);
				}
				//now delete any extra TimeOfDelete -1
				// NOTE do we need to update the tangents here?
				FControlRigSpaceChannelHelpers::DeleteTransformKeysAtThisTime(ControlRig, ControlRigSection, ControlKey.Name, Time - 1);
				bSetPreviousKey = false; //also don't set previous
			}
		}

		//do any compensation or previous key adding
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();

		// set previous key if we're going to switch space
		if (bSetPreviousKey)
		{
			FFrameTime GlobalTime(Time -1);
			GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();
			
			FMovieSceneContext SceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
			Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(SceneContext, *Sequencer);

			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Time - 1));
			ControlRig->SetControlGlobalTransform(ControlKey.Name, ControlWorldTransforms[0], true, Context);

			// need to update the tangents here to keep the arriving animation
			if (ControlRigSection  && NumChannels > 0)
			{
				SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, ControlRigSection,GlobalTime.GetFrame(), Tangents);
			}
		}

		// effectively switch to new space
		URigHierarchy::TElementDependencyMap Dependencies = ControlRig->GetHierarchy()->GetDependenciesForVM(ControlRig->GetVM());
		ControlRig->GetHierarchy()->SwitchToParent(ControlKey, SpaceKey, false, true, Dependencies, nullptr);

		// add new keys in the new space context
		int32 FramesIndex = 0;
		for (const FFrameNumber& Frame : Frames)
		{
			FFrameTime GlobalTime(Frame);
			GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();

			FMovieSceneContext SceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
			Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(SceneContext, *Sequencer);

			ControlRig->Evaluate_AnyThread();
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			ControlRig->SetControlGlobalTransform(ControlKey.Name, ControlWorldTransforms[FramesIndex], true, Context);

			// need to update the tangents here to keep the leaving animation
			if (bSetPreviousKey && FramesIndex == 0)
			{
				if (ControlRigSection && NumChannels > 0)
				{
					SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, ControlRigSection, GlobalTime.GetFrame(), Tangents);
				}
			}

			FramesIndex++;
		}
		Channel->FindSpaceIntervals();
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);

		Channel->GetUniqueSpaceList(&AfterKeys);
		Channel->BroadcastSpaceNoLongerUsed(BeforeKeys, AfterKeys);
	}

	return Handle;

}

//get the mask of the transform keys at the specified time
static EControlRigContextChannelToKey GetCurrentTransformKeysAtThisTime(UControlRig* ControlRig, FName ControlName, UMovieSceneSection* SectionToKey, FFrameNumber Time)
{
	EControlRigContextChannelToKey ChannelToKey = EControlRigContextChannelToKey::None;
	if (ControlRig && SectionToKey)
	{
		if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
		{
			TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
			FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlName);
			if (pChannelIndex != nullptr)
			{
				int32 ChannelIndex = pChannelIndex->ChannelIndex;

				if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
				{
					FMovieSceneControlRigSpaceBaseKey Value;
					switch (ControlElement->Settings.ControlType)
					{
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Rotator:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						if (FloatChannels[ChannelIndex]->GetData().FindKey(Time) != INDEX_NONE)
						{
							if (ControlElement->Settings.ControlType == ERigControlType::Rotator)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationX;
							}
							else if (ControlElement->Settings.ControlType == ERigControlType::Scale)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleX;
							}
							else
							{
								ChannelToKey |= EControlRigContextChannelToKey::TranslationX;
							}
						}
						if (FloatChannels[ChannelIndex + 1]->GetData().FindKey(Time) != INDEX_NONE)
						{
							if (ControlElement->Settings.ControlType == ERigControlType::Rotator)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationY;
							}
							else if (ControlElement->Settings.ControlType == ERigControlType::Scale)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleY;
							}
							else
							{
								ChannelToKey |= EControlRigContextChannelToKey::TranslationY;
							}
						}
						if (FloatChannels[ChannelIndex + 2]->GetData().FindKey(Time) != INDEX_NONE)
						{
							if (ControlElement->Settings.ControlType == ERigControlType::Rotator)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationZ;
							}
							else if (ControlElement->Settings.ControlType == ERigControlType::Scale)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleZ;
							}
							else
							{
								ChannelToKey |= EControlRigContextChannelToKey::TranslationZ;
							}
						}
						if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
							ControlElement->Settings.ControlType == ERigControlType::EulerTransform ||
							ControlElement->Settings.ControlType == ERigControlType::TransformNoScale
							)
						{
							if (FloatChannels[ChannelIndex + 3]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationX;
							}
							if (FloatChannels[ChannelIndex + 4]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationY;
							}
							if (FloatChannels[ChannelIndex + 5]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::RotationZ;
							}

						}
						if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
							ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
						{
							if (FloatChannels[ChannelIndex + 6]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleX;
							}
							if (FloatChannels[ChannelIndex + 7]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleY;
							}
							if (FloatChannels[ChannelIndex + 8]->GetData().FindKey(Time) != INDEX_NONE)
							{
								ChannelToKey |= EControlRigContextChannelToKey::ScaleZ;
							}
						}
						break;

					};

					}
				}
			}
		
		}

	}
	return ChannelToKey;
}

void  FControlRigSpaceChannelHelpers::SequencerSpaceChannelKeyDeleted(UControlRig* ControlRig, ISequencer* Sequencer, FName ControlName, FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneControlRigParameterSection* SectionToKey,
	FFrameNumber TimeOfDeletion)
{
	if (bDoNotCompensate == true)
	{
		return;
	}
	FMovieSceneControlRigSpaceBaseKey ExistingValue, PreviousValue;
	using namespace UE::MovieScene;
	EvaluateChannel(Channel, TimeOfDeletion - 1, PreviousValue);
	EvaluateChannel(Channel, TimeOfDeletion, ExistingValue);
	if (ExistingValue != PreviousValue) //if they are the same no need to do anything
	{
		TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);

		//find all key frames we need to compensate
		TArray<FFrameNumber> Frames;
		Frames.Add(TimeOfDeletion);
		TSortedMap<FFrameNumber, FFrameNumber> ExtraFrames;
		FControlRigSpaceChannelHelpers::GetFramesInThisSpaceAfterThisTime(ControlRig, ControlName, ExistingValue,
			Channel, SectionToKey, TimeOfDeletion, ExtraFrames);
		if (ExtraFrames.Num() > 0)
		{
			for (const TPair<FFrameNumber, FFrameNumber>& Frame : ExtraFrames)
			{
				Frames.Add(Frame.Value);
			}
		}
		TArray<FTransform> ControlRigParentWorldTransforms;
		ControlRigParentWorldTransforms.SetNum(Frames.Num());
		for (FTransform& WorldTransform : ControlRigParentWorldTransforms)
		{
			WorldTransform = FTransform::Identity;
		}
		TArray<FTransform> ControlWorldTransforms;
		FControlRigSnapper Snapper;
		Snapper.GetControlRigControlTransforms(Sequencer, ControlRig, ControlName, Frames, ControlRigParentWorldTransforms, ControlWorldTransforms);
		FRigElementKey ControlKey;
		ControlKey.Name = ControlName;
		ControlKey.Type = ERigElementType::Control;
		URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();

		int32 FramesIndex = 0;
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();

		for (const FFrameNumber& Frame : Frames)
		{
			//evaluate sequencer
			FFrameTime GlobalTime(Frame);
			GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();

			FMovieSceneContext SceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
			Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(SceneContext, *Sequencer);
			//make sure to set rig hierarchy correct since key is not deleted yet
			switch (PreviousValue.SpaceType)
			{
			case EMovieSceneControlRigSpaceType::Parent:
				RigHierarchy->SwitchToDefaultParent(ControlKey);
				break;
			case EMovieSceneControlRigSpaceType::World:
				RigHierarchy->SwitchToWorldSpace(ControlKey);
				break;
			case EMovieSceneControlRigSpaceType::ControlRig:
				URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
				RigHierarchy->SwitchToParent(ControlKey, PreviousValue.ControlRigElement, false, true, Dependencies, nullptr);
				break;
			}
			ControlRig->Evaluate_AnyThread();
			Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
			//make sure we only add keys to those that exist since they may be getting deleted also.
			Context.KeyMask = (uint32)GetCurrentTransformKeysAtThisTime(ControlRig, ControlKey.Name, SectionToKey, Frame);
			ControlRig->SetControlGlobalTransform(ControlKey.Name, ControlWorldTransforms[FramesIndex++], true, Context);
		}
		//now delete any extra TimeOfDelete -1
		FControlRigSpaceChannelHelpers::DeleteTransformKeysAtThisTime(ControlRig, SectionToKey, ControlName, TimeOfDeletion - 1);
		
	}
}

void FControlRigSpaceChannelHelpers::DeleteTransformKeysAtThisTime(UControlRig* ControlRig, UMovieSceneControlRigParameterSection* Section, FName ControlName, FFrameNumber Time)
{
	if (Section && ControlRig)
	{
		TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
		FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlName);
		if (pChannelIndex != nullptr)
		{
			int32 ChannelIndex = pChannelIndex->ChannelIndex;

			if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
			{
				FMovieSceneControlRigSpaceBaseKey Value;
				switch (ControlElement->Settings.ControlType)
				{
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Rotator:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						int NumChannels = 0;
						if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
							ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
						{
							NumChannels = 9;
						}
						else if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
						{
							NumChannels = 6;
						}
						else //vectors
						{
							NumChannels = 3;
						}
						for (int32 Index = 0; Index < NumChannels; ++Index)
						{
							int32 KeyIndex = 0;
							for (FFrameNumber Frame : FloatChannels[ChannelIndex]->GetData().GetTimes())
							{
								if (Frame == Time)
								{
									FloatChannels[ChannelIndex]->GetData().RemoveKey(KeyIndex);
									break;
								}
								else if (Frame > Time)
								{
									break;
								}
								++KeyIndex;
							}
							++ChannelIndex;
						}
						break;
					}
					default:
						break;

				}
			}
		}
	}
}

void FControlRigSpaceChannelHelpers::GetFramesInThisSpaceAfterThisTime(
	UControlRig* ControlRig,
	FName ControlName,
	FMovieSceneControlRigSpaceBaseKey CurrentValue,
	FMovieSceneControlRigSpaceChannel* Channel,
	UMovieSceneSection* SectionToKey,
	FFrameNumber Time,
	TSortedMap<FFrameNumber,FFrameNumber>& OutMoreFrames)
{
	OutMoreFrames.Reset();
	if (ControlRig && Channel && SectionToKey)
	{
		if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
		{
			TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
			FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlName);
			if (pChannelIndex != nullptr)
			{
				int32 ChannelIndex = pChannelIndex->ChannelIndex;

				if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
				{
					FMovieSceneControlRigSpaceBaseKey Value;
					switch (ControlElement->Settings.ControlType)
					{
						case ERigControlType::Position:
						case ERigControlType::Scale:
						case ERigControlType::Rotator:
						case ERigControlType::Transform:
						case ERigControlType::TransformNoScale:
						case ERigControlType::EulerTransform:
						{
							int NumChannels = 0;
							if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
								ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
							{
								NumChannels = 9;
							}
							else if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
							{
								NumChannels = 6;
							}
							else //vectors
							{
								NumChannels = 3;
							}
							for (int32 Index = 0; Index < NumChannels; ++Index)
							{
								for (FFrameNumber Frame : FloatChannels[ChannelIndex++]->GetData().GetTimes())
								{
									if (Frame > Time)
									{
										using namespace UE::MovieScene;
										EvaluateChannel(Channel, Frame, Value);
										if (CurrentValue == Value)
										{
											if (OutMoreFrames.Find(Frame) == nullptr)
											{
												OutMoreFrames.Add(Frame, Frame);
											}
										}
										else
										{
											break;
										}
									}
								}
							}
							break;
						}

					}
				}
			}
		}
	}
}

void FControlRigSpaceChannelHelpers::SequencerBakeControlInSpace(UControlRig* ControlRig, ISequencer* Sequencer, FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneSection* SectionToKey,
	TArray<FFrameNumber> Frames, URigHierarchy* RigHierarchy, const FRigElementKey& ControlKey, FRigSpacePickerBakeSettings Settings)
{
	if (bDoNotCompensate == true)
	{
		return;
	}
	if (ControlRig && Sequencer && Channel && SectionToKey && Frames.Num() > 0 && RigHierarchy
		&& Sequencer->GetFocusedMovieSceneSequence() && Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene())
	{
		if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
		{
			TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);
			TArray<FTransform> ControlRigParentWorldTransforms;
			ControlRigParentWorldTransforms.SetNum(Frames.Num());
			for (FTransform& Transform : ControlRigParentWorldTransforms)
			{
				Transform = FTransform::Identity;
			}
			//Store transforms
			FControlRigSnapper Snapper;
			TArray<FTransform> ControlWorldTransforms;
			Snapper.GetControlRigControlTransforms(Sequencer, ControlRig, ControlKey.Name, Frames, ControlRigParentWorldTransforms, ControlWorldTransforms);

			//Find all space keys in range and delete them since it will get replaced with new space when we components.
			Section->Modify();
			FFrameNumber StartFrame = Frames[0];
			FFrameNumber EndFrame = Frames[Frames.Num() - 1];
			TArray<FFrameNumber> Keys;
			TArray < FKeyHandle> KeyHandles;
			TRange<FFrameNumber> Range(StartFrame, EndFrame);
			Channel->GetKeys(Range, &Keys, &KeyHandles);
			Channel->DeleteKeys(KeyHandles);
			//also delete any transform keys at the deleted key times and times -1.
			for (const FFrameNumber& DeleteFrame : Keys)
			{
				FControlRigSpaceChannelHelpers::DeleteTransformKeysAtThisTime(ControlRig, Section, ControlKey.Name, DeleteFrame - 1);
				FControlRigSpaceChannelHelpers::DeleteTransformKeysAtThisTime(ControlRig, Section, ControlKey.Name, DeleteFrame);
			}

			URigHierarchy* Hierarchy = ControlRig->GetHierarchy();

			//now find space at start and end see if different than the new space if so we need to compensate
			FMovieSceneControlRigSpaceBaseKey StartFrameValue, EndFrameValue;
			using namespace UE::MovieScene;
			EvaluateChannel(Channel, StartFrame, StartFrameValue);
			EvaluateChannel(Channel, EndFrame, EndFrameValue);

			FMovieSceneControlRigSpaceBaseKey Value;
			if (Settings.TargetSpace == RigHierarchy->GetWorldSpaceReferenceKey())
			{
				Value.SpaceType = EMovieSceneControlRigSpaceType::World;
			}
			else if (Settings.TargetSpace == RigHierarchy->GetDefaultParentKey())
			{
				Value.SpaceType = EMovieSceneControlRigSpaceType::Parent;
			}
			else
			{
				FRigElementKey DefaultParent = RigHierarchy->GetFirstParent(ControlKey);
				if (DefaultParent == Settings.TargetSpace)
				{
					Value.SpaceType = EMovieSceneControlRigSpaceType::Parent;
				}
				else
				{
					Value.SpaceType = EMovieSceneControlRigSpaceType::ControlRig;
					Value.ControlRigElement = Settings.TargetSpace;
				}
			}
			const bool bCompensateStart = StartFrameValue != Value;
			TRange<FFrameNumber> PlaybackRange = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
			const bool bCompensateEnd = (EndFrameValue != Value && PlaybackRange.GetUpperBoundValue() != EndFrame);

			//if compensate at the start we need to set the channel key as the new value
			if (bCompensateStart)
			{
				TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = Channel->GetData();
				ChannelInterface.AddKey(StartFrame, Forward<FMovieSceneControlRigSpaceBaseKey>(Value));

			}
			//if we compensate at the end we change the last frame to frame -1(tick), and then later set the space to the other one and 
			if (bCompensateEnd)
			{
				Frames[Frames.Num() - 1] = Frames[Frames.Num() - 1] - 1;
			}
			//now set all of the key values
			FRigControlModifiedContext Context;
			Context.SetKey = EControlRigSetKey::Always;

			FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();

			URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
			RigHierarchy->SwitchToParent(ControlKey, Settings.TargetSpace, false, true, Dependencies, nullptr);
			ControlRig->Evaluate_AnyThread();

			FMovieSceneSequenceTransform RootToLocalTransform = Sequencer->GetFocusedMovieSceneSequenceTransform();

			for (int32 Index = 0; Index < Frames.Num(); ++Index)
			{
				const FTransform GlobalTransform = ControlWorldTransforms[Index];
				const FFrameNumber Frame = Frames[Index];

				//evaluate sequencer
				FFrameTime GlobalTime(Frame);
				GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();

				FMovieSceneContext SceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
				Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(SceneContext, *Sequencer);

				//evaluate control rig
				ControlRig->Evaluate_AnyThread();
				Context.LocalTime = TickResolution.AsSeconds(FFrameTime(Frame));
				ControlRig->SetControlGlobalTransform(ControlKey.Name, GlobalTransform, true, Context);
			}

			//if end compensated set the space that was active previously and set the compensated global value
			if (bCompensateEnd)
			{
				//EndFrameValue to SpaceKey todoo move to function
				switch (EndFrameValue.SpaceType)
				{
				case EMovieSceneControlRigSpaceType::Parent:
					RigHierarchy->SwitchToDefaultParent(ControlKey);
					break;
				case EMovieSceneControlRigSpaceType::World:
					RigHierarchy->SwitchToWorldSpace(ControlKey);
					break;
				case EMovieSceneControlRigSpaceType::ControlRig:
					RigHierarchy->SwitchToParent(ControlKey, EndFrameValue.ControlRigElement, false, true, Dependencies, nullptr);
					break;
				}

				//evaluate sequencer
				FFrameTime GlobalTime(EndFrame);
				GlobalTime = GlobalTime * RootToLocalTransform.InverseLinearOnly();

				FMovieSceneContext SceneContext = FMovieSceneContext(FMovieSceneEvaluationRange(GlobalTime, TickResolution), Sequencer->GetPlaybackStatus()).SetHasJumped(true);
				Sequencer->GetEvaluationTemplate().EvaluateSynchronousBlocking(SceneContext, *Sequencer);
		
				//evaluate control rig
				ControlRig->Evaluate_AnyThread();

				TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = Channel->GetData();
				ChannelInterface.AddKey(EndFrame, Forward<FMovieSceneControlRigSpaceBaseKey>(EndFrameValue));
				const FTransform GlobalTransform = ControlWorldTransforms[Frames.Num() - 1];
				Context.LocalTime = TickResolution.AsSeconds(FFrameTime(EndFrame));
				ControlRig->SetControlGlobalTransform(ControlKey.Name, GlobalTransform, true, Context);
			}
		
			// Fix any Rotation Channels
			Section->FixRotationWinding(ControlKey.Name, Frames[0], Frames[Frames.Num() - 1]);
			// Then reduce
			if (Settings.bReduceKeys)
			{
				TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
				FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlKey.Name);
				if (pChannelIndex != nullptr)
				{
					int32 ChannelIndex = pChannelIndex->ChannelIndex;

					if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlKey.Name))
					{
						FKeyDataOptimizationParams Params;
						Params.bAutoSetInterpolation = true;
						Params.Tolerance = Settings.Tolerance;
						Params.Range = TRange <FFrameNumber>(Frames[0], Frames[Frames.Num() - 1]);

						switch (ControlElement->Settings.ControlType)
						{
						case ERigControlType::Position:
						case ERigControlType::Scale:
						case ERigControlType::Rotator:
						{
							FloatChannels[ChannelIndex]->Optimize(Params);
							FloatChannels[ChannelIndex + 1]->Optimize(Params);
							FloatChannels[ChannelIndex + 2]->Optimize(Params);
							break;
						}

						case ERigControlType::Transform:
						case ERigControlType::TransformNoScale:
						case ERigControlType::EulerTransform:
						{

							FloatChannels[ChannelIndex]->Optimize(Params);
							FloatChannels[ChannelIndex + 1]->Optimize(Params);
							FloatChannels[ChannelIndex + 2]->Optimize(Params);
							FloatChannels[ChannelIndex + 3]->Optimize(Params);
							FloatChannels[ChannelIndex + 4]->Optimize(Params);
							FloatChannels[ChannelIndex + 5]->Optimize(Params);

							if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
								ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
							{
								FloatChannels[ChannelIndex + 6]->Optimize(Params);
								FloatChannels[ChannelIndex + 7]->Optimize(Params);
								FloatChannels[ChannelIndex + 8]->Optimize(Params);
							}
							break;

						}
						}
					}
				}
			}
			
		}
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded); //may have added channel
	}
}

void FControlRigSpaceChannelHelpers::HandleSpaceKeyTimeChanged(UControlRig* ControlRig, FName ControlName, FMovieSceneControlRigSpaceChannel* Channel, UMovieSceneSection* SectionToKey,
	FFrameNumber CurrentFrame, FFrameNumber NextFrame)
{
	if (ControlRig && Channel && SectionToKey && (CurrentFrame != NextFrame))
	{
		if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
		{
			TArrayView<FMovieSceneFloatChannel*> FloatChannels = Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
			FChannelMapInfo* pChannelIndex = Section->ControlChannelMap.Find(ControlName);
			if (pChannelIndex != nullptr)
			{
				int32 ChannelIndex = pChannelIndex->ChannelIndex;
				FFrameNumber Delta = NextFrame - CurrentFrame;
				if (FRigControlElement* ControlElement = ControlRig->FindControl(ControlName))
				{
					FMovieSceneControlRigSpaceBaseKey Value;
					switch (ControlElement->Settings.ControlType)
					{
						case ERigControlType::Position:
						case ERigControlType::Scale:
						case ERigControlType::Rotator:
						case ERigControlType::Transform:
						case ERigControlType::TransformNoScale:
						case ERigControlType::EulerTransform:
						{
							int NumChannels = 0;
							if (ControlElement->Settings.ControlType == ERigControlType::Transform ||
								ControlElement->Settings.ControlType == ERigControlType::EulerTransform)
							{
								NumChannels = 9;
							}
							else if (ControlElement->Settings.ControlType == ERigControlType::TransformNoScale)
							{
								NumChannels = 6;
							}
							else //vectors
							{
								NumChannels = 3;
							}
							for (int32 Index = 0; Index < NumChannels; ++Index)
							{
								FMovieSceneFloatChannel* FloatChannel = FloatChannels[ChannelIndex++];
								if (Delta > 0) //if we are moving keys positively in time we start from end frames and move them so we can use indices
								{
									for (int32 KeyIndex = FloatChannel->GetData().GetTimes().Num() - 1; KeyIndex >= 0; --KeyIndex)
									{
										const FFrameNumber Frame = FloatChannel->GetData().GetTimes()[KeyIndex];
										FFrameNumber Diff = Frame - CurrentFrame;
										FFrameNumber AbsDiff = Diff < 0 ? -Diff : Diff;
										if (AbsDiff <= 1)
										{
											FFrameNumber NewKeyTime = Frame + Delta;
											FloatChannel->GetData().MoveKey(KeyIndex, NewKeyTime);
										}
									}
								}
								else
								{
									for (int32 KeyIndex = 0; KeyIndex < FloatChannel->GetData().GetTimes().Num(); ++KeyIndex)
									{
										const FFrameNumber Frame = FloatChannel->GetData().GetTimes()[KeyIndex];
										FFrameNumber Diff = Frame - CurrentFrame;
										FFrameNumber AbsDiff = Diff < 0 ? -Diff : Diff;
										if (AbsDiff <= 1)
										{
											FFrameNumber NewKeyTime = Frame + Delta;
											FloatChannel->GetData().MoveKey(KeyIndex, NewKeyTime);
										}
									}
								}
							}
							break;
						}
						default:
							break;
					}
				}
			}
		}
	}
}


void FControlRigSpaceChannelHelpers::CompensateIfNeeded(
	UControlRig* ControlRig,
	ISequencer* Sequencer,
	UMovieSceneControlRigParameterSection* Section,
	FName ControlName,
	TOptional<FFrameNumber>& OptionalTime)
{
	if (bDoNotCompensate == true)
	{
		return;
	}

	// Frames to compensate
	TArray<FFrameNumber> OptionalTimeArray;
	if(OptionalTime.IsSet())
	{
		OptionalTimeArray.Add(OptionalTime.GetValue());
	}
	
	auto GetSpaceTimesToCompensate = [&OptionalTimeArray](const FSpaceControlNameAndChannel* Channel)->TArrayView<const FFrameNumber>
	{
		if (OptionalTimeArray.IsEmpty())
		{
			return Channel->SpaceCurve.GetData().GetTimes();
		}
		return OptionalTimeArray;
	};

	// keyframe context
	FRigControlModifiedContext KeyframeContext;
	KeyframeContext.SetKey = EControlRigSetKey::Always;
	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	
	//we need to check all controls for 1) space and 2) previous frame and if so we automatically compensate.
	bool bDidIt = false;

	// compensate spaces
	URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
	const TArray<FRigControlElement*> Controls = RigHierarchy->GetControls();
	for (const FRigControlElement* Control: Controls)
	{ 
		if(Control)// ac && Control->GetName() != ControlName)
		{ 
			//only if we have a channel
			if (FSpaceControlNameAndChannel* Channel = Section->GetSpaceChannel(Control->GetName()))
			{
				const TArrayView<const FFrameNumber> FramesToCompensate = GetSpaceTimesToCompensate(Channel);
				if (FramesToCompensate.Num() > 0)
				{
					for (const FFrameNumber& Time : FramesToCompensate)
					{
						FMovieSceneControlRigSpaceBaseKey ExistingValue, PreviousValue;
						using namespace UE::MovieScene;
						EvaluateChannel(&(Channel->SpaceCurve), Time - 1, PreviousValue);
						EvaluateChannel(&(Channel->SpaceCurve), Time, ExistingValue);

						if (ExistingValue != PreviousValue) //if they are the same no need to do anything
						{
							TGuardValue<bool> CompensateGuard(bDoNotCompensate, true);
							
							//find global value at current time
							TArray<FTransform> ControlRigParentWorldTransforms({FTransform::Identity});
							TArray<FTransform> ControlWorldTransforms;
							FControlRigSnapper Snapper;
							Snapper.GetControlRigControlTransforms(
								Sequencer, ControlRig, Control->GetName(),
								{Time},
								ControlRigParentWorldTransforms, ControlWorldTransforms);

							//set space to previous space value that's different.
							switch (PreviousValue.SpaceType)
							{
								case EMovieSceneControlRigSpaceType::Parent:
									RigHierarchy->SwitchToDefaultParent(Control->GetKey());
									break;
								case EMovieSceneControlRigSpaceType::World:
									RigHierarchy->SwitchToWorldSpace(Control->GetKey());
									break;
								case EMovieSceneControlRigSpaceType::ControlRig:
									URigHierarchy::TElementDependencyMap Dependencies = RigHierarchy->GetDependenciesForVM(ControlRig->GetVM());
									RigHierarchy->SwitchToParent(Control->GetKey(), PreviousValue.ControlRigElement, false, true, Dependencies, nullptr);
									break;
							}
							
							//now set time -1 frame value
							ControlRig->Evaluate_AnyThread();
							KeyframeContext.LocalTime = TickResolution.AsSeconds(FFrameTime(Time - 1));
							ControlRig->SetControlGlobalTransform(Control->GetName(), ControlWorldTransforms[0], true, KeyframeContext);
							
							bDidIt = true;
						}
					}
				}
			}
		}
	}
	
	if (bDidIt)
	{
		Sequencer->ForceEvaluate();
	}
}

FLinearColor FControlRigSpaceChannelHelpers::GetColor(const FMovieSceneControlRigSpaceBaseKey& Key)
{
	const UCurveEditorSettings* Settings = GetDefault<UCurveEditorSettings>();
	static TMap<FName, FLinearColor> ColorsForSpaces;
	switch (Key.SpaceType)
	{
		case EMovieSceneControlRigSpaceType::Parent:
		{
			TOptional<FLinearColor> OptColor = Settings->GetSpaceSwitchColor(FString(TEXT("Parent")));
			if (OptColor.IsSet())
			{
				return OptColor.GetValue();
			}

			return  FLinearColor(.93, .31, .19); //pastel orange

		}
		case EMovieSceneControlRigSpaceType::World:
		{
			TOptional<FLinearColor> OptColor = Settings->GetSpaceSwitchColor(FString(TEXT("World")));
			if (OptColor.IsSet())
			{
				return OptColor.GetValue();
			}

			return  FLinearColor(.198, .610, .558); //pastel teal
		}
		case EMovieSceneControlRigSpaceType::ControlRig:
		{
			TOptional<FLinearColor> OptColor = Settings->GetSpaceSwitchColor(Key.ControlRigElement.Name.ToString());
			if (OptColor.IsSet())
			{
				return OptColor.GetValue();
			}
			if (FLinearColor* Color = ColorsForSpaces.Find(Key.ControlRigElement.Name))
			{
				return *Color;
			}
			else
			{
				FLinearColor RandomColor = UCurveEditorSettings::GetNextRandomColor();
				ColorsForSpaces.Add(Key.ControlRigElement.Name, RandomColor);
				return RandomColor;
			}
		}
	};
	return  FLinearColor(FColor::White);

}

FReply FControlRigSpaceChannelHelpers::OpenBakeDialog(ISequencer* Sequencer, FMovieSceneControlRigSpaceChannel* Channel,int32 KeyIndex, UMovieSceneSection* SectionToKey )
{

	if (!Sequencer || !Sequencer->GetFocusedMovieSceneSequence()|| !Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene())
	{
		return FReply::Unhandled();
	}
	if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(SectionToKey))
	{
		if (UControlRig* ControlRig = Section->GetControlRig())
		{
			FName ControlName = Section->FindControlNameFromSpaceChannel(Channel);

			if (ControlName == NAME_None)
			{
				return FReply::Unhandled();
			}
			FRigSpacePickerBakeSettings Settings;

			const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
			FMovieSceneControlRigSpaceBaseKey Value;

			URigHierarchy* RigHierarchy = ControlRig->GetHierarchy();
			Settings.TargetSpace = URigHierarchy::GetDefaultParentKey();

			TRange<FFrameNumber> Range = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
			Settings.StartFrame = Range.GetLowerBoundValue();
			Settings.EndFrame = Range.GetUpperBoundValue();
			TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelData = Channel->GetData();
			TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
			if (KeyIndex >= 0 && KeyIndex < Times.Num())
			{
				Settings.StartFrame = Times[KeyIndex];
				if (KeyIndex + 1 < Times.Num())
				{
					Settings.EndFrame = Times[KeyIndex + 1];
				}
			}
			TArray<FRigElementKey> Controls;
			FRigElementKey Key;
			Key.Name = ControlName;
			Key.Type = ERigElementType::Control;
			Controls.Add(Key);
			TSharedRef<SRigSpacePickerBakeWidget> BakeWidget =
				SNew(SRigSpacePickerBakeWidget)
				.Settings(Settings)
				.Hierarchy(ControlRig->GetHierarchy())
				.Controls(Controls)
				.Sequencer(Sequencer)
				.GetControlCustomization_Lambda([ControlRig](URigHierarchy*, const FRigElementKey& InControlKey)
				{
					return ControlRig->GetControlCustomization(InControlKey);
				})
				.OnBake_Lambda([Sequencer, ControlRig, TickResolution](URigHierarchy* InHierarchy, TArray<FRigElementKey> InControls, FRigSpacePickerBakeSettings InSettings)
				{
					TArray<FFrameNumber> Frames;

					const FFrameRate& FrameRate = Sequencer->GetFocusedDisplayRate();
					FFrameNumber FrameRateInFrameNumber = TickResolution.AsFrameNumber(FrameRate.AsInterval());
					for (FFrameNumber& Frame = InSettings.StartFrame; Frame <= InSettings.EndFrame; Frame += FrameRateInFrameNumber)
					{
						Frames.Add(Frame);
					}
					FScopedTransaction Transaction(LOCTEXT("BakeControlToSpace", "Bake Control In Space"));
					for (const FRigElementKey& ControlKey : InControls)
					{
						FSpaceChannelAndSection SpaceChannelAndSection = FControlRigSpaceChannelHelpers::FindSpaceChannelAndSectionForControl(ControlRig, ControlKey.Name, Sequencer, false /*bCreateIfNeeded*/);
						if (SpaceChannelAndSection.SpaceChannel)
						{
							FControlRigSpaceChannelHelpers::SequencerBakeControlInSpace(ControlRig, Sequencer, SpaceChannelAndSection.SpaceChannel, SpaceChannelAndSection.SectionToKey,
								Frames, InHierarchy, ControlKey, InSettings);
						}
					}
					return FReply::Handled();
				});

			return BakeWidget->OpenDialog(true);
			
		}
	}
	return FReply::Unhandled();
}

TUniquePtr<FCurveModel> CreateCurveEditorModel(const TMovieSceneChannelHandle<FMovieSceneControlRigSpaceChannel>& Channel, UMovieSceneSection* OwningSection, TSharedRef<ISequencer> InSequencer)
{
	return MakeUnique<FControlRigSpaceChannelCurveModel>(Channel, OwningSection, InSequencer);
}

TArray<FKeyBarCurveModel::FBarRange> FControlRigSpaceChannelHelpers::FindRanges(FMovieSceneControlRigSpaceChannel* Channel, const UMovieSceneSection* Section)
{
	TArray<FKeyBarCurveModel::FBarRange> Range;
	if (Channel && Section)
	{
		FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
		TArray<FSpaceRange> SpaceRanges = Channel->FindSpaceIntervals();
		for (const FSpaceRange& SpaceRange : SpaceRanges)
		{
			FKeyBarCurveModel::FBarRange BarRange;
			double LowerValue = SpaceRange.Range.GetLowerBoundValue() / TickResolution;
			double UpperValue = SpaceRange.Range.GetUpperBoundValue() / TickResolution;

			BarRange.Range.SetLowerBound(TRangeBound<double>(LowerValue));
			BarRange.Range.SetUpperBound(TRangeBound<double>(UpperValue));

			BarRange.Name = SpaceRange.Key.GetName();
			BarRange.Color = FControlRigSpaceChannelHelpers::GetColor(SpaceRange.Key);

			Range.Add(BarRange);
		}
	}
	return  Range;
}

// NOTE use this function in HandleSpaceKeyTimeChanged, DeleteTransformKeysAtThisTime, ...
TPair<FRigControlElement*, FChannelMapInfo*> FControlRigSpaceChannelHelpers::GetControlAndChannelInfo(UControlRig* ControlRig, UMovieSceneControlRigParameterSection* Section, FName ControlName)
{
	FRigControlElement* ControlElement = ControlRig ? ControlRig->FindControl(ControlName) : nullptr;
	FChannelMapInfo* pChannelIndex = Section ? Section->ControlChannelMap.Find(ControlName) : nullptr;

	return TPair<FRigControlElement*, FChannelMapInfo*>(ControlElement, pChannelIndex);
}

// NOTE use this function in HandleSpaceKeyTimeChanged, DeleteTransformKeysAtThisTime, ...
int32 FControlRigSpaceChannelHelpers::GetNumFloatChannels(const ERigControlType InControlType)
{
	switch (InControlType)
	{
	case ERigControlType::Position:
	case ERigControlType::Scale:
	case ERigControlType::Rotator:
		return 3;
	case ERigControlType::TransformNoScale:
		return 6;
	case ERigControlType::Transform:
	case ERigControlType::EulerTransform:
		return 9;
	default:
		break;
	}
	return 0;
}

int32 DrawExtra(FMovieSceneControlRigSpaceChannel* Channel, const UMovieSceneSection* Owner, const FSequencerChannelPaintArgs& PaintArgs, int32 LayerId)
{
	using namespace UE::Sequencer;

	if (const UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(Owner))
	{
		TArray<FKeyBarCurveModel::FBarRange> Ranges = FControlRigSpaceChannelHelpers::FindRanges(Channel, Owner);
		const FSlateBrush* WhiteBrush = FAppStyle::GetBrush("WhiteBrush");
		const ESlateDrawEffect DrawEffects = ESlateDrawEffect::None;

		const FSlateFontInfo FontInfo = FCoreStyle::Get().GetFontStyle("ToolTip.LargerFont");
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		const FVector2D LocalSize = PaintArgs.Geometry.GetLocalSize();
		const float LaneTop = 0;

		const double InputMin = PaintArgs.TimeToPixel.PixelToSeconds(0.f);
		const double InputMax = PaintArgs.TimeToPixel.PixelToSeconds(LocalSize.X);

		int32 LastLabelPos = -1;
		for (int32 Index = 0; Index < Ranges.Num(); ++Index)
		{
			const FKeyBarCurveModel::FBarRange& Range = Ranges[Index];
			const double LowerSeconds = Range.Range.GetLowerBoundValue();
			const double UpperSeconds = Range.Range.GetUpperBoundValue();
			const bool bOutsideUpper = (Index != Ranges.Num() - 1) && UpperSeconds < InputMin;
			if (LowerSeconds > InputMax || bOutsideUpper) //out of range
			{
				continue;
			}

			FLinearColor CurveColor = Range.Color;
	
			static FLinearColor ZebraTint = FLinearColor::White.CopyWithNewOpacity(0.01f);
			if (CurveColor == FLinearColor::White)
			{
				CurveColor = ZebraTint;
			}
			else
			{
				CurveColor = CurveColor * (1.f - ZebraTint.A) + ZebraTint * ZebraTint.A;
			}
			

			if (CurveColor != FLinearColor::White)
			{
				const double LowerSecondsForBox = (Index == 0 && LowerSeconds > InputMin) ? InputMin : LowerSeconds;
				const double BoxPos = PaintArgs.TimeToPixel.SecondsToPixel(LowerSecondsForBox);

				const FPaintGeometry BoxGeometry = PaintArgs.Geometry.ToPaintGeometry(
					FVector2D(PaintArgs.Geometry.GetLocalSize().X, PaintArgs.Geometry.GetLocalSize().Y),
					FSlateLayoutTransform(FVector2D(BoxPos, LaneTop))
				);

				FSlateDrawElement::MakeBox(PaintArgs.DrawElements, LayerId, BoxGeometry, WhiteBrush, DrawEffects, CurveColor);
			}
			const double LowerSecondsForLabel = (InputMin > LowerSeconds) ? InputMin : LowerSeconds;
			double LabelPos = PaintArgs.TimeToPixel.SecondsToPixel(LowerSecondsForLabel) + 10;

			const FText Label = FText::FromName(Range.Name);
			const FVector2D TextSize = FontMeasure->Measure(Label, FontInfo);
			if (Index > 0)
			{
				LabelPos = (LabelPos < LastLabelPos) ? LastLabelPos + 5 : LabelPos;
			}
			LastLabelPos = LabelPos + TextSize.X + 15;
			const FVector2D Position(LabelPos, LaneTop + (PaintArgs.Geometry.GetLocalSize().Y - TextSize.Y) * .5f);

			const FPaintGeometry LabelGeometry = PaintArgs.Geometry.ToPaintGeometry(
				FSlateLayoutTransform(Position)
			);

			FSlateDrawElement::MakeText(PaintArgs.DrawElements, LayerId, LabelGeometry, Label, FontInfo, DrawEffects, FLinearColor::White);
		}
	}

	return LayerId + 1;
}

void DrawKeys(FMovieSceneControlRigSpaceChannel* Channel, TArrayView<const FKeyHandle> InKeyHandles, const UMovieSceneSection* InOwner, TArrayView<FKeyDrawParams> OutKeyDrawParams)
{
	if (const UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(InOwner))
	{
		TMovieSceneChannelData<FMovieSceneControlRigSpaceBaseKey> ChannelInterface = Channel->GetData();

		for (int32 Index = 0; Index < InKeyHandles.Num(); ++Index)
		{
			FKeyHandle Handle = InKeyHandles[Index];

			FKeyDrawParams Params;
			static const FName SquareKeyBrushName("Sequencer.KeySquare");
			const FSlateBrush* SquareKeyBrush = FAppStyle::GetBrush(SquareKeyBrushName);
			Params.FillBrush = FAppStyle::Get().GetBrush("FilledBorder");
			Params.BorderBrush = SquareKeyBrush;
			const int32 KeyIndex = ChannelInterface.GetIndex(Handle);

			OutKeyDrawParams[Index] = Params;
		}
	}
}

#undef LOCTEXT_NAMESPACE
