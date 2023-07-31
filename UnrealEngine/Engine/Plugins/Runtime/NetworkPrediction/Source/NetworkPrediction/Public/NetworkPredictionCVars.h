// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"

// ------------------------------------------------------------------------------------------------------------
//	"Shipping const" cvars: cvars that should compile out to const functions in shipping/test builds
//		This got a little tricky due to templated usage across different modules. Previous patterns for
//		this thing didn't work. This implementation requires manual finding of consolve variables so will be 
//		a bit slower but shouldn't matter since it is compiled out of shipping/test.
// ------------------------------------------------------------------------------------------------------------

inline IConsoleVariable* FindConsoleVarHelper(const TCHAR* VarName)
{
	return IConsoleManager::Get().FindConsoleVariable(VarName, false);
}

// Whether to treat these cvars as consts
#ifndef NETSIM_CONST_CVARS
	#define NETSIM_CONST_CVARS (UE_BUILD_SHIPPING || UE_BUILD_TEST)
#endif

// This is required because these cvars live in header files that will be included across different compilation units
// Just using standard FAutoConsoleVariableRef will cause multiple registrations of the same variable
struct FConditionalAutoConsoleRegister
{
	FConditionalAutoConsoleRegister(const TCHAR* VarName, int32 Value, const TCHAR* Help)
	{
		if (!IConsoleManager::Get().FindConsoleVariable(VarName, false))
		{
			IConsoleManager::Get().RegisterConsoleVariable(VarName, Value, Help, ECVF_Cheat);
		}
	}

	FConditionalAutoConsoleRegister(const TCHAR* VarName, float Value, const TCHAR* Help)
	{
		if (!IConsoleManager::Get().FindConsoleVariable(VarName, false))
		{
			IConsoleManager::Get().RegisterConsoleVariable(VarName, Value, Help, ECVF_Cheat);
		}
	}
};

#define NP_DEVCVAR_INT(Var,Value,VarName,Help) \
	int32 Var = Value; \
	static FAutoConsoleVariableRef Var##CVar(TEXT(VarName), Var, TEXT(Help), ECVF_Cheat );

#define NETSIM_DEVCVAR_INT(Var,Value,VarName,Help) \
	static FConditionalAutoConsoleRegister Var##Auto(TEXT(VarName),(int32)Value,TEXT(Help)); \
	inline int32 Var() \
	{ \
		static const auto* Existing = FindConsoleVarHelper(TEXT(VarName)); \
		check(Existing); \
		return Existing->GetInt(); \
	} \
	inline void Set##Var(int32 _V) \
	{ \
		static auto* Existing = FindConsoleVarHelper(TEXT(VarName)); \
		check(Existing); \
		Existing->Set(_V, ECVF_SetByConsole); \
	}

#if NETSIM_CONST_CVARS
#define NETSIM_DEVCVAR_SHIPCONST_INT(Var,Value,VarName,Help) \
	inline int32 Var() { return Value; } \
	inline void Set##Var(int32 _V) { }
#else
#define NETSIM_DEVCVAR_SHIPCONST_INT(Var,Value,VarName,Help) NETSIM_DEVCVAR_INT(Var,Value,VarName,Help)
#endif



#define NP_DEVCVAR_FLOAT(Var,Value,VarName,Help) \
	float Var = Value; \
	static FAutoConsoleVariableRef Var##CVar(TEXT(VarName), Var, TEXT(Help), ECVF_Cheat );

#define NETSIM_DEVCVAR_FLOAT(Var,Value,VarName,Help) \
	static FConditionalAutoConsoleRegister Var##Auto(TEXT(VarName),(float)Value,TEXT(Help)); \
	inline float Var() \
	{ \
		static const auto* Existing = FindConsoleVarHelper(TEXT(VarName)); \
		check(Existing); \
		return Existing->GetFloat(); \
	} \
	inline void Set##Var(float _V) \
	{ \
		static auto* Existing = FindConsoleVarHelper(TEXT(VarName)); \
		check(Existing); \
		Existing->Set(_V, ECVF_SetByConsole); \
	}

#if NETSIM_CONST_CVARS
#define NETSIM_DEVCVAR_SHIPCONST_FLOAT(Var,Value,VarName,Help) \
	inline float Var() { return Value; } \
	inline void Set##Var(float _V) { }
#else
#define NETSIM_DEVCVAR_SHIPCONST_FLOAT(Var,Value,VarName,Help) NETSIM_DEVCVAR_FLOAT(Var,Value,VarName,Help)
#endif



// Temp

namespace UE_NETWORK_PHYSICS
{
	NETSIM_DEVCVAR_SHIPCONST_INT(MockDebug, 0, "np2.Mock.MockDebug", "Enabled spammy log debugging of mock physics object state");

	NETSIM_DEVCVAR_SHIPCONST_INT(bFutureInputs, 0, "np2.FutureInputs", "Enable FutureInputs feature");		 // Not reimplemented yet
		
	NETSIM_DEVCVAR_SHIPCONST_INT(bInputDecay, 0, "np2.InputDecay", "Enable Input Decay Feature");		
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(InputDecayRate, 0.99f, "np2.InputDecayRate", "Rate of input decay");

	NETSIM_DEVCVAR_SHIPCONST_FLOAT(DragK, 1.12f, "np2.Mock.DragK", "Drag Coefficient (higher=more drag)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(MaxAngularVelocity, 30.f, "np2.Mock.MaxAngularVelocity", "Limits how fast character can possibly rotate.")
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(MovementK, 1.25, "np2.Mock.MovementK", "Movement Coefficient (higher=faster movement)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(RotationK, 1.25, "np2.Mock.RotationK", "Rotation Coefficient (higher=faster movement)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TurnK, 100000000.f, "np2.Mock.TurnK", "Coefficient for automatic turning (higher=quicker turning)");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(TurnDampK, 100.f, "np2.Mock.TurnDampK", "Coefficient for damping portion of turn. Higher=more damping but too higher will lead to instability.");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(DampYawVelocityK, 2.f, "np2.Mock.DampYawVelocityK", "Coefficient for damping angular velocity in yaw direction only. This is only enabled when auto target yaw is disabled.");

	NETSIM_DEVCVAR_SHIPCONST_FLOAT(JumpForce, 70, "np2.Mock.JumpForce", "Per-Frame force to apply while jumping.");
	NETSIM_DEVCVAR_SHIPCONST_INT(JumpFrameDuration, 4, "np2.Mock.JumpFrameDuration", "How many frames to apply jump force for");
	NETSIM_DEVCVAR_SHIPCONST_INT(JumpFudgeFrames, 10, "np2.Mock.JumpFudgeFrames", "How many frames after being in air do we still allow a jump to begin");
	NETSIM_DEVCVAR_SHIPCONST_INT(JumpHack, 0, "np2.Mock.JumpHack", "Make jump not rely on trace which currently causes non determinism");
	NETSIM_DEVCVAR_SHIPCONST_INT(JumpMisPredict, 0, "np2.Mock.JumpMisPredict", "Make jump do random impulse which will cause a misprediction");
	
	NETSIM_DEVCVAR_SHIPCONST_INT(MockImpulse, 1, "np2.Mock.BallImpulse", "Make jump not rely on trace which currently causes non determinism");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(MockImpulseX, 500.0f, "np2.Mock.BallImpulse.X", "X magnitude");
	NETSIM_DEVCVAR_SHIPCONST_FLOAT(MockImpulseZ, 300.0f, "np2.Mock.BallImpulse.Z", "Z magnitude");
}
