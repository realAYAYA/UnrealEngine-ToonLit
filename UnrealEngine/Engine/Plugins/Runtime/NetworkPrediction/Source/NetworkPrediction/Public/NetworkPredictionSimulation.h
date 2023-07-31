// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NetworkPredictionBuffer.h"
#include "NetworkPredictionStateTypes.h"

struct FNetSimCueDispatcher;

// Input state is just a collection of references to the simulation state types
template<typename StateTypes=TNetworkPredictionStateTypes<>>
using TNetSimInput = TNetworkPredictionState<StateTypes>;

// Output state: the output SyncState (always created) and TNetSimLazyWriter for the AuxState (created on demand since every tick does not generate a new aux frame)
template<typename StateType=TNetworkPredictionStateTypes<>>
struct TNetSimOutput
{
	using InputType = typename StateType::InputType;
	using SyncType = typename StateType::SyncType;
	using AuxType = typename StateType::AuxType;

	SyncType* Sync;
	const TNetSimLazyWriter<AuxType>& Aux;
	FNetSimCueDispatcher& CueDispatch;

	TNetSimOutput(SyncType* InSync, const TNetSimLazyWriter<AuxType>& InAux, FNetSimCueDispatcher& InCueDispatch)
		: Sync(InSync), Aux(InAux), CueDispatch(InCueDispatch) { }
};