// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builtin/ActorSelector/ActorSelectorFilter.h"

EFilterResult::Type UActorSelectorFilter::IsPropertyValid(const FIsPropertyValidParams& Params) const
{
	return DefaultResult;
}

EFilterResult::Type UActorSelectorFilter::IsDeletedActorValid(const FIsDeletedActorValidParams& Params) const
{
	return DefaultResult;
}

EFilterResult::Type UActorSelectorFilter::IsAddedActorValid(const FIsAddedActorValidParams& Params) const
{
	return DefaultResult;
}
