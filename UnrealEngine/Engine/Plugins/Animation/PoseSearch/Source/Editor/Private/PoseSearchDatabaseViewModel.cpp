// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseViewModel.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MirrorDataTable.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InstancedStruct.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchTrajectoryTypes.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchDatabaseDataDetails.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchEditor.h"
#include "PropertyEditorModule.h"

namespace UE::PoseSearch
{
#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<float> CVarDatabasePreviewDebugDrawSamplerSize(TEXT("a.DatabasePreview.DebugDrawSamplerSize"), 0.f, TEXT("Debug Draw Sampler Positions Size"));
#endif

constexpr float StepDeltaTime = 1.0f / 30.0f;

// FDatabasePreviewActor
bool FDatabasePreviewActor::SpawnPreviewActor(UWorld* World, const UPoseSearchDatabase* PoseSearchDatabase, int32 IndexAssetIdx, const FRole& Role, const FTransform& SamplerRootTransformOrigin, const FTransform* PrecalculatedRootTransformOrigin, int32 PoseIdxForTimeOffset)
{
	check(PoseSearchDatabase && PoseSearchDatabase->Schema);
	const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();

	USkeleton* Skeleton = PoseSearchDatabase->Schema->GetSkeleton(Role);
	if (!Skeleton)
	{
		UE_LOG(LogPoseSearchEditor, Error, TEXT("Couldn't spawn preview Actor because of missing Role '%s' in Schema '%s'"), *Role.ToString(), *PoseSearchDatabase->Schema->GetName());
		return false;
	}

	const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIdx];

	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetAnimationAssetBase(IndexAsset.GetSourceAssetIdx());
	UAnimationAsset* PreviewAsset = Cast<UAnimationAsset>(DatabaseAnimationAsset->GetAnimationAssetForRole(Role));
	if (!PreviewAsset)
	{
		return false;
	}

	ActorRole = Role;
	Sampler.Init(PreviewAsset, SamplerRootTransformOrigin, IndexAsset.GetBlendParameters());
	Sampler.Process();

	IndexAssetIndex = IndexAssetIdx;
	CurrentPoseIndex = INDEX_NONE;

	PlayTimeOffset = 0.f;
	if (PoseIdxForTimeOffset >= 0)
	{
		PlayTimeOffset = PoseSearchDatabase->GetRealAssetTime(PoseIdxForTimeOffset) - IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ActorPtr = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	ActorPtr->SetFlags(RF_Transient);

	UDebugSkelMeshComponent* Mesh = NewObject<UDebugSkelMeshComponent>(ActorPtr.Get());
	Mesh->RegisterComponentWithWorld(World);

	UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(Mesh);
	Mesh->PreviewInstance = AnimInstance;
	AnimInstance->InitializeAnimation();

	USkeletalMesh* DatabasePreviewMesh = PoseSearchDatabase->PreviewMesh;
	Mesh->SetSkeletalMesh(DatabasePreviewMesh ? DatabasePreviewMesh : PoseSearchDatabase->Schema->GetSkeleton(Role)->GetPreviewMesh(true));
	Mesh->EnablePreview(true, PreviewAsset);
		
	AnimInstance->SetAnimationAsset(PreviewAsset, IndexAsset.IsLooping(), 0.0f);
	AnimInstance->SetBlendSpacePosition(IndexAsset.GetBlendParameters());
		
	if (IndexAsset.IsMirrored() && PoseSearchDatabase->Schema)
	{
		const UMirrorDataTable* MirrorDataTable = PoseSearchDatabase->Schema->GetMirrorDataTable(Role);
		AnimInstance->SetMirrorDataTable(MirrorDataTable);
	}

	const FMirrorDataCache MirrorDataCache(AnimInstance->GetMirrorDataTable(), AnimInstance->GetRequiredBonesOnAnyThread());
	
	if (PrecalculatedRootTransformOrigin)
	{
		// using the PrecalculatedRootTransformOrigin if provided. useful to synchronize multiple roles preview actors
		RootTransformOrigin = *PrecalculatedRootTransformOrigin;
	}
	else
	{
		RootTransformOrigin = MirrorDataCache.MirrorTransform(Sampler.ExtractRootTransform(PlayTimeOffset));
	}

	AnimInstance->PlayAnim(IndexAsset.IsLooping(), 0.0f);
	if (!ActorPtr->GetRootComponent())
	{
		ActorPtr->SetRootComponent(Mesh);
	}

	AnimInstance->SetPlayRate(0.f);

	UE_LOG(LogPoseSearchEditor, Log, TEXT("Spawned preview Actor: %s"), *GetNameSafe(ActorPtr.Get()));
	return true;
}

void FDatabasePreviewActor::UpdatePreviewActor(const UPoseSearchDatabase* PoseSearchDatabase, float PlayTime, bool bQuantizeAnimationToPoseData)
{
	check(PoseSearchDatabase);

	const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();

	UAnimPreviewInstance* AnimInstance = GetAnimPreviewInstanceInternal();
	if (!AnimInstance || IndexAssetIndex >= SearchIndex.Assets.Num())
	{
		return;
	}

	const UAnimationAsset* PreviewAsset = AnimInstance->GetAnimationAsset();
	if (!PreviewAsset)
	{
		return;
	}

	CurrentTime = 0.f;
	const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
	float CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate) + PlayTimeOffset;
	FAnimationRuntime::AdvanceTime(false, CurrentPlayTime, CurrentTime, IndexAsset.GetLastSampleTime(PoseSearchDatabase->Schema->SampleRate));
			 
