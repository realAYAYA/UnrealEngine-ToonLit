// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassRequirementAccessDetector.h"
#if WITH_MASSENTITY_DEBUG
#include "MassEntityQuery.h"
#include "HAL/IConsoleManager.h"

namespace UE::Mass::Private
{
	bool bTrackRequirementsAccess = false;
	
	FAutoConsoleVariableRef CVarTrackRequirementsAccess(TEXT("mass.debug.TrackRequirementsAccess"), bTrackRequirementsAccess
		, TEXT("Enables Mass processing debugging mode where we monitor thread-safety of query requirements access."));
}

void FMassRequirementAccessDetector::Initialize()
{
	check(IsInGameThread());
	AddDetectors(FMassFragmentBitSet::FStructTrackerWrapper::StructTracker);
	AddDetectors(FMassChunkFragmentBitSet::FStructTrackerWrapper::StructTracker);
	AddDetectors(FMassSharedFragmentBitSet::FStructTrackerWrapper::StructTracker);
	AddDetectors(FMassExternalSubsystemBitSet::FStructTrackerWrapper::StructTracker);
}

void FMassRequirementAccessDetector::AddDetectors(const FStructTracker& StructTracker)
{
	TConstArrayView<TWeakObjectPtr<const UStruct>> Types = StructTracker.DebugGetAllStructTypes<UStruct>();
	for (TWeakObjectPtr<const UStruct> Type : Types)
	{
		check(Type.Get());
		Detectors.Add(Type.Get(), MakeShareable(new FRWAccessDetector()));
	}
}

void FMassRequirementAccessDetector::RequireAccess(const FMassEntityQuery& Query)
{
	if (UE::Mass::Private::bTrackRequirementsAccess)
	{
		Operation(Query.RequiredConstSubsystems, &FRWAccessDetector::AcquireReadAccess);
		Operation(Query.RequiredMutableSubsystems, &FRWAccessDetector::AcquireWriteAccess);
		
		Aquire(Query.FragmentRequirements);
		Aquire(Query.ChunkFragmentRequirements);
		Aquire(Query.ConstSharedFragmentRequirements);
		Aquire(Query.SharedFragmentRequirements);
	}
}

void FMassRequirementAccessDetector::ReleaseAccess(const FMassEntityQuery& Query)
{
	if (UE::Mass::Private::bTrackRequirementsAccess)
	{
		Operation(Query.RequiredConstSubsystems, &FRWAccessDetector::ReleaseReadAccess);
		Operation(Query.RequiredMutableSubsystems, &FRWAccessDetector::ReleaseWriteAccess);

		Release(Query.FragmentRequirements);
		Release(Query.ChunkFragmentRequirements);
		Release(Query.ConstSharedFragmentRequirements);
		Release(Query.SharedFragmentRequirements);
	}
}

#endif // WITH_MASSENTITY_DEBUG