// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE { namespace Anim 
{

// Built-in attributes that most nodes will share
struct FAttributes
{
	static ENGINE_API const FName Pose;
	static ENGINE_API const FName Curves;
	static ENGINE_API const FName Attributes;
};

}}	// namespace UE::Anim
