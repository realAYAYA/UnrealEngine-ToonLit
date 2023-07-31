// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "IMeshMergeUtilities.h"

class MESHMERGEUTILITIES_API IMeshMergeModule : public IModuleInterface
{
public:
	virtual const IMeshMergeUtilities& GetUtilities() const = 0;
	virtual IMeshMergeUtilities& GetUtilities() = 0;
};
