// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosFlesh/TetrahedralCollection.h"
#include "Templates/UniquePtr.h"
#include "CoreFwd.h"

namespace ChaosFlesh 
{
	namespace ExampleGeometry
	{
		struct UnitTetrahedron 
		{
			static TUniquePtr<FTetrahedralCollection> CHAOSFLESH_API Create();
		};
	}
}