	// time to pose index
	CurrentPoseIndex = PoseSearchDatabase->GetPoseIndexFromTime(CurrentTime, IndexAsset);

	const float QuantizedTime = CurrentPoseIndex >= 0 ? PoseSearchDatabase->GetRealAssetTime(CurrentPoseIndex) : CurrentTime;
	if (bQuantizeAnimationToPoseData)
	{
		CurrentTime = QuantizedTime;
	}

	// SetPosition is in [0..1] range for blendspaces
	AnimInstance->SetPosition(Sampler.ToNormalizedTime(CurrentTime));
	AnimInstance->SetPlayRate(0.f);
	AnimInstance->SetBlendSpacePosition(IndexAsset.GetBlendParameters());

	const FMirrorDataCache MirrorDataCache(AnimInstance->GetMirrorDataTable(), AnimInstance->GetRequiredBonesOnAnyThread());

	// updating root transforms
	RootTransformCurrentQuantizedTime = MirrorDataCache.MirrorTransform(Sampler.ExtractRootTransform(QuantizedTime));
	RootTransformCurrentQuantizedTime.SetToRelativeTransform(RootTransformOrigin);

	if (CurrentTime == QuantizedTime)
	{
		RootTransformCurrent = RootTransformCurrentQuantizedTime;
	}
	else
	{
		RootTransformCurrent = MirrorDataCache.MirrorTransform(Sampler.ExtractRootTransform(CurrentTime));
		RootTransformCurrent.SetToRelativeTransform(RootTransformOrigin);
	}

	check(ActorPtr != nullptr);
	ActorPtr->SetActorTransform(RootTransformCurrent);

	// @todo: optimize this bone container, since we only need the root bone here...
	const FBoneContainer& BoneContainer = AnimInstance->GetRequiredBonesOnAnyThread();
	if (BoneContainer.GetNumBones() > 0)
	{
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&BoneContainer);

		Sampler.ExtractPose(QuantizedTime, Pose);
		MirrorDataCache.MirrorPose(Pose);

		RootBoneTransformCurrentQuantizedTime = Pose[FCompactPoseBoneIndex(RootBoneIndexType)];
	}
}

void FDatabasePreviewActor::Destroy()
{
	if (ActorPtr != nullptr)
	{
		ActorPtr->Destroy();
	}
}

