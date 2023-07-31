// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConstraintChannelHelper.inl"
#include "ConstraintChannelHelper.h"
#include "ControlRigSpaceChannelEditors.h"
#include "ISequencer.h"

#include "Constraints/ControlRigTransformableHandle.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"

#include "TransformConstraint.h"
#include "Algo/Copy.h"

#include "Tools/BakingHelper.h"
#include "Tools/ConstraintBaker.h"
#include "ScopedTransaction.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "MovieSceneConstraintChannelHelper.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "Systems/MovieScenePropertyInstantiator.h"
#include "Tracks/MovieScene3DTransformTrack.h"


#define LOCTEXT_NAMESPACE "Constraints"

namespace
{
	bool CanAddKey(const FMovieSceneBoolChannel& ActiveChannel, const FFrameNumber& InTime, bool& ActiveValue)
	{
		const TMovieSceneChannelData<const bool> ChannelData = ActiveChannel.GetData();
		const TArrayView<const FFrameNumber> Times = ChannelData.GetTimes();
		if (Times.IsEmpty())
		{
			ActiveValue = true;
			return true;
		}

		const TArrayView<const bool> Values = ChannelData.GetValues();
		if (InTime < Times[0])
		{
			if (!Values[0])
			{
				ActiveValue = true;
				return true;
			}
			return false;
		}
		
		if (InTime > Times.Last())
		{
			ActiveValue = !Values.Last();
			return true;
		}

		return false;
	}

	// NOTE, because of restriction for hotfix changes this code some of the following functions are duplicated in
	// ConstraintBaker.cpp. This will be factorized as part of a refactor for 5.2
	
	UMovieScene* GetMovieScene(const TSharedPtr<ISequencer>& InSequencer)
	{
		const UMovieSceneSequence* MovieSceneSequence = InSequencer ? InSequencer->GetFocusedMovieSceneSequence() : nullptr;
		return MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	}
	
	UMovieSceneControlRigParameterSection* GetControlConstraintSection(
		const UTransformableControlHandle* InHandle,
		const TSharedPtr<ISequencer>& InSequencer)
	{
		const UMovieScene* MovieScene = GetMovieScene(InSequencer);
		if (!MovieScene)
		{
			return nullptr;
		}

		auto GetControlRigTrack = [InHandle, MovieScene]()->UMovieSceneControlRigParameterTrack*
		{
			const TWeakObjectPtr<UControlRig> ControlRig = InHandle->ControlRig.LoadSynchronous();
			if (ControlRig.IsValid())
			{	
				const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
				for (const FMovieSceneBinding& Binding : Bindings)
				{
					UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid());
					UMovieSceneControlRigParameterTrack* ControlRigTrack = Cast<UMovieSceneControlRigParameterTrack>(Track);
					if (ControlRigTrack && ControlRigTrack->GetControlRig() == ControlRig)
					{
						return ControlRigTrack;
					}
				}
			}
			return nullptr;
		};

		UMovieSceneControlRigParameterTrack* ControlRigTrack = GetControlRigTrack();
		if (!ControlRigTrack)
		{
			return nullptr;
		}

