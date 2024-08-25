// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

#include "UsdWrappers/ForwardDeclarations.h"

class AUsdStageActor;
class FScopedBlockMonitoringChangesForTransaction;
class FUsdInfoCache;
class FUsdLevelSequenceHelperImpl;
class ULevelSequence;
class UUsdPrimTwin;
enum class EUsdRootMotionHandling : uint8;

namespace UE
{
	class FUsdGeomBBoxCache;
}

/**
 * Builds and maintains the level sequence and subsequences for a Usd Stage
 */
class USDSTAGE_API FUsdLevelSequenceHelper
{
public:
	FUsdLevelSequenceHelper();
	virtual ~FUsdLevelSequenceHelper();

	// Copy semantics are there for convenience only. Copied FUsdLevelSequenceHelper are empty and require a call to Init().
	FUsdLevelSequenceHelper(const FUsdLevelSequenceHelper& Other);
	FUsdLevelSequenceHelper& operator=(const FUsdLevelSequenceHelper& Other);

	FUsdLevelSequenceHelper(FUsdLevelSequenceHelper&& Other);
	FUsdLevelSequenceHelper& operator=(FUsdLevelSequenceHelper&& Other);

public:
	/** Creates the main level sequence and subsequences from the usd stage layers */
	ULevelSequence* Init(const UE::FUsdStage& UsdStage);

	/** Sets the asset cache to use when fetching assets and asset info required for the level sequence animation, like UAnimSequences */
	void SetInfoCache(TSharedPtr<FUsdInfoCache> InInfoCache);

	/** Sets the BBoxCache to use when importing bound animations for prims. Needed for importing, where we don't have a stage actor to take the
	 * BBoxCache from */
	void SetBBoxCache(TSharedPtr<UE::FUsdGeomBBoxCache> InBBoxCache);

	/* Returns true if we have at least one possessable or a reference to a subsequence */
	bool HasData() const;

	/** Call this whenever the stage actor is renamed, to replace the possessable binding with a new one */
	void OnStageActorRenamed();

	/** Resets the helper, abandoning all managed LevelSequences */
	void Clear();

	/** Creates the time track for the StageActor. This will also set the root handling mode to the provided actor's, if any */
	void BindToUsdStageActor(AUsdStageActor* StageActor);
	void UnbindFromUsdStageActor();

	/**
	 * Gets the current root motion handling mode.
	 * We use this to prevent animation that has already been baked into AnimSequences as root joint motion from also
	 * being parsed as LevelSequence tracks.
	 */
	EUsdRootMotionHandling GetRootMotionHandling() const;

	/**
	 * Sets the current root motion handling mode.
	 */
	void SetRootMotionHandling(EUsdRootMotionHandling NewValue);

	/**
	 * Adds the necessary tracks for a given prim to the level sequence.
	 * If bForceVisibilityTracks is true, will add visibility tracks even if this prim
	 * doesn't actually have timeSamples on its visibility attribute (use this when
	 * a parent does have animated visibility, and we need to "bake" that out to a dedicated
	 * visibility track so that the standalone LevelSequence asset behaves as expected).
	 * We'll check the prim for animated bounds and add bounds tracks, unless bHasAnimatedBounds
	 * already provides whether the prim has animated bounds or not.
	 */
	void AddPrim(UUsdPrimTwin& PrimTwin, bool bForceVisibilityTracks = false, TOptional<bool> HasAnimatedBounds = {});

	/** Removes any track associated with this prim */
	void RemovePrim(const UUsdPrimTwin& PrimTwin);

	void UpdateControlRigTracks(UUsdPrimTwin& PrimTwin);

	/** Blocks updating the level sequences & tracks from object changes. */
	void StartMonitoringChanges();
	void StopMonitoringChanges();
	void BlockMonitoringChangesForThisTransaction();

	ULevelSequence* GetMainLevelSequence() const;
	TArray<ULevelSequence*> GetSubSequences() const;

	DECLARE_EVENT_OneParam(FUsdLevelSequenceHelper, FOnSkelAnimationBaked, const FString& /*SkeletonPrimPath*/);
	FOnSkelAnimationBaked& GetOnSkelAnimationBaked();

private:
	friend class FScopedBlockMonitoringChangesForTransaction;
	TUniquePtr<FUsdLevelSequenceHelperImpl> UsdSequencerImpl;
};

class USDSTAGE_API FScopedBlockMonitoringChangesForTransaction final
{
public:
	explicit FScopedBlockMonitoringChangesForTransaction(FUsdLevelSequenceHelper& InHelper);
	explicit FScopedBlockMonitoringChangesForTransaction(FUsdLevelSequenceHelperImpl& InHelperImpl);
	~FScopedBlockMonitoringChangesForTransaction();

	FScopedBlockMonitoringChangesForTransaction() = delete;
	FScopedBlockMonitoringChangesForTransaction(const FScopedBlockMonitoringChangesForTransaction&) = delete;
	FScopedBlockMonitoringChangesForTransaction(FScopedBlockMonitoringChangesForTransaction&&) = delete;
	FScopedBlockMonitoringChangesForTransaction& operator=(const FScopedBlockMonitoringChangesForTransaction&) = delete;
	FScopedBlockMonitoringChangesForTransaction& operator=(FScopedBlockMonitoringChangesForTransaction&&) = delete;

private:
	FUsdLevelSequenceHelperImpl& HelperImpl;
	bool bStoppedMonitoringChanges = false;
};