bool FDatabasePreviewActor::DrawPreviewActors(TArrayView<FDatabasePreviewActor> PreviewActors, const UPoseSearchDatabase* PoseSearchDatabase, bool bDisplayRootMotionSpeed, bool bDisplayBlockTransition, TConstArrayView<float> QueryVector)
{
	using namespace UE::PoseSearch;

	UWorld* CommonWorld = nullptr;
	int32 CommonCurrentPoseIndex = INDEX_NONE;
#if DO_CHECK
	int32 CommonIndexAssetIndex = INDEX_NONE;
#endif // DO_CHECK

	TArray<const USkinnedMeshComponent*> Meshes;
	FRoleToIndex RoleToIndex;
	TArray<FArchivedPoseHistory> ArchivedPoseHistories;
	TArray<const IPoseHistory*> PoseHistories;

	const int32 NumPreviewActors = PreviewActors.Num();

	Meshes.Reserve(NumPreviewActors);
	RoleToIndex.Reserve(NumPreviewActors);
	ArchivedPoseHistories.Reserve(NumPreviewActors);
	PoseHistories.Reserve(NumPreviewActors);

	for (FDatabasePreviewActor& PreviewActor : PreviewActors)
	{
		if (!PoseSearchDatabase->GetSearchIndex().IsValidPoseIndex(PreviewActor.GetCurrentPoseIndex()))
		{
			return false;
		}

		const UDebugSkelMeshComponent* Mesh = PreviewActor.GetDebugSkelMeshComponent();
		if (!Mesh)
		{
			return false;
		}

		if (!CommonWorld)
		{
			CommonWorld = Mesh->GetWorld();
		}
		else if (CommonWorld != Mesh->GetWorld())
		{
			return false;
		}

		// making sure PreviewActors are consistent with each other
		if (CommonCurrentPoseIndex == INDEX_NONE)
		{
			CommonCurrentPoseIndex = PreviewActor.GetCurrentPoseIndex();
		}
		else if (CommonCurrentPoseIndex != PreviewActor.GetCurrentPoseIndex())
		{
			checkNoEntry();
			return false;
		}

#if DO_CHECK
		if (CommonIndexAssetIndex == INDEX_NONE)
		{
			CommonIndexAssetIndex = PreviewActor.IndexAssetIndex;
		}
		else if (CommonIndexAssetIndex != PreviewActor.IndexAssetIndex)
		{
			checkNoEntry();
			return false;
		}
#endif // DO_CHECK

		RoleToIndex.Add(PreviewActor.ActorRole) = Meshes.Num();
		Meshes.Add(Mesh);

		const FTransform RootMotionTransform = PreviewActor.RootBoneTransformCurrentQuantizedTime * PreviewActor.RootTransformCurrentQuantizedTime;

		// @todo: reconstruct ArchivedPoseHistory::BoneToTransformMap and ArchivedPoseHistory::Entries if needed
		FArchivedPoseHistory& ArchivedPoseHistory = ArchivedPoseHistories.AddDefaulted_GetRef();
		// @todo: reconstruct multiple trajectory samples if needed
		// reconstructing the FPoseSearchQueryTrajectory with only one sample with AccumulatedSeconds at zero
		FPoseSearchQueryTrajectorySample& TrajectorySample = ArchivedPoseHistory.Trajectory.Samples.AddDefaulted_GetRef();
		TrajectorySample.SetTransform(RootMotionTransform);
		TrajectorySample.AccumulatedSeconds = 0.f;

		PoseHistories.Add(&ArchivedPoseHistory);
	}

	UE::PoseSearch::FDebugDrawParams DrawParams(Meshes, PoseHistories, RoleToIndex, PoseSearchDatabase);
	DrawParams.DrawFeatureVector(CommonCurrentPoseIndex);

	if (!QueryVector.IsEmpty())
	{
		DrawParams.DrawFeatureVector(QueryVector);
	}

	for (FDatabasePreviewActor& PreviewActor : PreviewActors)
	{
		const UDebugSkelMeshComponent* Mesh = PreviewActor.GetDebugSkelMeshComponent();
		const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.IndexAssetIndex];

		const FMirrorDataCache MirrorDataCache(Mesh->PreviewInstance->GetMirrorDataTable(), Mesh->PreviewInstance->GetRequiredBonesOnAnyThread());
		if (bDisplayRootMotionSpeed || bDisplayBlockTransition)
		{
			// initializing SampledRootMotion if required
			if (PreviewActor.SampledRootMotion.IsEmpty())
			{
				const int NumPoses = IndexAsset.GetNumPoses();
				if (NumPoses > 1)
				{
					PreviewActor.SampledRootMotion.SetNumUninitialized(NumPoses);
					PreviewActor.SampledRootMotionSpeed.SetNumUninitialized(NumPoses);

					for (int32 Index = 0; Index < NumPoses; ++Index)
					{
						const int32 IndexAssetPoseIdx = Index + IndexAsset.GetFirstPoseIdx();
						const float IndexAssetPoseTime = IndexAsset.GetTimeFromPoseIndex(IndexAssetPoseIdx, PoseSearchDatabase->Schema->SampleRate);
						FTransform IndexAssetPoseTransform = MirrorDataCache.MirrorTransform(PreviewActor.Sampler.ExtractRootTransform(IndexAssetPoseTime));
						IndexAssetPoseTransform.SetToRelativeTransform(PreviewActor.RootTransformOrigin);

						PreviewActor.SampledRootMotion[Index] = IndexAssetPoseTransform.GetTranslation();
					}

					for (int32 Index = 1; Index < NumPoses; ++Index)
					{
						const FVector& Start = PreviewActor.SampledRootMotion[Index - 1];
						const FVector& End = PreviewActor.SampledRootMotion[Index];
						PreviewActor.SampledRootMotionSpeed[Index] = (Start - End).Length() * PoseSearchDatabase->Schema->PermutationsSampleRate;
					}
					PreviewActor.SampledRootMotionSpeed[0] = PreviewActor.SampledRootMotionSpeed[1];
				}
			}
		}

		if (bDisplayRootMotionSpeed)
		{
			// drawing PreviewActor.SampledRootMotion
			const int32 SampledRootMotionNum = PreviewActor.SampledRootMotion.Num();
			if (SampledRootMotionNum > 1)
			{
				for (int32 Index = 0; Index < PreviewActor.SampledRootMotion.Num(); ++Index)
				{
					const FVector& EndDown = PreviewActor.SampledRootMotion[Index];
					const FVector EndUp = EndDown + (PreviewActor.SampledRootMotionSpeed[Index] * FVector::UpVector);

					DrawParams.DrawLine(EndDown, EndUp, FColor::Black);
					if (Index > 0)
					{
						const FColor RootMotionColor = Index % 2 == 0 ? FColor::Purple : FColor::Orange;
						const FVector& StartDown = PreviewActor.SampledRootMotion[Index - 1];
						const FVector StartUp = StartDown + (PreviewActor.SampledRootMotionSpeed[Index - 1] * FVector::UpVector);
						DrawParams.DrawLine(StartDown, EndDown, RootMotionColor);
						DrawParams.DrawLine(StartUp, EndUp, RootMotionColor);
					}
				}
			}
		}

		if (bDisplayBlockTransition)
		{
			const int NumPoses = IndexAsset.GetNumPoses();
			if (NumPoses == PreviewActor.SampledRootMotion.Num())
			{
				for (int32 Index = 0; Index < NumPoses; ++Index)
				{
					const int32 IndexAssetPoseIdx = Index + IndexAsset.GetFirstPoseIdx();
					if (SearchIndex.PoseMetadata[IndexAssetPoseIdx].IsBlockTransition())
					{
						DrawParams.DrawPoint(PreviewActor.SampledRootMotion[Index], FColor::Red);
					}
					else
					{
						DrawParams.DrawPoint(PreviewActor.SampledRootMotion[Index], FColor::Green);
					}
				}
			}
		}


#if ENABLE_ANIM_DEBUG
		const float DebugDrawSamplerSize = CVarDatabasePreviewDebugDrawSamplerSize.GetValueOnAnyThread();
		if (DebugDrawSamplerSize > UE_KINDA_SMALL_NUMBER)
		{
			// drawing the pose extracted from the Sampler to visually compare with the pose features and the mesh drawing
			FMemMark Mark(FMemStack::Get());
			FCompactPose Pose;
			Pose.SetBoneContainer(&PreviewActor.GetAnimPreviewInstance()->GetRequiredBonesOnAnyThread());

			PreviewActor.Sampler.ExtractPose(PreviewActor.CurrentTime, Pose);
			MirrorDataCache.MirrorPose(Pose);

			const FTransform RootTransform = MirrorDataCache.MirrorTransform(PreviewActor.Sampler.ExtractRootTransform(PreviewActor.CurrentTime));

			FCSPose<FCompactPose> ComponentSpacePose;
			ComponentSpacePose.InitPose(MoveTemp(Pose));

			for (int32 BoneIndex = 0; BoneIndex < ComponentSpacePose.GetPose().GetNumBones(); ++BoneIndex)
			{
				const FTransform BoneWorldTransforms = ComponentSpacePose.GetComponentSpaceTransform(FCompactPoseBoneIndex(BoneIndex)) * RootTransform;
				DrawParams.DrawPoint(BoneWorldTransforms.GetTranslation(), FColor::Red, DebugDrawSamplerSize);
			}
		}
#endif // ENABLE_ANIM_DEBUG
	}
	return true;
}

