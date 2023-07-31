// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionCues.h"
#include "NetworkPredictionConditionalState.h"
#include "NetworkPredictionSettings.h"
#include "NetworkPredictionDebug.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionStateView.h"
#include "Misc/StringBuilder.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"

struct FBodyInstance;

namespace Chaos
{
	class FRewindData;
};

// The "driver" represents the UE side of the NetworkPrediction System. Typically the driver will be an Unreal Actor or ActorComponent.
// This driver class is defined in the ModelDef (your version of FNetworkPredictionModelDef). For example, AActor or AMyPawn class.
// While the Simulation class is agnostic to all of this, the driver is responsible for specifying exactly how things should work, 
// generating input and handling output from the system.
//
// Its worth pointing out that a new ModelDef with a new Driver class is only required when you want to actually change behavior at the driver
// level. For example, if you define a ModelDef {MySimulation, AMyPawn_Base} you can still use this ModelDef with both AMyPawn_Player, AMyPawn_AI.
// You would only need to create a ModelDef {MySimulation, AMyPawn_Player} if your player class wanted to customize Driver calls for example.
// (And even then: making the Driver functions virtual on AMyPawn_Base would be enough, at the cost of virtual function calls).
//
// FNetworkPredictionDriver is how calls to the driver are made. FNetworkPredictionDriverBase is a default implementation that will be used.
// You can customize any of these calls by specializing FNetworkPredictionDriver to your ModelDef type. If you do this, you should inherit from 
// FNetworkPredictionDriverBase<YourModelDef> or FNetworkPredictionDriver<ParentModelDef> if you are extending an existing ModelDef.
// (otherwise you will need to implement every required function yourself).
//
//
// The default implementations can be broken down into a few categories:
//
//	1.	Simple stuff like ::GetDebugString() - we provide generic implemenations for AActor and UActorComponent. In general you won't need 
//		to implement these yourself unless you want to include extra information in the debug string that the system prints to logs.
//
//	2.	Calls that get forwarded to the Driver itself. For example ::InitializeSimulationState(FSyncState*, FAuxState*). We can't provide
//		a generic implementation because the state type is defined by the user. We forward this to the Driver because that is ultimately
//		where the initial simulation state is seeded from. Defining InitializeSimulationState on the Driver itself is the simplest way
//		of doing this and will make the most sense to anyone looking at the Driver class.
//
//		However there may be cases when this is not an option. For example if you want to create a Simulation in this system that can be driven
//		by an AActor. You wouldn't want to modify AActor itself to implement InitializeSimulationState. In those cases, you can specialize
//		FNetworkPredictionDriver<YourModelDef>::InitializeSimulationState(AActor*, FSyncState*, FAuxState*) and implement it there.
//
//	3.	Calls that get forwarded to the underlying StateTypes. For example ::ShouldReconcile(FSyncState*, FSyncState*). The default 
//		implementation for these calls will get forwarded to member functions on the state type itself. This allows the user struct
//		to define the default behavior while still giving the Driver type the option to override.
//
//
//	High level goals: maximize non-intrusive extendability, shield users from templated boiler plate where possible.

template<typename ModelDef>
struct FNetworkPredictionDriver;

template<typename ModelDef>
struct FNetworkPredictionDriverBase
{
	using DriverType = typename ModelDef::Driver;
	using Simulation = typename ModelDef::Simulation;
	using StateTypes = typename ModelDef::StateTypes;
	using InputType = typename StateTypes::InputType;
	using SyncType = typename StateTypes::SyncType;
	using AuxType = typename StateTypes::AuxType;
	using PhysicsState = typename ModelDef::PhysicsState;

	static constexpr bool HasNpState() { return !TIsVoidType<InputType>::Value || !TIsVoidType<SyncType>::Value || !TIsVoidType<AuxType>::Value; }
	static constexpr bool HasDriver() { return !TIsVoidType<DriverType>::Value; }
	static constexpr bool HasSimulation() { return !TIsVoidType<Simulation>::Value; }
	static constexpr bool HasInput() { return !TIsVoidType<InputType>::Value; }
	static constexpr bool HasPhysics() { return !TIsVoidType<PhysicsState>::Value; }

