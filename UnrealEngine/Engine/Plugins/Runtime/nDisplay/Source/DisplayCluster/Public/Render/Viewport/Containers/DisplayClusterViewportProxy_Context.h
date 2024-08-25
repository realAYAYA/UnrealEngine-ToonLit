// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* The viewport proxy context (rendering thread data)
*/
struct FDisplayClusterViewportProxy_Context
{
	// The number of context
	uint32  ContextNum = 0;

	// Profile description from ViewFamily
	FString ViewFamilyProfileDescription;
};
