// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContextualAnimViewModel.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "ISequencerModule.h"
#include "MovieSceneCommonHelpers.h"
#include "ContextualAnimMovieSceneSequence.h"
#include "ContextualAnimMovieSceneTrack.h"
#include "ContextualAnimMovieSceneSection.h"
#include "ContextualAnimMovieSceneNotifyTrackEditor.h"
#include "ContextualAnimMovieSceneNotifyTrack.h"
#include "ContextualAnimMovieSceneNotifySection.h"
#include "ContextualAnimPreviewScene.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EngineUtils.h"
#include "Misc/TransactionObjectEvent.h"
#include "ContextualAnimTypes.h"
#include "ContextualAnimEditorTypes.h"
#include "ContextualAnimUtilities.h"
#include "ContextualAnimSceneAsset.h"
#include "ContextualAnimManager.h"
#include "ContextualAnimSceneInstance.h"
#include "AnimNotifyState_IKWindow.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "MotionWarpingComponent.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "ContextualAnimSelectionCriterion.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "ContextualAnimViewModel"

FContextualAnimViewModel::FContextualAnimViewModel()
	: SceneAsset(nullptr)
	, MovieSceneSequence(nullptr)
	, MovieScene(nullptr)
	, ContextualAnimManager(nullptr)
{
}

FContextualAnimViewModel::~FContextualAnimViewModel()
{
	if (Sequencer.IsValid())
	{
		Sequencer->OnMovieSceneDataChanged().RemoveAll(this);
		Sequencer->OnGlobalTimeChanged().RemoveAll(this);
		Sequencer.Reset();
	}
}

void FContextualAnimViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SceneAsset);
	Collector.AddReferencedObject(MovieSceneSequence);
	Collector.AddReferencedObject(MovieScene);
	Collector.AddReferencedObject(ContextualAnimManager);
}

TSharedPtr<ISequencer> FContextualAnimViewModel::GetSequencer()
{
	return Sequencer;
}

void FContextualAnimViewModel::Initialize(UContextualAnimSceneAsset* InSceneAsset, const TSharedRef<FContextualAnimPreviewScene>& InPreviewScene)
{
	SceneAsset = InSceneAsset;
	PreviewScenePtr = InPreviewScene;

	ContextualAnimManager = NewObject<UContextualAnimManager>(GetWorld());

	CreateSequencer();

	SetDefaultMode();
}

void FContextualAnimViewModel::CreateSequencer()
{
	MovieSceneSequence = NewObject<UContextualAnimMovieSceneSequence>(GetTransientPackage());
	MovieSceneSequence->Initialize(AsShared());

	MovieScene = NewObject<UMovieScene>(MovieSceneSequence, FName("ContextualAnimMovieScene"), RF_Transactional);
	MovieScene->SetDisplayRate(FFrameRate(30, 1));

	FSequencerViewParams ViewParams(TEXT("ContextualAnimSequenceSettings"));
	{
		ViewParams.UniqueName = "ContextualAnimSequenceEditor";
		//ViewParams.OnGetAddMenuContent = OnGetSequencerAddMenuContent;
		//ViewParams.OnGetPlaybackSpeeds = ISequencer::FOnGetPlaybackSpeeds::CreateRaw(this, &FContextualAnimViewModel::GetPlaybackSpeeds);
	}

	FSequencerInitParams SequencerInitParams;
	{
		SequencerInitParams.ViewParams = ViewParams;
		SequencerInitParams.RootSequence = MovieSceneSequence;
		SequencerInitParams.bEditWithinLevelEditor = false;
		SequencerInitParams.ToolkitHost = nullptr;
		SequencerInitParams.PlaybackContext.Bind(this, &FContextualAnimViewModel::GetPlaybackContext);
	}

	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked< ISequencerModule >("Sequencer");
	Sequencer = SequencerModule.CreateSequencer(SequencerInitParams);
	Sequencer->OnMovieSceneDataChanged().AddRaw(this, &FContextualAnimViewModel::SequencerDataChanged);
	Sequencer->OnGlobalTimeChanged().AddRaw(this, &FContextualAnimViewModel::SequencerTimeChanged);
	Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Stopped);
}

float FContextualAnimViewModel::GetPlaybackTime() const
{
	return Sequencer->GetGlobalTime().AsSeconds();
}

void FContextualAnimViewModel::SetActiveSection(int32 SectionIdx)
{
	check(GetSceneAsset()->Sections.IsValidIndex(SectionIdx));

	ActiveSectionIdx = SectionIdx;

	SetDefaultMode();
}

void FContextualAnimViewModel::SetActiveAnimSetForSection(int32 SectionIdx, int32 AnimSetIdx)
{
	check(GetSceneAsset()->Sections.IsValidIndex(SectionIdx));
	check(GetSceneAsset()->Sections[SectionIdx].AnimSets.IsValidIndex(AnimSetIdx));

	int32& ActiveSetIdx = ActiveAnimSetMap.FindOrAdd(SectionIdx);
	ActiveSetIdx = AnimSetIdx;

	SetDefaultMode();
}

