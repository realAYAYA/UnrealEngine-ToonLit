// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkPredictionPhysics.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#include "Chaos/ParticleHandle.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

// -------------------------------------------------------------------------------------------------------------------------
//	Interpolation related functions. These require calls to the UPrimitiveComponent and cannot be implemented via FBodyInstance
//
//	If you are landing here with a nullptr Driver, see notes in FNetworkPredictionDriverBase::SafeCastDriverToPrimitiveComponent
// -------------------------------------------------------------------------------------------------------------------------

void FNetworkPredictionPhysicsState::BeginInterpolation(UPrimitiveComponent* Driver)
{
	npCheckSlow(Driver);
	Driver->SetSimulatePhysics(false);
}

void FNetworkPredictionPhysicsState::EndInterpolation(UPrimitiveComponent* Driver)
{
	npCheckSlow(Driver);
	Driver->SetSimulatePhysics(true);
}

void FNetworkPredictionPhysicsState::FinalizeInterpolatedPhysics(UPrimitiveComponent* Driver, FNetworkPredictionPhysicsState* InterpolatedState)
{
	npCheckSlow(Driver);
	npCheckSlow(InterpolatedState);
	npEnsure(Driver->IsSimulatingPhysics() == false);
	
	npEnsureSlow(InterpolatedState->Location.ContainsNaN() == false);
	npEnsureSlow(InterpolatedState->Rotation.ContainsNaN() == false);

	Driver->SetWorldLocationAndRotation(InterpolatedState->Location, InterpolatedState->Rotation, false);
}

void FNetworkPredictionPhysicsState::NetSend(const FNetSerializeParams& P, FBodyInstance* BodyInstance)
{
	npCheckSlow(BodyInstance);
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
	if (NetworkPredictionPhysicsCvars::FullPrecision())
	{
		npEnsure(Body_External.CanTreatAsKinematic());
		P.Ar << const_cast<Chaos::FVec3&>(Body_External.X());
		P.Ar << const_cast<Chaos::FRotation3&>(Body_External.R());
		Chaos::FVec3 V = Body_External.V();
		Chaos::FVec3 W = Body_External.W();
		P.Ar << V;
		P.Ar << W;
	}
	else
	{
		bool bSuccess = true;

		SerializePackedVector<100, 30>(const_cast<Chaos::FVec3&>(Body_External.X()), P.Ar);
		const_cast<Chaos::FRotation3&>(Body_External.R()).NetSerialize(P.Ar, nullptr, bSuccess);

		npEnsure(Body_External.CanTreatAsKinematic());
		Chaos::FVec3 V = Body_External.V();
		Chaos::FVec3 W = Body_External.W();
		SerializePackedVector<100, 30>(V, P.Ar);
		SerializePackedVector<100, 30>(W, P.Ar);
	}
}

bool FNetworkPredictionPhysicsState::ShouldReconcile(int32 PhysicsFrame, Chaos::FRewindData* RewindData, FBodyInstance* BodyInstance, FNetworkPredictionPhysicsState* RecvState)
{
	npCheckSlow(BodyInstance);
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();

	auto CompareVector = [](const FVector& Local, const FVector& Authority, const float Tolerance, const FAnsiStringView& DebugStr)
	{
		const FVector Delta = Local - Authority;

		/*
		if (Delta.SizeSquared() > (Tolerance * Tolerance))
		{
			UE_NP_TRACE_SYSTEM_FAULT("Physics Compare mismatch %s", StringCast<TCHAR>(DebugStr.GetData(), DebugStr.Len()).Get());
			UE_NP_TRACE_SYSTEM_FAULT("   Pred: %s", *Local.ToString());
			UE_NP_TRACE_SYSTEM_FAULT("   Auth: %s", *Authority.ToString());
			UE_NP_TRACE_SYSTEM_FAULT("   Delta: %s (%.f)", *Delta.ToString(), Delta.Size());
		}
		*/


		UE_NP_TRACE_RECONCILE(Delta.SizeSquared() > (Tolerance * Tolerance), DebugStr);

		return false;
	};

	auto CompareQuat = [](const FQuat& Local, const FQuat& Authority, const float Tolerance, const FAnsiStringView& DebugStr)
	{
		const float Error = FQuat::ErrorAutoNormalize(Local, Authority);
		UE_NP_TRACE_RECONCILE(Error > Tolerance, DebugStr);
		return false;
	};

	const Chaos::FGeometryParticleState LocalState = RewindData->GetPastStateAtFrame(*Handle->GetHandle_LowLevel(), PhysicsFrame);

	if (CompareVector(LocalState.X(), RecvState->Location, NetworkPredictionPhysicsCvars::ToleranceX(), "X:"))
	{
		return true;
	}

	if (CompareVector(LocalState.V(), RecvState->LinearVelocity, NetworkPredictionPhysicsCvars::ToleranceV(), "V:"))
	{
		return true;
	}

	if (CompareVector(LocalState.W(), RecvState->AngularVelocity, NetworkPredictionPhysicsCvars::ToleranceW(), "W:"))
	{
		return true;
	}

	if (CompareQuat(LocalState.R(), RecvState->Rotation, NetworkPredictionPhysicsCvars::ToleranceR(), "R:"))
	{
		return true;
	}

	return false;
}

void FNetworkPredictionPhysicsState::PerformRollback(FBodyInstance* BodyInstance, FNetworkPredictionPhysicsState* RecvState)
{
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
	Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();

	npCheckSlow(RecvState);

	npEnsureSlow(RecvState->Rotation.IsNormalized());
	npEnsureSlow(!RecvState->Location.ContainsNaN());

	Body_External.SetX(RecvState->Location);
	Body_External.SetR(RecvState->Rotation);

	npEnsureSlow(!RecvState->LinearVelocity.ContainsNaN());
	npEnsureSlow(!RecvState->AngularVelocity.ContainsNaN());

	Body_External.SetV(RecvState->LinearVelocity);
	Body_External.SetW(RecvState->AngularVelocity);
}

void FNetworkPredictionPhysicsState::ToString(int32 PhysicsFrame, Chaos::FRewindData* RewindData, FBodyInstance* BodyInstance, FAnsiStringBuilderBase& Builder)
{
	FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();

	const Chaos::FGeometryParticleState LocalState = RewindData->GetPastStateAtFrame(*Handle->GetHandle_LowLevel(), PhysicsFrame);
	ToStringInternal(LocalState.X(), LocalState.R(), LocalState.V(), LocalState.W(), Builder);
}

void FNetworkPredictionPhysicsState::ToString(FBodyInstance* BodyInstance, FAnsiStringBuilderBase& Builder)
{
	if (BodyInstance)
	{
		FPhysicsActorHandle& Handle = BodyInstance->GetPhysicsActorHandle();
		if (Handle)
		{
			Chaos::FRigidBodyHandle_External& Body_External = Handle->GetGameThreadAPI();
			npEnsure(Body_External.CanTreatAsKinematic());
			ToStringInternal(Body_External.X(), Body_External.R(), Body_External.V(), Body_External.W(), Builder);
		}
		else
		{
			Builder.Append("Null PhysicsActorHandle\n");
		}
	}
	else
	{
		Builder.Append("Null BodyInstance\n");
	}
}
