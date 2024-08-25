// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Engine/OverlapInfo.h"

struct FHitResult;
enum class ETeleportType : uint8;

/**
 * Enum that controls the scoping behavior of FScopedMovementUpdate.
 * Note that EScopedUpdate::ImmediateUpdates is not allowed within outer scopes that defer updates,
 * and any attempt to do so will change the new inner scope to use deferred updates instead.
 */
namespace EScopedUpdate
{
	enum Type
	{
		ImmediateUpdates,
		DeferredUpdates	
	};
}

/**
 * FScopedMovementUpdate creates a new movement scope, within which propagation of moves may be deferred until the end of the outermost scope that does not defer updates.
 * Moves within this scope will avoid updates such as UpdateBounds(), OnUpdateTransform(), UpdatePhysicsVolume(), UpdateChildTransforms() etc
 * until the move is committed (which happens when the last deferred scope goes out of context).
 *
 * Note that non-deferred scopes are not allowed within outer scopes that defer updates, and any attempt to use one will change the inner scope to use deferred updates.
 */
class FScopedMovementUpdate : private FNoncopyable
{
public:
	typedef TArray<FHitResult, TInlineAllocator<2>> TScopedBlockingHitArray;
	typedef TArray<FOverlapInfo, TInlineAllocator<3>> TScopedOverlapInfoArray;

	ENGINE_API FScopedMovementUpdate( USceneComponent* Component, EScopedUpdate::Type ScopeBehavior = EScopedUpdate::DeferredUpdates, bool bRequireOverlapsEventFlagToQueueOverlaps = true );
	ENGINE_API ~FScopedMovementUpdate();

	enum class EHasMovedTransformOption
	{
		eTestTransform,
		eIgnoreTransform
	};

	enum class EOverlapState
	{
		eUseParent,
		eUnknown,
		eIncludesOverlaps,
		eForceUpdate,
	};

	/** Get the scope containing this scope. A scope only has an outer scope if they both defer updates. */
	ENGINE_API const FScopedMovementUpdate* GetOuterDeferredScope() const;

	/** Return true if deferring updates, false if updates are applied immediately. */
	ENGINE_API bool IsDeferringUpdates() const;
	
	/** Revert movement to the initial location of the Component at the start of the scoped update. Also clears pending overlaps and sets bHasMoved to false. */
	ENGINE_API void RevertMove();

	/** Returns whether movement has occurred at all during this scope, optionally checking if the transform is different (since changing scale does not go through a move). RevertMove() sets this back to false. */
	ENGINE_API bool HasMoved(EHasMovedTransformOption CheckTransform) const;

	/** Returns true if the Component's transform differs from that at the start of the scoped update. */
	ENGINE_API bool IsTransformDirty() const;

	/** Returns true if there are pending overlaps queued in this scope. */
	ENGINE_API bool HasPendingOverlaps() const;

	/**
	* Returns true if we require GetGenerateOverlapEvents() on both the moving object and the overlapped object to add them to the pending overlaps list.
	* These flags will still be required when dispatching calls to UpdateOverlaps(), but this allows some custom processing of queued overlaps that would be otherwise missed along the way.
	*/
	ENGINE_API bool RequiresOverlapsEventFlag() const;

	/** Returns the pending overlaps within this scope. */
	ENGINE_API const TScopedOverlapInfoArray& GetPendingOverlaps() const;

	/** Returns the list of pending blocking hits, which will be used for notifications once the move is committed. */
	ENGINE_API const TScopedBlockingHitArray& GetPendingBlockingHits() const;

	//--------------------------------------------------------------------------------------------------------//
	// These methods are intended only to be used by SceneComponent and derived classes.

	/** Add overlaps to the queued overlaps array. This is intended for use only by SceneComponent and its derived classes whenever movement is performed. */
	ENGINE_API void AppendOverlapsAfterMove(const TOverlapArrayView& NewPendingOverlaps, bool bSweep, bool bIncludesOverlapsAtEnd);

	/** Keep current pending overlaps after a move but make note that there was movement (just a symmetric rotation). */
	ENGINE_API void KeepCurrentOverlapsAfterRotation(bool bSweep);

	/** Add blocking hit that will get processed once the move is committed. This is intended for use only by SceneComponent and its derived classes. */
	ENGINE_API void AppendBlockingHitAfterMove(const FHitResult& Hit);

	/** Clear overlap state at current location, we don't know what it is. */
	ENGINE_API void InvalidateCurrentOverlaps();

