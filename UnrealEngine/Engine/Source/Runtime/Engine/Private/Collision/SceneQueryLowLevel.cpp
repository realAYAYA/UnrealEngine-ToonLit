// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/PhysicsInterfaceDeclares.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "PhysicsInterfaceDeclaresCore.h"

#include "PhysicsEngine/CollisionQueryFilterCallback.h"
#include "PhysicsCore.h"


#include "SQAccelerator.h"

#include "SQVerifier.h"
#include "PhysTestSerializer.h"

#include "PBDRigidsSolver.h"
#include "Chaos/PBDRigidsEvolutionGBF.h"

int32 ForceStandardSQ = 0;
FAutoConsoleVariableRef CVarForceStandardSQ(TEXT("p.ForceStandardSQ"), ForceStandardSQ, TEXT("If enabled, we force the standard scene query even if custom SQ structure is enabled"));


#if !UE_BUILD_SHIPPING
int32 SerializeSQs = 0;
int32 SerializeSQSamples = 100;
int32 SerializeBadSQs = 0;
int32 ReplaySQs = 0;
int32 EnableRaycastSQCapture = 1;
int32 EnableOverlapSQCapture = 1;
int32 EnableSweepSQCapture = 1;

FAutoConsoleVariableRef CVarSerializeSQs(TEXT("p.SerializeSQs"), SerializeSQs, TEXT("If enabled, we create a sq capture per sq that takes more than provided value in microseconds. This can be very expensive as the entire scene is saved out"));
FAutoConsoleVariableRef CVarSerializeSQSamples(TEXT("p.SerializeSQSampleCount"), SerializeSQSamples, TEXT("If Query exceeds duration threshold, we will re-measure SQ this many times before serializing. Larger values cause hitching."));
FAutoConsoleVariableRef CVarReplaySweeps(TEXT("p.ReplaySQs"), ReplaySQs, TEXT("If enabled, we rerun the sq against chaos"));
FAutoConsoleVariableRef CVarSerializeBadSweeps(TEXT("p.SerializeBadSQs"), SerializeBadSQs, TEXT("If enabled, we create a sq capture whenever chaos and physx diverge"));
FAutoConsoleVariableRef CVarSerializeSQsRaycastEnabled(TEXT("p.SerializeSQsRaycastEnabled"), EnableRaycastSQCapture, TEXT("If disabled, p.SerializeSQs will not consider raycasts"));
FAutoConsoleVariableRef CVarSerializeSQsOverlapEnabled(TEXT("p.SerializeSQsOverlapEnabled"), EnableOverlapSQCapture, TEXT("If disabled, p.SerializeSQs will not consider overlaps"));
FAutoConsoleVariableRef CVarSerializeSQsSweepEnabled(TEXT("p.SerializeSQsSweepEnabled"), EnableSweepSQCapture, TEXT("If disabled, p.SerializeSQs will not consider sweeps"));

void FinalizeCapture(FPhysTestSerializer& Serializer)
{
	if (SerializeSQs)
	{
		Serializer.Serialize(TEXT("SQCapture"));
	}
#if 0
	if (ReplaySQs)
	{
		const bool bReplaySuccess = SQComparisonHelper(Serializer);
		if (!bReplaySuccess)
		{
			UE_LOG(LogPhysicsCore, Warning, TEXT("Chaos SQ does not match physx"));
			if (SerializeBadSQs && !SerializeSQs)
			{
				Serializer.Serialize(TEXT("BadSQCapture"));
			}
		}
	}
#endif
}
#else
constexpr int32 SerializeSQs = 0;
constexpr int32 ReplaySQs = 0;
constexpr int32 SerializeSQSamples = 0;
constexpr int32 EnableRaycastSQCapture = 0;
constexpr int32 EnableOverlapSQCapture = 0;
constexpr int32 EnableSweepSQCapture = 0;

// No-op in shipping
void FinalizeCapture(FPhysTestSerializer& Serializer) {}
#endif

