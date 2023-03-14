// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PBIKDebug.h"
#include "Core/PBIKSolver.h"
#include "Core/PBIKBody.h"

namespace PBIK
{
	void FDebugDraw::GetDebugLinesForBodies(TArray<FDebugLine>& OutLines)
	{
		OutLines.Empty();
		for (const FRigidBody& Body : Solver->Bodies)
		{
			if (Body.Bone->Children.Num() == 1)
			{
				FVector StartPos = Body.Position + Body.Rotation * Body.BoneLocalPosition;
				FVector EndPos = Body.Position + Body.Rotation * Body.ChildLocalPositions[0];
				OutLines.Emplace(StartPos, EndPos);
			}
		}
	}
}
