// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/MovieSceneCompiledDataID.h"
#include "Containers/Array.h"
#include "Containers/SortedMap.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityLedger.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/MovieScenePlayback.h"
#include "MovieSceneSequenceID.h"
#include "Templates/UniquePtr.h"

class IMovieScenePlayer;
class UMovieSceneEntitySystemLinker;
class UObject;
struct FFrameTime;
struct FMovieSceneTrackEvaluator;
template <typename ElementType> class TRange;

namespace UE
{
namespace MovieScene
{

struct FCompiledDataVolatilityManager;
struct FPreAnimatedStateExtension;
struct FSequenceInstance;
struct FSubSequencePath;
struct ISequenceUpdater;

/**
 * A sequence instance represents a specific instance of a currently playing sequence, either as a top-level sequence in an IMovieScenePlayer, or as a sub sequence.
 * Any given sequence asset may have any number of instances created for it at any given time depending on how many times it is referenced by playing sequences
 */
struct MOVIESCENE_API FSequenceInstance
{
	/** Ledger that tracks all currently instantiated entities for this instance */
	FEntityLedger Ledger;

public:

	/**
	 * Called only for top-level sequence instances before any updates to it with the specified context.
	 * This allows the sequence an opportunity to dissect the context into a series of distinct evaluations to force determinism.
	 *
	 * @param Linker     The linker that owns this sequence instance
	 * @param Context    The overall context that this sequence instance is being evaluated with
	 * @param OutDissections   An array to populate with dissected time ranges that should be evaluated separately, in order.
	 */
	void DissectContext(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& Context, TArray<TRange<FFrameTime>>& OutDissections);


	/**
	 * Called for root level instances that have either never evaluated, or have previously finished evaluating
	 *
	 * @param Linker     The linker that owns this sequence instance
	 * @param InContext  The context that this sequence instance is to be evaluated with
	 */
	void Start(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& InContext);


	/**
	 * Called when this instance has been queued for evaluation in order for it to do any pre-work setup.
	 *
	 * @param Linker     The linker that owns this sequence instance
	 */
	void PreEvaluation(UMovieSceneEntitySystemLinker* Linker);


	/**
	 * Called after dissection for root level instances in order for this sequence instacne to update any entities it needs for evaluation.
	 *
	 * @param Linker     The linker that owns this sequence instance
	 * @param InContext  The (potentially dissected) context that this sequence instance is to be evaluated with
	 */
	void Update(UMovieSceneEntitySystemLinker* Linker, const FMovieSceneContext& InContext);


	/**
	 * Mark this instance as finished, causing all its entities to be unlinked and the instance to become inactive at the end of the next update.
	 *
	 * @param Linker     The linker that owns this sequence instance
	 */
	void Finish(UMovieSceneEntitySystemLinker* Linker);

	/**
	 * Called when this sequence instance has been evaluated in order for it to do any clean-up or other post-update work
	 *
	 * @param Linker     The linker that owns this sequence instance
	 */
	void PostEvaluation(UMovieSceneEntitySystemLinker* Linker);

	/**
	 * Called to run legacy track templates
	 */
	void RunLegacyTrackTemplates();

public:

	/**
	 * Retrieve the IMovieScenePlayer that is playing back the top level sequence for this instance
	 *
	 * @return A pointer to this instance's player
	 */
	IMovieScenePlayer* GetPlayer() const;

	/**
	 * Retrieve the SequenceID for this instance
	 *
	 * @return This sequence instance's SequenceID within the root-sequences hierachy, or MovieSceneSequenceID::Root for master sequence instances.
	 */
	FMovieSceneSequenceID GetSequenceID() const
	{
		return SequenceID;
	}

	/**
	 * Get the evaluation context for the current frame
	 *
	 * @return This sequence instance's playback context
	 */
	const FMovieSceneContext& GetContext() const
	{
		return Context;
	}

	/**
	 * Gets the handle to this instance
	 */
	FInstanceHandle GetInstanceHandle() const
	{
		return InstanceHandle;
	}

	/**
	 * Get a handle to the root instance for this sub sequence instance
	 *
	 * @return A handle to the root sequence instance for this sub sequence, or a handle to this sequence instance if it is not a sub sequence
	 */
	FRootInstanceHandle GetRootInstanceHandle() const
	{
		return RootInstanceHandle;
	}

	/**
	 * Returns whether this instance is the root instance.
	 */
	bool IsRootSequence() const
	{
		return RootInstanceHandle == InstanceHandle;
	}

	/**
	 * Returns whether this instance is a sub sequence.
	 */
	bool IsSubSequence() const
	{
		return RootInstanceHandle != InstanceHandle;
	}

	/**
	 * Get the serial number for this sequence instance that it was created with on construction.
	 *
	 * @return The serial number of this instance that is used to check that handles to it are still valid.
	 */
	uint16 GetSerialNumber() const
	{
		return InstanceHandle.InstanceSerial;
	}