namespace
{
	void SweepSQCaptureHelper(float QueryDurationSeconds, const FChaosSQAccelerator& SQAccelerator, FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, const FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
	{
		float QueryDurationMicro = QueryDurationSeconds * 1000.0 * 1000.0;
		if (((SerializeSQs && QueryDurationMicro > SerializeSQs)) && IsInGameThread())
		{
			// Measure average time of query over multiple samples to reduce fluke from context switches or that kind of thing.
			uint32 Cycles = 0.0;
			const uint32 SampleCount = SerializeSQSamples;
			for (uint32 Samples = 0; Samples < SampleCount; ++Samples)
			{
				// Reset output to not skew times with large buffer
				FPhysicsHitCallback<FHitSweep> ScratchHitBuffer = FPhysicsHitCallback<FHitSweep>(HitBuffer.WantsSingleResult());

				uint32 StartTime = FPlatformTime::Cycles();
				SQAccelerator.Sweep(QueryGeom, StartTM, Dir, DeltaMag, ScratchHitBuffer, OutputFlags, QueryFilterData, *QueryCallback, DebugParams);
				Cycles += FPlatformTime::Cycles() - StartTime;
			}

			float Milliseconds = FPlatformTime::ToMilliseconds(Cycles);
			float AvgMicroseconds = (Milliseconds * 1000) / SampleCount;

			if (AvgMicroseconds > SerializeSQs)
			{
				FPhysTestSerializer Serializer;
				Serializer.SetPhysicsData(*Scene.GetSolver()->GetEvolution());
				FSQCapture& SweepCapture = Serializer.CaptureSQ();
				SweepCapture.StartCaptureChaosSweep(*Scene.GetSolver()->GetEvolution(), QueryGeom, StartTM, Dir, DeltaMag, OutputFlags, QueryFilterData, Filter, *QueryCallback);
				SweepCapture.EndCaptureChaosSweep(HitBuffer);

				FinalizeCapture(Serializer);
			}
		}
	}

	void RaycastSQCaptureHelper(float QueryDurationSeconds, const FChaosSQAccelerator& SQAccelerator, FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, const FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
	{
		float QueryDurationMicro = QueryDurationSeconds * 1000.0 * 1000.0;
		if (((!!SerializeSQs && QueryDurationMicro > SerializeSQs)) && IsInGameThread())
		{
			// Measure average time of query over multiple samples to reduce fluke from context switches or that kind of thing.
			uint32 Cycles = 0.0;
			const uint32 SampleCount = SerializeSQSamples;
			for (uint32 Samples = 0; Samples < SampleCount; ++Samples)
			{
				// Reset output to not skew times with large buffer
				FPhysicsHitCallback<FHitRaycast> ScratchHitBuffer = FPhysicsHitCallback<FHitRaycast>(HitBuffer.WantsSingleResult());

				uint32 StartTime = FPlatformTime::Cycles();
				SQAccelerator.Raycast(Start, Dir, DeltaMag, ScratchHitBuffer, OutputFlags, QueryFilterData, *QueryCallback, DebugParams);
				Cycles += FPlatformTime::Cycles() - StartTime;
			}

			float Milliseconds = FPlatformTime::ToMilliseconds(Cycles);
			float AvgMicroseconds = (Milliseconds * 1000) / SampleCount;

			if (AvgMicroseconds > SerializeSQs)
			{
				FPhysTestSerializer Serializer;
				Serializer.SetPhysicsData(*Scene.GetSolver()->GetEvolution());
				FSQCapture& RaycastCapture = Serializer.CaptureSQ();
				RaycastCapture.StartCaptureChaosRaycast(*Scene.GetSolver()->GetEvolution(), Start, Dir, DeltaMag, OutputFlags, QueryFilterData, Filter, *QueryCallback);
				RaycastCapture.EndCaptureChaosRaycast(HitBuffer);

				FinalizeCapture(Serializer);
			}
		}
	}

	void OverlapSQCaptureHelper(float QueryDurationSeconds, const FChaosSQAccelerator& SQAccelerator, FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, const FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
	{
		float QueryDurationMicro = QueryDurationSeconds * 1000.0 * 1000.0;
		if (((!!SerializeSQs && QueryDurationMicro > SerializeSQs)) && IsInGameThread())
		{
			// Measure average time of query over multiple samples to reduce fluke from context switches or that kind of thing.
			uint32 Cycles = 0.0;
			const uint32 SampleCount = SerializeSQSamples;
			for (uint32 Samples = 0; Samples < SampleCount; ++Samples)
			{
				// Reset output to not skew times with large buffer
				FPhysicsHitCallback<FHitOverlap> ScratchHitBuffer = FPhysicsHitCallback<FHitOverlap>(HitBuffer.WantsSingleResult());

				uint32 StartTime = FPlatformTime::Cycles();
				SQAccelerator.Overlap(QueryGeom, GeomPose, ScratchHitBuffer, QueryFilterData, *QueryCallback);
				Cycles += FPlatformTime::Cycles() - StartTime;
			}

			float Milliseconds = FPlatformTime::ToMilliseconds(Cycles);
			float AvgMicroseconds = (Milliseconds * 1000) / SampleCount;

			if (AvgMicroseconds > SerializeSQs)
			{
				FPhysTestSerializer Serializer;
				Serializer.SetPhysicsData(*Scene.GetSolver()->GetEvolution());
				FSQCapture& OverlapCapture = Serializer.CaptureSQ();
				OverlapCapture.StartCaptureChaosOverlap(*Scene.GetSolver()->GetEvolution(), QueryGeom, GeomPose, QueryFilterData, Filter, *QueryCallback);
				OverlapCapture.EndCaptureChaosOverlap(HitBuffer);

				FinalizeCapture(Serializer);
			}
		}
	}
}

template <typename THitRaycast>
void LowLevelRaycastImp(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<THitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	constexpr bool bGTData = std::is_same<THitRaycast, FHitRaycast>::value;
	const auto SolverAccelerationStructure = bGTData ? Scene.GetSpacialAcceleration() : Scene.GetSolver()->GetInternalAccelerationStructure_Internal();
	if (SolverAccelerationStructure)
	{
		FChaosSQAccelerator SQAccelerator(*SolverAccelerationStructure);
		double Time = 0.0;
		{
			FScopedDurationTimer Timer(Time);
			SQAccelerator.Raycast(Start, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFilterData, *QueryCallback, DebugParams);
		}

		if constexpr(bGTData)
		{
			if (!!SerializeSQs && !!EnableRaycastSQCapture)
			{
				RaycastSQCaptureHelper(Time, SQAccelerator, Scene, Start, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
			}
		}
	}
}

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitRaycast>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	LowLevelRaycastImp(Scene, Start, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}

void LowLevelRaycast(FPhysScene& Scene, const FVector& Start, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FPTRaycastHit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	LowLevelRaycastImp(Scene, Start, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}

template <typename THitSweep>
void LowLevelSweepImp(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<THitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	constexpr bool bGTData = std::is_same<THitSweep, FHitSweep>::value;
	const auto SolverAccelerationStructure = bGTData ? Scene.GetSpacialAcceleration() : Scene.GetSolver()->GetInternalAccelerationStructure_Internal();
	if (SolverAccelerationStructure)
	{
		FChaosSQAccelerator SQAccelerator(*SolverAccelerationStructure);
		{
			//ISQAccelerator* SQAccelerator = Scene.GetSQAccelerator();
			double Time = 0.0;
			{
				FScopedDurationTimer Timer(Time);
				SQAccelerator.Sweep(QueryGeom, StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFilterData, *QueryCallback, DebugParams);
			}

			if constexpr (bGTData)
			{
				if (!!SerializeSQs && !!EnableSweepSQCapture)
				{
					SweepSQCaptureHelper(Time, SQAccelerator, Scene, QueryGeom, StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
				}
			}
		}
	}
}

void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FHitSweep>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	return LowLevelSweepImp(Scene, QueryGeom, StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}

void LowLevelSweep(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& StartTM, const FVector& Dir, float DeltaMag, FPhysicsHitCallback<FPTSweepHit>& HitBuffer, EHitFlags OutputFlags, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	return LowLevelSweepImp(Scene, QueryGeom, StartTM, Dir, DeltaMag, HitBuffer, OutputFlags, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}

template <typename THitOverlap>
void LowLevelOverlapImp(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<THitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	constexpr bool bGTData = std::is_same<THitOverlap, FHitOverlap>::value;
	const auto SolverAccelerationStructure = bGTData ? Scene.GetSpacialAcceleration() : Scene.GetSolver()->GetInternalAccelerationStructure_Internal();
	if (SolverAccelerationStructure)
	{
		FChaosSQAccelerator SQAccelerator(*SolverAccelerationStructure);
		double Time = 0.0;
		{
			FScopedDurationTimer Timer(Time);
			SQAccelerator.Overlap(QueryGeom, GeomPose, HitBuffer, QueryFilterData, *QueryCallback, DebugParams);
		}

		if constexpr (bGTData)
		{
			if (!!SerializeSQs && !!EnableOverlapSQCapture)
			{
				OverlapSQCaptureHelper(Time, SQAccelerator, Scene, QueryGeom, GeomPose, HitBuffer, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
			}
		}
	}
}

void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FHitOverlap>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	return LowLevelOverlapImp(Scene, QueryGeom, GeomPose, HitBuffer, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}

void LowLevelOverlap(FPhysScene& Scene, const FPhysicsGeometry& QueryGeom, const FTransform& GeomPose, FPhysicsHitCallback<FPTOverlapHit>& HitBuffer, FQueryFlags QueryFlags, const FCollisionFilterData& Filter, const FQueryFilterData& QueryFilterData, ICollisionQueryFilterCallbackBase* QueryCallback, const FQueryDebugParams& DebugParams)
{
	return LowLevelOverlapImp(Scene, QueryGeom, GeomPose, HitBuffer, QueryFlags, Filter, QueryFilterData, QueryCallback, DebugParams);
}