		const TArray<UMovieSceneSection*>& AllSections = ControlRigTrack->GetAllSections();
		UMovieSceneSection* Section = AllSections.IsEmpty() ? ControlRigTrack->FindSection(0) : AllSections[0];
		return Cast<UMovieSceneControlRigParameterSection>(Section);
	}
	
	UMovieScene3DTransformSection* GetComponentConstraintSection(
		const TSharedPtr<ISequencer>& InSequencer,
		const FGuid& InGuid,
		const FTransform& InDefaultTransform)
	{
		UMovieScene* MovieScene = GetMovieScene(InSequencer);
		if (!MovieScene)
		{
			return nullptr;
		}

		UMovieScene3DTransformTrack* TransformTrack = MovieScene->FindTrack<UMovieScene3DTransformTrack>(InGuid);
		if (!TransformTrack)
		{
			MovieScene->Modify();
			TransformTrack = MovieScene->AddTrack<UMovieScene3DTransformTrack>(InGuid);
		}
		TransformTrack->Modify();

		const TArray<UMovieSceneSection*>& AllSections = TransformTrack->GetAllSections();

		static constexpr FFrameNumber Frame0;
		bool bSectionAdded = false;
		UMovieScene3DTransformSection* TransformSection = AllSections.IsEmpty() ?
			Cast<UMovieScene3DTransformSection>(TransformTrack->FindOrAddSection(Frame0, bSectionAdded)) :
			Cast<UMovieScene3DTransformSection>(AllSections[0]);
		if (!TransformSection)
		{
			return nullptr;
		}

		TransformSection->Modify();
		if (bSectionAdded)
		{
			TransformSection->SetRange(TRange<FFrameNumber>::All());

			const FMovieSceneChannelProxy& SectionChannelProxy = TransformSection->GetChannelProxy();
			const TMovieSceneChannelHandle<FMovieSceneDoubleChannel> DoubleChannels[] = {
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.X"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.Y"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Location.Z"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.X"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.Y"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Rotation.Z"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.X"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.Y"),
				SectionChannelProxy.GetChannelByName<FMovieSceneDoubleChannel>("Scale.Z")
			};

			const FVector Location0 = InDefaultTransform.GetLocation();
			const FRotator Rotation0 = InDefaultTransform.GetRotation().Rotator();
			const FVector Scale3D0 = InDefaultTransform.GetScale3D();
			const FTransform::FReal DefaultValues[] = { Location0.X, Location0.Y, Location0.Z,
														Rotation0.Roll, Rotation0.Pitch, Rotation0.Yaw,
														Scale3D0.X, Scale3D0.Y, Scale3D0.Z };
			for (int32 Index = 0; Index < 9; Index++)
			{
				if (FMovieSceneDoubleChannel* Channel = DoubleChannels[Index].Get())
				{
					Channel->SetDefault(DefaultValues[Index]);
				}
			}
		}

		return TransformSection;
	}

	// NOTE this has to be moved to a public header like MovieSceneToolsHelpers.h (or similar) for 5.2
	// see F3DTransformTrackEditor::RecomposeTransform for more details
	void RecomposeTransforms(
		const TSharedPtr<ISequencer>& InSequencer, USceneComponent* SceneComponent, UMovieSceneSection* Section,
		const TArray<FFrameNumber>& InFrames, TArray<FTransform>& InOutTransforms)
	{
		using namespace UE::MovieScene;

		FMovieSceneRootEvaluationTemplateInstance& EvaluationTemplate = InSequencer->GetEvaluationTemplate();
		UMovieSceneEntitySystemLinker* EntityLinker = EvaluationTemplate.GetEntitySystemLinker();
		if (!EntityLinker)
		{
			return;
		}

		UMovieScenePropertyInstantiatorSystem* System = EntityLinker->FindSystem<UMovieScenePropertyInstantiatorSystem>();
		if (!System)
		{
			return;
		}

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &EntityLinker->EntityManager);
		TArray<FMovieSceneEntityID> ImportedEntityIDs;
		EvaluationTemplate.FindEntitiesFromOwner(Section, InSequencer->GetFocusedTemplateID(), ImportedEntityIDs);
		if (ImportedEntityIDs.Num())
		{
			const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
			FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

			UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
			const FFrameRate TickResolution = MovieScene->GetTickResolution();
			const EMovieScenePlayerStatus::Type PlaybackStatus = InSequencer->GetPlaybackStatus();

			TArray<FMovieSceneEntityID> EntityIDs;
			FDecompositionQuery Query;
			Query.Object   = SceneComponent;
			Query.bConvertFromSourceEntityIDs = false;  // We already pass the children entity IDs
			
			// add keys
			for (int32 Index = 0; Index < InFrames.Num(); ++Index)
			{
				const FFrameNumber& FrameNumber = InFrames[Index];
				const FMovieSceneEvaluationRange EvaluationRange = FMovieSceneEvaluationRange(FFrameTime(FrameNumber), TickResolution);
				const FMovieSceneContext Context = FMovieSceneContext(EvaluationRange, PlaybackStatus).SetHasJumped(true);

				EvaluationTemplate.EvaluateSynchronousBlocking(Context, *InSequencer);

				if (EntityIDs.IsEmpty())
				{
					// In order to check for the result channels later, we need to look up the children entities that are
					// bound to the given animated object. Imported entities generally don't have the result channels.
					FEntityTaskBuilder()
					.ReadEntityIDs()
					.Read(BuiltInComponents->ParentEntity)
					.Read(BuiltInComponents->BoundObject)
					.FilterAll({ TrackComponents->ComponentTransform.PropertyTag })
					.Iterate_PerEntity(
						&EntityLinker->EntityManager, 
						[SceneComponent, ImportedEntityIDs, &EntityIDs](FMovieSceneEntityID EntityID, FMovieSceneEntityID ParentEntityID, UObject* BoundObject)
						{
							if (SceneComponent == BoundObject && ImportedEntityIDs.Contains(ParentEntityID))
							{
								EntityIDs.Add(EntityID);
							}
						});
					Query.Entities = MakeArrayView(EntityIDs);
				}
				
				FTransform& Transform = InOutTransforms[Index];

				const FIntermediate3DTransform CurrentValue(Transform.GetTranslation(), Transform.GetRotation().Rotator(), Transform.GetScale3D());

				TRecompositionResult<FIntermediate3DTransform> TransformData = System->RecomposeBlendOperational(TrackComponents->ComponentTransform, Query, CurrentValue);

				double CurrentTransformChannels[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				EMovieSceneTransformChannel ChannelsObtained(EMovieSceneTransformChannel::None);
				check(EntityIDs.Num() == TransformData.Values.Num());
				for (int32 EntityIndex = 0; EntityIndex < TransformData.Values.Num(); ++EntityIndex)
				{
					const FMovieSceneEntityID& EntityID = EntityIDs[EntityIndex];
					
					const FIntermediate3DTransform& EntityTransformData = TransformData.Values[EntityIndex];
					FComponentMask EntityType = EntityLinker->EntityManager.GetEntityType(EntityID);
					for (int32 CompositeIndex = 0; CompositeIndex < 9; ++CompositeIndex)
					{
						EMovieSceneTransformChannel ChannelMask = (EMovieSceneTransformChannel)(1 << CompositeIndex);
						if (!EnumHasAnyFlags(ChannelsObtained, ChannelMask) && EntityType.Contains(BuiltInComponents->DoubleResult[CompositeIndex]))
						{
							EnumAddFlags(ChannelsObtained, (EMovieSceneTransformChannel)(1 << CompositeIndex));
							CurrentTransformChannels[CompositeIndex] = EntityTransformData[CompositeIndex];
						}
					}
				}

				Transform = FTransform(
					FRotator(CurrentTransformChannels[4], CurrentTransformChannels[5], CurrentTransformChannels[3]), // pitch yaw roll
					FVector(CurrentTransformChannels[0], CurrentTransformChannels[1], CurrentTransformChannels[2]),
					FVector(CurrentTransformChannels[6], CurrentTransformChannels[7], CurrentTransformChannels[8]));
			}
		}
	}
}

