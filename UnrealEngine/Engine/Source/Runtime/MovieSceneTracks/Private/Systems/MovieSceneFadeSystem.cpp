// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneFadeSystem.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EngineGlobals.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"
#include "GameFramework/PlayerController.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneExecutionToken.h"
#include "MovieSceneSequence.h"
#include "Sections/MovieSceneFadeSection.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "MovieSceneTracksComponentTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneFadeSystem)

namespace UE::MovieScene
{

struct FFadeUtil
{
	static void ApplyFade(IMovieScenePlayer& Player, float FadeValue, const FLinearColor& FadeColor, bool bFadeAudio)
	{
		// Set editor preview/fade
		EMovieSceneViewportParams ViewportParams;
		ViewportParams.SetWhichViewportParam = ( EMovieSceneViewportParams::SetViewportParam )( EMovieSceneViewportParams::SVP_FadeAmount | EMovieSceneViewportParams::SVP_FadeColor );
		ViewportParams.FadeAmount = FadeValue;
		ViewportParams.FadeColor = FadeColor;

		TMap<FViewportClient*, EMovieSceneViewportParams> ViewportParamsMap;
		Player.GetViewportSettings( ViewportParamsMap );
		for( auto ViewportParamsPair : ViewportParamsMap )
		{
			ViewportParamsMap[ ViewportParamsPair.Key ] = ViewportParams;
		}
		Player.SetViewportSettings( ViewportParamsMap );

		// Set runtime fade
		UObject* Context = Player.GetPlaybackContext();
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		if( World && ( World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE ) )
		{
			APlayerController* PlayerController = World->GetGameInstance()->GetFirstLocalPlayerController();
			if( PlayerController != nullptr && IsValid(PlayerController->PlayerCameraManager) )
			{
				PlayerController->PlayerCameraManager->SetManualCameraFade( FadeValue, FadeColor, bFadeAudio );
			}
		}
	}
};

struct FPreAnimatedFadeState
{
	float FadeValue;
	FLinearColor FadeColor;
	bool bFadeAudio;

	static FPreAnimatedFadeState SaveState(IMovieScenePlayer* Player)
	{
		float FadeAmount = 0.f;
		FLinearColor FadeColor = FLinearColor::Black;
		bool bFadeAudio = false;

		UObject* Context = Player->GetPlaybackContext();
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
		{
			APlayerController* PlayerController = World->GetGameInstance()->GetFirstLocalPlayerController();
			if (PlayerController != nullptr && IsValid(PlayerController->PlayerCameraManager))
			{
				FadeAmount = PlayerController->PlayerCameraManager->FadeAmount;
				FadeColor = PlayerController->PlayerCameraManager->FadeColor;
				bFadeAudio = PlayerController->PlayerCameraManager->bFadeAudio;
			}
		}

		return FPreAnimatedFadeState{ FadeAmount, FadeColor, bFadeAudio };
	}

	void RestoreState(const FMovieSceneAnimTypeID& Unused, const FRestoreStateParams& Params)
	{
		IMovieScenePlayer* Player = Params.GetTerminalPlayer();
		if (!ensure(Player))
		{
			return;
		}
		
		FFadeUtil::ApplyFade(*Player, FadeValue, FadeColor, bFadeAudio);
	}
};

struct FPreAnimatedFadeStateStorage : TSimplePreAnimatedStateStorage<FMovieSceneAnimTypeID, FPreAnimatedFadeState>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedFadeStateStorage> StorageID;

	FPreAnimatedStorageID GetStorageType() const override { return StorageID; }
};

TAutoRegisterPreAnimatedStorageID<FPreAnimatedFadeStateStorage> FPreAnimatedFadeStateStorage::StorageID;

struct FEvaluateFade
{
	const FInstanceRegistry* InstanceRegistry;
	TSharedPtr<FPreAnimatedFadeStateStorage> PreAnimatedStorage;

	FEvaluateFade(const FInstanceRegistry* InInstanceRegistry, TSharedPtr<FPreAnimatedFadeStateStorage> InPreAnimatedStorage)
		: InstanceRegistry(InInstanceRegistry)
		, PreAnimatedStorage(InPreAnimatedStorage)
	{}

	void ForEachAllocation(const FEntityAllocation* Allocation, TRead<FMovieSceneEntityID> EntityIDs, TRead<FRootInstanceHandle> RootInstanceHandles, TRead<FFadeComponentData> FadeComponents, TRead<double> FadeAmounts) const
	{
		static const TMovieSceneAnimTypeID<FEvaluateFade> AnimTypeID;

		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		const int32 Num = Allocation->Num();
		const bool bWantsRestoreState = Allocation->HasComponent(BuiltInComponents->Tags.RestoreState);
		const FMovieSceneAnimTypeID Key = AnimTypeID;

		for (int32 Index = 0; Index < Num; ++Index)
		{
			FRootInstanceHandle RootInstanceHandle = RootInstanceHandles[Index];
			const FSequenceInstance& Instance = InstanceRegistry->GetInstance(RootInstanceHandle);
			IMovieScenePlayer* Player = Instance.GetPlayer();

			PreAnimatedStorage->BeginTrackingEntity(EntityIDs[Index], bWantsRestoreState, RootInstanceHandle, Key);
			PreAnimatedStorage->CachePreAnimatedValue(Key, [Player](const FMovieSceneAnimTypeID&) { return FPreAnimatedFadeState::SaveState(Player); });

			const FFadeComponentData& FadeComponent(FadeComponents[Index]);
			FFadeUtil::ApplyFade(*Player, FadeAmounts[Index], FadeComponent.FadeColor, FadeComponent.bFadeAudio);
		}
	}
};

} // namespace UE::MovieScene

UMovieSceneFadeSystem::UMovieSceneFadeSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	RelevantComponent = FMovieSceneTracksComponentTypes::Get()->Fade;
	Phase = ESystemPhase::Scheduling;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		DefineComponentConsumer(GetClass(), BuiltInComponents->DoubleResult[0]);
	}
}

void UMovieSceneFadeSystem::OnLink()
{
	using namespace UE::MovieScene;

	PreAnimatedStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedFadeStateStorage>();
}

void UMovieSceneFadeSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->RootInstanceHandle)
	.Read(TrackComponents->Fade)
	.Read(BuiltInComponents->DoubleResult[0])
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.Schedule_PerAllocation<FEvaluateFade>(& Linker->EntityManager, TaskScheduler, Linker->GetInstanceRegistry(), PreAnimatedStorage);
}

void UMovieSceneFadeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->RootInstanceHandle)
	.Read(TrackComponents->Fade)
	.Read(BuiltInComponents->DoubleResult[0])
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.Dispatch_PerAllocation<FEvaluateFade>(& Linker->EntityManager, InPrerequisites, &Subsequents, Linker->GetInstanceRegistry(), PreAnimatedStorage);
}

