// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/Misc/GuidReferences.h"

FGuidReferences::~FGuidReferences()
{
	if (Array != NULL)
	{
		delete Array;
		Array = NULL;
	}
}