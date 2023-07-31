// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionConfig.h"
#include "Chaos/Particles.h"
#include "Chaos/ParticleHandle.h"

// The PhysicsService does bookkeeping for physics objects
//	-Tracing
//	-Updates PendingFrame for physics-only sims (no backing NetworkPrediction simulation)
class IPhysicsService
{
public:

	virtual ~IPhysicsService() = default;
	virtual void PostResimulate(const FFixedTickState* TickState) = 0;
	virtual void PostNetworkPredictionFrame(const FFixedTickState* TickState) = 0;
	virtual void PostPhysics() = 0; // todo

	virtual void EnsureDataInSync(const TCHAR* ContextStr) = 0;
};

template<typename InModelDef>
class TPhysicsService : public IPhysicsService
{
public:

	using ModelDef = InModelDef;
	using DriverType = typename ModelDef::Driver;
	using PhysicsState = typename ModelDef::PhysicsState;

	enum { UpdatePendingFrame = !FNetworkPredictionDriver<ModelDef>::HasSimulation() };

	TPhysicsService(TModelDataStore<ModelDef>* InDataStore)
		: DataStore(InDataStore)
	{
		npCheckf(FNetworkPredictionDriver<ModelDef>::HasPhysics(), TEXT("TPhysicsService created for non physics having sim %s"), ModelDef::GetName());
	}

	void RegisterInstance(FNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->Instances.GetIndex(ID);
		NpResizeAndSetBit(InstanceBitArray, InstanceIdx);
	}

	void UnregisterInstance(FNetworkPredictionID ID)
	{
		const int32 InstanceIdx = DataStore->Instances.GetIndex(ID);
		InstanceBitArray[InstanceIdx] = false;
	}

	void PostResimulate(const FFixedTickState* TickState) override final
	{
		for (TConstSetBitIterator<> BitIt(InstanceBitArray); BitIt; ++BitIt)
		{
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(BitIt.GetIndex());

			FNetworkPredictionDriver<ModelDef>::PostPhysicsResimulate(InstanceData.Info.Driver);

			if (UpdatePendingFrame)
			{
				UE_NP_TRACE_SIM_TICK(InstanceData.TraceID);
			}
			else
			{
				UE_NP_TRACE_SIM(InstanceData.TraceID);
			}

			UE_NP_TRACE_PHYSICS_STATE_CURRENT(ModelDef, InstanceData.Info.Driver);
		}
	}

	void PostNetworkPredictionFrame(const FFixedTickState* TickState) override final
	{
		npCheckSlow(TickState);

		if (UpdatePendingFrame)
		{
			// This is needed so that we can UE_NP_TRACE_SIM_TICK to tell the trace system the sim ticked this frame
			// Right now this is under the assumption that physics is fixed tick once per engine/physics frame.
			// When that changes, this will need to change. We will probably just want another function on IPhysicsService
			// that is called after every Np fixed tick / Physics ticked frame, however that ends up looking.
			// For now this gets us traced physics state which is super valuable.
			const int32 ServerFrame = (TickState->PendingFrame-1) + TickState->Offset; // -1 because PendingFrame was already ++ this frame
			UE_NP_TRACE_PUSH_TICK(ServerFrame * TickState->FixedStepMS, TickState->FixedStepMS, ServerFrame+1);
		}

		for (TConstSetBitIterator<> BitIt(InstanceBitArray); BitIt; ++BitIt)
		{
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(BitIt.GetIndex());
			
			// This is needed for FReplicationProxy::Identical to cause the rep proxy to replicate.
			// Could maybe have physics only sims use different rep proxies that can query the sleeping state themselves
			if (UpdatePendingFrame)
			{
				// Trace explicit "we ticked a frame" (because this has no backing NP sim that would have already traced it)
				UE_NP_TRACE_SIM_TICK(InstanceData.TraceID);

				// This actually is no good: it will cause clients to not be corrected if they incorrectly predict taking the physics object
				// out of sleep. We will need to have some kind of implicit correction window to handle this case in order to have this 
				// optimization work.

				// if (Instance.Handle->ObjectState() != Chaos::EObjectStateType::Sleeping)
				{
					InstanceData.Info.View->PendingFrame = TickState->PendingFrame;
				}
			}
			else
			{
				// We already traced the tick itself earlier, but we still need to push the TraceID in order to trace the resulting physics state
				UE_NP_TRACE_SIM(InstanceData.TraceID);
			}

			UE_NP_TRACE_PHYSICS_STATE_CURRENT(ModelDef, InstanceData.Info.Driver);
		}
	}

	void PostPhysics() override final
	{
		// Not implemented yet but the idea would be to do anything we need to do after physics run for the frame
		// Tracing is the obvious example though it complicates the general tracing system a bit since it adds
		// another point in the frame that we need to sample (pre NP tick (Net Recv/Rollback), post NP tick, post Physics tick)
	}

	void EnsureDataInSync(const TCHAR* ContextStr)
	{
		for (TConstSetBitIterator<> BitIt(InstanceBitArray); BitIt; ++BitIt)
		{
			TInstanceData<ModelDef>& InstanceData = DataStore->Instances.GetByIndexChecked(BitIt.GetIndex());

			if (!FNetworkPredictionDriver<ModelDef>::PhysicsStateIsConsistent(InstanceData.Info.Driver))
			{
				TStringBuilder<128> Builder;
				FNetworkPredictionDriver<ModelDef>::GetDebugString(InstanceData.Info.Driver, Builder);
				UE_LOG(LogNetworkPrediction, Warning, TEXT("Physics State out of sync on %s (Context: %s)"), Builder.ToString(),  ContextStr);
				npEnsure(false);
			}
		}
	}
	
private:

	TBitArray<> InstanceBitArray; // index into DataStore->Instances
	TModelDataStore<ModelDef>* DataStore;
};