bool FConstraintChannelHelper::IsKeyframingAvailable()
{
	const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
	if (!WeakSequencer.IsValid())
	{
		return false;
	}

	if (!WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		return false;
	}

	return true;
}

void FConstraintChannelHelper::SmartConstraintKey(UTickableTransformConstraint* InConstraint)
{
	const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
	if (!WeakSequencer.IsValid() || !WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		return;
	}

	if (const UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(InConstraint->ChildTRSHandle))
	{
		SmartComponentConstraintKey(InConstraint, WeakSequencer.Pin());
	}
	
	else if (const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(InConstraint->ChildTRSHandle))
	{
		SmartControlConstraintKey(InConstraint, WeakSequencer.Pin());
	}

	FConstraintChannelHelper::CreateBindingIDForHandle(InConstraint->ChildTRSHandle);
	FConstraintChannelHelper::CreateBindingIDForHandle(InConstraint->ParentTRSHandle);
}


void FConstraintChannelHelper::CreateBindingIDForHandle(UTransformableHandle* InHandle)
{
	const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
	if (InHandle == nullptr || WeakSequencer.IsValid() == false)
	{
		return;
	}
	if (USceneComponent* SceneComponent = Cast<USceneComponent>(InHandle->GetTarget().Get()))
	{
		AActor* Actor = SceneComponent->GetTypedOuter<AActor>();
		if (Actor)
		{
			TOptional<FMovieSceneSpawnableAnnotation> Spawnable = FMovieSceneSpawnableAnnotation::Find(Actor);
			if (Spawnable.IsSet())
			{
				// Check whether the spawnable is underneath the current sequence, if so, we can remap it to a local sequence ID
				InHandle->ConstraintBindingID = UE::MovieScene::FRelativeObjectBindingID(WeakSequencer.Pin()->GetFocusedTemplateID(), Spawnable->SequenceID, Spawnable->ObjectBindingID,
					*(WeakSequencer.Pin().Get()));
			}
			else
			{
				FGuid Guid = WeakSequencer.Pin()->GetHandleToObject(Actor, false); //don't create it???
				InHandle->ConstraintBindingID = UE::MovieScene::FRelativeObjectBindingID(Guid);
			}
		}
	}
}