	// Defines what the ModelDef can do. This is a compile time thing only.
	static constexpr FNetworkPredictionModelDefCapabilities GetCapabilities()
	{
		FNetworkPredictionModelDefCapabilities Capabilities;
		if (HasPhysics())
		{
			// If you have physics state, you must use fixed tick
			// (except if you want to dual support physics/non physics within the same simulation type, this will have to be a customization override)
			Capabilities.SupportedTickingPolicies = ENetworkPredictionTickingPolicy::Fixed;
		}

		if (HasSimulation() == false && HasPhysics() == false)
		{
			// We have nothing to tick, so no SimExtrapolation or ForwardPrediction
			Capabilities.FixedNetworkLODs.AP = ENetworkLOD::Interpolated;
			Capabilities.FixedNetworkLODs.SP = ENetworkLOD::Interpolated;
			Capabilities.IndependentNetworkLODs.AP = ENetworkLOD::Interpolated;
			Capabilities.IndependentNetworkLODs.SP = ENetworkLOD::Interpolated;
		}
		return Capabilities;
	}

	// Defines the default settings for a spawn instance.
	static bool GetDefaultArchetype(FNetworkPredictionInstanceArchetype& Archetype, ENetworkPredictionTickingPolicy PreferredTickingPolicy, bool bCanPhysicsTickPhysics)
	{
		static constexpr FNetworkPredictionModelDefCapabilities Capabilities = FNetworkPredictionDriver<ModelDef>::GetCapabilities();
		if (HasPhysics() && bCanPhysicsTickPhysics == false && !EnumHasAnyFlags(Capabilities.SupportedTickingPolicies, ENetworkPredictionTickingPolicy::Independent))
		{
			// We require fix ticking physics to function and its not enabled. Return false.
			return false;
		}

		// Use preferred ticking policy if we support it
		if (EnumHasAnyFlags(Capabilities.SupportedTickingPolicies, PreferredTickingPolicy))
		{
			Archetype.TickingMode = PreferredTickingPolicy;
		}
		else
		{
			// else use the one we support (assumes only 2 modes)
			Archetype.TickingMode = Capabilities.SupportedTickingPolicies;
		}
		
		return true;
	}

