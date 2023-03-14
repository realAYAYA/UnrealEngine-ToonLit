// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/Core.h"
#include "Chaos/Vector.h"


#if UE_TRACE_ENABLED
#	define CHAOS_VISUAL_DEBUGGER_ENABLED 0   // Disabled while under development
#else
#	define CHAOS_VISUAL_DEBUGGER_ENABLED 0
#endif


// Note: This is the absolute minimum runtime interface for implementing a vertical slice of the visual debugger
class ChaosVisualDebugger
{
public:
	static void CHAOS_API ParticlePositionLog(const Chaos::FVec3& Position);
};
