// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Containers/SortedMap.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequence.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "Evaluation/Instances/MovieSceneTrackEvaluator.h"

#include "Sections/MovieSceneSubSection.h"

#include "UObject/Package.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneEvaluationTemplateInstance)

DECLARE_CYCLE_STAT(TEXT("Entire Evaluation Cost"), MovieSceneEval_EntireEvaluationCost, STATGROUP_MovieSceneEval);


FMovieSceneRootEvaluationTemplateInstance::FMovieSceneRootEvaluationTemplateInstance()
	: CompiledDataManager(nullptr)
	, EntitySystemLinker(nullptr)
	, RootID(MovieSceneSequenceID::Root)
{
#if WITH_EDITOR
	EmulatedNetworkMask = EMovieSceneServerClientMask::All;
#endif
}

void FMovieSceneRootEvaluationTemplateInstance::BeginDestroy()
{
	if (EntitySystemLinker && IsValidChecked(EntitySystemLinker) && !EntitySystemLinker->IsUnreachable() && !EntitySystemLinker->HasAnyFlags(RF_BeginDestroyed))
	{
		if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin())
		{
			Runner->DiscardQueuedUpdates(RootInstanceHandle);
		}

		EntitySystemLinker->GetInstanceRegistry()->DestroyInstance(RootInstanceHandle);
		EntitySystemLinker->ResetActiveRunners();
	}

	RootInstanceHandle = UE::MovieScene::FRootInstanceHandle();
	CompiledDataManager = nullptr;
	EntitySystemLinker = nullptr;
}

FMovieSceneRootEvaluationTemplateInstance::~FMovieSceneRootEvaluationTemplateInstance()
{
	BeginDestroy();
}

UMovieSceneEntitySystemLinker* FMovieSceneRootEvaluationTemplateInstance::ConstructEntityLinker(IMovieScenePlayer& Player)
{
	UMovieSceneEntitySystemLinker* Linker = Player.ConstructEntitySystemLinker();
	if (Linker)
	{
		return Linker;
	}

	UObject* PlaybackContext = Player.GetPlaybackContext();
	return UMovieSceneEntitySystemLinker::FindOrCreateLinker(PlaybackContext, UE::MovieScene::EEntitySystemLinkerRole::Standalone, TEXT("DefaultEntitySystemLinker"));
}

void FMovieSceneRootEvaluationTemplateInstance::Initialize(UMovieSceneSequence& InRootSequence, IMovieScenePlayer& Player, UMovieSceneCompiledDataManager* InCompiledDataManager, TWeakPtr<FMovieSceneEntitySystemRunner> InWeakRunner)
{
	bool bReinitialize = (
			// Initialize if we weren't initialized before and this is our first sequence.
			WeakRootSequence.Get() == nullptr ||
			// Initialize if we lost our linker.
			EntitySystemLinker == nullptr);

	const UMovieSceneCompiledDataManager* PreviousCompiledDataManager = CompiledDataManager;
	if (InCompiledDataManager)
	{
		CompiledDataManager = InCompiledDataManager;
	}
	else
	{
#if WITH_EDITOR
		EMovieSceneServerClientMask Mask = EmulatedNetworkMask;
		if (Mask == EMovieSceneServerClientMask::All)
		{
			UObject* PlaybackContext = Player.GetPlaybackContext();
			UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

			if (World)
			{
				ENetMode NetMode = World->GetNetMode();
				if (NetMode == ENetMode::NM_DedicatedServer)
				{
					Mask = EMovieSceneServerClientMask::Server;
				}
				else if (NetMode == ENetMode::NM_Client)
				{
					Mask = EMovieSceneServerClientMask::Client;
				}
			}
		}
		CompiledDataManager = UMovieSceneCompiledDataManager::GetPrecompiledData(Mask);
#else
		CompiledDataManager = UMovieSceneCompiledDataManager::GetPrecompiledData();
#endif
	}
	bReinitialize |= (PreviousCompiledDataManager != CompiledDataManager);

	TSharedPtr<FMovieSceneEntitySystemRunner> PreviousRunner = WeakRunner.Pin();

	if (UMovieSceneSequence* ExistingSequence = WeakRootSequence.Get())
	{
		if (ExistingSequence != &InRootSequence)
		{
			bReinitialize = true;
		}
	}

	CompiledDataID = CompiledDataManager->GetDataID(&InRootSequence);
	WeakRootSequence = &InRootSequence;
	RootID = MovieSceneSequenceID::Root;
	WeakRunner = InWeakRunner;

	if (bReinitialize)
	{
		if (RootInstanceHandle.IsValid())
		{
			if (PreviousRunner)
			{
				PreviousRunner->AbandonAndDestroyInstance(RootInstanceHandle);
			}
			else if (EntitySystemLinker)
			{
				EntitySystemLinker->GetInstanceRegistry()->DestroyInstance(RootInstanceHandle);
			}
			else
			{
				ensureMsgf(false, TEXT("Unable to destroy previously allocated sequence instance - this could indicate a memory leak."));
			}
		}

		Player.State.PersistentEntityData.Reset();
		Player.State.PersistentSharedData.Reset();

		EntitySystemLinker = ConstructEntityLinker(Player);
		RootInstanceHandle = EntitySystemLinker->GetInstanceRegistry()->AllocateRootInstance(&Player);

		TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin();
		if (ensure(Runner) && EntitySystemLinker)
		{
			if (Runner->IsAttachedToLinker() && Runner->GetLinker() != EntitySystemLinker)
			{
				Runner->DetachFromLinker();
			}

			if (!Runner->IsAttachedToLinker())
			{
				Runner->AttachToLinker(EntitySystemLinker);
			}
		}

		Player.PreAnimatedState.Initialize(EntitySystemLinker, RootInstanceHandle);
	}
}