const UDebugSkelMeshComponent* FDatabasePreviewActor::GetDebugSkelMeshComponent() const
{
	if (ActorPtr != nullptr)
	{
		return Cast<UDebugSkelMeshComponent>(ActorPtr->GetRootComponent());
	}
	return nullptr;
}

const UAnimPreviewInstance* FDatabasePreviewActor::GetAnimPreviewInstance() const
{
	if (const UDebugSkelMeshComponent* Mesh = GetDebugSkelMeshComponent())
	{
		return Mesh->PreviewInstance.Get();
	}
	return nullptr;
}

UAnimPreviewInstance* FDatabasePreviewActor::GetAnimPreviewInstanceInternal()
{
	if (ActorPtr != nullptr)
	{
		if (UDebugSkelMeshComponent* Mesh = Cast<UDebugSkelMeshComponent>(ActorPtr->GetRootComponent()))
		{
			return Mesh->PreviewInstance.Get();
		}
	}
	return nullptr;
}

// FDatabaseViewModel
void FDatabaseViewModel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PoseSearchDatabasePtr);
}

void FDatabaseViewModel::Initialize(UPoseSearchDatabase* InPoseSearchDatabase, const TSharedRef<FDatabasePreviewScene>& InPreviewScene, const TSharedRef<SDatabaseDataDetails>& InDatabaseDataDetails)
{
	PoseSearchDatabasePtr = InPoseSearchDatabase;
	PreviewScenePtr = InPreviewScene;
	DatabaseDataDetails = InDatabaseDataDetails;

	RemovePreviewActors();
}

