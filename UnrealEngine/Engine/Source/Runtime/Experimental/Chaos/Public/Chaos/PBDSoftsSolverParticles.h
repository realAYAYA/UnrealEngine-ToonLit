// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Real.h"
#include "Chaos/Vector.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/DynamicParticles.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"

namespace Chaos::Softs
{

struct FPAndInvM
{
	FSolverVec3 P;
	FSolverReal InvM;
};

class FSolverParticles : public TDynamicParticles<FSolverReal, 3>
{
public:
	typedef TDynamicParticles<FSolverReal, 3> Base;

	FSolverParticles()
	    : Base()
	{
		TArrayCollection::AddArray(&MPAndInvM);
	}
	FSolverParticles(const FSolverParticles& Other) = delete;
	FSolverParticles(FSolverParticles&& Other)
	    : Base(MoveTemp(Other)), MPAndInvM(MoveTemp(Other.MPAndInvM))
	{
		TArrayCollection::AddArray(&MPAndInvM);
	}

	const FSolverVec3& P(const int32 index) const { return MPAndInvM[index].P; }
	FSolverVec3& P(const int32 index) { return MPAndInvM[index].P; }

	const FPAndInvM& PAndInvM(const int32 index) const { return MPAndInvM[index]; }
	FPAndInvM& PAndInvM(const int32 index) { return MPAndInvM[index]; }

	TArrayCollectionArray<FPAndInvM>& GetPAndInvM() { return MPAndInvM; }
	const TArrayCollectionArray<FPAndInvM>& GetPAndInvM() const { return MPAndInvM; }

private:
	// Note that InvM is copied from TDynamicParticles to here during ApplyPreSimulationTransforms (when P is updated)
	TArrayCollectionArray<FPAndInvM> MPAndInvM;
};

}
