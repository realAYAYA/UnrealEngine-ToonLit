// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Engine/EngineTypes.h"

#include "LightWeightInstanceBlueprintFunctionLibrary.generated.h"

namespace LWIUtils
{
	/**
	 * Returns true if the object this handle represents supports the interface of type U
	 */
	template<typename U>
	static ENGINE_API bool DoesHandleSupportInterface(const FActorInstanceHandle& Handle);

	/**
	 * Returns an object implementing the interface I.
	 */
	template<typename I>
	static ENGINE_API I* FetchInterfaceFromHandle(const FActorInstanceHandle& Handle);


	/**
	 * Implementations of inline or template functions go below here
	 */

	template<typename U>
	static bool DoesHandleSupportInterface(const FActorInstanceHandle& Handle)
	{
		// if we have a valid actor, see if it supports the interface
		if (const UObject* Obj = Handle.GetActorAsUObject())
		{
			return Obj->GetClass()->ImplementsInterface(U);
		}

		// no valid actor, ask the instance manager instead
		return FLightWeightInstanceSubsystem::Get().IsInterfaceSupported<U>(Handle);
	}

	template<typename I>
	static I* FetchInterfaceFromHandle(const FActorInstanceHandle& Handle)
	{
		if (Handle.IsActorValid())
		{
			return Cast<I>(Actor.Get());
		}

		return FLightWeightInstanceSubsystem::Get().FetchInterfaceObject<I>(Handle);
	}
};