// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UObject/NameTypes.h"

extern const FName FoliageEditAppIdentifier;

class ULevel;

/**
 * Foliage Edit mode module interface
 */
class IFoliageEditModule : public IModuleInterface
{
public:

#if WITH_EDITOR
	/** Move the selected foliage to the specified level */
	virtual void MoveSelectedFoliageToLevel(ULevel* InTargetLevel) = 0;
	/** Notifies us that the foliage has been externally changed and needs refreshing.  */
	virtual void UpdateMeshList() = 0;
	virtual bool CanMoveSelectedFoliageToLevel(ULevel* InTargetLevel) const = 0;
#endif
};
