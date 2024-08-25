// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamTypeHandle.h"

namespace UE::AnimNext
{

struct FSchedulePortAdapterContext
{
	// TODO: templated type-checked data access for data/objects

	TConstArrayView<uint8> InputData;
	TArrayView<uint8> OutputData;
	UObject* InputObject = nullptr;
	UObject* OutputObject = nullptr;
	FParamTypeHandle InputType;
	FParamTypeHandle OutputType;
};

}