void FDatabaseViewModel::BuildSearchIndex()
{
	using namespace UE::PoseSearch;
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(GetPoseSearchDatabase(), ERequestAsyncBuildFlag::NewRequest);
}

void FDatabaseViewModel::PreviewBackwardEnd()
{
	SetPlayTime(MinPreviewPlayLength, false);
}

void FDatabaseViewModel::PreviewBackwardStep()
{
	const float NewPlayTime = FMath::Clamp(PlayTime - StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	SetPlayTime(NewPlayTime, false);
}

void FDatabaseViewModel::PreviewBackward()
{
	DeltaTimeMultiplier = -1.f;
}

void FDatabaseViewModel::PreviewPause()
{
	DeltaTimeMultiplier = 0.f;
}

void FDatabaseViewModel::PreviewForward()
{
	DeltaTimeMultiplier = 1.f;
}

void FDatabaseViewModel::PreviewForwardStep()
{
	const float NewPlayTime = FMath::Clamp(PlayTime + StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	SetPlayTime(NewPlayTime, false);
}

void FDatabaseViewModel::PreviewForwardEnd()
{
	SetPlayTime(MaxPreviewPlayLength, false);
}

UWorld* FDatabaseViewModel::GetWorld()
{
	check(PreviewScenePtr.IsValid());
	return PreviewScenePtr.Pin()->GetWorld();
}

void FDatabaseViewModel::OnPreviewActorClassChanged()
{
	// todo: implement
}

void FDatabaseViewModel::Tick(float DeltaSeconds)
{
	const float DeltaPlayTime = DeltaSeconds * DeltaTimeMultiplier;
	if (!FMath::IsNearlyZero(DeltaPlayTime))
	{
		PlayTime += DeltaPlayTime;
		PlayTime = FMath::Clamp(PlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);

		if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
		{
			if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				for (TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
				{
					for (FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
					{
						PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
					}
				}
			}
		}
	}
}

void FDatabaseViewModel::RemovePreviewActors()
{
	PlayTime = 0.f;
	DeltaTimeMultiplier = 1.f;
	MaxPreviewPlayLength = 0.f;
	MinPreviewPlayLength = 0.f;
	bIsEditorSelection = true;
	bDrawQueryVector = false;

	for (TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
	{
		for (FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
		{
			PreviewActor.Destroy();
		}
	}

	PreviewActors.Reset();
}

void FDatabaseViewModel::AddSequenceToDatabase(UAnimSequence* AnimSequence)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		Database->Modify();
		
		FPoseSearchDatabaseSequence NewAsset;
		NewAsset.Sequence = AnimSequence;
		Database->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddBlendSpaceToDatabase(UBlendSpace* BlendSpace)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		Database->Modify();
		
		FPoseSearchDatabaseBlendSpace NewAsset;
		NewAsset.BlendSpace = BlendSpace;
		Database->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddAnimCompositeToDatabase(UAnimComposite* AnimComposite)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		FPoseSearchDatabaseAnimComposite NewAsset;
		NewAsset.AnimComposite = AnimComposite;
		Database->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddAnimMontageToDatabase(UAnimMontage* AnimMontage)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		Database->Modify();
		
		FPoseSearchDatabaseAnimMontage NewAsset;
		NewAsset.AnimMontage = AnimMontage;
		Database->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
	}
}

void FDatabaseViewModel::AddMultiSequenceToDatabase()
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		Database->Modify();
		
		FPoseSearchDatabaseMultiSequence NewAsset;
		Database->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
	}
}

bool FDatabaseViewModel::DeleteFromDatabase(int32 AnimationAssetIndex)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			Database->Modify();
			
			if (UAnimSequenceBase* AnimSequenceBase = Cast<UAnimSequenceBase>(DatabaseAnimationAssetBase->GetAnimationAsset()))
			{
				bool bModified = false;
				for (int32 NotifyIndex = AnimSequenceBase->Notifies.Num() - 1; NotifyIndex >= 0; --NotifyIndex)
				{
					const FAnimNotifyEvent& NotifyEvent = AnimSequenceBase->Notifies[NotifyIndex];
					if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
					{
						if (PoseSearchBranchIn->Database == Database)
						{
							if (!bModified)
							{
								AnimSequenceBase->Modify();
								bModified = true;
							}

							AnimSequenceBase->Notifies.RemoveAt(NotifyIndex);
						}
					}
				}

				if (bModified)
				{
					AnimSequenceBase->RefreshCacheData();	
				}
			}

			Database->RemoveAnimationAssetAt(AnimationAssetIndex);

			return true;
		}
	}

	return false;
}

