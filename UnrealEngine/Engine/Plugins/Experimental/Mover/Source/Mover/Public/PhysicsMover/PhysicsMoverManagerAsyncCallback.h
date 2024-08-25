// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/SimCallbackObject.h"
#include "PhysicsMover/PhysicsMoverSimulationTypes.h"

//////////////////////////////////////////////////////////////////////////

struct FPhysicsMoverManagerAsyncInput : public Chaos::FSimCallbackInput
{
	TArray<TUniquePtr<FPhysicsMoverAsyncInput>> AsyncInput;

	void Reset()
	{
		AsyncInput.Reset();
	}
};

struct FPhysicsMoverManagerAsyncOutput : public Chaos::FSimCallbackOutput
{
	TMap<Chaos::FUniqueIdx, TUniquePtr<FPhysicsMoverAsyncOutput>> PhysicsMoverToAsyncOutput;

	void Reset()
	{
		PhysicsMoverToAsyncOutput.Reset();
	}
};

class FPhysicsMoverManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FPhysicsMoverManagerAsyncInput,
	FPhysicsMoverManagerAsyncOutput,
	Chaos::ESimCallbackOptions::Presimulate |
	Chaos::ESimCallbackOptions::ContactModification |
	Chaos::ESimCallbackOptions::Rewind>
{

private:
	virtual void ProcessInputs_Internal(int32 PhysicsStep) override;
	virtual void OnPreSimulate_Internal() override;
	virtual void OnContactModification_Internal(Chaos::FCollisionContactModifier& Modifier) override;
};