	/**
	 * Check whether this sequence instance has finished evaluating
	 *
	 * @return True if this sequence has finished evaluation, false otherwise
	 */
	bool HasFinished() const
	{
		return bFinished;
	}

	/**
	 * Check whether this sequence instance has ever been updated or not
	 */
	bool HasEverUpdated() const
	{
		return bHasEverUpdated;
	}

	/**
	 * Attempt to locate a sub instance based on its sequence ID
	 */
	FInstanceHandle FindSubInstance(FMovieSceneSequenceID SequenceID) const;

	/**
	 * Attempt to locate an entity given its owner and ID
	 */
	FMovieSceneEntityID FindEntity(UObject* Owner, uint32 EntityID) const;

	/**
	 * Attempt to locate all entities given their owner
	 */
	void FindEntities(UObject* Owner, TArray<FMovieSceneEntityID>& OutEntityIDs) const;

	/**
	 * Retrieve the legacy evaluator for this sequence, if it is available (may return nullptr)
	 */
	const FMovieSceneTrackEvaluator* GetLegacyEvaluator() const
	{
		return LegacyEvaluator.Get();
	}

	/**
	 * Retrieve a path for this sequence instance back to the root
	 */
	FSubSequencePath GetSubSequencePath() const;

public:

	/**
	 * Get the evaluation context for the current frame
	 *
	 * @return This sequence instance's playback context
	 */
	void SetContext(const FMovieSceneContext& InContext)
	{
		Context = InContext;
	}

	/**
	 * Indicate that this sequence instance has finished evaluation and should remove its entities
	 */
	void SetFinished(bool bInFinished)
	{
		bFinished = bInFinished;
	}

	/**
	 * Invalidate any cached data that may be being used for evaluation due to a change in the source asset data
	 */
	void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker);

	/**
	 * Destroy this sequence instance immediately - Finish must previously have been called
	 */
	void DestroyImmediately(UMovieSceneEntitySystemLinker* Linker);

	/**
	 * Called to override the simulated root sequence ID for this instance. Only valid for IsRootSequence() instances.
	 */
	void OverrideRootSequence(UMovieSceneEntitySystemLinker* Linker, FMovieSceneSequenceID NewRootSequenceID);

public:

	/** Constructor for top level sequences */
	explicit FSequenceInstance(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* Player, FRootInstanceHandle ThisInstanceHandle);

	/** Constructor for sub sequences */
	explicit FSequenceInstance(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* Player, FInstanceHandle ThisInstanceHandle, FRootInstanceHandle RootInstanceHandle, FMovieSceneSequenceID InSequenceID, FMovieSceneCompiledDataID InCompiledDataID);

	/** Destructor */
	~FSequenceInstance();

	/** Sequence instances are not copyable */
	FSequenceInstance(const FSequenceInstance&) = delete;
	FSequenceInstance& operator=(const FSequenceInstance&) = delete;

	/** Move constructors implemented in cpp to avoid includes for TUniquePtr */
	FSequenceInstance(FSequenceInstance&&);
	FSequenceInstance& operator=(FSequenceInstance&&);

private:

	void InitializeLegacyEvaluator(UMovieSceneEntitySystemLinker* Linker);

private:

	FMovieSceneContext Context;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	/** Name of the root sequence */
	FString RootSequenceName;
#endif

	/** For top-level sequences only - legacy track template evaluator for the entire sequence */
	TUniquePtr<FMovieSceneTrackEvaluator> LegacyEvaluator;
	/** For top-level sequences only - interface for either a flat or hierarchical entity updater */
	TUniquePtr<ISequenceUpdater> SequenceUpdater;
	/** For top-level sequences only - a utility class that is used to ensure that volatile sequences are up to date. Only valid in editor, or for sequences that have the volatile flag. */
	TUniquePtr<FCompiledDataVolatilityManager> VolatilityManager;


	/** Delegate Binding for when an object binding is invalidated in this instance . */
	FDelegateHandle OnInvalidateObjectBindingHandle;
	/** This sequence's compiled data ID. */
	FMovieSceneCompiledDataID CompiledDataID;
	/** This sequence instances sequence ID, or MovieSceneSequenceID::Root for top-level sequences. */
	FMovieSceneSequenceID SequenceID;
	/** When SequenceID != MovieSceneSequenceID::Root, specifies an ID to override as a simulated root. */
	FMovieSceneSequenceID RootOverrideSequenceID;
	/** The index of this instance's IMovieScenePlayer retrievable through IMovieScenePlayer::Get(). */
	int32 PlayerIndex;
	/** This instance's handle. */
	FInstanceHandle InstanceHandle;
	/** This instance's root handle, if it is a sub sequence. */
	FRootInstanceHandle RootInstanceHandle;
	/** Flag that is set when this sequence has or (will be) finished. */
	bool bFinished : 1;
	/** Flag that is set if this sequence has ever updated. */
	bool bHasEverUpdated : 1;
};


} // namespace MovieScene
} // namespace UE
