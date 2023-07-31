// Copyright Epic Games, Inc. All Rights Reserved.

#include "Builtin/PropertySelector/PropertySelectorFilter.h"

EFilterResult::Type UPropertySelectorFilter::IsActorValid(const FIsActorValidParams& Params) const
{
	return DefaultResult;
}

EFilterResult::Type UPropertySelectorFilter::IsDeletedActorValid(const FIsDeletedActorValidParams& Params) const
{
	return DefaultResult;
}

EFilterResult::Type UPropertySelectorFilter::IsAddedActorValid(const FIsAddedActorValidParams& Params) const
{
	return DefaultResult;
}
