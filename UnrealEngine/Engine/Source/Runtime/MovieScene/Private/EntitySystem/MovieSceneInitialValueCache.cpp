// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneInitialValueCache.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

namespace UE
{
namespace MovieScene
{


TEntitySystemLinkerExtensionID<FInitialValueCache> FInitialValueCache::GetExtensionID()
{
	static TEntitySystemLinkerExtensionID<FInitialValueCache> ID = UMovieSceneEntitySystemLinker::RegisterExtension<FInitialValueCache>();
	return ID;
}

TSharedPtr<FInitialValueCache> FInitialValueCache::GetGlobalInitialValues()
{
	static TWeakPtr<FInitialValueCache> GSharedInitialValues = MakeShared<FInitialValueCache>();

	TSharedPtr<FInitialValueCache> Ptr = GSharedInitialValues.Pin();
	if (!Ptr)
	{
		Ptr = MakeShared<FInitialValueCache>();
		GSharedInitialValues = Ptr;
	}
	return Ptr;
}

} // namespace MovieScene
} // namespace UE