UMovieSceneControlRigParameterSection* FConstraintChannelHelper::GetControlSection(
	const UTransformableControlHandle* InHandle,
	const TSharedPtr<ISequencer>& InSequencer)
{
	const UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!MovieScene)
	{
		return nullptr;
	}

	const TWeakObjectPtr<UControlRig> ControlRig = InHandle->ControlRig.LoadSynchronous();
	if(ControlRig.IsValid())
	{	
		const TArray<FMovieSceneBinding>& Bindings = MovieScene->GetBindings();
		for (const FMovieSceneBinding& Binding : Bindings)
		{
			UMovieSceneTrack* Track = MovieScene->FindTrack(UMovieSceneControlRigParameterTrack::StaticClass(), Binding.GetObjectGuid());
			UMovieSceneControlRigParameterTrack* ControlRigTrack = Cast<UMovieSceneControlRigParameterTrack>(Track);
			if (ControlRigTrack && ControlRigTrack->GetControlRig() == ControlRig)
			{
				return Cast<UMovieSceneControlRigParameterSection>(ControlRigTrack->FindSection(0));
			}
		}
	}

	return nullptr;
}

UMovieScene3DTransformSection* FConstraintChannelHelper::GetTransformSection(
	const UTransformableComponentHandle* InHandle,
	const TSharedPtr<ISequencer>& InSequencer)
{
	AActor* Actor = InHandle->Component->GetOwner();
	if (!Actor)
	{
		return nullptr;
	}
	
	const FGuid Guid = InSequencer->GetHandleToObject(Actor,true);
	if (!Guid.IsValid())
	{
		return nullptr;
	}
	
	return MovieSceneToolHelpers::GetTransformSection(InSequencer.Get(), Guid);
}

void FConstraintChannelHelper::SmartControlConstraintKey(
	UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer)
{
	const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(InConstraint->ChildTRSHandle);
	if (!ControlHandle)
	{
		return;
	}

	UMovieSceneControlRigParameterSection* ConstraintSection = GetControlConstraintSection(ControlHandle, InSequencer);
	UMovieSceneControlRigParameterSection* TransformSection = GetControlSection(ControlHandle, InSequencer);
	if ((!ConstraintSection) || (!TransformSection))
	{
		return;
	}
	
	FScopedTransaction Transaction(LOCTEXT("KeyConstraintaKehy", "Key Constraint Key"));
	ConstraintSection->Modify();
	TransformSection->Modify();

	// set constraint as dynamic
	InConstraint->bDynamicOffset = true;
	
	// add the channel
	ConstraintSection->AddConstraintChannel(InConstraint);

	// add key if needed
	if (FConstraintAndActiveChannel* Channel = ConstraintSection->GetConstraintChannel(InConstraint->GetFName()))
	{
		const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();
		const FFrameTime FrameTime = InSequencer->GetLocalTime().ConvertTo(TickResolution);
		const FFrameNumber Time = FrameTime.GetFrame();

		bool ActiveValueToBeSet = false;
		if (CanAddKey(Channel->ActiveChannel, Time, ActiveValueToBeSet))
		{
			const bool bNeedsCompensation = InConstraint->NeedsCompensation();
			
			TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);
			
			UControlRig* ControlRig = ControlHandle->ControlRig.Get();
			const FName& ControlName = ControlHandle->ControlName;
			
			// store the frames to compensate
			const TArrayView<FMovieSceneFloatChannel*> Channels = ControlHandle->GetFloatChannels(TransformSection);
			TArray<FFrameNumber> FramesToCompensate;
			if (bNeedsCompensation)
			{
				FMovieSceneConstraintChannelHelper::GetFramesToCompensate<FMovieSceneFloatChannel>(Channel->ActiveChannel, ActiveValueToBeSet, Time, Channels, FramesToCompensate);
			}
			else
			{
				FramesToCompensate.Add(Time);
			}

			// store child and space transforms for these frames
			FCompensationEvaluator Evaluator(InConstraint);
			Evaluator.ComputeLocalTransforms(ControlRig->GetWorld(), InSequencer, FramesToCompensate, ActiveValueToBeSet);
			TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;
			
			// store tangents at this time
			TArray<FMovieSceneTangentData> Tangents;
			int32 ChannelIndex = 0, NumChannels = 0;

			FChannelMapInfo* pChannelIndex = nullptr;
			FRigControlElement* ControlElement = nullptr;
			Tie(ControlElement, pChannelIndex) = FControlRigSpaceChannelHelpers::GetControlAndChannelInfo(ControlRig, TransformSection, ControlName);

			if (pChannelIndex && ControlElement)
			{
				// get the number of float channels to treat
				NumChannels = FControlRigSpaceChannelHelpers::GetNumFloatChannels(ControlElement->Settings.ControlType);
				if (bNeedsCompensation && NumChannels > 0)
				{
					ChannelIndex = pChannelIndex->ChannelIndex;
					EvaluateTangentAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, Time, Tangents);
				}
			}
		
			const EMovieSceneTransformChannel ChannelsToKey =InConstraint->GetChannelsToKey();
			
			// add child's transform key at Time-1 to keep animation
			if (bNeedsCompensation)
			{
				const FFrameNumber TimeMinusOne(Time - 1);

				ControlHandle->AddTransformKeys({ TimeMinusOne },
					{ ChildLocals[0] }, ChannelsToKey, TickResolution, nullptr,true);

				// set tangents at Time-1
				if (NumChannels > 0)
				{
					SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, TimeMinusOne, Tangents);
				}
			}

			// add active key
			{
				TMovieSceneChannelData<bool> ChannelData = Channel->ActiveChannel.GetData();
				ChannelData.AddKey(Time, ActiveValueToBeSet);
			}

			// compensate
			{
				// we need to remove the first transforms as we store NumFrames+1 transforms
				ChildLocals.RemoveAt(0);

				// add keys
				ControlHandle->AddTransformKeys(FramesToCompensate,
					ChildLocals, ChannelsToKey, TickResolution, nullptr,true);

				// set tangents at Time
				if (bNeedsCompensation && NumChannels > 0)
				{
					SetTangentsAtThisTime<FMovieSceneFloatChannel>(ChannelIndex, NumChannels, TransformSection, Time, Tangents);
				}
			}
		}
	}
}

