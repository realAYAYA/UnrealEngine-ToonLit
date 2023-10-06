// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDConstraintContainer.h"
#include "Chaos/Evolution/SolverConstraintContainer.h"

namespace Chaos
{
	FPBDConstraintContainer::FPBDConstraintContainer(FConstraintHandleTypeID InConstraintHandleType)
		: ConstraintHandleType(InConstraintHandleType)
		, ContainerId(INDEX_NONE)
	{
	}

	FPBDConstraintContainer::~FPBDConstraintContainer()
	{
	}

}
