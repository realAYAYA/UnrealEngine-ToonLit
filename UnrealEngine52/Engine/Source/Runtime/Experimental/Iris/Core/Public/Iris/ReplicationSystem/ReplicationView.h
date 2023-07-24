// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Vector.h"

namespace UE::Net
{

struct FReplicationView
{
	struct FView
	{
		FVector Pos;
		FVector Dir;
		float FoVRadians;
	};

	TArray<FView, TInlineAllocator<4>> Views;
};

}
