// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseEnvironment.h"

FTypedElementDatabaseEnvironment::FTypedElementDatabaseEnvironment(
	FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager)
	: ScratchBuffer(MakeShared<FTypedElementDatabaseScratchBuffer>())
	, MassEntityManager(InMassEntityManager)
	, MassPhaseManager(InMassPhaseManager)
{
}

FTypedElementDatabaseIndexTable& FTypedElementDatabaseEnvironment::GetIndexTable()
{
	return IndexTable;
}

const FTypedElementDatabaseIndexTable& FTypedElementDatabaseEnvironment::GetIndexTable() const
{
	return IndexTable;
}

FTypedElementDatabaseScratchBuffer& FTypedElementDatabaseEnvironment::GetScratchBuffer()
{
	return *ScratchBuffer;
}

const FTypedElementDatabaseScratchBuffer& FTypedElementDatabaseEnvironment::GetScratchBuffer() const
{
	return *ScratchBuffer;
}

FMassEntityManager& FTypedElementDatabaseEnvironment::GetMassEntityManager()
{
	return MassEntityManager;
}

const FMassEntityManager& FTypedElementDatabaseEnvironment::GetMassEntityManager() const
{
	return MassEntityManager;
}

FMassProcessingPhaseManager& FTypedElementDatabaseEnvironment::GetMassPhaseManager()
{
	return MassPhaseManager;
}

const FMassProcessingPhaseManager& FTypedElementDatabaseEnvironment::GetMassPhaseManager() const
{
	return MassPhaseManager;
}

void FTypedElementDatabaseEnvironment::NextUpdateCycle()
{
	UpdateCycleId++;
}

uint64 FTypedElementDatabaseEnvironment::GetUpdateCycleId() const
{
	return UpdateCycleId;
}