	/** Force full overlap update once this scope finishes. */
	ENGINE_API void ForceOverlapUpdate();

	/** Registers that this move is a teleport */
	ENGINE_API void SetHasTeleported(ETeleportType InTeleportType);

protected:
	/** Fills in the list of overlaps at the end location (in EndOverlaps). Returns pointer to the list, or null if it can't be computed. */
	ENGINE_API TOptional<TOverlapArrayView> GetOverlapsAtEnd(class UPrimitiveComponent& PrimComponent, TInlineOverlapInfoArray& OutEndOverlaps, bool bTransformChanged) const;

	ENGINE_API bool SetWorldLocationAndRotation(FVector NewLocation, const FQuat& NewQuat, bool bNoPhysics, ETeleportType Teleport);

private:
	/** Notify this scope that the given inner scope completed its update (ie is going out of scope). Only occurs for deferred updates. */
	ENGINE_API void OnInnerScopeComplete(const FScopedMovementUpdate& InnerScope);
	
	// This class can only be created on the stack, otherwise the ordering constraints
	// of the constructor and destructor between encapsulated scopes could be violated.
	ENGINE_API void*	operator new		(size_t);
	void*	operator new[]		(size_t);
	ENGINE_API void	operator delete		(void *);
	void	operator delete[]	(void*);

protected:
	USceneComponent* Owner;
	FScopedMovementUpdate* OuterDeferredScope;

	EOverlapState CurrentOverlapState;
	ETeleportType TeleportType;

	FTransform InitialTransform;
	FVector InitialRelativeLocation;
	FRotator InitialRelativeRotation;
	FVector InitialRelativeScale;

	int32 FinalOverlapCandidatesIndex;		// If not INDEX_NONE, overlaps at this index and beyond in PendingOverlaps are at the final destination
	TScopedOverlapInfoArray PendingOverlaps;	// All overlaps encountered during the scope of moves.
	TScopedBlockingHitArray BlockingHits;		// All blocking hits encountered during the scope of moves.

	uint8 bDeferUpdates:1;
	uint8 bHasMoved:1;
	uint8 bRequireOverlapsEventFlag:1;

	friend class USceneComponent;
};

//////////////////////////////////////////////////////////////////////////
// FScopedMovementUpdate inlines

FORCEINLINE const FScopedMovementUpdate* FScopedMovementUpdate::GetOuterDeferredScope() const
{
	return OuterDeferredScope;
}

FORCEINLINE bool FScopedMovementUpdate::IsDeferringUpdates() const
{
	return bDeferUpdates;
}

FORCEINLINE bool FScopedMovementUpdate::HasMoved(EHasMovedTransformOption CheckTransform) const
{
	return bHasMoved || (CheckTransform == EHasMovedTransformOption::eTestTransform && IsTransformDirty());
}

FORCEINLINE bool FScopedMovementUpdate::HasPendingOverlaps() const
{
	return PendingOverlaps.Num() > 0;
}

FORCEINLINE bool FScopedMovementUpdate::RequiresOverlapsEventFlag() const
{
	return bRequireOverlapsEventFlag;
}

FORCEINLINE const FScopedMovementUpdate::TScopedOverlapInfoArray& FScopedMovementUpdate::GetPendingOverlaps() const
{
	return PendingOverlaps;
}

FORCEINLINE const FScopedMovementUpdate::TScopedBlockingHitArray& FScopedMovementUpdate::GetPendingBlockingHits() const
{
	return BlockingHits;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::AppendBlockingHitAfterMove(const FHitResult& Hit)
{
	BlockingHits.Add(Hit);
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::KeepCurrentOverlapsAfterRotation(bool bSweep)
{
	bHasMoved = true;
	// CurrentOverlapState is unchanged
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::InvalidateCurrentOverlaps()
{
	bHasMoved = true;
	CurrentOverlapState = EOverlapState::eUnknown;
	FinalOverlapCandidatesIndex = INDEX_NONE;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::ForceOverlapUpdate()
{
	bHasMoved = true;
	CurrentOverlapState = EOverlapState::eForceUpdate;
	FinalOverlapCandidatesIndex = INDEX_NONE;
}

FORCEINLINE_DEBUGGABLE void FScopedMovementUpdate::SetHasTeleported(ETeleportType InTeleportType)
{
	// Request an initialization. Teleport type can only go higher - i.e. if we have requested a reset, then a teleport will still reset fully
	TeleportType = ((InTeleportType > TeleportType) ? InTeleportType : TeleportType); 
}
