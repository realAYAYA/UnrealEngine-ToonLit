// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "MoverSimulationTypes.h"
#include "MoverBackendLiaison.generated.h"


/**
 * MoverBackendLiaisonInterface: any object or system wanting to be the driver of Mover actors must implement this. The intent is to act as a
 * middleman between the Mover actor and the system that drives it, such as the Network Prediction plugin.
 * In practice, objects implementing this interface should be some kind of UActorComponent. The Mover actor instantiates its backend liaison
 * when initialized, then relies on the liaison to call various functions as the simulation progresses. See @MoverComponent.
 */
UINTERFACE(MinimalAPI)
class UMoverBackendLiaisonInterface : public UInterface
{
	GENERATED_BODY()
};

class IMoverBackendLiaisonInterface
{
	GENERATED_BODY()

public:
	virtual float GetCurrentSimTimeMs() = 0;
	virtual int32 GetCurrentSimFrame() = 0;

	virtual bool ReadPendingSyncState(OUT FMoverSyncState& OutSyncState) { return false; }
	virtual bool WritePendingSyncState(const FMoverSyncState& SyncStateToWrite) { return false; }
};