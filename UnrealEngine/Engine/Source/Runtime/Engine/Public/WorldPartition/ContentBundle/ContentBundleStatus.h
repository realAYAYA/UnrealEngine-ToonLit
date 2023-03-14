// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "ContentBundleStatus.generated.h"

UENUM()
enum class EContentBundleStatus
{
	Registered,
	ReadyToInject,
	FailedToInject,
	ContentInjected,
	Unknown = -1
};