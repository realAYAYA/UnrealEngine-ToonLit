// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Chaos
{
	class FJointConstraint;
}

template <typename T> class FConstraintProxy;

typedef FConstraintProxy< Chaos::FJointConstraint > FJointConstraintProxy;