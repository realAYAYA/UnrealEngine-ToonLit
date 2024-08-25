// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"

struct FSoftObjectPath;

namespace UE::ConcertSharedSlate::ObjectUtils
{
	/** @return Checks whether the given object is an actor. */
	CONCERTSHAREDSLATE_API bool IsActor(const FSoftObjectPath& Object);
	
	/** @return Get the owning actor of Subobject. If Subobject is an actor, then this returns unset. */
	CONCERTSHAREDSLATE_API TOptional<FSoftObjectPath> GetActorOf(const FSoftObjectPath& Subobject);
	
	/** @return Gets the last object name in the subpath. */
	CONCERTSHAREDSLATE_API FString ExtractObjectDisplayStringFromPath(const FSoftObjectPath& Object);
};
