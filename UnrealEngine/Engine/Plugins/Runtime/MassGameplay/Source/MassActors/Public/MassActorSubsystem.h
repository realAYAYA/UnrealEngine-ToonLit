// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplate.h"
#include "Misc/MTAccessDetector.h"
#include "UObject/ObjectKey.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassActorSubsystem.generated.h"

struct FMassEntityHandle;
class AActor;
struct FMassEntityManager;

/**
 * Fragment to save the actor pointer of a mass entity if it exist
 */
USTRUCT()
struct MASSACTORS_API FMassActorFragment : public FObjectWrapperFragment
{
	GENERATED_BODY()

	/**
	 * Set the actor associated to a mass agent, will also keep the map back in MassActorSubsystem up to date.
	 * @param MassAgent to associated with the actor
	 * @param InActor the actor associated with the mass agent
	 * @param bInIsOwnedByMass tell whether the actors was spawned by mass(MassVisualization) or externally(ReplicatedActors)
	 */
	void SetAndUpdateHandleMap(const FMassEntityHandle MassAgent, AActor* InActor, const bool bInIsOwnedByMass);

	/** Resets the actor pointed by this fragment, will also keep the map back in MassActorSubsystem up to date */
	void ResetAndUpdateHandleMap();

	/**
	 * Set the actor associated to a mass agent, will NOT keep map back in MassActorSubsystem up to date.
	 * The user needs to do the map update by hand.
	 * Useful in multithreaded environment, to queue the update of the map inside a deferred command
	 * @param MassAgent to associated with the actor
	 * @param InActor the actor associated with the mass agent
	 * @param bInIsOwnedByMass tell whether the actors was spawned by mass(MassVisualization) or externally(ReplicatedActors)
	 */
	void SetNoHandleMapUpdate(const FMassEntityHandle MassAgent, AActor* InActor, const bool bInIsOwnedByMass);

	/** Resets the actor pointed by this fragment, will NOT keep map back in MassActorSubsystem up to date.
	 * The user needs to do the map update by hand.
	 * Useful in multithreaded environment, to queue the update of the map inside a deferred command
	 */
	void ResetNoHandleMapUpdate();


	/** @return none const pointer to the actor	*/
	FORCEINLINE AActor* GetMutable() { return Actor.Get(); }

	/** @return none const pointer to the actor	only if owned by mass */
	FORCEINLINE AActor* GetOwnedByMassMutable() { return bIsOwnedByMass ? Actor.Get() : nullptr; }

	/** @return none const pointer to the actor	only if owned by mass */
	FORCEINLINE const AActor* Get() const { return Actor.Get(); }

	/** @return if the actor is owned by mass */
	FORCEINLINE bool IsOwnedByMass() const { return bIsOwnedByMass; }

	/** @return if the actor is a valid pointer */
	FORCEINLINE bool IsValid() const { return Actor.IsValid(); }

private:
	TWeakObjectPtr<AActor> Actor;

	/** Ownership of the actor */
	bool bIsOwnedByMass = false;
};

/**
 * A subsystem managing communication between Actors and Mass
 */
UCLASS()
class MASSACTORS_API UMassActorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Get mass handle from an actor */
	FMassEntityHandle GetEntityHandleFromActor(const TObjectKey<const AActor> Actor);

	/** Set the mass handle associated to an actor */
	void SetHandleForActor(const TObjectKey<const AActor> Actor, const FMassEntityHandle Handle);

	/** Remove entry associated to an actor */
	void RemoveHandleForActor(const TObjectKey<const AActor> Actor);

	/** Get an actor pointer from a mass handle */
	AActor* GetActorFromHandle(const FMassEntityHandle Handle) const;

	/** 
	 *  Removes the connection between Actor and the given entity. Does all the required book keeping 
	 *  (as opposed to straight up RemoveHandleForActor call). If the Handle doesn't match Actor no action is taken.
	 */
	void DisconnectActor(const TObjectKey<const AActor> Actor, const FMassEntityHandle Handle);

protected:
	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// USubsystem END
	
	TMap<TObjectKey<const AActor>, FMassEntityHandle> ActorHandleMap;
	UE_MT_DECLARE_RW_ACCESS_DETECTOR(ActorHandleMapDetector);
	
	TSharedPtr<FMassEntityManager> EntityManager;
};

template<>
struct TMassExternalSubsystemTraits<UMassActorSubsystem> final
{
	enum
	{
		GameThreadOnly = false
	};
};
