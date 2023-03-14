// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "Chaos/Real.h"

namespace Chaos
{
	/**
	*  Constraint settings all for generic defaults and scalars to the constraint parameters so
	*  that converting between existing (pre v4.26) simulation parameters and Chaos can produce similar 
	*  simulation results. 
	*/
	class PHYSICSCORE_API ConstraintSettings
	{
		ConstraintSettings(){};

	public:
		static FReal JointStiffness();
		static FReal LinearDriveStiffnessScale();
		static FReal LinearDriveDampingScale();
		static FReal AngularDriveStiffnessScale();
		static FReal AngularDriveDampingScale();

		static int SoftLinearForceMode();
		static FReal SoftLinearStiffnessScale();
		static FReal SoftLinearDampingScale();

		static int SoftAngularForceMode();
		static FReal SoftAngularStiffnessScale();
		static FReal SoftAngularDampingScale();

		static FReal LinearBreakScale();
		static FReal AngularBreakScale();
	};
}