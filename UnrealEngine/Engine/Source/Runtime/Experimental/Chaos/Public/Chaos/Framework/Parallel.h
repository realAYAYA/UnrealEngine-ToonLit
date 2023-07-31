// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Function.h"

namespace Chaos
{
	void CHAOS_API PhysicsParallelForRange(int32 InNum, TFunctionRef<void(int32, int32)> InCallable, const int32 MinBatchSize, bool bForceSingleThreaded = false);
	void CHAOS_API PhysicsParallelFor(int32 InNum, TFunctionRef<void(int32)> InCallable, bool bForceSingleThreaded = false);
	void CHAOS_API InnerPhysicsParallelForRange(int32 InNum, TFunctionRef<void(int32, int32)> InCallable, const int32 MinBatchSize, bool bForceSingleThreaded = false);
	void CHAOS_API InnerPhysicsParallelFor(int32 InNum, TFunctionRef<void(int32)> InCallable, bool bForceSingleThreaded = false);
	void CHAOS_API PhysicsParallelForWithContext(int32 InNum, TFunctionRef<int32 (int32, int32)> InContextCreator, TFunctionRef<void(int32, int32)> InCallable, bool bForceSingleThreaded = false);
	//void CHAOS_API PhysicsParallelFor_RecursiveDivide(int32 InNum, TFunctionRef<void(int32)> InCallable, bool bForceSingleThreaded = false);


	CHAOS_API extern int32 MaxNumWorkers;
	CHAOS_API extern int32 SmallBatchSize;
	CHAOS_API extern int32 LargeBatchSize;
#if UE_BUILD_SHIPPING
	const bool bDisablePhysicsParallelFor = false;
	const bool bDisableParticleParallelFor = false;
	const bool bDisableCollisionParallelFor = false;
#else
	CHAOS_API extern bool bDisablePhysicsParallelFor;
	CHAOS_API extern bool bDisableParticleParallelFor;
	CHAOS_API extern bool bDisableCollisionParallelFor;
#endif
}