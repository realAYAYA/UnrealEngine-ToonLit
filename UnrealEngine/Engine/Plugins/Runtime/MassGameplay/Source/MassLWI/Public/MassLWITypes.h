// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "MassEntityTypes.h"

#include "MassLWITypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMassLWI, Log, All)

class AMassLWIStaticMeshManager;

USTRUCT()
struct FMassLWIManagerSharedFragment : public FMassSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TSoftObjectPtr<AMassLWIStaticMeshManager> LWIManager = nullptr;
};
