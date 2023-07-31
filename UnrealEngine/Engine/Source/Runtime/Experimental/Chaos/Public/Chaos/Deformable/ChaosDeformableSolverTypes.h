// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Deformable/ChaosDeformableSolverProxy.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "CoreMinimal.h"

class UDeformableSolverComponent;

namespace Chaos::Softs
{
	struct CHAOS_API FDeformableSolverProperties
	{
		FDeformableSolverProperties(
			int32 InNumSolverSubSteps = 2,
			int32 InNumSolverIterations = 5,
			bool InFixTimeStep = false,
			FSolverReal InTimeStepSize = (FSolverReal)0.05,
			bool InCacheToFile = false,
			bool InbEnableKinematics = true,
			bool InbUseFloor = true,
			bool InbDoSelfCollision = false,
			bool InbUseGridBasedConstraints = false,
			FSolverReal InGridDx = (FSolverReal)1. )
			: NumSolverSubSteps(InNumSolverSubSteps)
			, NumSolverIterations(InNumSolverIterations)
			, FixTimeStep(InFixTimeStep)
			, TimeStepSize(InTimeStepSize)
			, CacheToFile(InCacheToFile)
			, bEnableKinematics(InbEnableKinematics)
			, bUseFloor(InbUseFloor)
			, bDoSelfCollision(InbDoSelfCollision)
			, bUseGridBasedConstraints(InbUseGridBasedConstraints)
			, GridDx(InGridDx)
		{}

		FDeformableSolverProperties(const FDeformableSolverProperties& InProp)
			: NumSolverSubSteps(InProp.NumSolverSubSteps)
			, NumSolverIterations(InProp.NumSolverIterations)
			, FixTimeStep(InProp.FixTimeStep)
			, TimeStepSize(InProp.TimeStepSize)
			, CacheToFile(InProp.CacheToFile)
			, bEnableKinematics(InProp.bEnableKinematics)
			, bUseFloor(InProp.bUseFloor)
			, bDoSelfCollision(InProp.bDoSelfCollision)
			, bUseGridBasedConstraints(InProp.bUseGridBasedConstraints)
			, GridDx(InProp.GridDx)
		{}

		int32 NumSolverSubSteps = 5;
		int32 NumSolverIterations = 5;
		bool FixTimeStep = false;
		FSolverReal TimeStepSize = (FSolverReal)0.05;
		bool CacheToFile = false;
		bool bEnableKinematics = true;
		bool bUseFloor = true;
		bool bDoSelfCollision = false;
		bool bUseGridBasedConstraints = false;
		FSolverReal GridDx = (FSolverReal)1.;
	};


	/*Data Transfer*/
	typedef TSharedPtr<const FThreadingProxy::FBuffer> FDataMapValue; // Buffer Pointer
	typedef TMap<FThreadingProxy::FKey, FDataMapValue > FDeformableDataMap; // <const UObject*,FBufferSharedPtr>

	struct CHAOS_API FDeformablePackage {
		FDeformablePackage()
		{}

		FDeformablePackage(int32 InFrame, FDeformableDataMap&& InMap)
			: Frame(InFrame)
			, ObjectMap(InMap)
		{}

		int32 Frame = INDEX_NONE;
		FDeformableDataMap ObjectMap;
	};

	/* Accessor for the Game Thread*/
	class CHAOS_API FGameThreadAccessor
	{
	public:
		friend class UDeformableSolverComponent;
#if PLATFORM_WINDOWS
	protected:
#endif
		FGameThreadAccessor() {}
	};


	/* Accessor for the Physics Thread*/
	class CHAOS_API FPhysicsThreadAccessor
	{
	public:
		friend class UDeformableSolverComponent;
#if PLATFORM_WINDOWS
	protected:
#endif
		FPhysicsThreadAccessor() {}
	};


}; // namesapce Chaos::Softs