void FConstraintChannelHelper::SmartComponentConstraintKey(
	UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer)
{
	const UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(InConstraint->ChildTRSHandle);
	if (!ComponentHandle)
	{
		return;
	}
	AActor* Actor = ComponentHandle->Component->GetOwner();
	if (!Actor)
	{
		return;
	}

	const FTransform LocalTransform = ComponentHandle->GetLocalTransform();
	const FGuid Guid = InSequencer->GetHandleToObject(Actor, true);
	if (!Guid.IsValid())
	{
		return;
	}

	UMovieScene3DTransformSection* TransformSection = MovieSceneToolHelpers::GetTransformSection(InSequencer.Get(), Guid, LocalTransform);
	UMovieScene3DTransformSection* ConstraintSection = GetComponentConstraintSection(InSequencer, Guid, LocalTransform);
	if ((!ConstraintSection) || (!TransformSection))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("KeyConstraintaKehy", "Key Constraint Key"));
	TransformSection->Modify();
	ConstraintSection->Modify();

	// set constraint as dynamic
	InConstraint->bDynamicOffset = true;
	
	// add the channel
	ConstraintSection->AddConstraintChannel(InConstraint);

	// add key if needed
	if (FConstraintAndActiveChannel* Channel = ConstraintSection->GetConstraintChannel(InConstraint->GetFName()))
	{
		const FFrameRate TickResolution = InSequencer->GetFocusedTickResolution();
		const FFrameTime FrameTime = InSequencer->GetLocalTime().ConvertTo(TickResolution);
		const FFrameNumber Time = FrameTime.GetFrame();

		bool ActiveValueToBeSet = false;
		if (CanAddKey(Channel->ActiveChannel, Time, ActiveValueToBeSet))
		{
			const bool bNeedsCompensation = InConstraint->NeedsCompensation();

			//new for compensation

			TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);

			// store the frames to compensate
			const TArrayView<FMovieSceneDoubleChannel*> Channels = ComponentHandle->GetDoubleChannels(TransformSection);
			TArray<FFrameNumber> FramesToCompensate;
			if (bNeedsCompensation)
			{
				FMovieSceneConstraintChannelHelper::GetFramesToCompensate<FMovieSceneDoubleChannel>(Channel->ActiveChannel, ActiveValueToBeSet, Time, Channels, FramesToCompensate);
			}
			else
			{
				FramesToCompensate.Add(Time);
			}

			// store child and space transforms for these frames
			FCompensationEvaluator Evaluator(InConstraint);
			Evaluator.ComputeLocalTransforms(Actor->GetWorld(), InSequencer, FramesToCompensate, ActiveValueToBeSet);
			TArray<FTransform>& ChildLocals = Evaluator.ChildLocals;

			// store tangents at this time
			TArray<FMovieSceneTangentData> Tangents;
			const int32 ChannelIndex = 0;
			const int32 NumChannels = 9;

			if (bNeedsCompensation)
			{
				EvaluateTangentAtThisTime<FMovieSceneDoubleChannel>(ChannelIndex, NumChannels, TransformSection, Time, Tangents);
			}

			const EMovieSceneTransformChannel ChannelsToKey = InConstraint->GetChannelsToKey();

			// add child's transform key at Time-1 to keep animation
			if (bNeedsCompensation)
			{
				const FFrameNumber TimeMinusOne(Time - 1);

				TArray<FTransform> TransformMinusOne({ChildLocals[0]} );
				if (ConstraintSection != TransformSection)
				{
					RecomposeTransforms(InSequencer, ComponentHandle->Component.Get(), TransformSection, { TimeMinusOne },TransformMinusOne);
				}
				MovieSceneToolHelpers::AddTransformKeys(TransformSection, { TimeMinusOne }, TransformMinusOne, ChannelsToKey);

				SetTangentsAtThisTime<FMovieSceneDoubleChannel>(ChannelIndex, NumChannels, TransformSection, TimeMinusOne, Tangents);
			}

			// add active key
			{
				TMovieSceneChannelData<bool> ChannelData = Channel->ActiveChannel.GetData();
				ChannelData.AddKey(Time, ActiveValueToBeSet);
			}

			// compensate
			{
				// we need to remove the first transforms as we store NumFrames+1 transforms
				ChildLocals.RemoveAt(0);

				if (ConstraintSection != TransformSection)
				{
					RecomposeTransforms(InSequencer, ComponentHandle->Component.Get(), TransformSection, FramesToCompensate,ChildLocals);
				}

				// add keys
				MovieSceneToolHelpers::AddTransformKeys(TransformSection, FramesToCompensate, ChildLocals, ChannelsToKey);

				// set tangents at Time
				if (bNeedsCompensation)
				{
					SetTangentsAtThisTime<FMovieSceneDoubleChannel>(ChannelIndex,NumChannels, TransformSection, Time, Tangents);
				}
			}
			// evaluate the constraint, this is needed so the global transform will be set up on the component 
			//Todo do we need to evaluate all constraints?
			InConstraint->SetActive(true); //will be false
			InConstraint->Evaluate();

			//need to fire this event so the transform values set by the constraint propragate to the transform section
			//first turn off autokey though
			EAutoChangeMode AutoChangeMode = InSequencer->GetAutoChangeMode();
			if (AutoChangeMode == EAutoChangeMode::AutoKey || AutoChangeMode == EAutoChangeMode::All)
			{
				InSequencer->SetAutoChangeMode(EAutoChangeMode::None);
			};
			FProperty* TransformProperty = FindFProperty<FProperty>(USceneComponent::StaticClass(), USceneComponent::GetRelativeLocationPropertyName());
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(TransformProperty);
			FCoreUObjectDelegates::OnPreObjectPropertyChanged.Broadcast(Actor, PropertyChain);
			FPropertyChangedEvent PropertyChangedEvent(TransformProperty, EPropertyChangeType::ValueSet);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(Actor, PropertyChangedEvent);
			InSequencer->RequestEvaluate();
			if (AutoChangeMode == EAutoChangeMode::AutoKey || AutoChangeMode == EAutoChangeMode::All)
			{
				InSequencer->SetAutoChangeMode(AutoChangeMode);
			}
		}
	}
}

