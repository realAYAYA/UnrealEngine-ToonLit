// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseSubobjectDataFactory.h"
#include "SubobjectData.h"

TSharedPtr<FSubobjectData> FBaseSubobjectDataFactory::CreateSubobjectData(const FCreateSubobjectParams& Params)
{
	return TSharedPtr<FSubobjectData>(new FSubobjectData(Params.Context, Params.ParentHandle));
}

bool FBaseSubobjectDataFactory::ShouldCreateSubobjectData(const FCreateSubobjectParams& Params) const
{
	// By default, this will just return false to ensure that if anyone
	// adds a custom subobject data factory that they won't have to change this function
	return false;
}