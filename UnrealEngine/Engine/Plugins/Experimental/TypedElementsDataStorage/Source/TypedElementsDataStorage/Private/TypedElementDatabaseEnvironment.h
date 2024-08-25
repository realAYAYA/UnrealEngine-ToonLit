// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityManager.h"
#include "MassProcessingPhaseManager.h"
#include "TypedElementDatabaseScratchBuffer.h"
#include "TypedElementDatabaseIndexTable.h"

class FTypedElementDatabaseEnvironment final
{
public:
	FTypedElementDatabaseEnvironment(FMassEntityManager& InMassEntityManager, FMassProcessingPhaseManager& InMassPhaseManager);

	FTypedElementDatabaseIndexTable& GetIndexTable();
	const FTypedElementDatabaseIndexTable& GetIndexTable() const;

	FTypedElementDatabaseScratchBuffer& GetScratchBuffer();
	const FTypedElementDatabaseScratchBuffer& GetScratchBuffer() const;

	FMassEntityManager& GetMassEntityManager();
	const FMassEntityManager& GetMassEntityManager() const;
	
	FMassProcessingPhaseManager& GetMassPhaseManager();
	const FMassProcessingPhaseManager& GetMassPhaseManager() const;

	void NextUpdateCycle();
	uint64 GetUpdateCycleId() const;

private:
	FTypedElementDatabaseIndexTable IndexTable;
	TSharedPtr<FTypedElementDatabaseScratchBuffer> ScratchBuffer;
	
	FMassEntityManager& MassEntityManager;
	FMassProcessingPhaseManager& MassPhaseManager;

	uint64 UpdateCycleId = 0;
};