void FConstraintChannelHelper::Compensate(UTickableTransformConstraint* InConstraint, const bool bAllTimes)
{
	const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
	if (!WeakSequencer.IsValid() || !WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		return;
	}
	const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	IMovieSceneConstrainedSection* Section = nullptr;
	UWorld* World = nullptr;
	if (const UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(InConstraint->ChildTRSHandle))
	{
		AActor* Actor = ComponentHandle->Component->GetOwner();
		if (!Actor)
		{
			return;
		}

		const FGuid Guid = Sequencer->GetHandleToObject(Actor, true);
		if (!Guid.IsValid())
		{
			return;
		}

		World = Actor->GetWorld();
		
		const FTransform LocalTransform = ComponentHandle->GetLocalTransform();
		Section = GetComponentConstraintSection(Sequencer, Guid, LocalTransform);
	}

	if (const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(InConstraint->ChildTRSHandle))
	{
		const UControlRig* ControlRig = ControlHandle->ControlRig.LoadSynchronous();
		if (!ControlRig)
		{
			return;
		}
		World = ControlRig->GetWorld();
		Section = GetControlConstraintSection(ControlHandle,Sequencer);
	}

	if (!Section)
	{
		return;
	}
	
	const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
	const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
	const FFrameNumber Time = FrameTime.GetFrame();

	const TOptional<FFrameNumber> OptTime = bAllTimes ? TOptional<FFrameNumber>() : TOptional<FFrameNumber>(Time);
	CompensateIfNeeded(World, Sequencer, Section, OptTime);
}

