// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FWebMFrame
{
	TArray<uint8> Data;
	FTimespan Time;
	FTimespan Duration;
};