	// Defines the default config for an instance, given their archetype and Role/NetConnection
	static FNetworkPredictionInstanceConfig GetConfig(const FNetworkPredictionInstanceArchetype& Archetype, const FNetworkPredictionSettings& GlobalSettings, ENetRole Role, bool bHasNetConnection)
	{
		static constexpr FNetworkPredictionModelDefCapabilities Capabilities = FNetworkPredictionDriver<ModelDef>::GetCapabilities();
		FNetworkPredictionInstanceConfig Config;
		switch (Role)
		{
		case ROLE_Authority:
			Config.InputPolicy = bHasNetConnection ? ENetworkPredictionLocalInputPolicy::Passive : ENetworkPredictionLocalInputPolicy::PollPerSimFrame;
			break;
		case ROLE_AutonomousProxy:
			Config.InputPolicy = ENetworkPredictionLocalInputPolicy::PollPerSimFrame;
			Config.NetworkLOD = Archetype.TickingMode == ENetworkPredictionTickingPolicy::Fixed ? GetHighestNetworkLOD(Capabilities.FixedNetworkLODs.AP) : GetHighestNetworkLOD(Capabilities.IndependentNetworkLODs.AP);
			break;
		case ROLE_SimulatedProxy:
			Config.InputPolicy = ENetworkPredictionLocalInputPolicy::Passive;

			// Use preferred SP NetworkLOD if we support it
			const ENetworkLOD CapableLODs = (Archetype.TickingMode == ENetworkPredictionTickingPolicy::Fixed) ? Capabilities.FixedNetworkLODs.SP : Capabilities.IndependentNetworkLODs.SP;
			if (EnumHasAnyFlags(CapableLODs, GlobalSettings.SimulatedProxyNetworkLOD))
			{
				Config.NetworkLOD = GlobalSettings.SimulatedProxyNetworkLOD;
			}
			else
			{
				// Use highest allowed LOD otherwise
				Config.NetworkLOD = GetHighestNetworkLOD(CapableLODs);
			}
			break;
		};

		return Config;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	Basic string/debug info
	// -----------------------------------------------------------------------------------------------------------------------------------

	static void GetDebugString(AActor* Actor, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s %s"), ModelDef::GetName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), Actor->GetLocalRole()));
	}
	
	static void GetDebugString(UActorComponent* ActorComp, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s %s"), ModelDef::GetName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), ActorComp->GetOwnerRole()));
	}

	static void GetDebugString(void* NoDriver, FStringBuilderBase& Builder)
	{
		Builder.Append(ModelDef::GetName());
	}

	static void GetDebugStringFull(AActor* Actor, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s. Driver: %s. Role: %s."), ModelDef::GetName(), *Actor->GetPathName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), Actor->GetLocalRole()));
	}

	static void GetDebugStringFull(UActorComponent* ActorComp, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s. Driver: %s. Role: %s."), ModelDef::GetName(), *ActorComp->GetPathName(), *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), ActorComp->GetOwnerRole()));
	}

	static void GetDebugStringFull(void* NoDriver, FStringBuilderBase& Builder)
	{
		Builder.Append(ModelDef::GetName());
	}

	static void GetTraceString(UActorComponent* ActorComp, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s: %s %s"), ModelDef::GetName(), *ActorComp->GetOwner()->GetName(), *ActorComp->GetName());
	}

	static void GetTraceString(AActor* Actor, FStringBuilderBase& Builder)
	{
		Builder.Appendf(TEXT("%s: %s."), ModelDef::GetName(), *Actor->GetName());
	}
	
	static void GetTraceString(void* NoDriver, FStringBuilderBase& Builder)
	{
		Builder.Append(ModelDef::GetName());
	}

	static FTransform GetDebugWorldTransform(AActor* DriverActor)
	{
		return DriverActor->GetTransform();
	}

	static FTransform GetDebugWorldTransform(UActorComponent* DriverComponent)
	{
		return GetDebugWorldTransform(DriverComponent->GetOwner());
	}

	static FTransform GetDebugWorldTransform(void* NoDriver)
	{
		npEnsure(false);
		return FTransform::Identity;
	}

	static FBox GetDebugBoundingBox(AActor* DriverActor)
	{
		return DriverActor->CalculateComponentsBoundingBoxInLocalSpace();
	}

	static FBox GetDebugBoundingBox(UActorComponent* DriverComponent)
	{
		return GetDebugBoundingBox(DriverComponent->GetOwner());
	}

	static FBox GetDebugBoundingBox(void* NoDriver)
	{
		npEnsure(false);
		return FBox();
	}

	static void DrawDebugOutline(DriverType* Driver, FColor Color, float Lifetime=0.f)
	{
		NetworkPredictionDebug::DrawDebugOutline(GetDebugWorldTransform(Driver), GetDebugBoundingBox(Driver), Color, Lifetime);
	}

	static void DrawDebugText3D(DriverType* Driver, const TCHAR* Str, FColor Color, float Lifetime=0.f, float ZOffset=50.f)
	{
		FTransform Transform = GetDebugWorldTransform(Driver);
		Transform.AddToTranslation(FVector(0.f, 0.f, ZOffset));
		NetworkPredictionDebug::DrawDebugText3D(Str, Transform , Color, Lifetime);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	InitializeSimulationState
	//
	//	Set the given simulation state to the current state of the driver.
	//	Called whenever the simulation is fully initialized and is ready to have its initial state set.
	//	This will be called if the instance is reconfigured as well (E.g, went from AP->SP, Interpolated->Forward Predicted, etc).
	//
	//	The default implementation will call InitializeSimulationState(FSyncState*, FAuxState*) on the Driver class itself.
	// -----------------------------------------------------------------------------------------------------------------------------------
	static void InitializeSimulationState(DriverType* Driver, FNetworkPredictionStateView* View)
	{
		npCheckSlow(View);
		InitializeSimulationState(Driver, (SyncType*)View->PendingSyncState, (AuxType*)View->PendingAuxState);
	}
	
	static void InitializeSimulationState(DriverType* Driver, SyncType* Sync, AuxType* Aux)
	{
		CallInitializeSimulationStateMemberFunc(Driver, Sync, Aux);
	}
	
	struct CInitializeSimulationStateFuncable
	{
		template <typename InDriverType, typename...>
		auto Requires(InDriverType* Driver, SyncType* Sync, AuxType* Aux) -> decltype(Driver->InitializeSimulationState(Sync, Aux));
	};

	static constexpr bool HasInitializeSimulationState = TModels<CInitializeSimulationStateFuncable, DriverType, SyncType, AuxType>::Value;
	
	template<bool HasFunc=HasInitializeSimulationState>
	static typename TEnableIf<HasFunc>::Type CallInitializeSimulationStateMemberFunc(DriverType* Driver, SyncType* Sync, AuxType* Aux)
	{
		npCheckSlow(Driver);
		Driver->InitializeSimulationState(Sync, Aux);
	}

	template<bool HasFunc=HasInitializeSimulationState>
	static typename TEnableIf<!HasFunc>::Type CallInitializeSimulationStateMemberFunc(DriverType* Driver, SyncType* Sync, AuxType* Aux)
	{
		npCheckf(!HasNpState(), TEXT("No InitializeSimulationState implementation found. Implement DriverType::ProduceInput or ModelDef::ProduceInput"));
	}	

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	ProduceInput
	//
	//	Called on locally controlled simulations prior to ticking a new frame. This is to allow input to be as fresh as possible.
	//  submitting input from an Actor tick would be too late in the frame. NOTE: currently input is sampled/broadcast in PC tick and 
	//	this is still causing a frame of latency in the samples. This will be fixed in the future.
	// -----------------------------------------------------------------------------------------------------------------------------------

	static void ProduceInput(DriverType* Driver, int32 DeltaTimeMS, InputType* InputCmd)
	{
		CallProduceInputMemberFunc(Driver, DeltaTimeMS, InputCmd);
	}
	
	struct CProduceInputMemberFuncable
	{
		template <typename InDriverType, typename...>
		auto Requires(InDriverType* Driver, int32 TimeMS, InputType* InputCmd) -> decltype(Driver->ProduceInput(TimeMS, InputCmd));
	};

	static constexpr bool HasProduceInput = TModels<CProduceInputMemberFuncable, DriverType, int32, InputType>::Value;

	template<bool HasFunc=HasProduceInput>
	static typename TEnableIf<HasFunc>::Type CallProduceInputMemberFunc(DriverType* Driver, int32 DeltaTimeMS, InputType* InputCmd)
	{
		npCheckSlow(Driver);
		Driver->ProduceInput(DeltaTimeMS, InputCmd);
	}

	template<bool HasFunc=HasProduceInput>
	static typename TEnableIf<!HasFunc>::Type CallProduceInputMemberFunc(DriverType* Driver, int32 DeltaTimeMS, InputType* InputCmd)
	{
		npCheckf(false, TEXT("No ProduceInput implementation found. Implement DriverType::ProduceInput or ModelDef::ProduceInput"));
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	FinalizeFrame
	//
	//	Called every engine frame to push the final result of the NetworkPrediction system to the driver
	// -----------------------------------------------------------------------------------------------------------------------------------
	static void FinalizeFrame(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		CallFinalizeFrameMemberFunc(Driver, SyncState, AuxState);
	}
	
	struct CFinalizeFrameMemberFuncable
	{
		template <typename InDriverType, typename...>
		auto Requires(InDriverType* Driver, const SyncType* S, const AuxType* A) -> decltype(Driver->FinalizeFrame(S, A));
	};

	static constexpr bool HasFinalizeFrame = TModels<CFinalizeFrameMemberFuncable, DriverType, SyncType, AuxType>::Value;

	template<bool HasFunc=HasFinalizeFrame>
	static typename TEnableIf<HasFunc>::Type CallFinalizeFrameMemberFunc(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		npCheckSlow(Driver);
		Driver->FinalizeFrame(SyncState, AuxState);
	}

	template<bool HasFunc=HasFinalizeFrame>
	static typename TEnableIf<!HasFunc>::Type CallFinalizeFrameMemberFunc(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		// This is only fatal if we have NpState. If we just had physics for example, a FinalizeFrame call isn't needed.
		npCheckf(!HasNpState(), TEXT("No FinalizeFrame implementation found. Implement DriverType::FinalizeFrame or ModelDef::FinalizeFrame"));
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	RestoreFrame
	//
	//	Called prior to beginning rollback frames. This instance should put itself in whatever state it needs to be in for resimulation to
	//	run. In practice this should mean getting right collision+component states in sync so that any scene queries will get the correct
	//	data.
	//
	//	This can be automated for physics (the PhysicsState can generically marshal the data). We can't do this for kinematic sims.
	//	
	//	
	// -----------------------------------------------------------------------------------------------------------------------------------

	static void RestoreFrame(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		FNetworkPredictionDriver<ModelDef>::MarshalPhysicsToComponent(Driver);
		FNetworkPredictionDriver<ModelDef>::CallRestoreFrameMemberFunc(Driver, SyncState, AuxState);
	}

	struct CRestoreFrameMemberFuncable
	{
		template <typename InDriverType, typename...>
		auto Requires(InDriverType* Driver, const SyncType* S, const AuxType* A) -> decltype(Driver->RestoreFrame(S, A));
	};

	static constexpr bool HasRestoreFrame = TModels<CRestoreFrameMemberFuncable, DriverType, SyncType, AuxType>::Value;

	template<bool HasFunc=HasRestoreFrame>
	static typename TEnableIf<HasFunc>::Type CallRestoreFrameMemberFunc(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		npCheckSlow(Driver);
		Driver->RestoreFrame(SyncState, AuxState);
	}

	template<bool HasFunc=HasRestoreFrame>
	static typename TEnableIf<!HasFunc>::Type CallRestoreFrameMemberFunc(DriverType* Driver, const SyncType* SyncState, const AuxType* AuxState)
	{
		// This isn't a problem but we should probably do something if there is no RestoreFrame function:
		//	-Warn/complain (but user may not care in all cases. So may need a trait to opt out?)
		//	-Call FinalizeFrame: less boiler plate to add (but causes confusion and could lead to slow FinalizeFrames being called too often)
		//	-Force both Restore/Finalize Frame to be implemented but always implicitly call RestoreFrame before FinalizeFrame? (nah)
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	CallServerRPC
	//
	//	Tells the driver to call the Server RPC to send InputCmds to the server. UNetworkPredictionComponent::CallServerRPC is the default
	//	implementation and shouldn't need to be defined by the user.
	// -----------------------------------------------------------------------------------------------------------------------------------
		
	static void CallServerRPC(DriverType* Driver)
	{
		CallServerRPCMemberFunc(Driver);
	}
	
	struct CCallServerRPCMemberFuncable
	{
		template <typename InDriverType>
		auto Requires(InDriverType* Driver) -> decltype(Driver->CallServerRPC());
	};

	static constexpr bool HasCallServerRPC = TModels<CCallServerRPCMemberFuncable, DriverType>::Value;

	template<bool HasFunc=HasCallServerRPC>
	static typename TEnableIf<HasFunc>::Type CallServerRPCMemberFunc(DriverType* Driver)
	{
		npCheckSlow(Driver);
		Driver->CallServerRPC();
	}

	template<bool HasFunc=HasCallServerRPC>
	static typename TEnableIf<!HasFunc>::Type CallServerRPCMemberFunc(DriverType* Driver)
	{
		npCheckf(false, TEXT("No CallServerRPC implementation found. Implement DriverType::CallServerRPC or ModelDef::CallServerRPC"));
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	Dispatch Cues
	//
	//	Forwards call to CueDispatche's DispatchCueRecord which will invoke the queued HandleCue events to the driver.
	// -----------------------------------------------------------------------------------------------------------------------------------
	
	template<typename InDriverType=DriverType>
	static void DispatchCues(TNetSimCueDispatcher<ModelDef>* CueDispatcher, InDriverType* Driver, int32 SimFrame, int32 SimTimeMS, const int32 FixedStepMS)
	{
		npCheckSlow(Driver);
		CueDispatcher-> template DispatchCueRecord<InDriverType>(*Driver, SimFrame, SimTimeMS, FixedStepMS);
	}

	static void DispatchCues(TNetSimCueDispatcher<ModelDef>* CueDispatcher, void* Driver, int32 SimFrame, int32 SimTimeMS, const int32 FixedStepMS)
	{
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	ShouldReconcile
	//
	//	Determines if Sync/Aux state have diverged enough to force a correction.
	//	The default implementation will call ShouldReconcile on the state itself: bool FMySyncState::ShouldReconcile(const FMySyncState& Authority) const.
	// -----------------------------------------------------------------------------------------------------------------------------------
	static bool ShouldReconcile(const TSyncAuxPair<StateTypes>& Predicted, const TSyncAuxPair<StateTypes>& Authority)
	{
		return ShouldReconcile(Predicted.Sync, Authority.Sync) || ShouldReconcile(Predicted.Aux, Authority.Aux);
	}

	template<typename StateType>
	static bool ShouldReconcile(const StateType* Predicted, const StateType* Authority)
	{
		return Predicted->ShouldReconcile(*Authority);
	}
	
	static bool ShouldReconcile(const void* Predicted, const void* Authority) { return false; }

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	Interpolate
	//
	//	Blend between From/To set of Sync/Aux states
	// -----------------------------------------------------------------------------------------------------------------------------------
	static void Interpolate(const TSyncAuxPair<StateTypes>& From, const TSyncAuxPair<StateTypes>& To, const float PCT, SyncType* SyncOut, AuxType* AuxOut)
	{
		InterpolateState(From.Sync, To.Sync, PCT, SyncOut);
		InterpolateState(From.Aux, To.Aux, PCT, AuxOut);
	}

	template<typename StateType>
	static void InterpolateState(const StateType* From, const StateType* To, const float PCT, StateType* Out)
	{
		Out->Interpolate(From, To, PCT);
	}
	
	static void InterpolateState(const void* From, const void* To, const float PCT, void* Out)
	{
	}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	Show/Hide ForInterpolation
	//
	//	Interpolated sims are initially hidden until there are two valid states to interpolate between	
	// -----------------------------------------------------------------------------------------------------------------------------------

	static void SetHiddenForInterpolation(DriverType* Driver, bool bHide)
	{
		CallSetHiddenForInterpolation(Driver, bHide);
	}

	struct CSetHiddenForInterpolationFuncable
	{
		template <typename InDriverType>
		auto Requires(InDriverType* Driver, bool bHidden) -> decltype(Driver->SetHiddenForInterpolation(bHidden));
	};
	
	static constexpr bool HasSetHiddenForInterpolation = TModels<CSetHiddenForInterpolationFuncable, DriverType>::Value;

	template<bool HasFunc=HasSetHiddenForInterpolation>
	static typename TEnableIf<HasFunc>::Type CallSetHiddenForInterpolation(DriverType* Driver, bool bHide)
	{
		npCheckSlow(Driver);
		Driver->SetHiddenForInterpolation(bHide);
	}

	template<bool HasFunc=HasSetHiddenForInterpolation>
	static typename TEnableIf<!HasFunc>::Type CallSetHiddenForInterpolation(DriverType* Driver, bool bHide)
	{
		CallSetHiddenForInterpolationFallback(Driver, bHide);
	}

	static void CallSetHiddenForInterpolationFallback(AActor* Driver, bool bHide)
	{
		Driver->SetHidden(bHide);
	}

	static void CallSetHiddenForInterpolationFallback(UActorComponent* Driver, bool bHide)
	{
		Driver->GetOwner()->SetHidden(bHide);
	}	

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	Physics
	//
	//	Note: a ModelDef can support physics but if an instance is running in independent ticking mode, physics reconcile/net serialization will
	//	effectively be off. (It doesn't make sense to reconcile physics state that is ticked out of step from the independent simulation tick).
	//	So below will only be "on" when in fixed tick mode.
	//
	//	UPrimitiveComponent::SyncComponentToRBPhysics
	// -----------------------------------------------------------------------------------------------------------------------------------

	// ------------------------------------------
	//	GetPhysicsPrimitiveComponent: returns UPrimitiveComponent to forward to PhysicsState function calls.
	//	(it would be nice to not require this but the interpolatin path currently requires calls to kinematic bodies to go through the component, not PhysicsActorHandle)
	// ------------------------------------------
	
	struct CGetPhysicsPrimitiveComponentMemberFuncable
	{
		template <typename InDriverType>
		auto Requires(InDriverType* Driver) -> decltype(Driver->GetPhysicsPrimitiveComponent());
	};

	static constexpr bool HasGetPrimitiveComp = TModels<CGetPhysicsPrimitiveComponentMemberFuncable, DriverType>::Value;
	
	template<bool HasFunc=HasGetPrimitiveComp>
	static typename TEnableIf<HasFunc, UPrimitiveComponent*>::Type GetPhysicsPrimitiveComponent(DriverType* Driver)
	{
		npCheckSlow(Driver);
		return Driver->GetPhysicsPrimitiveComponent();
	}
	
	template<bool HasFunc=HasGetPrimitiveComp>
	static typename TEnableIf<!HasFunc, UPrimitiveComponent*>::Type GetPhysicsPrimitiveComponent(DriverType* Driver)
	{
		return SafeCastDriverToPrimitiveComponent(Driver);
	}	

	template<typename InDriverType>
	static UPrimitiveComponent* SafeCastDriverToPrimitiveComponent(InDriverType* Driver)
	{
		//	If you are landing here with a nullptr Driver:
		//	You are using interpolated physics, we need to get a UPrimitiveComponent from your ModelDef Driver. 3 choices:
		//		-A) Your ModelDef::Driver could be a UPrimitiveComponent (or inherit from one). Nothing else required.
		//		-B) Your ModelDef::Driver class can implement a UPrimitiveComponent* GetPhysicsPrimitiveComponent() function and return it. Good for "UpdatedPrimitive pattern".
		//		-C) You can specialize FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent to do something else. Good for non intrusive case.
		npCheckf(false, TEXT("Could not obtain UPrimitiveComponent from driver. See notes above in NetworkPredictionDriver.h"));
		return nullptr;
	}

	static UPrimitiveComponent* SafeCastDriverToPrimitiveComponent(UPrimitiveComponent* Driver)
	{
		return Driver;
	}

	// ------------------------------------------
	//	GetPhysicsBodyInstance
	// ------------------------------------------

	struct CGetPhysicsBodyInstanceMemberFuncable
	{
		template <typename InDriverType>
		auto Requires(InDriverType* Driver) -> decltype(Driver->GetPhysicsBodyInstance());
	};

	static constexpr bool HasGetBodyInstance = TModels<CGetPhysicsBodyInstanceMemberFuncable, DriverType>::Value;

	template<bool HasFunc=HasGetBodyInstance>
	static typename TEnableIf<HasFunc, FBodyInstance*>::Type GetPhysicsBodyInstance(DriverType* Driver)
	{
		npCheckSlow(Driver);
		return Driver->GetPhysicsBodyInstance();
	}

	template<bool HasFunc=HasGetBodyInstance>
	static typename TEnableIf<!HasFunc, FBodyInstance*>::Type GetPhysicsBodyInstance(DriverType* Driver)
	{
		// Non explicit version: get the primitive component and get the body instance off of that
		UPrimitiveComponent* PrimitiveComponent = FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver);
		npCheckSlow(PrimitiveComponent);
		return PrimitiveComponent->GetBodyInstance();
	}

	// ------------------------------------------
	//	ShouldReconcilePhysics
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable, bool>::Type ShouldReconcilePhysics(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, TConditionalState<PhysicsState>& RecvState)
	{
		return PhysicsState::ShouldReconcile(PhysicsFrame, RewindData, FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver), RecvState);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable, bool>::Type ShouldReconcilePhysics(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, TConditionalState<PhysicsState>& RecvState)
 	{
		return false;
	}

	// ------------------------------------------
	//	PerformPhysicsRollback
	// ------------------------------------------

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type PerformPhysicsRollback(DriverType* Driver, TConditionalState<PhysicsState>& RecvState)
	{
		PhysicsState::PerformRollback(FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver), RecvState);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type PerformPhysicsRollback(DriverType* Driver, TConditionalState<PhysicsState>& RecvState) {}

	// ------------------------------------------
	//	PostPhysicsResimulate
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type PostPhysicsResimulate(DriverType* Driver)
	{
		PhysicsState::PostResimulate(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver), FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type PostPhysicsResimulate(DriverType* Driver)
	{
		npCheck(false); // Not expected to be called in non physics based sims
	}

	// ------------------------------------------
	//	MarshalPhysicsToComponent
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type MarshalPhysicsToComponent(DriverType* Driver)
	{
		npCheckSlow(Driver);
		PhysicsState::MarshalPhysicsToComponent(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver), FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type MarshalPhysicsToComponent(DriverType* Driver) { }

	// ------------------------------------------
	//	InterpolatePhysics
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type InterpolatePhysics(TConditionalState<PhysicsState>& FromState, TConditionalState<PhysicsState>& ToState, float PCT, TConditionalState<PhysicsState>& OutState)
	{
		PhysicsState::Interpolate(FromState, ToState, PCT, OutState);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type InterpolatePhysics(TConditionalState<PhysicsState>& FromState, TConditionalState<PhysicsState>& ToState, float PCT, TConditionalState<PhysicsState>& OutState) {}

	// ------------------------------------------
	//	FinalizeInterpolatedPhysics
	//	Only called when needing to push NP interpolated physics state to the driver.
	//	E.g, the physics sim is not running
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type FinalizeInterpolatedPhysics(DriverType* Driver, TConditionalState<PhysicsState>& Physics)
	{
		PhysicsState::FinalizeInterpolatedPhysics(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver), Physics);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type FinalizeInterpolatedPhysics(DriverType* Driver, TConditionalState<PhysicsState>& Physics) {}

	// ------------------------------------------
	//	BeginInterpolatedPhysics - turn off physics simulation so that NP can interpolate the physics state
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type BeginInterpolatedPhysics(DriverType* Driver)
	{
		PhysicsState::BeginInterpolation(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type BeginInterpolatedPhysics(DriverType* Driver) {}

	// ------------------------------------------
	//	EndInterpolatedPhysics - turn on physics simulation
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type EndInterpolatedPhysics(DriverType* Driver)
	{
		PhysicsState::EndInterpolation(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type EndInterpolatedPhysics(DriverType* Driver) {}

	// ------------------------------------------
	//	PhysicsNetSend
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type PhysicsNetSend(const FNetSerializeParams& P, DriverType* Driver)
	{
		PhysicsState::NetSend(P, FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type PhysicsNetSend(const FNetSerializeParams& P, DriverType* Driver) {}
	
	// ------------------------------------------
	//	PhysicsNetRecv
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type PhysicsNetRecv(const FNetSerializeParams& P, TConditionalState<PhysicsState>& RecvState)
	{
		PhysicsState::NetRecv(P, RecvState);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type PhysicsNetRecv(const FNetSerializeParams& P, TConditionalState<PhysicsState>& RecvState) {}

	// ------------------------------------------
	//	PhysicsStateIsConsistent
	//	Only used in debugging - checks if UPrimitiveComponent and FPhysicsBody state are in sync with each other
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable, bool>::Type PhysicsStateIsConsistent(DriverType* Driver)
	{
		return PhysicsState::StateIsConsistent(FNetworkPredictionDriver<ModelDef>::GetPhysicsPrimitiveComponent(Driver),
												FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver));
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable, bool>::Type PhysicsStateIsConsistent(DriverType* Driver)
	{
		npCheck(false);
		return false;
	}

	// ------------------------------------------
	//	LogPhysicsState
	// ------------------------------------------
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type LogPhysicsState(TConditionalState<PhysicsState>& RecvState, FOutputDevice& Ar=*GLog)
	{
		TAnsiStringBuilder<256> Builder;
		PhysicsState::ToString(RecvState, Builder);

		Ar.Log(StringCast<TCHAR>(Builder.ToString()).Get());
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type LogPhysicsState(TConditionalState<PhysicsState>& RecvState, FOutputDevice& Ar=*GLog) { }

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type LogPhysicsState(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, FOutputDevice& Ar=*GLog)
	{		
		TAnsiStringBuilder<256> Builder;
		PhysicsState::ToString(PhysicsFrame, RewindData, FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver), Builder);

		Ar.Log(StringCast<TCHAR>(Builder.ToString()).Get());
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type LogPhysicsState(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, FOutputDevice& Ar=*GLog) {}

	// ------------------------------------------
	//	TracePhysicsState
	// ------------------------------------------
	
	// Received physics state
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type TracePhysicsStateRecv(const TConditionalState<PhysicsState>& RecvState, FAnsiStringBuilderBase& Builder)
	{
		PhysicsState::ToString(RecvState.Get(), Builder);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type TracePhysicsStateRecv(const TConditionalState<PhysicsState>& RecvState, FAnsiStringBuilderBase& Builder) { }

	// Current latest physics state
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type TracePhysicsState(DriverType* Driver, FAnsiStringBuilderBase& Builder)
	{
		PhysicsState::ToString(FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver), Builder);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type TracePhysicsState(DriverType* Driver, FAnsiStringBuilderBase& Builder) {}

	// Local historic physics state
	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<bEnable>::Type TracePhysicsState(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, FAnsiStringBuilderBase& Builder)
	{
		PhysicsState::ToString(PhysicsFrame, RewindData, FNetworkPredictionDriver<ModelDef>::GetPhysicsBodyInstance(Driver), Builder);
	}

	template<bool bEnable=HasPhysics()>
	static typename TEnableIf<!bEnable>::Type TracePhysicsState(int32 PhysicsFrame, Chaos::FRewindData* RewindData, DriverType* Driver, FAnsiStringBuilderBase& Builder) {}

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	ToString
	//
	//	Utility functions for turning user state into strings. User states can should define ToString(FAnsiStringBuilderBase&) and append
	//	*ANSI* strings to the builder. (E.g, don't use the TEXT macro).	
	//
	//	Ansi was chosen because for Trace purposes:
	//	We want tracing user state strings to be as fast and efficient as possible for Insights so that it can be enabled during development.
	//
	//	Logging is primary intended as a last resort for printf style debugging. The system should not output UserStates to the log in 
	//	normal circumstances. (Opting in via cvars or verbose logging categories would be ok).
	//
	//	If you really need to return the string's produced here, use an FStringOutputDevice. Otherwise they will be stacked allocated
	// -----------------------------------------------------------------------------------------------------------------------------------
	static void LogUserStates(const TNetworkPredictionState<StateTypes>& UserStates, FOutputDevice& Ar=*GLog)
	{
		TAnsiStringBuilder<512> Builder;

		Builder << '\n';
		ToString((InputType*)UserStates.Cmd, Builder);
		Builder << '\n';		
		ToString((SyncType*)UserStates.Sync, Builder);
		Builder << '\n';
		ToString((AuxType*)UserStates.Aux, Builder);

		Ar.Log(StringCast<TCHAR>(Builder.ToString()).Get());
	}

	template<typename StateType>
	static void LogUserState(const StateType* State, FOutputDevice& Ar=*GLog)
	{
		TAnsiStringBuilder<256> Builder;

		Builder << '\n';
		ToString(State, Builder);

		Ar.Log(StringCast<TCHAR>(Builder.ToString()).Get());
	}

	template<typename StateType>
	static void TraceUserStateString(const StateType* State, FAnsiStringBuilderBase& Builder)
	{
		ToString(State, Builder);
	}

	// Eventually: TraceUserStateBinary for Insights -> Editor debugging

	template<typename StateType>
	static void ToString(const StateType* State, FAnsiStringBuilderBase& Builder)
	{
		State->ToString(Builder);
	}

	static void ToString(const void* State, FAnsiStringBuilderBase& Builder) { }

	// -----------------------------------------------------------------------------------------------------------------------------------
	//	NetSerialize
	//
	//	Forwards NetSerialize call to the user type.
	// -----------------------------------------------------------------------------------------------------------------------------------
	template<typename StateType>
	static void NetSerialize(TConditionalState<StateType>& State, const FNetSerializeParams& P)
	{
		State->NetSerialize(P);
	}

	static void NetSerialize(TConditionalState<void>& State, const FNetSerializeParams& P) { }
};


// This is the actual template to specialize when wanting to override functions
template<typename ModelDef>
struct FNetworkPredictionDriver : FNetworkPredictionDriverBase<ModelDef>
{

};