void FMovieSceneRootEvaluationTemplateInstance::Evaluate(FMovieSceneContext Context, IMovieScenePlayer& Player)
{
	EvaluateSynchronousBlocking(Context, Player);
}

void FMovieSceneRootEvaluationTemplateInstance::EvaluateSynchronousBlocking(FMovieSceneContext Context, IMovieScenePlayer& Player)
{
	if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin())
	{
		Runner->QueueUpdate(Context, RootInstanceHandle);
		Runner->Flush();
	}
}

void FMovieSceneRootEvaluationTemplateInstance::OnFinished()
{
	ResetDirectorInstances();
}

void FMovieSceneRootEvaluationTemplateInstance::ResetDirectorInstances()
{
	DirectorInstances.Reset();
}

void FMovieSceneRootEvaluationTemplateInstance::EnableGlobalPreAnimatedStateCapture()
{
	using namespace UE::MovieScene;

	if (ensure(EntitySystemLinker))
	{
		const FSequenceInstance& Instance = EntitySystemLinker->GetInstanceRegistry()->GetInstance(RootInstanceHandle);
		Instance.GetPlayer()->PreAnimatedState.EnableGlobalPreAnimatedStateCapture();
	}
}

UMovieSceneSequence* FMovieSceneRootEvaluationTemplateInstance::GetSequence(FMovieSceneSequenceIDRef SequenceID) const
{
	if (SequenceID == MovieSceneSequenceID::Root)
	{
		return WeakRootSequence.Get();
	}
	else if (CompiledDataID.IsValid())
	{
		const FMovieSceneSequenceHierarchy* Hierarchy       = CompiledDataManager->FindHierarchy(CompiledDataID);
		const FMovieSceneSubSequenceData*   SubSequenceData = Hierarchy ? Hierarchy->FindSubData(SequenceID) : nullptr;
		if (SubSequenceData)
		{
			return SubSequenceData->GetSequence();
		}
	}
	return nullptr;
}

UMovieSceneEntitySystemLinker* FMovieSceneRootEvaluationTemplateInstance::GetEntitySystemLinker() const
{
	return EntitySystemLinker;
}

bool FMovieSceneRootEvaluationTemplateInstance::HasEverUpdated() const
{
	if (EntitySystemLinker)
	{
		const UE::MovieScene::FSequenceInstance* SequenceInstance = &EntitySystemLinker->GetInstanceRegistry()->GetInstance(RootInstanceHandle);
		return SequenceInstance && SequenceInstance->HasEverUpdated();
	}

	return false;
}

const FMovieSceneSequenceHierarchy* FMovieSceneRootEvaluationTemplateInstance::GetHierarchy() const
{
	return CompiledDataManager->FindHierarchy(GetCompiledDataID());
}