void FConstraintChannelHelper::CompensateIfNeeded(
	UWorld* InWorld,
	const TSharedPtr<ISequencer>& InSequencer,
	IMovieSceneConstrainedSection* ConstraintSection,
	const TOptional<FFrameNumber>& OptionalTime)
{
	if (FMovieSceneConstraintChannelHelper::bDoNotCompensate)
	{
		return;
	}

	TGuardValue<bool> CompensateGuard(FMovieSceneConstraintChannelHelper::bDoNotCompensate, true);

	// Frames to compensate
	TArray<FFrameNumber> OptionalTimeArray;
	if (OptionalTime.IsSet())
	{
		OptionalTimeArray.Add(OptionalTime.GetValue());
	}

	auto GetSpaceTimesToCompensate = [&OptionalTimeArray](const FConstraintAndActiveChannel& Channel)->TArrayView<const FFrameNumber>
	{
		if (OptionalTimeArray.IsEmpty())
		{
			return Channel.ActiveChannel.GetData().GetTimes();
		}
		return OptionalTimeArray;
	};

	bool bNeedsEvaluation = false;

	// gather all transform constraints
	TArray<FConstraintAndActiveChannel>& ConstraintChannels = ConstraintSection->GetConstraintsChannels();
	TArray<FConstraintAndActiveChannel> TransformConstraintsChannels;
	Algo::CopyIf(ConstraintChannels, TransformConstraintsChannels,
		[](const FConstraintAndActiveChannel& InChannel)
		{
			if (!InChannel.Constraint.IsValid())
			{
				return false;
			}

			const UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InChannel.Constraint.Get());
			return Constraint && Constraint->NeedsCompensation();
		}
	);

	// compensate constraints
	for (const FConstraintAndActiveChannel& Channel : TransformConstraintsChannels)
	{
		const TArrayView<const FFrameNumber> FramesToCompensate = GetSpaceTimesToCompensate(Channel);
		for (const FFrameNumber& Time : FramesToCompensate)
		{
			const FFrameNumber TimeMinusOne(Time - 1);

			bool CurrentValue = false, PreviousValue = false;
			Channel.ActiveChannel.Evaluate(TimeMinusOne, PreviousValue);
			Channel.ActiveChannel.Evaluate(Time, CurrentValue);

			if (CurrentValue != PreviousValue) //if they are the same no need to do anything
			{
				UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(Channel.Constraint.Get());

				// compute transform to set
				// if switching from active to inactive then we must add a key at T-1 in the constraint space
				// if switching from inactive to active then we must add a key at T-1 in the previous constraint or parent space
				FCompensationEvaluator Evaluator(Constraint);
				Evaluator.ComputeCompensation(InWorld, InSequencer, Time);
				const TArray<FTransform>& LocalTransforms = Evaluator.ChildLocals;

				const EMovieSceneTransformChannel ChannelsToKey = Constraint->GetChannelsToKey();
				FConstraintBaker::AddTransformKeys(
					InSequencer, Constraint->ChildTRSHandle, { TimeMinusOne }, LocalTransforms, ChannelsToKey);
				bNeedsEvaluation = true;
			}
		}
	}

	if (bNeedsEvaluation)
	{
		InSequencer->ForceEvaluate();
	}
}

#undef LOCTEXT_NAMESPACE
