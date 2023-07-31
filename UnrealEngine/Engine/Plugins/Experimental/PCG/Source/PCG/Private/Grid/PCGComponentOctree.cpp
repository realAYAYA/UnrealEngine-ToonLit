// Copyright Epic Games, Inc. All Rights Reserved.

#include "Grid/PCGComponentOctree.h"
#include "PCGComponent.h"

FPCGComponentRef::FPCGComponentRef(UPCGComponent* InComponent, const FPCGComponentOctreeIDSharedRef& InIdShared)
	: IdShared(InIdShared)
{
	check(InComponent);

	Component = InComponent;

	UpdateBounds();
}

void FPCGComponentRef::UpdateBounds()
{
	check(Component);

	Bounds = Component->GetGridBounds();
}
