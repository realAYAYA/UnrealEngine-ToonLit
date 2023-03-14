// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Interface.h"
#include "LODSyncInterface.generated.h"

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class ENGINE_API ULODSyncInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};


/* This is interface class for getting/setting LOD info by LODSyncComponent
 *
 * Implemented by SkeletalMeshComponent, GroomComponent
 */

class ENGINE_API ILODSyncInterface
{
	GENERATED_IINTERFACE_BODY()

	/** returns what is your desired LOD by 0 based index */
	virtual int32 GetDesiredSyncLOD() const = 0;

	/** sets what is synced LOD by 0 based index */
	virtual void SetSyncLOD(int32 LODIndex) = 0;

	/** returns number of LODs */
	virtual int32 GetNumSyncLODs() const = 0;

	/** Returns what the current sync LOD has bee set to */
	virtual int32 GetCurrentSyncLOD() const = 0;
};
