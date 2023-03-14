// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrimitiveStats.h"

#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"

UPrimitiveStats::UPrimitiveStats(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPrimitiveStats::UpdateNames()
{
	if( Object.IsValid() )
	{
		Type = Object.Get()->GetClass()->GetName();
	}
}
