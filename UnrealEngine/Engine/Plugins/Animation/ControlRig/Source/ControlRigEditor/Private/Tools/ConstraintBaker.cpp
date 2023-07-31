// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ConstraintBaker.h"

#include "IControlRigObjectBinding.h"
#include "TransformConstraint.h"

#include "ISequencer.h"

#include "ControlRig.h"
#include "IKeyArea.h"
#include "LevelSequence.h"
#include "MovieSceneToolHelpers.h"
#include "TransformableHandle.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Constraints/ControlRigTransformableHandle.h"
#include "MovieSceneConstraintChannelHelper.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "ConstraintChannelHelper.h"
#include "ConstraintChannel.h"
#include "ConstraintChannelHelper.inl"
#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Systems/MovieScenePropertyInstantiator.h"

#define LOCTEXT_NAMESPACE "ConstraintBaker"

namespace MoveMeFor52
{
	UMovieScene* GetMovieScene(const TSharedPtr<ISequencer>& InSequencer)
	{
		const UMovieSceneSequence* MovieSceneSequence = InSequencer ? InSequencer->GetFocusedMovieSceneSequence() : nullptr;
		return MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	}
	
	UMovieScene3DTransformSection* GetTransformSection(
		const TSharedPtr<ISequencer>& InSequencer,
		AActor* InActor,
		const FTransform& InTransform0)
	{
		if (!InSequencer || !InSequencer->GetFocusedMovieSceneSequence())
		{
			return nullptr;
		}

		const FGuid Guid = InSequencer->GetHandleToObject(InActor, true);
		if (!Guid.IsValid())
		{
			return nullptr;
		}

		return MovieSceneToolHelpers::GetTransformSection(InSequencer.Get(), Guid, InTransform0);
	}

