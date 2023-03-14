// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MovieSceneLatentActionManager.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
	#include "MovieSceneSequenceTickManager.h"
	#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#endif

#include "UMGSequenceTickManager.generated.h"

class FMovieSceneEntitySystemRunner;
class UMovieSceneEntitySystemLinker;
class UUserWidget;

USTRUCT()
struct FSequenceTickManagerWidgetData
{
	GENERATED_BODY()

	/** The widget was ticked. */
	bool bIsTicking = true;

	/** The widget was ticked last update. */
	bool bLastKnownTickState = true;

	/** The widget's animation was ticked this frame. */
	bool bActionsAndAnimationTicked = false;
};


/**
 * An automatically created global object that will manage all widget animations.
 */
UCLASS()
class UMG_API UUMGSequenceTickManager : public UObject
{
public:
	GENERATED_BODY()

	UUMGSequenceTickManager(const FObjectInitializer& Init);

	UMovieSceneEntitySystemLinker* GetLinker() { return Linker; }
	TSharedPtr<FMovieSceneEntitySystemRunner> GetRunner() { return Runner; }

	void AddLatentAction(FMovieSceneSequenceLatentActionDelegate Delegate);
	void ClearLatentActions(UObject* Object);
	void RunLatentActions();

	static UUMGSequenceTickManager* Get(UObject* PlaybackContext);

	void ForceFlush();

	void AddWidget(UUserWidget* InWidget);
	void RemoveWidget(UUserWidget* InWidget);

	void OnWidgetTicked(UUserWidget* InWidget);

private:
	virtual void BeginDestroy() override;

	void HandleSlatePostTick(float DeltaSeconds);
	void TickWidgetAnimations(float DeltaSeconds);

private:

	UPROPERTY(transient)
	TMap<TWeakObjectPtr<UUserWidget>, FSequenceTickManagerWidgetData> WeakUserWidgetData;

	UPROPERTY(transient)
	TObjectPtr<UMovieSceneEntitySystemLinker> Linker;

	TSharedPtr<FMovieSceneEntitySystemRunner> Runner;

	FDelegateHandle SlateApplicationPreTickHandle, SlateApplicationPostTickHandle;

	FMovieSceneLatentActionManager LatentActionManager;

	bool bIsTicking;
};
