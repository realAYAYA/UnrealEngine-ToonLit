// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"

namespace Chaos
{
	class FPBDCollisionConstraint;

	/**
	 * @brief Return value in collision visitors to indicate whether we should continue visiting for stop
	*/
	enum class ECollisionVisitorResult
	{
		Stop,
		Continue
	};

}