void FMovieSceneRootEvaluationTemplateInstance::GetSequenceParentage(const UE::MovieScene::FInstanceHandle InstanceHandle, TArray<UE::MovieScene::FInstanceHandle>& OutParentHandles) const
{
	using namespace UE::MovieScene;

	if (!ensure(EntitySystemLinker))
	{
		return;
	}

	// Get the root instance so we can find all necessary sub-instances from it.
	const FInstanceRegistry* InstanceRegistry = EntitySystemLinker->GetInstanceRegistry();

	check(InstanceHandle.IsValid());
	const FSequenceInstance& Instance = InstanceRegistry->GetInstance(InstanceHandle);

	checkf(Instance.GetRootInstanceHandle() == RootInstanceHandle, TEXT("The provided instance handle relates to a different root sequence."));
	const FSequenceInstance& RootInstance = InstanceRegistry->GetInstance(RootInstanceHandle);

	// Find the hierarchy node for the provided instance, and walk up from there to populate the output array.
	const FMovieSceneSequenceHierarchy* Hierarchy = GetHierarchy();
	if (!ensure(Hierarchy))
	{
		return;
	}

	const FMovieSceneSequenceHierarchyNode* HierarchyNode = Hierarchy->FindNode(Instance.GetSequenceID());
	while (HierarchyNode && HierarchyNode->ParentID.IsValid())
	{
		if (HierarchyNode->ParentID != MovieSceneSequenceID::Root)
		{
			const FInstanceHandle ParentHandle = RootInstance.FindSubInstance(HierarchyNode->ParentID);
			OutParentHandles.Add(ParentHandle);
		}
		else
		{
			OutParentHandles.Add(RootInstanceHandle);
		}
		HierarchyNode = Hierarchy->FindNode(HierarchyNode->ParentID);
	}
}

UE::MovieScene::FSequenceInstance* FMovieSceneRootEvaluationTemplateInstance::FindInstance(FMovieSceneSequenceID SequenceID)
{
	using namespace UE::MovieScene;

	if (ensure(EntitySystemLinker))
	{
		FSequenceInstance* SequenceInstance = &EntitySystemLinker->GetInstanceRegistry()->MutateInstance(RootInstanceHandle);

		if (SequenceID == MovieSceneSequenceID::Root)
		{
			return SequenceInstance;
		}

		FInstanceHandle SubHandle = SequenceInstance->FindSubInstance(SequenceID);
		if (SubHandle.IsValid())
		{
			return &EntitySystemLinker->GetInstanceRegistry()->MutateInstance(SubHandle);
		}
	}

	return nullptr;
}

const UE::MovieScene::FSequenceInstance* FMovieSceneRootEvaluationTemplateInstance::FindInstance(FMovieSceneSequenceID SequenceID) const
{
	using namespace UE::MovieScene;

	if (ensure(EntitySystemLinker))
	{
		const FSequenceInstance* SequenceInstance = &EntitySystemLinker->GetInstanceRegistry()->GetInstance(RootInstanceHandle);

		if (SequenceID == MovieSceneSequenceID::Root)
		{
			return SequenceInstance;
		}

		FInstanceHandle SubHandle = SequenceInstance->FindSubInstance(SequenceID);
		if (SubHandle.IsValid())
		{
			return &EntitySystemLinker->GetInstanceRegistry()->GetInstance(SubHandle);
		}
	}

	return nullptr;
}

UE::MovieScene::FMovieSceneEntityID FMovieSceneRootEvaluationTemplateInstance::FindEntityFromOwner(UObject* Owner, uint32 EntityID, FMovieSceneSequenceID SequenceID) const
{
	using namespace UE::MovieScene;

	if (const FSequenceInstance* SequenceInstance = FindInstance(SequenceID))
	{
		return SequenceInstance->FindEntity(Owner, EntityID);
	}

	return FMovieSceneEntityID::Invalid();
}

void FMovieSceneRootEvaluationTemplateInstance::FindEntitiesFromOwner(UObject* Owner, FMovieSceneSequenceID SequenceID, TArray<UE::MovieScene::FMovieSceneEntityID>& OutEntityIDs) const
{
	using namespace UE::MovieScene;

	if (const FSequenceInstance* SequenceInstance = FindInstance(SequenceID))
	{
		SequenceInstance->FindEntities(Owner, OutEntityIDs);
	}
}

