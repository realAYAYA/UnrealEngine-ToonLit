// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Trace/Trace.inl"

UE_TRACE_CHANNEL_DEFINE(PhysicsChannel);

UE_TRACE_EVENT_BEGIN(Physics, ParticlePosition)
UE_TRACE_EVENT_FIELD(Chaos::FReal, PositionX)
UE_TRACE_EVENT_FIELD(Chaos::FReal, PositionY)
UE_TRACE_EVENT_FIELD(Chaos::FReal, PositionZ)
UE_TRACE_EVENT_END()

using namespace Chaos;

//A very minimal test debug log function
void ChaosVisualDebugger::ParticlePositionLog(const FVec3& Position)
{
	UE_TRACE_LOG(Physics, ParticlePosition, PhysicsChannel)
					<< ParticlePosition.PositionX(Position.X)
					<< ParticlePosition.PositionY(Position.Y)
					<< ParticlePosition.PositionZ(Position.Z);
}