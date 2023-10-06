// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/World.h"
#include "Subsystems/WorldSubsystem.h"

/**
 * Traits describing how a given piece of code can be used by Mass. We require author or user of a given subsystem to 
 * define its traits. To do it add the following in an accessible location. 
 *
 * template<>
 * struct TMassExternalSubsystemTraits<UMyCustomManager>
 * {
 *		enum { GameThreadOnly = false; }
 * }
 *
 * this will let Mass know it can access UMyCustomManager on any thread.
 *
 * This information is being used to calculate processor and query dependencies as well as appropriate distribution of
 * calculations across threads.
 */
template <typename T>
struct TMassExternalSubsystemTraits final
{
	enum
	{
		// Note that we're not supplying a default value for this property to be able to statically catch code that tries
		// to access given subsystem without including the appropriate headers. See the comment above if you want to use
		// a UWorldSubsystem that has not had its traits defined before. 
		// GameThreadOnly = true,
		ThreadSafeRead = false,
		ThreadSafeWrite = false,
	};
};

namespace FMassExternalSubsystemTraits
{
	/**
	 * Every TMassExternalSubsystemTraits specialization needs to implement the following. Not supplying default implementations
	 * to be able to catch missing implementations and header inclusion at compilation time.
	 * 
	 * This is a getter function that given a UWorld* fetches an instance.
	*/
	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, UWorldSubsystem>::IsDerived>::Type>
	UE_DEPRECATED(5.2, "FMassExternalSubsystemTraits::GetInstance has been marked as deprecated. Use FMassSubsystemAccess::FetchSubsystemInstance instead.")
	FORCEINLINE T* GetInstance(const UWorld* World)
	{ 
		// note that the default implementation works only for UWorldSubsystems
		return UWorld::GetSubsystem<T>(World);
	}

	template<typename T, typename = typename TEnableIf<TIsDerivedFrom<T, UWorldSubsystem>::IsDerived>::Type>
	UE_DEPRECATED(5.2, "FMassExternalSubsystemTraits::GetInstance has been marked as deprecated. Use FMassSubsystemAccess::FetchSubsystemInstance instead.")
	FORCEINLINE T* GetInstance(const UWorld* World, const TSubclassOf<UWorldSubsystem> SubsystemClass)
	{
		return World ? Cast<T>(World->GetSubsystemBase(SubsystemClass)) : (T*)nullptr;
	}
}

/** 
 * Shared Fragments' traits.
 * @see TMassExternalSubsystemTraits
 */
template <typename T>
struct TMassSharedFragmentTraits final
{
	enum
	{
		GameThreadOnly = false,
	};
};
