// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/Blending/MovieSceneInitialValueStore.h"
#include "Evaluation/Blending/MovieSceneBlendingActuator.h"

struct FMovieSceneRemoveInitialValueToken : IMovieScenePreAnimatedToken
{
	FMovieSceneRemoveInitialValueToken(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
		: WeakActuator(InWeakActuator)
	{}

	virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& Params) override
	{
		TSharedPtr<IMovieSceneBlendingActuator> Store = WeakActuator.Pin();
		if (Store.IsValid())
		{
			Store->RemoveInitialValueForObject(FObjectKey(&Object));
		}
	}

private:
	/** The store to remove the initial value from */
	TWeakPtr<IMovieSceneBlendingActuator> WeakActuator;
};

struct FMovieSceneRemoveInitialGlobalValueToken : IMovieScenePreAnimatedGlobalToken
{
	FMovieSceneRemoveInitialGlobalValueToken(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
		: WeakActuator(InWeakActuator)
	{}

	virtual void RestoreState(const UE::MovieScene::FRestoreStateParams& Params) override
	{
		TSharedPtr<IMovieSceneBlendingActuator> Store = WeakActuator.Pin();
		if (Store.IsValid())
		{
			Store->RemoveInitialValueForObject(FObjectKey());
		}
	}

private:
	/** The store to remove the initial value from */
	TWeakPtr<IMovieSceneBlendingActuator> WeakActuator;
};

FMovieSceneRemoveInitialValueTokenProducer::FMovieSceneRemoveInitialValueTokenProducer(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
	: WeakActuator(InWeakActuator)
{
}

IMovieScenePreAnimatedTokenPtr FMovieSceneRemoveInitialValueTokenProducer::CacheExistingState(UObject& Object) const
{
	return FMovieSceneRemoveInitialValueToken(WeakActuator);
}

FMovieSceneRemoveInitialGlobalValueTokenProducer::FMovieSceneRemoveInitialGlobalValueTokenProducer(TWeakPtr<IMovieSceneBlendingActuator> InWeakActuator)
	: WeakActuator(InWeakActuator)
{
}

IMovieScenePreAnimatedGlobalTokenPtr FMovieSceneRemoveInitialGlobalValueTokenProducer::CacheExistingState() const
{
	return FMovieSceneRemoveInitialGlobalValueToken(WeakActuator);
}