UObject* FMovieSceneRootEvaluationTemplateInstance::GetOrCreateDirectorInstance(FMovieSceneSequenceIDRef SequenceID, IMovieScenePlayer& Player)
{
	UObject* ExistingDirectorInstance = DirectorInstances.FindRef(SequenceID);
	if (ExistingDirectorInstance)
	{
		return ExistingDirectorInstance;
	}

	UObject* NewDirectorInstance = nullptr;
	if (SequenceID == MovieSceneSequenceID::Root)
	{
		if (UMovieSceneSequence* Sequence = WeakRootSequence.Get())
		{
			NewDirectorInstance = Sequence->CreateDirectorInstance(Player, SequenceID);
		}
	}
	else if (const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(CompiledDataID))
	{
		const FMovieSceneSubSequenceData* SubData = Hierarchy->FindSubData(SequenceID);
		check(SubData);
		NewDirectorInstance = SubData->GetSequence()->CreateDirectorInstance(Player, SequenceID);
	}

	if (NewDirectorInstance)
	{
		DirectorInstances.Add(SequenceID, NewDirectorInstance);
	}

	return NewDirectorInstance;
}

void FMovieSceneRootEvaluationTemplateInstance::PlaybackContextChanged(IMovieScenePlayer& Player)
{
	using namespace UE::MovieScene;

	const bool bGlobalCapture = Player.PreAnimatedState.IsCapturingGlobalPreAnimatedState();

	if (RootInstanceHandle.IsValid() && EntitySystemLinker && IsValidChecked(EntitySystemLinker) && !EntitySystemLinker->IsUnreachable() && !EntitySystemLinker->HasAnyFlags(RF_BeginDestroyed))
	{
		EntitySystemLinker->CleanupInvalidBoundObjects();

		if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin())
		{
			Runner->QueueFinalUpdate(RootInstanceHandle);
			Runner->Flush();
		}

		if (bGlobalCapture)
		{
			Player.RestorePreAnimatedState();
		}
		EntitySystemLinker->GetInstanceRegistry()->DestroyInstance(RootInstanceHandle);
	}

	EntitySystemLinker = ConstructEntityLinker(Player);
	if (TSharedPtr<FMovieSceneEntitySystemRunner> Runner = WeakRunner.Pin())
	{
		if (Runner->IsAttachedToLinker())
		{
			Runner->DetachFromLinker();
		}

		Runner->AttachToLinker(EntitySystemLinker);
	}

	RootInstanceHandle = EntitySystemLinker->GetInstanceRegistry()->AllocateRootInstance(&Player);
	DirectorInstances.Reset();

	Player.PreAnimatedState.Initialize(EntitySystemLinker, RootInstanceHandle);
	if (bGlobalCapture)
	{
		Player.PreAnimatedState.EnableGlobalPreAnimatedStateCapture();
	}
}


const FMovieSceneSubSequenceData* FMovieSceneRootEvaluationTemplateInstance::FindSubData(FMovieSceneSequenceIDRef SequenceID) const
{
	const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(CompiledDataID);
	return Hierarchy ? Hierarchy->FindSubData(SequenceID) : nullptr;
}

void FMovieSceneRootEvaluationTemplateInstance::CopyActuators(FMovieSceneBlendingAccumulator& Accumulator) const
{
	using namespace UE::MovieScene;

	const FSequenceInstance& SequenceInstance = EntitySystemLinker->GetInstanceRegistry()->GetInstance(RootInstanceHandle);
	const FMovieSceneTrackEvaluator* LegacyEvaluator = SequenceInstance.GetLegacyEvaluator();

	if (LegacyEvaluator)
	{
		LegacyEvaluator->CopyActuators(Accumulator);
	}
}

#if WITH_EDITOR
void FMovieSceneRootEvaluationTemplateInstance::SetEmulatedNetworkMask(EMovieSceneServerClientMask InNewMask, IMovieScenePlayer& Player)
{
	check(InNewMask != EMovieSceneServerClientMask::None);
	EmulatedNetworkMask = InNewMask;
}
EMovieSceneServerClientMask FMovieSceneRootEvaluationTemplateInstance::GetEmulatedNetworkMask() const
{
	return EmulatedNetworkMask;
}
#endif