void FDatabaseViewModel::SetDisableReselection(int32 AnimationAssetIndex, bool bEnabled)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetMutableAnimationAssetBase(AnimationAssetIndex))
		{
			Database->Modify();
			
			DatabaseAnimationAsset->SetDisableReselection(bEnabled);
		}
	}
}

bool FDatabaseViewModel::IsDisableReselection(int32 AnimationAssetIndex) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			return DatabaseAnimationAsset->IsDisableReselection();
		}
	}

	return false;
}

void FDatabaseViewModel::SetIsEnabled(int32 AnimationAssetIndex, bool bEnabled)
{
	if (UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetMutableAnimationAssetBase(AnimationAssetIndex))
		{
			Database->Modify();

			DatabaseAnimationAsset->SetIsEnabled(bEnabled);
		}
	}
}

bool FDatabaseViewModel::IsEnabled(int32 AnimationAssetIndex) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(AnimationAssetIndex))
		{
			return DatabaseAnimationAsset->IsEnabled();
		}
	}

	return false;
}

int32 FDatabaseViewModel::SetSelectedNode(int32 PoseIdx, bool bClearSelection, bool bDrawQuery, TConstArrayView<float> InQueryVector)
{
	int32 SelectedSourceAssetIdx = INDEX_NONE;

	if (bClearSelection)
	{
		RemovePreviewActors();
	}

	bIsEditorSelection = false;
	bDrawQueryVector = bDrawQuery;
	QueryVector = InQueryVector;

	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			if (SearchIndex.PoseMetadata.IsValidIndex(PoseIdx))
			{
				const uint32 IndexAssetIndex = SearchIndex.PoseMetadata[PoseIdx].GetAssetIndex();
				if (SearchIndex.Assets.IsValidIndex(IndexAssetIndex))
				{
					const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
					const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(IndexAsset.GetSourceAssetIdx());
					check(DatabaseAnimationAsset);
					int32 PreviewActorGroupIndex = INDEX_NONE;
					bool bIsSynchronizedRootTransformOriginValid = false;
					FTransform SynchronizedRootTransformOrigin = FTransform::Identity;
					for (int32 RoleIndex = 0; RoleIndex < DatabaseAnimationAsset->GetNumRoles(); ++RoleIndex)
					{
						FDatabasePreviewActor PreviewActor;
						const UE::PoseSearch::FRole Role = DatabaseAnimationAsset->GetRole(RoleIndex);
						const FTransform& RootTransformOrigin = DatabaseAnimationAsset->GetRootTransformOriginForRole(Role);
						if (PreviewActor.SpawnPreviewActor(GetWorld(), Database, IndexAssetIndex, Role, RootTransformOrigin, bIsSynchronizedRootTransformOriginValid ? &SynchronizedRootTransformOrigin : nullptr, PoseIdx))
						{
							if (PreviewActorGroupIndex == INDEX_NONE)
							{
								PreviewActorGroupIndex = PreviewActors.AddDefaulted();
							}

							if (!bIsSynchronizedRootTransformOriginValid)
							{
								bIsSynchronizedRootTransformOriginValid = true;
								SynchronizedRootTransformOrigin = PreviewActor.GetRootTransformOrigin();
							}

							MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(Database->Schema->SampleRate) - PreviewActor.GetPlayTimeOffset());
							MinPreviewPlayLength = FMath::Min(MinPreviewPlayLength, IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate) - PreviewActor.GetPlayTimeOffset());
							PreviewActors[PreviewActorGroupIndex].Add(PreviewActor);
							SelectedSourceAssetIdx = IndexAsset.GetSourceAssetIdx();
						}
					}
				}
			}

			DatabaseDataDetails.Pin()->Reconstruct();

			for (TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
			{
				for (FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
				{
					PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
				}
			}

			SetPlayTime(0.f, false);
		}
	}

	ProcessSelectedActor(nullptr);

	return SelectedSourceAssetIdx;
}

