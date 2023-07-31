// Copyright Epic Games, Inc. All Rights Reserved.

#include "Templates/RefCounting.h"

void 
FRefCountBase::CheckRefCount() const
{
	// Verify that Release() is not being called on an object which is already at a zero refcount
	check(NumRefs != 0);
}