	// NOTE, because of restriction for hotfix changes this code some of the following functions are duplicated in
	// ConstraintChannelHelper.cpp. This will be factorized as part of a refactor for 5.2
	UMovieScene3DTransformSection* GetComponentConstraintSection(
		const TSharedPtr<ISequencer>& InSequencer,
		const FGuid& InGuid,
		const FTransform& InDefaultTransform)
	{
		UMovieScene* MovieScene = MoveMeFor52::GetMovieScene(InSequencer);
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
	
	void BakeComponent(
		const TSharedPtr<ISequencer>& InSequencer,
		const UTransformableComponentHandle* InComponentHandle,
		const TArray<FFrameNumber>& InFrames,
		const TArray<FTransform>& InTransforms,
		const EMovieSceneTransformChannel& InChannels)
	{
		ensure(InTransforms.Num());

		if (!InComponentHandle->IsValid())
		{
			return;
		}
		AActor* Actor = InComponentHandle->Component->GetOwner();
		if (!Actor)
		{
			return;
		}
		
		UMovieScene3DTransformSection* TransformSection = MoveMeFor52::GetTransformSection(InSequencer, Actor, InTransforms[0]);
		if (!TransformSection)
		{
			return;
		}

		const UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		const FFrameRate TickResolution = MovieScene->GetTickResolution();

		const FGuid Guid = InSequencer->GetHandleToObject(Actor, false);
		const bool bNeedsRecomposition = TransformSection != MoveMeFor52::GetComponentConstraintSection(InSequencer, Guid, InTransforms[0]);
		if (bNeedsRecomposition)
		{
			TArray<FTransform> Transforms(InTransforms);
			MoveMeFor52::RecomposeTransforms(InSequencer, InComponentHandle->Component.Get(), TransformSection, InFrames, Transforms);
			InComponentHandle->AddTransformKeys(InFrames, Transforms, InChannels, TickResolution, TransformSection, true);
		}
		else
		{
			InComponentHandle->AddTransformKeys(InFrames, InTransforms, InChannels, TickResolution, TransformSection, true);
		}
	}

	void BakeControl(
		const TSharedPtr<ISequencer>& InSequencer,
		const UTransformableControlHandle* InControlHandle,
		const TArray<FFrameNumber>& InFrames,
		const TArray<FTransform>& InLocalTransforms,
		const EMovieSceneTransformChannel& InChannels)
	{
		ensure(InLocalTransforms.Num());

		if (!InControlHandle->IsValid())
		{
			return;
		}

		const UMovieScene* MovieScene = InSequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
		UMovieSceneSection* Section = nullptr; //cnntrol rig doesn't need section it instead
		InControlHandle->AddTransformKeys(InFrames, InLocalTransforms, InChannels, MovieScene->GetTickResolution(), Section, true);
	}

	UMovieSceneControlRigParameterSection* GetControlConstraintSection(
	const UTransformableControlHandle* InHandle,
	const TSharedPtr<ISequencer>& InSequencer)
	{
		const UMovieScene* MovieScene = MoveMeFor52::GetMovieScene(InSequencer);
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
}


void FConstraintBaker::AddTransformKeys(
	const TSharedPtr<ISequencer>& InSequencer,
	UTransformableHandle* InHandle,
	const TArray<FFrameNumber>& InFrames,
	const TArray<FTransform>& InTransforms,
	const EMovieSceneTransformChannel& InChannels)
{
	if (const UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(InHandle))
	{
		return MoveMeFor52::BakeComponent(InSequencer, ComponentHandle, InFrames, InTransforms, InChannels);
	}
	
	if (const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(InHandle))
	{
		return MoveMeFor52::BakeControl(InSequencer, ControlHandle, InFrames, InTransforms, InChannels);
	}
}

// at this stage, we suppose that any of these arguments are safe to use. Make sure to test them before using that function
void FConstraintBaker::GetMinimalFramesToBake(
	UWorld* InWorld,
	const UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer,
	IMovieSceneConstrainedSection* InSection,
	TArray<FFrameNumber>& OutFramesToBake)
{
	const UMovieSceneSequence* MovieSceneSequence = InSequencer->GetFocusedMovieSceneSequence();
	const UMovieScene* MovieScene = MovieSceneSequence ? MovieSceneSequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	// get constraint channel data if any
	TArrayView<const FFrameNumber> ConstraintFrames;
	TArrayView<const bool> ConstraintValues;

	// note that we might want to bake a constraint which is not animated
	FConstraintAndActiveChannel* ThisActiveChannel = InSection->GetConstraintChannel(InConstraint->GetFName());
	if (ThisActiveChannel)
	{
		const TMovieSceneChannelData<const bool> ConstraintChannelData = ThisActiveChannel->ActiveChannel.GetData();
		ConstraintFrames = ConstraintChannelData.GetTimes();
		ConstraintValues = ConstraintChannelData.GetValues();
	}

	// there's no channel or an empty one and the constraint is inactive so no need to do anything if the constraint
	// is not active
	if (ConstraintFrames.IsEmpty() && !InConstraint->IsFullyActive())
	{
		return;
	}

	// default init bounds to the scene bounds
	FFrameNumber FirstBakingFrame = MovieScene->GetPlaybackRange().GetLowerBoundValue();
	FFrameNumber LastBakingFrame = MovieScene->GetPlaybackRange().GetUpperBoundValue();
	
	// set start to first active frame if any
	const int32 FirstActiveIndex = ConstraintValues.IndexOfByKey(true);
	if (ConstraintValues.IsValidIndex(FirstActiveIndex))
	{
		FirstBakingFrame = ConstraintFrames[FirstActiveIndex];
	}

	// set end to the last key if inactive
	const bool bIsLastKeyInactive = (ConstraintValues.Last() == false);
	if (bIsLastKeyInactive)
	{
		LastBakingFrame = ConstraintFrames.Last();
	}

	// then compute range from first to last
	TArray<FFrameNumber> Frames;
	MovieSceneToolHelpers::CalculateFramesBetween(MovieScene, FirstBakingFrame, LastBakingFrame, Frames);
		
	// Fill the frames we want to bake
	{
		// add constraint keys
		for (const FFrameNumber& ConstraintFrame: ConstraintFrames)
		{
			OutFramesToBake.Add(ConstraintFrame);
		}
		
		// keep frames within active state
		auto IsConstraintActive = [InConstraint, ThisActiveChannel](const FFrameNumber& Time)
		{
			if (ThisActiveChannel)
			{
				bool bIsActive = false; ThisActiveChannel->ActiveChannel.Evaluate(Time, bIsActive);
				return bIsActive;
			}
			return InConstraint->IsFullyActive();
		};
		
		for (const FFrameNumber& InFrame: Frames)
		{
			if (IsConstraintActive(InFrame))
			{
				OutFramesToBake.Add(InFrame);
			}
		}
	}

	// we also need to store which T-1 frames need to be kept for other constraints compensation
	{
		// gather the sorted child's constraint
		static constexpr bool bSorted = true;
		const uint32 ChildHash = InConstraint->ChildTRSHandle->GetHash();
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		using ConstraintPtr = TObjectPtr<UTickableConstraint>;
		const TArray<ConstraintPtr> Constraints = Controller.GetParentConstraints(ChildHash, bSorted);

		// store the other channels that may need compensation
		TArray<FConstraintAndActiveChannel*> OtherChannels;
		for (const ConstraintPtr& Constraint: Constraints)
		{
			const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint.Get());
			if (TransformConstraint && TransformConstraint->NeedsCompensation() && TransformConstraint != InConstraint) 
			{
				const FName ConstraintName = TransformConstraint->GetFName();
				if (FConstraintAndActiveChannel* ConstraintChannel = InSection->GetConstraintChannel(ConstraintName))
				{
					OtherChannels.Add(ConstraintChannel);
				}
			}
		}

		// check if any other channel needs to compensate at T
		for (const FFrameNumber& ConstraintFrame: ConstraintFrames)
		{
			const bool bNeedsCompensation = OtherChannels.ContainsByPredicate(
		[ConstraintFrame](const FConstraintAndActiveChannel* OtherChannel)
			{
				const bool bHasAtLeastOneActiveKey = OtherChannel->ActiveChannel.GetValues().Contains(true);
				if (!bHasAtLeastOneActiveKey)
				{
					return false;
				}
				return OtherChannel->ActiveChannel.GetTimes().Contains(ConstraintFrame);
			} );

			// add T-1 if needed so that we will compensate int the remaining constraints' space 
			if (bNeedsCompensation)
			{
				const FFrameNumber& FrameMinusOne(ConstraintFrame-1);
				OutFramesToBake.Add(FrameMinusOne);
			}
		}
	}
	
	// uniqueness
	OutFramesToBake.Sort();
	OutFramesToBake.SetNum(Algo::Unique(OutFramesToBake));
}

void FConstraintBaker::Bake(UWorld* InWorld, 
	UTickableTransformConstraint* InConstraint,
	const TSharedPtr<ISequencer>& InSequencer, 
	const TOptional<TArray<FFrameNumber>>& InFrames)
{
	if ((InFrames && InFrames->IsEmpty()) || (InConstraint == nullptr) || (InConstraint->ChildTRSHandle == nullptr))
	{
		return;
	}

	const TObjectPtr<UTransformableHandle>& ChildHandle = InConstraint->ChildTRSHandle;
	
	//get the section to be used later to delete the extra transform keys at the frame -1 times, abort if not there for some reason
	UMovieSceneSection* ConstraintSection = nullptr;
	UMovieSceneSection* TransformSection = nullptr;
	if (const UTransformableControlHandle* ControlHandle = Cast<UTransformableControlHandle>(ChildHandle))
	{
		TransformSection = FConstraintChannelHelper::GetControlSection(ControlHandle, InSequencer);
		ConstraintSection = MoveMeFor52::GetControlConstraintSection(ControlHandle, InSequencer);
	}
	else if (const UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(ChildHandle))
	{ 
		//todo move to function also used by SmartConstraintKey
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

		TransformSection = MovieSceneToolHelpers::GetTransformSection(InSequencer.Get(), Guid, LocalTransform);
		ConstraintSection = MoveMeFor52::GetComponentConstraintSection(InSequencer, Guid, LocalTransform);
	}

	IMovieSceneConstrainedSection* ConstrainedSection = Cast<IMovieSceneConstrainedSection>(ConstraintSection);
	if (ConstrainedSection == nullptr || TransformSection == nullptr)
	{
		return;
	}
	
	FConstraintAndActiveChannel* ActiveChannel = ConstrainedSection->GetConstraintChannel(InConstraint->GetFName());
	if (ActiveChannel == nullptr)
	{
		return;
	}

	// compute transforms
	TArray<FFrameNumber> FramesToBake;
	if (InFrames)
	{
		FramesToBake = *InFrames;
	}
	else
	{
		GetMinimalFramesToBake(InWorld, InConstraint, InSequencer, ConstrainedSection, FramesToBake);

		// if it needs recomposition 
		if (ConstraintSection != TransformSection && !FramesToBake.IsEmpty())
		{
			const TMovieSceneChannelData<const bool> ConstraintChannelData = ActiveChannel->ActiveChannel.GetData();
			const TArrayView<const FFrameNumber> ConstraintFrames = ConstraintChannelData.GetTimes();
			const TArrayView<const bool> ConstraintValues = ConstraintChannelData.GetValues();
			for (int32 Index = 0; Index < ConstraintFrames.Num(); ++Index)
			{
				// if the key is active
				if (ConstraintValues[Index])
				{
					const int32 FrameIndex = FramesToBake.IndexOfByKey(ConstraintFrames[Index]);
					if (FrameIndex != INDEX_NONE)
					{
						// T-1 is not part of the frames to bake, then we need to compensate:
						// in the context of additive, we need to add a 'zeroed' key to prevent from popping
						const FFrameNumber FrameMinusOne(ConstraintFrames[Index] - 1);
						const bool bAddMinusOne = (FrameIndex == 0) ? true : !ConstraintFrames.Contains(FrameMinusOne);  
						if (bAddMinusOne)
						{
							FramesToBake.Insert(FrameMinusOne, FrameIndex);
						}
					}
				}
			}
		}
	}
	
	FCompensationEvaluator Evaluator(InConstraint);
	Evaluator.ComputeLocalTransformsForBaking(InWorld, InSequencer, FramesToBake);
	const TArray<FTransform>& Transforms = Evaluator.ChildLocals;
	if (FramesToBake.Num() != Transforms.Num())
	{
		return;
	}
	
	ConstraintSection->Modify();

	// disable constraint and delete extra transform keys
	TMovieSceneChannelData<bool> ConstraintChannelData = ActiveChannel->ActiveChannel.GetData();
	const TArrayView<const FFrameNumber> ConstraintFrames = ConstraintChannelData.GetTimes();
	
	// get transform channels
	const TArrayView<FMovieSceneFloatChannel*> FloatTransformChannels = ChildHandle->GetFloatChannels(TransformSection);
	const TArrayView<FMovieSceneDoubleChannel*> DoubleTransformChannels = ChildHandle->GetDoubleChannels(TransformSection);

	for (int32 Index = 0; Index < ConstraintFrames.Num(); ++Index)
	{
		const FFrameNumber Frame = ConstraintFrames[Index];
		//todo we need to add a key at the begin/end if there is no frame there
		if (Frame >= FramesToBake[0] && Frame <= FramesToBake.Last())
		{
			// set constraint key to inactive
			ConstraintChannelData.UpdateOrAddKey(Frame, false);

			// delete minus one transform frames
			const FFrameNumber FrameMinusOne = Frame - 1;
			if (FloatTransformChannels.Num() > 0)
			{
				FMovieSceneConstraintChannelHelper::DeleteTransformKeys(FloatTransformChannels, FrameMinusOne);
			}
			else if (DoubleTransformChannels.Num() > 0)
			{
				FMovieSceneConstraintChannelHelper::DeleteTransformKeys(DoubleTransformChannels, FrameMinusOne);
			}
		}
	}

	// now bake to channel curves
	const EMovieSceneTransformChannel Channels = InConstraint->GetChannelsToKey();
	AddTransformKeys(InSequencer, ChildHandle, FramesToBake, Transforms, Channels);

	// notify
	InSequencer->RequestEvaluate();
}

#undef LOCTEXT_NAMESPACE