void FDatabaseViewModel::SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes)
{
	RemovePreviewActors();

	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			TMap<int32, int32> AssociatedAssetIndices;
			for (int32 i = 0; i < InSelectedNodes.Num(); ++i)
			{
				AssociatedAssetIndices.FindOrAdd(InSelectedNodes[i]->SourceAssetIdx) = i;
			}

			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			for (int32 IndexAssetIndex = 0; IndexAssetIndex < SearchIndex.Assets.Num(); ++IndexAssetIndex)
			{
				const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
				if (AssociatedAssetIndices.Find(IndexAsset.GetSourceAssetIdx()))
				{
					const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(IndexAsset.GetSourceAssetIdx());
					check(DatabaseAnimationAsset);
					int32 PreviewActorGroupIndex = INDEX_NONE;
					bool bIsSynchronizedRootTransformOriginValid = false;
					FTransform SynchronizedRootTransformOrigin = FTransform::Identity;
					for (int32 RoleIndex = 0; RoleIndex < DatabaseAnimationAsset->GetNumRoles(); ++RoleIndex)
					{
						FDatabasePreviewActor PreviewActor;
						const UE::PoseSearch::FRole Role = DatabaseAnimationAsset->GetRole(RoleIndex);
						const FTransform& RootTransformOrigin = DatabaseAnimationAsset->GetRootTransformOriginForRole(Role);
						if (PreviewActor.SpawnPreviewActor(GetWorld(), Database, IndexAssetIndex, Role, RootTransformOrigin, bIsSynchronizedRootTransformOriginValid ? &SynchronizedRootTransformOrigin : nullptr))
						{
							if (PreviewActorGroupIndex == INDEX_NONE)
							{
								PreviewActorGroupIndex = PreviewActors.AddDefaulted();
							}

							if (!bIsSynchronizedRootTransformOriginValid)
							{
								bIsSynchronizedRootTransformOriginValid = true;
								SynchronizedRootTransformOrigin = PreviewActor.GetRootTransformOrigin();
							}

							MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(Database->Schema->SampleRate) - IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate));
							PreviewActors[PreviewActorGroupIndex].Add(PreviewActor);
						}
					}
				}
			}

			DatabaseDataDetails.Pin()->Reconstruct();
			for (TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
			{
				for (FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
				{
					PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
				}
			}
		}

		ProcessSelectedActor(nullptr);
	}
}

