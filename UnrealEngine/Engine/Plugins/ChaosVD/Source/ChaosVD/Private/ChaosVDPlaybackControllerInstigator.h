// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/Guid.h"

/** Common interface for any object that alters a ChaosVDPlaybackController */
class IChaosVDPlaybackControllerInstigator
{
public:
	IChaosVDPlaybackControllerInstigator() : ID(FGuid::NewGuid())
	{
	}

	static FGuid InvalidGuid;

	const FGuid& GetInstigatorID() const { return ID;};
private:
	FGuid ID;
};
