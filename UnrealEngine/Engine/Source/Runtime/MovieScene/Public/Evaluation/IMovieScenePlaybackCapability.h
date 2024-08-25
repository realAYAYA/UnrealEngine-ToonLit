// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/SharedPointer.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"

class IMovieScenePlayer;
class UMovieSceneEntitySystemLinker;

namespace UE::MovieScene
{

struct FInstanceHandle;
struct FSharedPlaybackState;

/**
 * An identifier for a playback capability.
 */
struct FPlaybackCapabilityID
{
	int32 Index = INDEX_NONE;

	bool IsValid() const
	{
		return Index != INDEX_NONE;
	}

protected:
	static MOVIESCENE_API FPlaybackCapabilityID Register();
};

/**
 * A strongly-typed identifier for a specific playback capability class.
 *
 * The base capablity class must create a static ID member that returns its own typed ID. We will use
 * this as a convention to quickly get the ID of a base capability class in the capabilities container.
 */
template<typename T>
struct TPlaybackCapabilityID : FPlaybackCapabilityID
{
public:
	using CapabilityType = T;

private:
	// Only T should construct this (to ensure safe construction over DLL boundaries)
	friend T;

	TPlaybackCapabilityID()
	{}

	explicit TPlaybackCapabilityID(int32 InIndex)
		: FPlaybackCapabilityID{ InIndex }
	{}

	static TPlaybackCapabilityID<T> Register()
	{
		FPlaybackCapabilityID StaticID = FPlaybackCapabilityID::Register();
		return TPlaybackCapabilityID<T>(StaticID.Index);
	}
};

struct IPlaybackCapability
{
	virtual ~IPlaybackCapability() {}

	/** Called after this capability has been added to a shared playback state */
	virtual void Initialize(TSharedRef<const FSharedPlaybackState> Owner) {}
	/** Called when a new sequence instance has been created and added to the sequence hierarchy */
	virtual void OnSubInstanceCreated(TSharedRef<const FSharedPlaybackState> Owner, const FInstanceHandle InstanceHandle) {}
	/** Called when the root sequence is cleaning cached data */
	virtual void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker) {}
};

} // namespace UE::MovieScene
