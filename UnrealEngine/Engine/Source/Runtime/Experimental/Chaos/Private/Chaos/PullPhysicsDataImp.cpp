// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PullPhysicsDataImp.h"

namespace Chaos
{

void FPullPhysicsData::Reset()
{
	DirtyRigids.Reset();
	DirtyRigidErrors.Reset();
	DirtyGeometryCollections.Reset();
	DirtyClusterUnions.Reset();
	DirtyJointConstraints.Reset();
	DirtyCharacterGroundConstraints.Reset();
}

}