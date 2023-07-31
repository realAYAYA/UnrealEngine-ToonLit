// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FConcertSessionActivity;

namespace UE::ConcertSharedSlate::Private
{
	FText GetOperationName(const FConcertSessionActivity& Activity);
	FText GetPackageName(const FConcertSessionActivity& Activity);
}
