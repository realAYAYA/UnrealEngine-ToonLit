// Copyright Epic Games, Inc. All Rights Reserved.

#include "Context.h"
#include "Param/ParamType.h"
#include "Param/ParamStack.h"

namespace UE::AnimNext
{

FContext::FContext()
	: Stack(FParamStack::Get())
	, DeltaTime(0.0f)
{
}

FContext::FContext(float InDeltaTime)
	: Stack(FParamStack::Get())
	, DeltaTime(InDeltaTime)
{
}

}