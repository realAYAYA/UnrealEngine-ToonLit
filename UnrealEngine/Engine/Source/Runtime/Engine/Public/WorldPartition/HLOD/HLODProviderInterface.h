// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HLODProviderInterface.generated.h"


class AWorldPartitionHLOD;


UINTERFACE(MinimalAPI)
class UWorldPartitionHLODProvider : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};


class IWorldPartitionHLODProvider
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual AWorldPartitionHLOD* CreateHLODActor() = 0;
};
