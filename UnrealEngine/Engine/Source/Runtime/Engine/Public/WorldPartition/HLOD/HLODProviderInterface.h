// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
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
	DECLARE_DELEGATE_RetVal_OneParam(bool, FPackageModifiedDelegate, UPackage*)

	struct FBuildHLODActorParams
	{
		bool bForceRebuild;
		FPackageModifiedDelegate OnPackageModified;
	};

	virtual bool BuildHLODActor(const FBuildHLODActorParams& BuildHLODActorParams) = 0;
};
