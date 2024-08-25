// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePreAnimatedState.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedCaptureSources.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectTokenStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedRootTokenStorage.h"

DECLARE_CYCLE_STAT(TEXT("Save Pre Animated State"), MovieSceneEval_SavePreAnimatedState, STATGROUP_MovieSceneEval);

FMovieScenePreAnimatedState::~FMovieScenePreAnimatedState()
{
	// Ensure that the global state capture request is removed
	UMovieSceneEntitySystemLinker* CurrentLinker = WeakLinker.Get();
	if (CurrentLinker && bCapturingGlobalPreAnimatedState)
	{
		checkf(CurrentLinker->PreAnimatedState.NumRequestsForGlobalState > 0, TEXT("Increment/Decrement mismatch on FPreAnimatedState::NumRequestsForGlobalState"));
		--CurrentLinker->PreAnimatedState.NumRequestsForGlobalState;
	}
}

void FMovieScenePreAnimatedState::Initialize(UMovieSceneEntitySystemLinker* Linker, UE::MovieScene::FRootInstanceHandle InInstanceHandle)
{
	// If we're re-using a pre-animated state class and it was previously
	// capturing global state make sure to decrement that request
	UMovieSceneEntitySystemLinker* PreviousLinker = WeakLinker.Get();
	if (PreviousLinker && bCapturingGlobalPreAnimatedState)
	{
		checkf(PreviousLinker->PreAnimatedState.NumRequestsForGlobalState > 0, TEXT("Increment/Decrement mismatch on FPreAnimatedState::NumRequestsForGlobalState"));
		--PreviousLinker->PreAnimatedState.NumRequestsForGlobalState;

	}

	bCapturingGlobalPreAnimatedState = false;

	WeakLinker = Linker;
	InstanceHandle = InInstanceHandle;
}

UMovieSceneEntitySystemLinker* FMovieScenePreAnimatedState::GetLinker() const
{
	return WeakLinker.Get();
}

bool FMovieScenePreAnimatedState::IsCapturingGlobalPreAnimatedState() const
{
	return bCapturingGlobalPreAnimatedState;
}

void FMovieScenePreAnimatedState::EnableGlobalPreAnimatedStateCapture()
{
	if (bCapturingGlobalPreAnimatedState)
	{
		return;
	}

	bCapturingGlobalPreAnimatedState = true;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (ensure(Linker))
	{
		++Linker->PreAnimatedState.NumRequestsForGlobalState;
	}
}

void FMovieScenePreAnimatedState::SavePreAnimatedState(UObject& InObject, FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedTokenProducer& Producer)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	if (bCapturingGlobalPreAnimatedState || Linker->PreAnimatedState.HasActiveCaptureSource())
	{
		Linker->PreAnimatedState.SavePreAnimatedStateDirectly(InObject, InTokenType, Producer);
	}
}

void FMovieScenePreAnimatedState::SavePreAnimatedState(FMovieSceneAnimTypeID InTokenType, const IMovieScenePreAnimatedGlobalTokenProducer& Producer)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	if (bCapturingGlobalPreAnimatedState || Linker->PreAnimatedState.HasActiveCaptureSource())
	{
		Linker->PreAnimatedState.SavePreAnimatedStateDirectly(InTokenType, Producer);
	}
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState()
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (Linker)
	{
		Linker->PreAnimatedState.RestoreGlobalState(FRestoreStateParams{Linker, InstanceHandle});
	}
}

void FMovieScenePreAnimatedState::DiscardPreAnimatedState()
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (Linker)
	{
		Linker->PreAnimatedState.DiscardGlobalState(FRestoreStateParams{ Linker, InstanceHandle });
	}
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(UObject& Object)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	TSharedPtr<FPreAnimatedObjectGroupManager> ObjectGroupManager = Linker->PreAnimatedState.FindGroupManager<FPreAnimatedObjectGroupManager>();
	if (!ObjectGroupManager)
	{
		return;
	}

	FPreAnimatedStorageGroupHandle Group = ObjectGroupManager->FindGroupForKey(&Object);
	if (!Group)
	{
		return;
	}

	Linker->PreAnimatedState.RestoreStateForGroup(Group, FRestoreStateParams{Linker, InstanceHandle});
}

void FMovieScenePreAnimatedState::RestorePreAnimatedState(UClass* GeneratedClass)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	TSharedPtr<FPreAnimatedObjectGroupManager> ObjectGroupManager = Linker->PreAnimatedState.FindGroupManager<FPreAnimatedObjectGroupManager>();
	if (ObjectGroupManager)
	{
		TArray<FPreAnimatedStorageGroupHandle> Handles;
		ObjectGroupManager->GetGroupsByClass(GeneratedClass, Handles);

		FRestoreStateParams Params{Linker, InstanceHandle};
		for (FPreAnimatedStorageGroupHandle GroupHandle : Handles)
		{
			Linker->PreAnimatedState.RestoreStateForGroup(GroupHandle, Params);
		}
	}
}


void FMovieScenePreAnimatedState::RestorePreAnimatedState(UObject& Object, TFunctionRef<bool(FMovieSceneAnimTypeID)> InFilter)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	TSharedPtr<FAnimTypePreAnimatedStateObjectStorage> ObjectStorage = Linker->PreAnimatedState.FindStorage(FAnimTypePreAnimatedStateObjectStorage::StorageID);
	if (!ObjectStorage)
	{
		return;
	}

	struct FRestoreMask : FAnimTypePreAnimatedStateObjectStorage::IRestoreMask
	{
		TFunctionRef<bool(FMovieSceneAnimTypeID)>* Filter;

		virtual bool CanRestore(const FPreAnimatedObjectTokenTraits::KeyType& InKey) const override
		{
			return (*Filter)(InKey.Get<1>());
		}
	} RestoreMask;
	RestoreMask.Filter = &InFilter;

	ObjectStorage->SetRestoreMask(&RestoreMask);

	RestorePreAnimatedState(Object);

	ObjectStorage->SetRestoreMask(nullptr);
}

void FMovieScenePreAnimatedState::DiscardEntityTokens()
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (Linker)
	{
		Linker->PreAnimatedState.DiscardTransientState();
	}
}

void FMovieScenePreAnimatedState::DiscardAndRemoveEntityTokensForObject(UObject& Object)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	TSharedPtr<FPreAnimatedObjectGroupManager> ObjectGroupManager = Linker->PreAnimatedState.FindGroupManager<FPreAnimatedObjectGroupManager>();
	if (!ObjectGroupManager)
	{
		return;
	}

	FPreAnimatedStorageGroupHandle Group = ObjectGroupManager->FindGroupForKey(&Object);
	if (!Group)
	{
		return;
	}

	Linker->PreAnimatedState.DiscardStateForGroup(Group);
}

void FMovieScenePreAnimatedState::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	if (!Linker)
	{
		return;
	}

	TSharedPtr<FPreAnimatedObjectGroupManager> ObjectGroupManager = Linker->PreAnimatedState.FindGroupManager<FPreAnimatedObjectGroupManager>();
	if (ObjectGroupManager)
	{
		ObjectGroupManager->OnObjectsReplaced(ReplacementMap);
	}
}

bool FMovieScenePreAnimatedState::ContainsAnyStateForSequence() const
{
	using namespace UE::MovieScene;

	UMovieSceneEntitySystemLinker* Linker = WeakLinker.Get();
	return Linker && InstanceHandle.IsValid() && Linker->PreAnimatedState.ContainsAnyStateForInstanceHandle(InstanceHandle);
}