AActor* FContextualAnimViewModel::SpawnPreviewActor(const FContextualAnimTrack& AnimTrack)
{
	if (AnimTrack.Animation == nullptr)
	{
		return nullptr;
	}

	USkeleton* Skeleton = AnimTrack.Animation->GetSkeleton();

	if (Skeleton == nullptr)
	{
		return nullptr;
	}

	const FContextualAnimRoleDefinition* RoleDef = GetSceneAsset()->RolesAsset ? GetSceneAsset()->RolesAsset->FindRoleDefinitionByName(AnimTrack.Role) : nullptr;
	const bool bIsCharacter = (RoleDef && RoleDef->bIsCharacter);

	const FTransform SpawnTransform = AnimTrack.GetRootTransformAtTime(0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (bIsCharacter)
	{
		ACharacter* PreviewCharacter = GetWorld()->SpawnActor<ACharacter>(ACharacter::StaticClass(), SpawnTransform, Params);
		PreviewCharacter->SetFlags(RF_Transient);

		USkeletalMeshComponent* SkelMeshComp = PreviewCharacter->GetMesh();
		SkelMeshComp->SetRelativeLocation(FVector(0.f, 0.f, -PreviewCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		SkelMeshComp->SetRelativeRotation(RoleDef->MeshToComponent.GetRotation());
		SkelMeshComp->SetSkeletalMesh(Skeleton->GetPreviewMesh(true));
		SkelMeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		SkelMeshComp->SetAnimInstanceClass(UAnimPreviewInstance::StaticClass());

		UAnimPreviewInstance* AnimInstance = Cast<UAnimPreviewInstance>(SkelMeshComp->GetAnimInstance());
		AnimInstance->SetAnimationAsset(AnimTrack.Animation, false, 0.f);
		AnimInstance->PlayAnim(false, 0.f);

		PreviewCharacter->CacheInitialMeshOffset(SkelMeshComp->GetRelativeLocation(), SkelMeshComp->GetRelativeRotation());

		if (UCharacterMovementComponent* CharacterMovementComp = PreviewCharacter->GetCharacterMovement())
		{
			CharacterMovementComp->bOrientRotationToMovement = true;
			CharacterMovementComp->bUseControllerDesiredRotation = false;
			CharacterMovementComp->RotationRate = FRotator(0.f, 540.0, 0.f);
			CharacterMovementComp->bRunPhysicsWithNoController = true;

			CharacterMovementComp->SetMovementMode(AnimTrack.bRequireFlyingMode ? EMovementMode::MOVE_Flying : EMovementMode::MOVE_Walking);
		}

		UMotionWarpingComponent* MotionWarpingComp = NewObject<UMotionWarpingComponent>(PreviewCharacter);
		MotionWarpingComp->RegisterComponentWithWorld(GetWorld());
		MotionWarpingComp->InitializeComponent();

		return PreviewCharacter;
	}
	else
	{
		AActor* PreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnTransform, Params);
		PreviewActor->SetFlags(RF_Transient);

		UDebugSkelMeshComponent* SkelMeshComp = NewObject<UDebugSkelMeshComponent>(PreviewActor);
		SkelMeshComp->RegisterComponentWithWorld(GetWorld());

		UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(SkelMeshComp);
		SkelMeshComp->PreviewInstance = AnimInstance;
		AnimInstance->InitializeAnimation();

		SkelMeshComp->SetSkeletalMesh(Skeleton->GetPreviewMesh(true));
		SkelMeshComp->EnablePreview(true, AnimTrack.Animation);

		AnimInstance->SetAnimationAsset(AnimTrack.Animation, false, 0.0f);

		AnimInstance->PlayAnim(false, 0.0f);

		if (!PreviewActor->GetRootComponent())
		{
			PreviewActor->SetRootComponent(SkelMeshComp);
		}

		return PreviewActor;
	}
}

void FContextualAnimViewModel::ResetTimeline()
{
	for (int32 MasterTrackIdx = MovieScene->GetMasterTracks().Num() - 1; MasterTrackIdx >= 0; MasterTrackIdx--)
	{
		UMovieSceneTrack& MasterTrack = *MovieScene->GetMasterTracks()[MasterTrackIdx];
		MovieScene->RemoveMasterTrack(MasterTrack);
	}

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FContextualAnimViewModel::SetDefaultMode()
{
	if (EditingAnimation.IsValid())
	{
		EditingAnimation->UnregisterOnNotifyChanged(this);
		EditingAnimation.Reset();
	}

	TimelineMode = ETimelineMode::Default;

	ResetTimeline();

	if (!SceneAsset->Sections.IsValidIndex(ActiveSectionIdx))
	{
		RefreshPreviewScene();
		return;
	}

	TArray<FName> Roles = SceneAsset->GetRoles();
	for (FName Role : Roles)
	{
		UContextualAnimMovieSceneTrack* MovieSceneAnimTrack = MovieSceneSequence->GetMovieScene()->AddMasterTrack<UContextualAnimMovieSceneTrack>();
		check(MovieSceneAnimTrack);

		MovieSceneAnimTrack->Initialize(Role);
	}

	FContextualAnimSceneSection& ContextualAnimSection = SceneAsset->Sections[ActiveSectionIdx];
	for (int32 AnimSetIdx = 0; AnimSetIdx < ContextualAnimSection.AnimSets.Num(); AnimSetIdx++)
	{
		FContextualAnimSet& AnimSet = ContextualAnimSection.AnimSets[AnimSetIdx];
		for (int32 AnimTrackIdx = 0; AnimTrackIdx < AnimSet.Tracks.Num(); AnimTrackIdx++)
		{
			const FContextualAnimTrack& AnimTrack = AnimSet.Tracks[AnimTrackIdx];

			UAnimSequenceBase* Animation = AnimTrack.Animation;
			if (Animation)
			{
				UContextualAnimMovieSceneTrack* MovieSceneTrack = FindMasterTrackByRole(AnimTrack.Role);
				check(MovieSceneTrack)

				UContextualAnimMovieSceneSection* NewSection = NewObject<UContextualAnimMovieSceneSection>(MovieSceneTrack, UContextualAnimMovieSceneSection::StaticClass(), NAME_None, RF_Transactional);
				check(NewSection);

				NewSection->Initialize(ActiveSectionIdx, AnimSetIdx, AnimTrackIdx);

				const float AnimLength = Animation->GetPlayLength();
				const FFrameRate TickResolution = MovieSceneSequence->GetMovieScene()->GetTickResolution();
				NewSection->SetRange(TRange<FFrameNumber>::Inclusive(0, (AnimLength * TickResolution).RoundToFrame()));

				NewSection->SetRowIndex(AnimSetIdx);

				const int32& ActiveSetIdx = ActiveAnimSetMap.FindOrAdd(ActiveSectionIdx);
				NewSection->SetIsActive(AnimSetIdx == ActiveSetIdx);

				MovieSceneTrack->AddSection(*NewSection);
				MovieSceneTrack->SetTrackRowDisplayName(FText::FromString(FString::Printf(TEXT("%d"), AnimSetIdx)), AnimSetIdx);
			}
		}
	}

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);

	RefreshPreviewScene();
}

void FContextualAnimViewModel::SetNotifiesMode(const FContextualAnimTrack& AnimTrack)
{
	ResetTimeline();

	TimelineMode = ETimelineMode::Notifies;

	if(EditingAnimation.IsValid())
	{
		EditingAnimation->UnregisterOnNotifyChanged(this);
	}

	EditingAnimation = AnimTrack.Animation;

	// Add Animation Track
	{
		UContextualAnimMovieSceneTrack* MovieSceneAnimTrack = MovieSceneSequence->GetMovieScene()->AddMasterTrack<UContextualAnimMovieSceneTrack>();
		check(MovieSceneAnimTrack);

		MovieSceneAnimTrack->SetDisplayName(FText::FromString(GetNameSafe(EditingAnimation.Get())));

		UContextualAnimMovieSceneSection* NewSection = NewObject<UContextualAnimMovieSceneSection>(MovieSceneAnimTrack, UContextualAnimMovieSceneSection::StaticClass(), NAME_None, RF_Transactional);
		check(NewSection);

		NewSection->Initialize(AnimTrack.SectionIdx, AnimTrack.AnimSetIdx, AnimTrack.AnimTrackIdx);

		FFrameRate TickResolution = MovieSceneSequence->GetMovieScene()->GetTickResolution();
		FFrameNumber StartFrame(0);
		FFrameNumber EndFrame = (EditingAnimation->GetPlayLength() * TickResolution).RoundToFrame();
		NewSection->SetRange(TRange<FFrameNumber>::Exclusive(StartFrame, EndFrame));

		MovieSceneAnimTrack->AddSection(*NewSection);
	}

	// Add Notify Tracks
	{
		for (const FAnimNotifyTrack& NotifyTrack : EditingAnimation->AnimNotifyTracks)
		{
			UContextualAnimMovieSceneNotifyTrack* Track = MovieSceneSequence->GetMovieScene()->AddMasterTrack<UContextualAnimMovieSceneNotifyTrack>();
			check(Track);

			Track->Initialize(*EditingAnimation, NotifyTrack);
		}

		// Listen for when the notifies in the animation changes, so we can refresh the notify sections here
		AnimTrack.Animation->RegisterOnNotifyChanged(UAnimSequenceBase::FOnNotifyChanged::CreateSP(this, &FContextualAnimViewModel::OnAnimNotifyChanged, EditingAnimation.Get()));
	}

	Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}

void FContextualAnimViewModel::RefreshPreviewScene()
{
	if (SceneInstance.IsValid())
	{
		SceneInstance->GetBindings().Reset();
		ContextualAnimManager->OnSceneInstanceEnded(SceneInstance.Get());
		SceneInstance.Reset();
	}

	for (const FContextualAnimSceneBinding& Binding : SceneBindings)
	{
		if (AActor* Actor = Binding.GetActor())
		{
			Actor->Destroy();
		}
	}

	if (!SceneAsset->Sections.IsValidIndex(ActiveSectionIdx))
	{
		return;
	}

	const FContextualAnimSceneSection& ContextualAnimSection = SceneAsset->Sections[ActiveSectionIdx];
	const int32 ActiveAnimSetIdx = ActiveAnimSetMap.FindOrAdd(ActiveSectionIdx);
	if (!ContextualAnimSection.AnimSets.IsValidIndex(ActiveAnimSetIdx))
	{
		return;
	}
	
	FContextualAnimStartSceneParams StartSceneParams;
	StartSceneParams.SectionIdx = ActiveSectionIdx;
	StartSceneParams.AnimSetIdx = ActiveAnimSetIdx;

	const FContextualAnimSet& AnimSet = ContextualAnimSection.AnimSets[ActiveAnimSetIdx];
	for (const FContextualAnimTrack& AnimTrack : AnimSet.Tracks)
	{
		if (AActor* PreviewActor = SpawnPreviewActor(AnimTrack))
		{
			StartSceneParams.RoleToActorMap.Add(AnimTrack.Role, PreviewActor);
		}
	}

	SceneInstance = ContextualAnimManager->ForceStartScene(*GetSceneAsset(), StartSceneParams);

	// Disable auto blend out
	if (SceneInstance.IsValid())
	{
		SceneBindings = SceneInstance->GetBindings();

		for (const FContextualAnimSceneBinding& Binding : SceneBindings)
		{
			if (FAnimMontageInstance* MontageInstance = Binding.GetAnimMontageInstance())
			{
				MontageInstance->Pause();
				MontageInstance->bEnableAutoBlendOut = false;
			}
		}
	}
}

void FContextualAnimViewModel::AddNewAnimSet(const FContextualAnimNewAnimSetParams& Params)
{
	FContextualAnimSet AnimSet;
	for (const FContextualAnimNewAnimSetData& Data : Params.Data)
	{
		FContextualAnimTrack AnimTrack;
		AnimTrack.Role = Data.RoleName;
		AnimTrack.Animation = Data.Animation;
		AnimTrack.bRequireFlyingMode = Data.bRequiresFlyingMode;
		AnimSet.Tracks.Add(AnimTrack);
	}

	int32 SectionIdx = INDEX_NONE;
	int32 AnimSetIdx = INDEX_NONE;
	for(int32 Idx = 0; Idx < SceneAsset->Sections.Num(); Idx++)
	{
		FContextualAnimSceneSection& Section = SceneAsset->Sections[Idx];
		if(Section.Name == Params.SectionName)
		{
			AnimSetIdx = Section.AnimSets.Add(AnimSet);
			SectionIdx = Idx;
			break;
		}
	}
	
	if(SectionIdx == INDEX_NONE)
	{
		FContextualAnimSceneSection NewSection;
		NewSection.Name = Params.SectionName;
		AnimSetIdx = NewSection.AnimSets.Add(AnimSet);
		SectionIdx = SceneAsset->Sections.Add(NewSection);
	}

	check(SectionIdx != INDEX_NONE);
	check(AnimSetIdx != INDEX_NONE);

	SceneAsset->PrecomputeData();

	SceneAsset->MarkPackageDirty();

	// Set active AnimSet and refresh sequencer panel
	SetActiveAnimSetForSection(SectionIdx, AnimSetIdx);
}

void FContextualAnimViewModel::AddNewIKTarget(const UContextualAnimNewIKTargetParams& Params)
{
	check(SceneAsset->Sections.IsValidIndex(Params.SectionIdx));

	// Add IK Target definition to the scene asset
	FContextualAnimIKTargetDefinition IKTargetDef;
	IKTargetDef.GoalName = Params.GoalName;
	IKTargetDef.BoneName = Params.SourceBone.BoneName;
	IKTargetDef.Provider = Params.Provider;
	IKTargetDef.TargetRoleName = Params.TargetRole;
	IKTargetDef.TargetBoneName = Params.TargetBone.BoneName;

	if (FContextualAnimIKTargetDefContainer* ContainerPtr = SceneAsset->Sections[Params.SectionIdx].RoleToIKTargetDefsMap.Find(Params.SourceRole))
	{
		ContainerPtr->IKTargetDefs.AddUnique(IKTargetDef);
	}
	else
	{
		FContextualAnimIKTargetDefContainer Container;
		Container.IKTargetDefs.AddUnique(IKTargetDef);
		SceneAsset->Sections[Params.SectionIdx].RoleToIKTargetDefsMap.Add(Params.SourceRole, Container);
	}

	SceneAsset->PrecomputeData();

	SceneAsset->MarkPackageDirty();
}

void FContextualAnimViewModel::ToggleSimulateMode()
{
	if (SimulateModeState == ESimulateModeState::Inactive)
	{
		SimulateModeState = ESimulateModeState::Paused;

		if (SceneInstance.IsValid())
		{
			SceneInstance->Stop();
		}
	}
	else
	{
		SimulateModeState = ESimulateModeState::Inactive;

		if (SceneInstance.IsValid())
		{
			for (auto& Binding : SceneInstance->GetBindings())
			{
				if (UMotionWarpingComponent* MotionWarpComp = Binding.GetActor()->FindComponentByClass<UMotionWarpingComponent>())
				{
					for (const FContextualAnimSetPivotDefinition& Def : SceneAsset->GetAnimSetPivotDefinitionsInSection(SceneInstance->GetBindings().GetSectionIdx()))
					{
						MotionWarpComp->RemoveWarpTarget(Def.Name);
					}
				}
			}
		}

		SetDefaultMode();
	}

	ClearSelection();
};

void FContextualAnimViewModel::StartSimulation()
{
	FContextualAnimStartSceneParams Params;
	for (const FContextualAnimSceneBinding& Binding : SceneBindings)
	{
		Params.RoleToActorMap.Add(SceneBindings.GetRoleFromBinding(Binding), Binding.GetContext());
	}
	Params.SectionIdx = 0;
	SceneInstance = ContextualAnimManager->TryStartScene(*GetSceneAsset(), Params);

	if(SceneInstance == nullptr)
	{
		//@TODO: This should be a message on the screen
		UE_LOG(LogContextualAnim, Warning, TEXT("Can't start scene"));
		return;
	}

	SceneBindings = SceneInstance->GetBindings();

	SimulateModeState = ESimulateModeState::Playing;
}

UWorld* FContextualAnimViewModel::GetWorld() const
{
	check(PreviewScenePtr.IsValid());
	return PreviewScenePtr.Pin()->GetWorld();
}

UObject* FContextualAnimViewModel::GetPlaybackContext() const
{
	return GetWorld();
}

void FContextualAnimViewModel::UpdatePreviewActorTransform(const FContextualAnimSceneBinding& Binding, float Time)
{
	if (AActor* PreviewActor = Binding.GetActor())
	{
		FTransform Transform = FTransform::Identity;

		// When modifying the actor transform, use the 'temp' transform instead of the one saved in the asset
		// This allows the user to scrub the time line while maintaining the new MeshToScene transform 
		if(ModifyingActorTransformInSceneState != EModifyActorTransformInSceneState::Inactive && Binding.GetActor() == ModifyingTransformInSceneCachedActor.Get())
		{
			const UAnimSequenceBase* Animation = SceneBindings.GetAnimTrackFromBinding(Binding).Animation;
			if (Animation)
			{
				Transform = UContextualAnimUtilities::ExtractRootTransformFromAnimation(Animation, Time);
			}

			Transform *= NewMeshToSceneTransform;
		}
		else
		{
			Transform = SceneBindings.GetAnimTrackFromBinding(Binding).GetRootTransformAtTime(Time);
		}

		// Special case for Character
		if (ACharacter* PreviewCharacter = Cast<ACharacter>(PreviewActor))
		{
			if (UCharacterMovementComponent* MovementComp = Binding.GetActor()->FindComponentByClass<UCharacterMovementComponent>())
			{
				MovementComp->StopMovementImmediately();
			}

			const float MIN_FLOOR_DIST = 1.9f; //from CharacterMovementComp, including in this offset to avoid jittering in walking mode
			const float CapsuleHalfHeight = PreviewCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Transform.SetLocation(Transform.GetLocation() + (PreviewCharacter->GetActorQuat().GetUpVector() * CapsuleHalfHeight + MIN_FLOOR_DIST));

			Transform.SetRotation(PreviewCharacter->GetBaseRotationOffset().Inverse() * Transform.GetRotation());
		}

		PreviewActor->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation());
	}
}

UContextualAnimMovieSceneTrack* FContextualAnimViewModel::FindMasterTrackByRole(const FName& Role) const
{
	const TArray<UMovieSceneTrack*>& MasterTracks = MovieScene->GetMasterTracks();
	for (UMovieSceneTrack* MasterTrack : MasterTracks)
	{
		UContextualAnimMovieSceneTrack* ContextualAnimTrack = Cast<UContextualAnimMovieSceneTrack>(MasterTrack);
		check(ContextualAnimTrack);

		if (ContextualAnimTrack->GetRole() == Role)
		{
			return ContextualAnimTrack;
		}
	}

	return nullptr;
}

void FContextualAnimViewModel::SequencerTimeChanged()
{
	EMovieScenePlayerStatus::Type CurrentStatus = Sequencer->GetPlaybackStatus();
	const float CurrentSequencerTime = Sequencer->GetGlobalTime().AsSeconds();
	const float PlaybackSpeed = Sequencer->GetPlaybackSpeed();

	if (SceneInstance.IsValid())
	{
		for (auto& Binding : SceneInstance->GetBindings())
		{
			if (FAnimMontageInstance* MontageInstance = Binding.GetAnimMontageInstance())
			{
				const float AnimPlayLength = MontageInstance->Montage->GetPlayLength();
				float PreviousTime = FMath::Clamp(PreviousSequencerTime, 0.f, AnimPlayLength);
				float CurrentTime = FMath::Clamp(CurrentSequencerTime, 0.f, AnimPlayLength);

				if (CurrentStatus == EMovieScenePlayerStatus::Stopped || CurrentStatus == EMovieScenePlayerStatus::Scrubbing)
				{
					UpdatePreviewActorTransform(Binding, CurrentTime);

					if (MontageInstance->IsPlaying())
					{
						MontageInstance->Pause();
					}

					MontageInstance->SetPosition(CurrentTime);
				}
				else if (CurrentStatus == EMovieScenePlayerStatus::Playing)
				{
					if (PlaybackSpeed > 0.f && CurrentTime < PreviousTime)
					{
						UpdatePreviewActorTransform(Binding, CurrentTime);
						MontageInstance->SetPosition(CurrentTime);
					}

					if (!MontageInstance->IsPlaying())
					{
						MontageInstance->SetPlaying(true);
					}
				}
			}
		}
	}

	PreviousSequencerStatus = CurrentStatus;
	PreviousSequencerTime = CurrentSequencerTime;
}

void FContextualAnimViewModel::SequencerDataChanged(EMovieSceneDataChangeType DataChangeType)
{
	UE_LOG(LogContextualAnim, Log, TEXT("FContextualAnimViewModel::OnMovieSceneDataChanged DataChangeType: %d"), (int32)DataChangeType);

	if(!SceneInstance.IsValid())
	{
		return;
	}

	if(DataChangeType == EMovieSceneDataChangeType::TrackValueChanged)
	{
		// Update IK AnimNotify's bEnable flag based on the Active state of the section
		// @TODO: Temp brute-force approach until having a way to override FMovieSceneSection::SetIsActive or something similar

		// @TODO: Commented out for now until we add the new behavior where the user needs to double-click on the animation to edit the notifies
		/*for (const auto& Binding : SceneInstance->GetBindings())
		{
			TArray<UMovieSceneTrack*> Tracks = MovieSceneSequence->GetMovieScene()->FindTracks(UContextualAnimMovieSceneNotifyTrack::StaticClass(), Binding.Guid);
			for(UMovieSceneTrack* Track : Tracks)
			{
				const TArray<UMovieSceneSection*>& Sections = Track->GetAllSections();
				for(UMovieSceneSection* Section : Sections)
				{
					if(UContextualAnimMovieSceneNotifySection* NotifySection = Cast<UContextualAnimMovieSceneNotifySection>(Section))
					{
						if(UAnimNotifyState_IKWindow* IKNotify = Cast<UAnimNotifyState_IKWindow>(NotifySection->GetAnimNotifyState()))
						{
							if(IKNotify->bEnable != NotifySection->IsActive())
							{
								IKNotify->bEnable = NotifySection->IsActive();
								IKNotify->MarkPackageDirty();
							}
						}
					}
				}
			}
		}*/
	}
}

void FContextualAnimViewModel::OnAnimNotifyChanged(UAnimSequenceBase* Animation)
{
	// Do not refresh sequencer tracks if the change to the notifies came from us
	if (bUpdatingAnimationFromSequencer)
	{
		return;
	}

	UE_LOG(LogContextualAnim, Log, TEXT("FContextualAnimViewModel::OnAnimNotifyChanged Anim: %s. Refreshing Sequencer Tracks"), *GetNameSafe(Animation));

	// Refresh notifies view if active
	if(TimelineMode == ETimelineMode::Notifies && EditingAnimation.Get() == Animation)
	{
		if(const FContextualAnimTrack* AnimTrack = GetSceneAsset()->FindAnimTrackByAnimation(Animation))
		{
			SetNotifiesMode(*AnimTrack);
		}
	}
}

void FContextualAnimViewModel::AnimationModified(UAnimSequenceBase& Animation)
{
	TGuardValue<bool> UpdateGuard(bUpdatingAnimationFromSequencer, true);

	Animation.RefreshCacheData();
	Animation.PostEditChange();
	Animation.MarkPackageDirty();
}

void FContextualAnimViewModel::UpdateSelection(const AActor* SelectedActor)
{
	const FContextualAnimSceneBinding* Binding = SceneBindings.FindBindingByActor(SelectedActor);
	if (Binding)
	{
		// Discard changes if we were modifying the transform of an actor in the scene but never confirmed the changes before selecting a different actor
		if(ModifyingActorTransformInSceneState == EModifyActorTransformInSceneState::WaitingForConfirmation && 
			Binding->GetActor() != ModifyingTransformInSceneCachedActor.Get())
		{
			DiscardChangeToActorTransformInScene();
		}

		UpdateSelection(SceneBindings.GetRoleFromBinding(*Binding));
	}
	else
	{
		ClearSelection();
	}
}

void FContextualAnimViewModel::UpdateSelection(FName Role, int32 CriterionIdx, int32 CriterionDataIdx)
{
	Sequencer->EmptySelection();

	SelectionInfo.Role = Role;
	SelectionInfo.Criterion.Key = CriterionIdx;
	SelectionInfo.Criterion.Value = CriterionDataIdx;
}

void FContextualAnimViewModel::ClearSelection()
{
	SelectionInfo.Reset();
}

FContextualAnimSceneBinding* FContextualAnimViewModel::GetSelectedBinding() const
{
	return const_cast<FContextualAnimSceneBinding*>(SceneBindings.FindBindingByRole(SelectionInfo.Role));
}

AActor* FContextualAnimViewModel::GetSelectedActor() const
{
	const FContextualAnimSceneBinding* Binding = GetSelectedBinding();
	return Binding ? Binding->GetActor() : nullptr;
}

FContextualAnimTrack* FContextualAnimViewModel::GetSelectedAnimTrack() const
{
	FContextualAnimSceneBinding* Binding = GetSelectedBinding();
	return Binding ? const_cast<FContextualAnimTrack*>(&SceneBindings.GetAnimTrackFromBinding(*Binding)) : nullptr;
}

UContextualAnimSelectionCriterion* FContextualAnimViewModel::GetSelectedSelectionCriterion() const
{
	if (SelectionInfo.Criterion.Key != INDEX_NONE && SelectionInfo.Criterion.Value != INDEX_NONE)
	{
		FContextualAnimTrack* AnimTrack = GetSelectedAnimTrack();
		if (AnimTrack && AnimTrack->SelectionCriteria.IsValidIndex(SelectionInfo.Criterion.Key))
		{
			return AnimTrack->SelectionCriteria[SelectionInfo.Criterion.Key];
		}
	}

	return nullptr;
}

FText FContextualAnimViewModel::GetSelectionDebugText() const
{
	AActor* SelectedActor = GetSelectedActor();
	return FText::FromString(FString::Printf(TEXT("Selection Info:\n Role: %s \n Actor: %s \n Criterion: %d (%d)"),
		*SelectionInfo.Role.ToString(), *GetNameSafe(SelectedActor), SelectionInfo.Criterion.Key, SelectionInfo.Criterion.Value));
}

bool FContextualAnimViewModel::ProcessInputDelta(FVector& InDrag, FRotator& InRot, FVector& InScale)
{
	if (IsSimulateModePaused())
	{
		// In Simulate Mode (while paused) the user can drag the preview actor around to simulate the interaction from different start positions
		if (AActor* SelectedActor = GetSelectedActor())
		{
			SelectedActor->SetActorLocationAndRotation(SelectedActor->GetActorLocation() + InDrag, SelectedActor->GetActorRotation() + InRot);
			return true;
		}
	}
	else if (IsSimulateModeInactive())
	{
		if (UContextualAnimSelectionCriterion_TriggerArea* Spatial = Cast<UContextualAnimSelectionCriterion_TriggerArea>(GetSelectedSelectionCriterion()))
		{
			FMatrix WidgetCoordSystem = FMatrix::Identity;
			GetCustomDrawingCoordinateSystem(WidgetCoordSystem, nullptr);

			InDrag = WidgetCoordSystem.InverseTransformVector(InDrag);

			FVector& Point = Spatial->PolygonPoints[SelectionInfo.Criterion.Value >= 4 ? SelectionInfo.Criterion.Value - 4 : SelectionInfo.Criterion.Value];
			Point.X += InDrag.X;
			Point.Y += InDrag.Y;

			if (InDrag.Z != 0.f)
			{
				if (SelectionInfo.Criterion.Value < 4)
				{
					for (int32 Idx = 0; Idx < Spatial->PolygonPoints.Num(); Idx++)
					{
						Spatial->PolygonPoints[Idx].Z += InDrag.Z;
					}

					Spatial->Height = FMath::Max(Spatial->Height - InDrag.Z, 0.f);
				}
				else
				{
					Spatial->Height = FMath::Max(Spatial->Height + InDrag.Z, 0.f);
				}
			}

			return true;
		}
		else if (ModifyingActorTransformInSceneState == EModifyActorTransformInSceneState::Modifying)
		{
			if (WantsToModifyMeshToSceneForSelectedActor())
			{
				if (AActor* SelectedActor = GetSelectedActor())
				{
					NewMeshToSceneTransform.SetLocation(NewMeshToSceneTransform.GetLocation() + InDrag);
					NewMeshToSceneTransform.SetRotation(FQuat(InRot) * NewMeshToSceneTransform.GetRotation());
					SelectedActor->SetActorLocationAndRotation(SelectedActor->GetActorLocation() + InDrag, SelectedActor->GetActorRotation() + InRot);
				}
			}

			return true;
		}
	}

	return false;
}

bool FContextualAnimViewModel::GetCustomDrawingCoordinateSystem(FMatrix& InMatrix, void* InData)
{
	if (SelectionInfo.Criterion.Value != INDEX_NONE)
	{
		if (SceneInstance.IsValid())
		{
			FTransform PrimaryActorTransform = FTransform::Identity;
			if (const FContextualAnimSceneBinding* Binding = SceneInstance->FindBindingByRole(GetSceneAsset()->GetPrimaryRole()))
			{
				PrimaryActorTransform = Binding->GetTransform();
				InMatrix = PrimaryActorTransform.ToMatrixNoScale().RemoveTranslation();
				return true;
			}
		}
	}
	else if (AActor* SelectedActor = GetSelectedActor())
	{
		InMatrix = SelectedActor->GetActorTransform().ToMatrixNoScale().RemoveTranslation();
		return true;
	}

	return false;
}

bool FContextualAnimViewModel::WantsToModifyMeshToSceneForSelectedActor() const
{
	//@TODO: Communicate this to the user somehow
	return FSlateApplication::Get().GetModifierKeys().IsShiftDown();
}

bool FContextualAnimViewModel::ShouldPreviewSceneDrawWidget() const
{
	// When Simulate Mode is inactive we show the widget if an editable selection criterion is selected (only Trigger Area for now) 
	// or if an actor is selected and user wants to modify the MeshToScene transform
	if (IsSimulateModeInactive())
	{
		const UContextualAnimSelectionCriterion* SelectionCriterion = GetSelectedSelectionCriterion();
		return ((SelectionCriterion && SelectionCriterion->GetClass()->IsChildOf<UContextualAnimSelectionCriterion_TriggerArea>()) ||
			(WantsToModifyMeshToSceneForSelectedActor() && GetSelectedActor()));
	}
	// When Simulate Mode is Paused we show the widget if an actor is selected, so the user can modify the position of any of the actor before triggering the interaction
	else if (IsSimulateModePaused())
	{
		return GetSelectedActor() != nullptr;
	}

	return false;
}

FVector FContextualAnimViewModel::GetWidgetLocationFromSelection() const
{
	if (SelectionInfo.Criterion.Value != INDEX_NONE)
	{
		if (const UContextualAnimSelectionCriterion_TriggerArea* Spatial = Cast<UContextualAnimSelectionCriterion_TriggerArea>(GetSelectedSelectionCriterion()))
		{
			FVector Location = FVector::ZeroVector;
			if (SelectionInfo.Criterion.Value < 4)
			{
				Location = Spatial->PolygonPoints[SelectionInfo.Criterion.Value];
			}
			else
			{
				Location = Spatial->PolygonPoints[SelectionInfo.Criterion.Value - 4] + FVector::UpVector * Spatial->Height;
			}

			FTransform PrimaryActorTransform = FTransform::Identity;
			if (const FContextualAnimSceneBinding* Binding = SceneInstance->FindBindingByRole(SceneAsset->GetPrimaryRole()))
			{
				PrimaryActorTransform = Binding->GetTransform();
			}

			return PrimaryActorTransform.TransformPositionNoScale(Location);
		}
	}
	else if (AActor* SelectedActor = GetSelectedActor())
	{
		return SelectedActor->GetActorLocation();
	}

	return FVector::ZeroVector;
}

bool FContextualAnimViewModel::StartTracking()
{
	if (IsSimulateModeInactive() && WantsToModifyMeshToSceneForSelectedActor())
	{
		if (const FContextualAnimSceneBinding* Binding = GetSelectedBinding())
		{
			NewMeshToSceneTransform = SceneBindings.GetAnimTrackFromBinding(*Binding).MeshToScene;
			ModifyingActorTransformInSceneState = EModifyActorTransformInSceneState::Modifying;
			ModifyingTransformInSceneCachedActor = Binding->GetActor();
			return true;
		}
	}

	return false;
}

bool FContextualAnimViewModel::EndTracking()
{
	if (ModifyingActorTransformInSceneState == EModifyActorTransformInSceneState::Modifying)
	{
		const FContextualAnimSceneBinding* Binding = GetSelectedBinding();
		if (Binding && !SceneBindings.GetAnimTrackFromBinding(*Binding).MeshToScene.Equals(NewMeshToSceneTransform))
		{
			ModifyingActorTransformInSceneState = EModifyActorTransformInSceneState::WaitingForConfirmation;
		}
		else
		{
			ModifyingActorTransformInSceneState = EModifyActorTransformInSceneState::Inactive;
			ModifyingTransformInSceneCachedActor.Reset();
		}

		return true;		
	}

	return false;
}

bool FContextualAnimViewModel::IsChangeToActorTransformInSceneWaitingForConfirmation() const
{
	const FContextualAnimSceneBinding* Binding = GetSelectedBinding();
	return (Binding && Binding->GetActor() == ModifyingTransformInSceneCachedActor.Get() && ModifyingActorTransformInSceneState == EModifyActorTransformInSceneState::WaitingForConfirmation);
}

void FContextualAnimViewModel::ApplyChangeToActorTransformInScene()
{
	const FContextualAnimSceneBinding* Binding = GetSelectedBinding();
	if(Binding && Binding->GetActor() == ModifyingTransformInSceneCachedActor.Get())
	{
		FScopedTransaction Transaction(LOCTEXT("ModifyMeshToSceneTransform", "Modify MeshToScene Transform"));
		SceneAsset->Modify();

		FTransform& MeshToSpaceTransform = (const_cast<FContextualAnimTrack*>(&SceneBindings.GetAnimTrackFromBinding(*Binding)))->MeshToScene;
		MeshToSpaceTransform = NewMeshToSceneTransform;

		UpdatePreviewActorTransform(*Binding, Sequencer->GetGlobalTime().AsSeconds());
	}
	
	ModifyingActorTransformInSceneState = EModifyActorTransformInSceneState::Inactive;
	ModifyingTransformInSceneCachedActor.Reset();
}

void FContextualAnimViewModel::DiscardChangeToActorTransformInScene()
{
	ModifyingActorTransformInSceneState = EModifyActorTransformInSceneState::Inactive;

	if (const FContextualAnimSceneBinding* Binding = SceneInstance->FindBindingByActor(ModifyingTransformInSceneCachedActor.Get()))
	{
		UpdatePreviewActorTransform(*Binding, Sequencer->GetGlobalTime().AsSeconds());
	}

	ModifyingTransformInSceneCachedActor.Reset();
}

void FContextualAnimViewModel::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

	// Refresh the time line if the Sections array or the AnimSets array inside a section changes
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UContextualAnimSceneAsset, Sections) &&
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContextualAnimSceneAsset, Sections) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FContextualAnimSceneSection, AnimSets)))
	{
		SetDefaultMode();
	}
}

bool FContextualAnimViewModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	// Ensure that we only react to modifications to the SceneAsset
	if (SceneAsset)
	{
		for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
		{
			UObject* Object = TransactionObjectPair.Key;
			while (Object != nullptr)
			{
				if (Object == SceneAsset)
				{
					return true;
				}

				Object = Object->GetOuter();
			}
		}
	}

	return false;
}

void FContextualAnimViewModel::PostUndo(bool bSuccess)
{
	// Refresh everything after a Undo operation
	SetDefaultMode();
}

void FContextualAnimViewModel::PostRedo(bool bSuccess)
{
	// Refresh everything after a Redo operation
	SetDefaultMode();
}

#undef LOCTEXT_NAMESPACE