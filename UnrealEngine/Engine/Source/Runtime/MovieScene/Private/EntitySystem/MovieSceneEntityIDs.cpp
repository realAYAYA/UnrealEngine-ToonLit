// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityManager.h"

namespace UE
{
namespace MovieScene
{

bool FEntityHandle::IsValid(const FEntityManager& InEntityManager) const
{
	return InEntityManager.IsHandleValid(*this);
}

} // namespace MovieScene
} // namespace UE
