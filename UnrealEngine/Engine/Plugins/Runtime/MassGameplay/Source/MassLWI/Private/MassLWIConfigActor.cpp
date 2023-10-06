// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLWIConfigActor.h"
#include "MassLWISubsystem.h"

//-----------------------------------------------------------------------------
// ALWIxMassConfigActor
//-----------------------------------------------------------------------------
void AMassLWIConfigActor::PostLoad()
{
	if (UMassLWISubsystem* MassLWI = UWorld::GetSubsystem<UMassLWISubsystem>(GetWorld()))
	{
		MassLWI->RegisterConfigActor(*this);
	}
	Super::PostLoad();
}

