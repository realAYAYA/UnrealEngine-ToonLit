// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "Subsystems/WorldSubsystem.h"
#include "MassSignalSubsystem.generated.h"

namespace UE::MassSignal 
{
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSignalDelegate, FName /*SignalName*/, TConstArrayView<FMassEntityHandle> /*Entities*/);
} // UE::MassSignal

/**
* A subsystem for handling Signals in Mass
*/
UCLASS()
class MASSSIGNALS_API UMassSignalSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:

	/** 
	 * Retrieve the delegate dispatcher from the signal name
	 * @param SignalName is the name of the signal to get the delegate dispatcher from
	 */
	UE::MassSignal::FSignalDelegate& GetSignalDelegateByName(FName SignalName)
	{
		return NamedSignals.FindOrAdd(SignalName);
	}

	/**
	 * Inform a single entity of a signal being raised
	 * @param SignalName is the name of the signal raised
	 * @param Entity entity that should be informed that signal 'SignalName' was raised
	 */
	void SignalEntity(FName SignalName, const FMassEntityHandle Entity);

	/**
	 * Inform multiple entities of a signal being raised
	 * @param SignalName is the name of the signal raised
	 * @param Entities list of entities that should be informed that signal 'SignalName' was raised
	 */
	void SignalEntities(FName SignalName, TConstArrayView<FMassEntityHandle> Entities);

	/**
	 * Inform a single entity of a signal being raised in a certain amount of seconds
	 * @param SignalName is the name of the signal raised
	 * @param Entity entity that should be informed that signal 'SignalName' was raised
	 * @param DelayInSeconds is the amount of time before signaling the entity
	 */
	void DelaySignalEntity(FName SignalName, const FMassEntityHandle Entity, const float DelayInSeconds);

 	/**
	 * Inform multiple entities of a signal being raised in a certain amount of seconds
	 * @param SignalName is the name of the signal raised
	 * @param Entities being informed of the raised signal
	 * @param DelayInSeconds is the amount of time before signaling the entities
	 */
	void DelaySignalEntities(FName SignalName, TConstArrayView<FMassEntityHandle> Entities, const float DelayInSeconds);

	/**
	 * Inform single entity of a signal being raised asynchronously using the Mass Command Buffer
	 * @param Context is the Entity System execution context to push the async command
	 * @param SignalName is the name of the signal raised
	 * @param Entity entity that should be informed that signal 'SignalName' was raised
	 */
	void SignalEntityDeferred(FMassExecutionContext& Context, FName SignalName, const FMassEntityHandle Entity);

	/**
	 * Inform multiple entities of a signal being raised asynchronously using the Mass Command Buffer
	 * @param Context is the Entity System execution context to push the async command
	 * @param SignalName is the name of the signal raised
	 * @param Entities list of entities that should be informed that signal 'SignalName' was raised
	 */
	void SignalEntitiesDeferred(FMassExecutionContext& Context, FName SignalName, TConstArrayView<FMassEntityHandle> Entities);

 	/**
	 * Inform single entity of a signal being raised asynchronously using the Mass Command Buffer
	 * @param Context is the Entity System execution context to push the async command
	 * @param SignalName is the name of the signal raised
	 * @param Entity entity that should be informed that signal 'SignalName' was raised
	 * @param DelayInSeconds is the amount of time before signaling the entities
	 */
	void DelaySignalEntityDeferred(FMassExecutionContext& Context, FName SignalName, const FMassEntityHandle Entity, const float DelayInSeconds);

 	/**
	 * Inform multiple entities of a signal being raised asynchronously using the Mass Command Buffer
	 * @param Context is the Entity System execution context to push the async command
	 * @param SignalName is the name of the signal raised
	 * @param Entities being informed of that signal was raised
	 * @param DelayInSeconds is the amount of time before signaling the entities
	 */
	void DelaySignalEntitiesDeferred(FMassExecutionContext& Context, FName SignalName, TConstArrayView<FMassEntityHandle> Entities, const float DelayInSeconds);

protected:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	TMap<FName, UE::MassSignal::FSignalDelegate> NamedSignals;

	struct FDelayedSignal
	{
		FName SignalName;
		TArray<FMassEntityHandle> Entities;
		float DelayInSeconds;
	};

	TArray<FDelayedSignal> DelayedSignals;
};

template<>
struct TMassExternalSubsystemTraits<UMassSignalSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