void FDatabaseViewModel::ProcessSelectedActor(AActor* Actor)
{
	SelectedActorIndexAssetIndex = INDEX_NONE;

	for (const TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
	{
		for (const FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
		{
			if (PreviewActor.GetActor() == Actor)
			{
				SelectedActorIndexAssetIndex = PreviewActor.GetIndexAssetIndex();
				return;
			}
		}
	}
}

void FDatabaseViewModel::SetDrawQueryVector(bool bValue)
{
	if (bDrawQueryVector != bValue)
	{
		bDrawQueryVector = bValue;
		DatabaseDataDetails.Pin()->Reconstruct();
	}
}

const FSearchIndexAsset* FDatabaseViewModel::GetSelectedActorIndexAsset() const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			if (SearchIndex.Assets.IsValidIndex(SelectedActorIndexAssetIndex))
			{
				return &SearchIndex.Assets[SelectedActorIndexAssetIndex];
			}
		}
	}
	return nullptr;
}

TRange<double> FDatabaseViewModel::GetPreviewPlayRange() const
{
	constexpr double ViewRangeSlack = 0.2;
	return TRange<double>(MinPreviewPlayLength - ViewRangeSlack, MaxPreviewPlayLength + ViewRangeSlack);
}

float FDatabaseViewModel::GetPlayTime() const
{
	return PlayTime;
}

void FDatabaseViewModel::SetPlayTime(float NewPlayTime, bool bInTickPlayTime)
{
	NewPlayTime = FMath::Clamp(NewPlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);
	DeltaTimeMultiplier = bInTickPlayTime ? DeltaTimeMultiplier : 0.0f;

	if (!FMath::IsNearlyEqual(PlayTime, NewPlayTime))
	{
		PlayTime = NewPlayTime;
		
		if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
		{
			if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				for (TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
				{
					for (FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
					{
						PreviewActor.UpdatePreviewActor(Database, PlayTime, bQuantizeAnimationToPoseData);
					}
				}
			}
		}
	}
}

bool FDatabaseViewModel::GetAnimationTime(int32 SourceAssetIdx, float& CurrentPlayTime, FVector& BlendParameters) const
{
	if (const UPoseSearchDatabase* Database = GetPoseSearchDatabase())
	{
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = Database->GetSearchIndex();
			for (const TArray<FDatabasePreviewActor>& PreviewActorGroup : PreviewActors)
			{
				for (const FDatabasePreviewActor& PreviewActor : PreviewActorGroup)
				{
					if (PreviewActor.GetIndexAssetIndex() >= 0 && PreviewActor.GetIndexAssetIndex() < SearchIndex.Assets.Num())
					{
						const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.GetIndexAssetIndex()];
						if (IndexAsset.GetSourceAssetIdx() == SourceAssetIdx)
						{
							CurrentPlayTime = PreviewActor.GetSampler().ToNormalizedTime(PlayTime + IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate) + PreviewActor.GetPlayTimeOffset());
							BlendParameters = IndexAsset.GetBlendParameters();
							return true;
						}
					}
				}
			}

			for (const FSearchIndexAsset& IndexAsset : SearchIndex.Assets)
			{
				if (IndexAsset.GetSourceAssetIdx() == SourceAssetIdx)
				{
					CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(Database->Schema->SampleRate);
					BlendParameters = IndexAsset.GetBlendParameters();

					const bool bIsBlendSpace = Database->GetAnimationAssetStruct(IndexAsset).GetPtr<FPoseSearchDatabaseBlendSpace>() != nullptr;
					if (bIsBlendSpace && !FMath::IsNearlyEqual(MaxPreviewPlayLength, MinPreviewPlayLength))
					{
						CurrentPlayTime = (CurrentPlayTime - MaxPreviewPlayLength) / (MaxPreviewPlayLength - MinPreviewPlayLength);
					}
					return true;
				}
			}
		}
	}

	CurrentPlayTime = 0.f;
	BlendParameters = FVector::ZeroVector;
	return false;
}
}
