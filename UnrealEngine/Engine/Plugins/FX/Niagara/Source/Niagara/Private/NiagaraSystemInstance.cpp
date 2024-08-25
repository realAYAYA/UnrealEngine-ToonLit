// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemInstance.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "Stateless/NiagaraStatelessEmitterInstance.h"
#include "NiagaraSystemGpuComputeProxy.h"
#include "NiagaraCommon.h"
#include "NiagaraComputeExecutionContext.h"
#include "NiagaraDataInterface.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraEmitterInstanceImpl.h"
#include "NiagaraGPUSystemTick.h"
#include "NiagaraStats.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraSystemSimulation.h"
#include "NiagaraWorldManager.h"
#include "NiagaraComponent.h"
#include "NiagaraSimCache.h"
#include "NiagaraGpuComputeDebug.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraCrashReporterHandler.h"
#include "NiagaraSystemImpl.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Templates/AlignmentTemplates.h"

DECLARE_CYCLE_STAT(TEXT("System Activate [GT]"), STAT_NiagaraSystemActivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Deactivate [GT]"), STAT_NiagaraSystemDeactivate, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Complete [GT]"), STAT_NiagaraSystemComplete, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Reset [GT]"), STAT_NiagaraSystemReset, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Reinit [GT]"), STAT_NiagaraSystemReinit, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Init Emitters [GT]"), STAT_NiagaraSystemInitEmitters, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Advance Simulation [GT] "), STAT_NiagaraSystemAdvanceSim, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System SetSolo[GT] "), STAT_NiagaraSystemSetSolo, STATGROUP_Niagara);

//High level stats for system instance ticks.
DECLARE_CYCLE_STAT(TEXT("System Instance Tick (Component) [GT]"), STAT_NiagaraSystemInst_ComponentTickGT, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Instance Tick [GT]"), STAT_NiagaraSystemInst_TickGT, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Instance Tick [CNC]"), STAT_NiagaraSystemInst_TickCNC, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Instance Finalize [GT]"), STAT_NiagaraSystemInst_FinalizeGT, STATGROUP_Niagara);
DECLARE_CYCLE_STAT(TEXT("System Instance WaitForAsyncTick [GT]"), STAT_NiagaraSystemWaitForAsyncTick, STATGROUP_Niagara);

DECLARE_CYCLE_STAT(TEXT("InitGPUSystemTick"), STAT_NiagaraInitGPUSystemTick, STATGROUP_Niagara);

static float GWaitForAsyncStallWarnThresholdMS = 0.0f;
static FAutoConsoleVariableRef CVarWaitForAsyncStallWarnThresholdMS(
	TEXT("fx.WaitForAsyncStallWarnThresholdMS"),
	GWaitForAsyncStallWarnThresholdMS,
	TEXT("If we stall in WaitForAsync for longer than this threshold then we emit a stall warning message."),
	ECVF_Default
);

/** Safety time to allow for the LastRenderTime coming back from the RT. This is overkill but that's ok.*/
float GLastRenderTimeSafetyBias = 0.1f;
static FAutoConsoleVariableRef CVarLastRenderTimeSafetyBias(
	TEXT("fx.LastRenderTimeSafetyBias"),
	GLastRenderTimeSafetyBias,
	TEXT("The time to bias the LastRenderTime value to allow for the delay from it being written by the RT."),
	ECVF_Default
);

static int GNiagaraForceLastTickGroup = 0;
static FAutoConsoleVariableRef CVarNiagaraForceLastTickGroup(
	TEXT("fx.Niagara.ForceLastTickGroup"),
	GNiagaraForceLastTickGroup,
	TEXT("Force Niagara ticks to be in the last tick group, this mirrors old behavour and can be useful to test for async overlapping issues."),
	ECVF_Default
);

static float GNiagaraEmitterBoundsDynamicSnapValue = 0.0f;
static FAutoConsoleVariableRef CVarNiagaraEmitterBoundsDynamicSnapValue(
	TEXT("fx.Niagara.EmitterBounds.DynamicSnapValue"),
	GNiagaraEmitterBoundsDynamicSnapValue,
	TEXT("The value used to snap (round up) dynamic bounds calculations to.")
	TEXT("For example, a snap of 128 and a value of 1 would result in 128"),
	ECVF_Default
);

static float GNiagaraEmitterBoundsDynamicExpandMultiplier = 1.1f;
static FAutoConsoleVariableRef CVarNiagaraEmitterBoundsDynamicExpandMultiplier(
	TEXT("fx.Niagara.EmitterBounds.DynamicExpandMultiplier"),
	GNiagaraEmitterBoundsDynamicExpandMultiplier,
	TEXT("Multiplier used on dynamic bounds gathering, i.e. 1 means no change, 1.1 means increase by 10%.\n")
	TEXT("This value is applied after we calculate any dynamic bounds snapping."),
	ECVF_Default
);

static float GNiagaraEmitterBoundsFixedExpandMultiplier = 1.0f;
static FAutoConsoleVariableRef CVarNiagaraEmitterBoundsFixedExpandMultiplier(
	TEXT("fx.Niagara.EmitterBounds.FixedExpandMultiplier"),
	GNiagaraEmitterBoundsFixedExpandMultiplier,
	TEXT("Multiplier used on fixed bounds gathering, i.e. 1 means no change, 1.1 means increase by 10%."),
	ECVF_Default
);

static int GNiagaraAllowDeferredReset = 1;
static FAutoConsoleVariableRef CVarNiagaraAllowDeferredReset(
	TEXT("fx.Niagara.AllowDeferredReset"),
	GNiagaraAllowDeferredReset,
	TEXT("If we are running async work when a reset is requested we will instead queue for the finalize to perform, this avoid stalling the GameThread."),
	ECVF_Default
);

FNiagaraSystemInstance::FNiagaraSystemInstance(UWorld& InWorld, UNiagaraSystem& InSystem, FNiagaraUserRedirectionParameterStore* InOverrideParameters,
                                               USceneComponent* InAttachComponent, ENiagaraTickBehavior InTickBehavior, bool bInPooled)
	: SystemInstanceIndex(INDEX_NONE)
	  , SignificanceIndex(INDEX_NONE)
	  , World(&InWorld)
	  , System(&InSystem)
	  , OverrideParameters(InOverrideParameters)
	  , AttachComponent(InAttachComponent)
	  , TickBehavior(InTickBehavior)
	  , Age(0.0f)
	  , LastRenderTime(0.0f)
	  , TickCount(0)
	  , RandomSeed(0)
	  , RandomSeedOffset(0)
	  , LODDistance(0.0f)
	  , MaxLODDistance(FLT_MAX)
#if NIAGARA_SYSTEM_CAPTURE
	  , bWasSoloPriorToCaptureRequest(false)
#endif
	  , GlobalParameters{}
	  , SystemParameters{}
	  , CurrentFrameIndex(1)
	  , ParametersValid(false)
	  , bSolo(false)
	  , bForceSolo(false)
	  , bNotifyOnCompletion(false), bHasGPUEmitters(false), bDataInterfacesHaveTickPrereqs(false), bDataInterfacesHaveTickPostreqs(false)
	  , bDataInterfacesInitialized(false)
	  , bAlreadyBound(false)
	  , bLODDistanceIsValid(false)
	  , bLODDistanceIsOverridden(false)
	  , bPooled(bInPooled)
#if WITH_EDITOR
	  , bNeedsUIResync(false)
#endif
	  , CachedDeltaSeconds(0.0f)
	  , TimeSinceLastForceUpdateTransform(0.0f)
	  , FixedBounds_GT(ForceInit)
	  , FixedBounds_CNC(ForceInit)
	  , LocalBounds(ForceInit)
	  , RequestedExecutionState(ENiagaraExecutionState::Complete)
	  , ActualExecutionState(ENiagaraExecutionState::Complete)
	  , FeatureLevel(GMaxRHIFeatureLevel)
{
	static TAtomic<uint64> IDCounter(1);
	ID = IDCounter.IncrementExchange();

	LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	if (InAttachComponent)
	{
		InstanceParameters.SetOwner(InAttachComponent);
	}

	ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World);
	FeatureLevel = World->GetFeatureLevel();

	// In some cases the system may have already stated that you should ignore dependencies and tick as early as possible.
	if (!InSystem.bRequireCurrentFrameData)
	{
		TickBehavior = ENiagaraTickBehavior::ForceTickFirst;
	}
}


void FNiagaraSystemInstance::SetEmitterEnable(FName EmitterName, bool bNewEnableState)
{
	for (const TSharedRef<FNiagaraEmitterInstance, ESPMode::ThreadSafe>& Emitter : Emitters)
	{
		if (Emitter->GetEmitterHandle().GetName() == EmitterName)
		{
			Emitter->SetEmitterEnable(bNewEnableState);
			return;
		}
	}

	// Failed to find emitter
	UE_LOG(LogNiagara, Warning, TEXT("SetEmitterEnable: Failed to find Emitter(%s) System(%s) Component(%s)"), *EmitterName.ToString(), *GetNameSafe(System), *GetFullNameSafe(AttachComponent.Get()));
}

void FNiagaraSystemInstance::Init(bool bInForceSolo)
{
	// We warn if async is not complete here as we should never wait
	WaitForConcurrentTickAndFinalize(true);

	bForceSolo = bInForceSolo;
	ActualExecutionState = ENiagaraExecutionState::Inactive;
	RequestedExecutionState = ENiagaraExecutionState::Inactive;
	bAlreadyBound = false;

	//InstanceParameters = GetSystem()->GetInstanceParameters();
	// In order to get user data interface parameters in the component to work properly,
	// we need to bind here, otherwise the instances when we init data interfaces during reset will potentially
	// be the defaults (i.e. null) for things like static mesh data interfaces.
	Reset(EResetMode::ReInit);

#if WITH_EDITORONLY_DATA
	InstanceParameters.DebugName = *FString::Printf(TEXT("SystemInstance %p"), this);
#endif
#if WITH_EDITOR
	OnInitializedDelegate.Broadcast();
#endif
}

void FNiagaraSystemInstance::SetRequestedExecutionState(ENiagaraExecutionState InState)
{
	//Once in disabled state we can never get out except on Reinit.
	if (RequestedExecutionState != InState && RequestedExecutionState != ENiagaraExecutionState::Disabled)
	{
		/*const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Component \"%s\" System \"%s\" requested change state: %s to %s, actual %s"), *GetComponent()->GetName(), *GetSystem()->GetName(), *EnumPtr->GetNameStringByValue((int64)RequestedExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState), *EnumPtr->GetNameStringByValue((int64)ActualExecutionState));
		*/
		if (InState == ENiagaraExecutionState::Disabled)
		{
			//Really move to disabled straight away.
			ActualExecutionState = ENiagaraExecutionState::Disabled;
			Cleanup();
		}
		RequestedExecutionState = InState;
	}
}

void FNiagaraSystemInstance::SetActualExecutionState(ENiagaraExecutionState InState)
{

	//Once in disabled state we can never get out except on Reinit.
	if (ActualExecutionState != InState && ActualExecutionState != ENiagaraExecutionState::Disabled)
	{
		/*const UEnum* EnumPtr = FNiagaraTypeDefinition::GetExecutionStateEnum();
		UE_LOG(LogNiagara, Log, TEXT("Component \"%s\" System \"%s\" actual change state: %s to %s"), *GetComponent()->GetName(), *GetSystem()->GetName(), *EnumPtr->GetNameStringByValue((int64)ActualExecutionState),
			*EnumPtr->GetNameStringByValue((int64)InState));
		*/
		ActualExecutionState = InState;

		if (ActualExecutionState == ENiagaraExecutionState::Active)
		{
			// We only need to notify completion once after each successful active.
			// Here's when we know that we just became active.
			bNotifyOnCompletion = true;

			// We may also end up calling HandleCompletion on each emitter.
			// This may happen *before* we've successfully pulled data off of a
			// simulation run. This means that we need to synchronize the execution
			// states upon activation.
			for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
			{
				//-TODO:Stateless: Avoid Casting
				FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
				if (Emitter == nullptr)
				{
					continue;
				}
				Emitter->SetExecutionState(ENiagaraExecutionState::Active);
			}
		}
	}
}

void FNiagaraSystemInstance::Dump()const
{
	if (GetSystemSimulation())
	{
		GetSystemSimulation()->DumpInstance(this);
		for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
		{
			//-TODO:Stateless: Avoid Casting
			FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
			if (Emitter == nullptr)
			{
				continue;
			}
			Emitter->Dump();
		}
	}
}

void FNiagaraSystemInstance::DumpTickInfo(FOutputDevice& Ar)
{
	static const UEnum* TickingGroupEnum = FindObjectChecked<UEnum>(nullptr, TEXT("/Script/Engine.ETickingGroup"));

	FString PrereqInfo;
	UActorComponent* PrereqComponent = GetPrereqComponent();
	if (PrereqComponent != nullptr)
	{
		ETickingGroup PrereqTG = FMath::Max(PrereqComponent->PrimaryComponentTick.TickGroup, PrereqComponent->PrimaryComponentTick.EndTickGroup);
		PrereqInfo.Appendf(TEXT(" PreReq(%s = %s)"), *PrereqComponent->GetFullName(), *TickingGroupEnum->GetNameStringByIndex(PrereqTG));
	}

	if (bDataInterfacesHaveTickPrereqs || bDataInterfacesHaveTickPostreqs)
	{
		for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
		{
			if (UNiagaraDataInterface* Interface = Pair.Key.Get())
			{
				ETickingGroup PrereqTG = Interface->CalculateTickGroup(&DataInterfaceInstanceData[Pair.Value]);
				ETickingGroup PostreqTG = Interface->CalculateFinalTickGroup(&DataInterfaceInstanceData[Pair.Value]);
				PrereqInfo.Appendf(TEXT(" DataInterface(%s = %s - %s)"), *Interface->GetFullName(), *TickingGroupEnum->GetNameStringByIndex(PrereqTG), *TickingGroupEnum->GetNameStringByIndex(PostreqTG));
			}
		}
	}

	Ar.Logf(TEXT("\t\t\tInstance%s"), *PrereqInfo);
}

#if NIAGARA_SYSTEM_CAPTURE
bool FNiagaraSystemInstance::RequestCapture(const FGuid& RequestId)
{
	if (IsComplete() || CurrentCapture.IsValid())
	{
		return false;
	}

	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize();
	if (IsComplete())
	{
		return false;
	}

	UE_LOG(LogNiagara, Warning, TEXT("Capture requested!"));

	bWasSoloPriorToCaptureRequest = bSolo;
	SetSolo(true);

	// Go ahead and populate the shared array so that we don't have to do this on the game thread and potentially race.
	TSharedRef<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe> TempCaptureHolder =
		MakeShared<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe>();

	TempCaptureHolder->Add(MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(NAME_None, ENiagaraScriptUsage::SystemSpawnScript, FGuid()));
	TempCaptureHolder->Add(MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(NAME_None, ENiagaraScriptUsage::SystemUpdateScript, FGuid()));

	for (const FNiagaraEmitterHandle& Handle : GetSystem()->GetEmitterHandles())
	{
		TArray<UNiagaraScript*> Scripts;
		FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData();
		if (EmitterData && Handle.GetIsEnabled())
		{
			EmitterData->GetScripts(Scripts, false);

			for (UNiagaraScript* Script : Scripts)
			{
				if (Script->IsGPUScript(Script->Usage) && EmitterData->SimTarget == ENiagaraSimTarget::CPUSim)
					continue;
				TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe> DebugInfoPtr = MakeShared<FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>(Handle.GetIdName(), Script->GetUsage(), Script->GetUsageId());
				DebugInfoPtr->bWritten = false;

				TempCaptureHolder->Add(DebugInfoPtr);
			}
		}
	}
	CapturedFrames.Add(RequestId, TempCaptureHolder);
	CurrentCapture = TempCaptureHolder;
	CurrentCaptureGuid = MakeShared<FGuid, ESPMode::ThreadSafe>(RequestId);
	return true;
}

void FNiagaraSystemInstance::FinishCapture()
{
	if (!CurrentCapture.IsValid())
	{
		return;
	}

	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize();

	SetSolo(bWasSoloPriorToCaptureRequest);
	CurrentCapture.Reset();
	CurrentCaptureGuid.Reset();
}

bool FNiagaraSystemInstance::QueryCaptureResults(const FGuid& RequestId, TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>& OutCaptureResults)
{
	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize();

	if (CurrentCaptureGuid.IsValid() && RequestId == *CurrentCaptureGuid.Get())
	{
		return false;
	}

	const TSharedPtr<TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>, ESPMode::ThreadSafe>* FoundEntry = CapturedFrames.Find(RequestId);
	if (FoundEntry != nullptr)
	{
		TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>* Array = FoundEntry->Get();
		OutCaptureResults.SetNum(Array->Num());

		bool bWaitForGPU = false;
		{
			for (int32 i = 0; i < FoundEntry->Get()->Num(); i++)
			{
				if ((*Array)[i]->bWaitForGPU && (*Array)[i]->bWritten == false)
				{
					bWaitForGPU = true;
					break;
				}
			}

			if (bWaitForGPU)
			{
				for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
				{
					//-TODO:Stateless: Avoid Casting
					FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
					if (Emitter == nullptr)
					{
						continue;
					}
					Emitter->WaitForDebugInfo();
				}
				return false;
			}
		}


		for (int32 i = 0; i < FoundEntry->Get()->Num(); i++)
		{
			OutCaptureResults[i] = (*Array)[i];
		}
		CapturedFrames.Remove(RequestId);
		return true;
	}

	return false;
}

TArray<TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>>* FNiagaraSystemInstance::GetActiveCaptureResults()
{
	return CurrentCapture.Get();
}

TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe> FNiagaraSystemInstance::GetActiveCaptureWrite(const FName& InHandleName, ENiagaraScriptUsage InUsage, const FGuid& InUsageId)
{
	if (CurrentCapture.IsValid())
	{
		TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>* FoundEntry = CurrentCapture->FindByPredicate([&](const TSharedPtr<struct FNiagaraScriptDebuggerInfo, ESPMode::ThreadSafe>& Entry)
		{
			return Entry->HandleName == InHandleName && UNiagaraScript::IsEquivalentUsage(Entry->Usage, InUsage) && Entry->UsageId == InUsageId;
		});

		if (FoundEntry != nullptr)
		{
			return *FoundEntry;
		}
	}
	return nullptr;
}

bool FNiagaraSystemInstance::ShouldCaptureThisFrame() const
{
	return CurrentCapture.IsValid();
}
#endif//NIAGARA_SYSTEM_CAPTURE

void FNiagaraSystemInstance::SetSolo(bool bInSolo)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemSetSolo);
	if (bSolo == bInSolo)
	{
		return;
	}

	// Wait for any async operations
	WaitForConcurrentTickDoNotFinalize();

	// We only need to transfer the instance into a new simulation if this one is within a simulation
	if ( SystemSimulation.IsValid() )
	{
		if (bInSolo)
		{
			TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> NewSoloSim = MakeShared<FNiagaraSystemSimulation, ESPMode::ThreadSafe>();
			NewSoloSim->Init(System, World, true, TG_MAX);

			NewSoloSim->TransferInstance(this);
		}
		else
		{
			const ETickingGroup TickGroup = CalculateTickGroup();
			TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> NewSim = GetWorldManager()->GetSystemSimulation(TickGroup, System);

			NewSim->TransferInstance(this);
		}
	}

	bSolo = bInSolo;

	// Execute any pending finalize
	if (FinalizeRef.IsPending())
	{
		FinalizeTick_GameThread();
	}
}

void FNiagaraSystemInstance::SetGpuComputeDebug(bool bEnableDebug)
{
#if NIAGARA_COMPUTEDEBUG_ENABLED
	if (ComputeDispatchInterface == nullptr || !::IsValid(System))
	{
		return;
	}

	if (bEnableDebug)
	{
		FString SystemName = System->GetName();
		if (USceneComponent* Owner = AttachComponent.Get())
		{
			SystemName.Append(TEXT("/"));
			if (AActor* Actor = Owner->GetTypedOuter<AActor>())
			{
				SystemName.Append(GetNameSafe(Actor));
			}
			else
			{
				SystemName.Append(GetNameSafe(Owner));
			}
		}

		ENQUEUE_RENDER_COMMAND(NiagaraAddGPUSystemDebug)
		(
			[RT_ComputeDispatchInterface=ComputeDispatchInterface, RT_InstanceID=GetId(), RT_SystemName=SystemName](FRHICommandListImmediate& RHICmdList)
			{
				if (FNiagaraGpuComputeDebug* GpuComputeDebug = RT_ComputeDispatchInterface->GetGpuComputeDebugPrivate())
				{
					GpuComputeDebug->AddSystemInstance(RT_InstanceID, RT_SystemName);
				}
			}
		);
	}
	else
	{
		ENQUEUE_RENDER_COMMAND(NiagaraRemoveGPUSystemDebug)
		(
			[RT_ComputeDispatchInterface=ComputeDispatchInterface, RT_InstanceID=GetId()](FRHICommandListImmediate& RHICmdList)
			{
				if (FNiagaraGpuComputeDebug* GpuComputeDebug = RT_ComputeDispatchInterface->GetGpuComputeDebugPrivate())
				{
					GpuComputeDebug->RemoveSystemInstance(RT_InstanceID);
				}
			}
		);
	}
#endif
}

void FNiagaraSystemInstance::SetWarmupSettings(int32 InWarmupTickCount, float InWarmupTickDelta)
{
	WarmupTickCount = InWarmupTickCount;
	WarmupTickDelta = InWarmupTickDelta;
}

UActorComponent* FNiagaraSystemInstance::GetPrereqComponent() const
{
	UActorComponent* PrereqComponent = AttachComponent.Get();

	// This is to maintain legacy behavior (and perf benefit) of ticking in PrePhysics with unattached UNiagaraComponents that have no DI prereqs
	// NOTE: This means that the system likely ticks with frame-behind transform if the component is moved, but likely doesn't manifest as an issue with local-space emitters
	// TODO: Is there a better way to detect being able to tick early for these perf wins by default, even when not using a NiagaraComponent?
	if (UNiagaraComponent* NiagaraComponent = Cast<UNiagaraComponent>(PrereqComponent))
	{
		PrereqComponent = NiagaraComponent->GetAttachParent();
	}
	return PrereqComponent;
}

void FNiagaraSystemInstance::Activate(EResetMode InResetMode)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemActivate);

	if (::IsValid(System) && System->IsValid() && IsReadyToRun())
	{
		if (GNiagaraAllowDeferredReset && SystemInstanceIndex != INDEX_NONE && FinalizeRef.IsPending() )
		{
			DeferredResetMode = InResetMode;
		}
		else
		{
			// Wait for any async operations, can complete the system
			WaitForConcurrentTickAndFinalize();

			DeferredResetMode = EResetMode::None;
			Reset(InResetMode);
		}
	}
	else
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
	}
}

void FNiagaraSystemInstance::Deactivate(bool bImmediate)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemDeactivate);

	// Clear our pending reset mode
	DeferredResetMode = EResetMode::None;

	if (bImmediate)
	{
		// Wait for any async operations, can complete the system
		WaitForConcurrentTickAndFinalize();

		if (!IsComplete())
		{
			Complete(true);
		}
	}
	else
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Inactive);
	}
}

bool FNiagaraSystemInstance::AllocateSystemInstance(FNiagaraSystemInstancePtr& OutSystemInstanceAllocation, UWorld& InWorld, UNiagaraSystem& InSystem,
	FNiagaraUserRedirectionParameterStore* InOverrideParameters, USceneComponent* InAttachComponent, ENiagaraTickBehavior InTickBehavior, bool bInPooled)
{
	OutSystemInstanceAllocation = MakeShared<FNiagaraSystemInstance, ESPMode::ThreadSafe>(InWorld, InSystem, InOverrideParameters, InAttachComponent, InTickBehavior, bInPooled);
	return true;
}

bool FNiagaraSystemInstance::DeallocateSystemInstance(FNiagaraSystemInstancePtr& SystemInstanceAllocation)
{
	if (SystemInstanceAllocation.IsValid())
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = SystemInstanceAllocation->GetSystemSimulation();

		// Make sure we remove the instance
		if (SystemInstanceAllocation->SystemInstanceIndex != INDEX_NONE)
		{
			SystemSim->RemoveInstance(SystemInstanceAllocation.Get());
		}
		SystemInstanceAllocation->UnbindParameters();

		// Release the render proxy
		if (SystemInstanceAllocation->SystemGpuComputeProxy)
		{
			FNiagaraSystemGpuComputeProxy* Proxy = SystemInstanceAllocation->SystemGpuComputeProxy.Release();
			Proxy->RemoveFromRenderThread(SystemInstanceAllocation->GetComputeDispatchInterface(), true);
		}

		// Queue deferred deletion from the WorldManager
		FNiagaraWorldManager* WorldManager = SystemInstanceAllocation->GetWorldManager();
		check(WorldManager != nullptr);

		// Make sure we abandon any external interface at this point
		SystemInstanceAllocation->OverrideParameters = nullptr;
		SystemInstanceAllocation->OnPostTickDelegate.Unbind();
		SystemInstanceAllocation->OnCompleteDelegate.Unbind();

		WorldManager->DestroySystemInstance(SystemInstanceAllocation);
		check(SystemInstanceAllocation == nullptr);
	}
	SystemInstanceAllocation = nullptr;

	return true;
}

void FNiagaraSystemInstance::Complete(bool bExternalCompletion)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemComplete);

	// Only notify others if have yet to complete
	bool bNeedToNotifyOthers = bNotifyOnCompletion;

	//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::Complete { %p"), this);

	if (SystemInstanceIndex != INDEX_NONE)
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
		SystemSim->RemoveInstance(this);

		SetActualExecutionState(ENiagaraExecutionState::Complete);
		SetRequestedExecutionState(ENiagaraExecutionState::Complete);

		for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
		{
			Emitter->HandleCompletion(true);
		}
	}
	else
	{
		SetActualExecutionState(ENiagaraExecutionState::Complete);
		SetRequestedExecutionState(ENiagaraExecutionState::Complete);
	}

	DestroyDataInterfaceInstanceData();

	if (SystemGpuComputeProxy)
	{
		FNiagaraSystemGpuComputeProxy* Proxy = SystemGpuComputeProxy.Release();
		Proxy->RemoveFromRenderThread(GetComputeDispatchInterface(), true);
	}

	if (!bPooled)
	{
		UnbindParameters(true);
	}

	if (bNeedToNotifyOthers)
	{
		// We've already notified once, no need to do so again.
		bNotifyOnCompletion = false;
		if (OnCompleteDelegate.IsBound())
		{
			OnCompleteDelegate.Execute(bExternalCompletion);
		}
	}
}

void FNiagaraSystemInstance::OnPooledReuse(UWorld& NewWorld)
{
	World = &NewWorld;

	FixedBounds_GT.Init();
	FixedBounds_CNC.Init();
	PreviousLocation.Reset();

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		Emitter->OnPooledReuse();
	}
}

void FNiagaraSystemInstance::SetPaused(bool bInPaused)
{
	if (bInPaused == IsPaused())
	{
		return;
	}

	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize();

	if (SystemInstanceIndex != INDEX_NONE)
	{
		FNiagaraSystemSimulation* SystemSim = GetSystemSimulation().Get();
		if (SystemSim)
		{
			if (bInPaused)
			{
				SystemSim->PauseInstance(this);
			}
			else
			{
				SystemSim->UnpauseInstance(this);
			}
		}
	}
}

void FNiagaraSystemInstance::Reset(EResetMode Mode)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemReset);
	FScopeCycleCounterUObject AdditionalScope(GetSystem(), GET_STATID(STAT_NiagaraSystemReset));

	if (Mode == EResetMode::None)
	{
		// Right now we don't support binding with reset mode none.
		/*if (Mode == EResetMode::None && bBindParams)
		{
			BindParameters();
		}*/
		return;
	}

	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize();

	LastRenderTime = World->GetTimeSeconds();

	SetPaused(false);

	if (SystemSimulation.IsValid())
	{
		SystemSimulation->RemoveInstance(this);
	}
	else
	{
		Mode = EResetMode::ReInit;
	}

	//If we were disabled, try to reinit on reset.
	if (IsDisabled())
	{
		Mode = EResetMode::ReInit;
	}

	// Remove any existing proxy from the diaptcher
	// This MUST be done before the emitters array is re-initialized
	if (SystemGpuComputeProxy.IsValid())
	{
		if (IsComplete() || (Mode != EResetMode::ResetSystem))
		{
			FNiagaraSystemGpuComputeProxy* Proxy = SystemGpuComputeProxy.Release();
			Proxy->RemoveFromRenderThread(GetComputeDispatchInterface(), true);
		}
	}

	// Set tile for LWC offset
	LWCTile = FVector3f::ZeroVector;
	if (AttachComponent.IsValid() && GetSystem()->SupportsLargeWorldCoordinates())
	{
		LWCTile = FLargeWorldRenderScalar::GetTileFor(AttachComponent->GetComponentLocation());
	}

	// Depending on the rest mode we may need to bind or can possibly skip it
	// We must bind if we were previously complete as unbind will have been called, we can not get here if the system was disabled
	bool bBindParams = IsComplete();
	if (Mode == EResetMode::ResetSystem)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::Reset false"));
		ResetInternal(false);
	}
	else if (Mode == EResetMode::ResetAll)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::Reset true"));
		ResetInternal(true);
		bBindParams = !IsDisabled();
	}
	else if (Mode == EResetMode::ReInit)
	{
		//UE_LOG(LogNiagara, Log, TEXT("FNiagaraSystemInstance::ReInit"));
		ReInitInternal();
		bBindParams = !IsDisabled();
	}

	//If none of our emitters actually made it out of the init process we can just bail here before we ever tick.
	bool bHasActiveEmitters = false;
	for (auto& Inst : Emitters)
	{
		if (!Inst->IsComplete())
		{
			bHasActiveEmitters = true;
			break;
		}
	}

	SetRequestedExecutionState(ENiagaraExecutionState::Active);
	if (bHasActiveEmitters)
	{
		if (bBindParams)
		{
			ResetParameters();
			BindParameters();
		}

		SetActualExecutionState(ENiagaraExecutionState::Active);

		if (bBindParams)
		{
			InitDataInterfaces();
		}

		//Interface init can disable the system.
		if (!IsComplete())
		{
			// Create the shared context for the dispatcher if we have a single active GPU emitter in the system
			if (bHasGPUEmitters && !SystemGpuComputeProxy.IsValid())
			{
				SystemGpuComputeProxy.Reset(new FNiagaraSystemGpuComputeProxy(this));
				SystemGpuComputeProxy->AddToRenderThread(GetComputeDispatchInterface());
			}

			// Create new random seed
			RandomSeed = GetSystem()->NeedsDeterminism() ? GetSystem()->GetRandomSeed() : FMath::Rand();

			// Add instance to simulation
			SystemSimulation->AddInstance(this);

			int32 WarmupTicks = WarmupTickCount;
			float WarmupDt = WarmupTickDelta;
			if (WarmupTickCount == -1)
			{
				WarmupTicks = System->GetWarmupTickCount();
				WarmupDt = System->GetWarmupTickDelta();
			}
			
			if (WarmupTicks > 0 && WarmupDt > SMALL_NUMBER)
			{
				AdvanceSimulation(WarmupTicks, WarmupDt);

				//Reset age to zero.
				Age = 0.0f;
				TickCount = 0;
			}
		}
	}
	else
	{
		SetActualExecutionState(ENiagaraExecutionState::Complete);
		Complete(true);
	}
}

void FNiagaraSystemInstance::ResetInternal(bool bResetSimulations)
{
	check(SystemInstanceIndex == INDEX_NONE);
	ensure(!FinalizeRef.IsPending());

	Age = 0;
	TickCount = 0;
	CachedDeltaSeconds = 0.0f;
	InitSystemState();

	if(!bLODDistanceIsOverridden)
	{
		bLODDistanceIsValid = false;
	}

	TotalGPUParamSize = 0;
	ActiveGPUEmitterCount = 0;
	GPUParamIncludeInterpolation = false;
	// Note: We do not need to update our bounds here as they are still valid

	if (!::IsValid(System) || IsDisabled())
	{
		return;
	}

#if WITH_EDITOR
	check(World);
	if (OverrideParameters && World->WorldType == EWorldType::Editor)
	{
		InitDataInterfaces();
	}
#endif

	bool bAllReadyToRun = IsReadyToRun();
	if (!bAllReadyToRun)
	{
		return;
	}

	if (!System->IsValid())
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
		UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara System due to invalid asset! System(%s) Component(%s)"), *System->GetName(), *GetFullNameSafe(AttachComponent.Get()));
		return;
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		Emitter->ResetSimulation(bResetSimulations);
	}

#if WITH_EDITOR
	//UE_LOG(LogNiagara, Log, TEXT("OnResetInternal %p"), this);
	OnResetDelegate.Broadcast();
#endif
}

UNiagaraParameterCollectionInstance* FNiagaraSystemInstance::GetParameterCollectionInstance(UNiagaraParameterCollection* Collection)
{
	return SystemSimulation->GetParameterCollectionInstance(Collection);
}

void FNiagaraSystemInstance::AdvanceSimulation(int32 TickCountToSimulate, float TickDeltaSeconds)
{
	if (TickCountToSimulate > 0 && !IsPaused())
	{
		// Wait for any async operations, can complete the system
		WaitForConcurrentTickAndFinalize();
		if (IsComplete())
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemAdvanceSim);
		bool bWasSolo = bSolo;
		SetSolo(true);

		for (int32 TickIdx = 0; TickIdx < TickCountToSimulate; ++TickIdx)
		{
			//Cannot do multiple tick off the game thread here without additional work. So we pass in null for the completion event which will force GT execution.
			//If this becomes a perf problem I can add a new path for the tick code to handle multiple ticks.
			ManualTick(TickDeltaSeconds, nullptr);
		}
		SetSolo(bWasSolo);
	}
}

bool FNiagaraSystemInstance::IsReadyToRun() const
{
	// check world
	if (World == nullptr || World->bIsTearingDown)
	{
		return false;
	}

	// check system
	if (!::IsValid(System) || !System->IsReadyToRun())
	{
		return false;
	}

	// check emitters
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
	{
		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
		if (Emitter == nullptr)
		{
			continue;
		}
		if (!Emitter->IsReadyToRun())
		{
			return false;
		}
	}
	return true;
}

void FNiagaraSystemInstance::ReInitInternal()
{
	check(SystemInstanceIndex == INDEX_NONE);
	ensure(!FinalizeRef.IsPending());

	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemReinit);

	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);

	Age = 0;
	TickCount = 0;
	LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);
	CachedDeltaSeconds = 0.0f;
	bAlreadyBound = false;
	bSolo = bForceSolo;
	InitSystemState();

	if (!::IsValid(System))
	{
		return;
	}

	//Bypass the SetExecutionState() and it's check for disabled.
	RequestedExecutionState = ENiagaraExecutionState::Inactive;
	ActualExecutionState = ENiagaraExecutionState::Inactive;

	// Do we need to run in solo mode? NOTE: set this here before the early outs
	bSolo = bForceSolo;

	bool bAllReadyToRun = IsReadyToRun();
	if (!bAllReadyToRun)
	{
		return;
	}

	if (!System->IsValid())
	{
		SetRequestedExecutionState(ENiagaraExecutionState::Disabled);
		if ( Emitters.Num() != 0 )
		{
			UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara System due to invalid asset! System(%s) Component(%s)"), *System->GetName(), *GetFullNameSafe(AttachComponent.Get()));
		}
		return;
	}

	if (bSolo)
	{
		if (!SystemSimulation.IsValid())
		{
			SystemSimulation = MakeShared<FNiagaraSystemSimulation, ESPMode::ThreadSafe>();
			SystemSimulation->Init(System, World, true, TG_MAX);
		}
	}
	else
	{
		const ETickingGroup TickGroup = CalculateTickGroup();
		SystemSimulation = GetWorldManager()->GetSystemSimulation(TickGroup, System);
	}

	// Make sure that we've gotten propagated instance parameters before calling InitEmitters, as they might bind to them.
	const FNiagaraSystemCompiledData& SystemCompiledData = System->GetSystemCompiledData();
	InstanceParameters = SystemCompiledData.InstanceParamStore;

	//When re initializing, throw away old emitters and init new ones.
	Emitters.Reset();
	InitEmitters();

	// rebind now after all parameters have been added
	InstanceParameters.Rebind();

	TickInstanceParameters_GameThread(0.01f);
	TickInstanceParameters_Concurrent();

#if WITH_EDITOR
	//UE_LOG(LogNiagara, Log, TEXT("OnResetInternal %p"), this);
	OnResetDelegate.Broadcast();
#endif

}

void FNiagaraSystemInstance::ResetParameters()
{
	if (!::IsValid(System))
	{
		return;
	}

	CurrentFrameIndex = 1;
	ParametersValid = false;

	GlobalParameters[0] = FNiagaraGlobalParameters();
	GlobalParameters[1] = FNiagaraGlobalParameters();
	SystemParameters[0] = FNiagaraSystemParameters();
	SystemParameters[1] = FNiagaraSystemParameters();
	OwnerParameters[0] = FNiagaraOwnerParameters();
	OwnerParameters[1] = FNiagaraOwnerParameters();

	EmitterParameters.Reset(Emitters.Num() * 2);
	EmitterParameters.AddDefaulted(Emitters.Num() * 2);
	GatheredInstanceParameters.Init(Emitters.Num());
}

FNiagaraSystemInstance::~FNiagaraSystemInstance()
{
	//UE_LOG(LogNiagara, Log, TEXT("~FNiagaraSystemInstance %p"), this);

	//FlushRenderingCommands();

	Cleanup();

// #if WITH_EDITOR
// 	OnDestroyedDelegate.Broadcast();
// #endif
}

void FNiagaraSystemInstance::Cleanup()
{
	// We should have no sync operations pending but we will be safe and wait
	WaitForConcurrentTickDoNotFinalize(true);

	if (SystemInstanceIndex != INDEX_NONE)
	{
		TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
		SystemSim->RemoveInstance(this);
	}

	check(!FinalizeRef.IsPending());

	DestroyDataInterfaceInstanceData();

	if (SystemGpuComputeProxy)
	{
		FNiagaraSystemGpuComputeProxy* Proxy = SystemGpuComputeProxy.Release();
		Proxy->RemoveFromRenderThread(GetComputeDispatchInterface(), true);
	}

	UnbindParameters();

	// Clear out the emitters.
	Emitters.Empty(0);

	// clean up any event datasets that we're holding onto for our child emitters
	ClearEventDataSets();
}

//Unsure on usage of this atm. Possibly useful in future.
// void FNiagaraSystemInstance::RebindParameterCollection(UNiagaraParameterCollectionInstance* OldInstance, UNiagaraParameterCollectionInstance* NewInstance)
// {
// 	OldInstance->GetParameterStore().Unbind(&InstanceParameters);
// 	NewInstance->GetParameterStore().Bind(&InstanceParameters);
//
// 	for (FNiagaraEmitterInstanceRef& Simulation : Emitters)
// 	{
// 		Simulation->RebindParameterCollection(OldInstance, NewInstance);
// 	}
//
// 	//Have to re init the instance data for data interfaces.
// 	//This is actually lots more work than absolutely needed in some cases so we can improve it a fair bit.
// 	InitDataInterfaces();
// }

void FNiagaraSystemInstance::BindParameters()
{
	if (OverrideParameters != nullptr)
	{
		if (!bAlreadyBound)
		{
			// NOTE: We don't rebind if it's already bound to improve reset times.
			OverrideParameters->Bind(&InstanceParameters);
		}
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("OverrideParameters is null.  Component(%s) System(%s)"), *GetFullNameSafe(AttachComponent.Get()), *GetFullNameSafe(GetSystem()));
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		Emitter->BindParameters(bAlreadyBound);
	}

	bAlreadyBound = true;
}

void FNiagaraSystemInstance::UnbindParameters(bool bFromComplete)
{
	if (OverrideParameters != nullptr)
	{
		if (!bFromComplete)
		{
			// NOTE: We don't unbind this on complete to improve reset times.
			OverrideParameters->Unbind(&InstanceParameters);
		}

		if (SystemSimulation.IsValid() && SystemSimulation->GetIsSolo())
		{
			OverrideParameters->Unbind(&SystemSimulation->GetSpawnExecutionContext()->Parameters);
			OverrideParameters->Unbind(&SystemSimulation->GetUpdateExecutionContext()->Parameters);
		}
	}

	bAlreadyBound = bFromComplete && bAlreadyBound;
	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		Emitter->UnbindParameters(bFromComplete);
	}
}

void FNiagaraSystemInstance::BindToParameterStore(FNiagaraParameterStore& ParameterStore)
{
	if (ParameterStore.IsEmpty())
	{
		return;
	}

	if (SystemSimulation.IsValid())
	{
		SystemSimulation->GetSpawnExecutionContext()->Parameters.Bind(&ParameterStore);
		SystemSimulation->GetUpdateExecutionContext()->Parameters.Bind(&ParameterStore);
	}

	InstanceParameters.Bind(&ParameterStore);

	if (OverrideParameters)
	{
		OverrideParameters->Bind(&ParameterStore);
	}
}

void FNiagaraSystemInstance::UnbindFromParameterStore(FNiagaraParameterStore& ParameterStore)
{
	if (ParameterStore.IsEmpty())
	{
		return;
	}

	if (SystemSimulation.IsValid())
	{
		SystemSimulation->GetSpawnExecutionContext()->Parameters.Unbind(&ParameterStore);
		SystemSimulation->GetUpdateExecutionContext()->Parameters.Unbind(&ParameterStore);
	}

	InstanceParameters.Unbind(&ParameterStore);

	if (OverrideParameters)
	{
		OverrideParameters->Unbind(&ParameterStore);
	}
}

FNiagaraLWCConverter FNiagaraSystemInstance::GetLWCConverter(bool bLocalSpaceEmitter) const
{
	if (bLocalSpaceEmitter)
	{
		return FNiagaraLWCConverter();
	}
	return FNiagaraLWCConverter(FVector(LWCTile) * FLargeWorldRenderScalar::GetTileSize());
}

FTransform FNiagaraSystemInstance::GetLWCSimToWorld(bool bLocalSpaceEmitter) const
{
	FTransform SimToWorld;
	if (bLocalSpaceEmitter)
	{
		SimToWorld = WorldTransform;
	}
	SimToWorld.AddToTranslation(FVector(LWCTile) * FLargeWorldRenderScalar::GetTileSize());
	return SimToWorld;
}

FNiagaraWorldManager* FNiagaraSystemInstance::GetWorldManager() const
{
	check(World);
	return FNiagaraWorldManager::Get(World);
}

bool FNiagaraSystemInstance::RequiresGlobalDistanceField() const
{
	if (!bHasGPUEmitters)
	{
		return false;
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		if (FNiagaraComputeExecutionContext* GPUContext = Emitter->GetGPUContext())
		{
			for (UNiagaraDataInterface* DataInterface : GPUContext->CombinedParamStore.GetDataInterfaces())
			{
				if (DataInterface && DataInterface->RequiresGlobalDistanceField())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FNiagaraSystemInstance::RequiresDepthBuffer() const
{
	if (!bHasGPUEmitters)
	{
		return false;
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		FNiagaraComputeExecutionContext* GPUContext = Emitter->GetGPUContext();
		if (GPUContext)
		{
			for (UNiagaraDataInterface* DataInterface : GPUContext->CombinedParamStore.GetDataInterfaces())
			{
				if (DataInterface && DataInterface->RequiresDepthBuffer())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FNiagaraSystemInstance::RequiresEarlyViewData() const
{
	if (!bHasGPUEmitters)
	{
		return false;
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		FNiagaraComputeExecutionContext* GPUContext = Emitter->GetGPUContext();
		if (GPUContext)
		{
			for (UNiagaraDataInterface* DataInterface : GPUContext->CombinedParamStore.GetDataInterfaces())
			{
				if (DataInterface && DataInterface->RequiresEarlyViewData())
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FNiagaraSystemInstance::RequiresViewUniformBuffer() const
{
	if (!bHasGPUEmitters)
	{
		return false;
	}

	for (const FNiagaraEmitterInstanceRef& EmitterInstance : Emitters)
	{
		if (EmitterInstance->GetGPUContext() && EmitterInstance->NeedsEarlyViewUniformBuffer())
		{
			return true;
		}
	}

	return false;
}

bool FNiagaraSystemInstance::RequiresRayTracingScene() const
{
	if (!bHasGPUEmitters)
	{
		return false;
	}

	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		FNiagaraComputeExecutionContext* GPUContext = Emitter->GetGPUContext();
		if (GPUContext)
		{
			for (UNiagaraDataInterface* DataInterface : GPUContext->CombinedParamStore.GetDataInterfaces())
			{
				if (DataInterface && DataInterface->RequiresRayTracingScene())
				{
					return true;
				}
			}
		}
	}

	return false;
}

FNDIStageTickHandler* FNiagaraSystemInstance::GetSystemDIStageTickHandler(ENiagaraScriptUsage Usage)
{
	if(Usage == ENiagaraScriptUsage::SystemSpawnScript || Usage == ENiagaraScriptUsage::EmitterSpawnScript)
	{
		return &SystemSpawnDIStageTickHandler;
	}
	else if(Usage == ENiagaraScriptUsage::SystemUpdateScript || Usage == ENiagaraScriptUsage::EmitterUpdateScript)
	{
		return &SystemUpdateDIStageTickHandler;
	}
	return nullptr;
}

void FNiagaraSystemInstance::InitDataInterfaces()
{
	bDataInterfacesHaveTickPrereqs = false;
	bDataInterfacesHaveTickPostreqs = false;

	// If the System is invalid, it is possible that our cached data interfaces are now bogus and could point to invalid memory.
	// Only the UNiagaraComponent or UNiagaraSystem can hold onto GC references to the DataInterfaces.
	if (GetSystem() == nullptr || IsDisabled())
	{
		return;
	}

	// Wait for any async operations, can complete the system
	WaitForConcurrentTickAndFinalize(true);

	if (OverrideParameters != nullptr)
	{
		OverrideParameters->ResolvePositions(GetLWCConverter());
		OverrideParameters->Tick();
	}
	// Make sure the owner has flushed it's parameters before we initialize data interfaces
	InstanceParameters.Tick();

	// Destroy data interface data
	// Note: This invalidates any ticks pending on the render thread
	DestroyDataInterfaceInstanceData();

	if (SystemGpuComputeProxy)
	{
		SystemGpuComputeProxy->ClearTicksFromRenderThread(GetComputeDispatchInterface());
	}

	PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Spawn].Reset();
	PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Update].Reset();

	ResolveUserDataInterfaceBindings();

	//Now the interfaces in the simulations are all correct, we can build the per instance data table.
	int32 InstanceDataSize = 0;
	DataInterfaceInstanceDataOffsets.Empty();
	auto CalcInstDataSize = [&](const FNiagaraParameterStore& ParamStore, bool bIsCPUScript, bool bIsGPUScript, bool bSearchInstanceParams)
	{
		const TArrayView<const FNiagaraVariableWithOffset> Params = ParamStore.ReadParameterVariables();
		const TArray<UNiagaraDataInterface*>& Interfaces = ParamStore.GetDataInterfaces();
		for (const FNiagaraVariableWithOffset& Var : Params)
		{
			if (Var.IsDataInterface())
			{
				UNiagaraDataInterface* Interface = Interfaces[Var.Offset];
				//In scripts that deal with multiple instances we have to manually search for this DI in the instance parameters as it's not going to be in the script's exec param store.
				//Otherwise we'll end up initializing pointless default DIs that just happen to be in those stores from the script.
				//They'll never be used as we bind to the per instance functions.
				if (bSearchInstanceParams)
				{
					if (UNiagaraDataInterface* InstParamDI = InstanceParameters.GetDataInterface(Var))
					{
						Interface = InstParamDI;
					}
				}

				if (Interface)
				{
					if (int32 Size = Interface->PerInstanceDataSize())
					{
						auto* ExistingInstanceDataOffset = DataInterfaceInstanceDataOffsets.FindByPredicate([&](auto& Pair) { return Pair.Key.Get() == Interface; });
						if (!ExistingInstanceDataOffset)//Don't add instance data for interfaces we've seen before.
						{
							//UE_LOG(LogNiagara, Log, TEXT("Adding DI %p %s %s"), Interface, *Interface->GetClass()->GetName(), *Interface->GetPathName());
							auto& NewPair = DataInterfaceInstanceDataOffsets.AddDefaulted_GetRef();
							NewPair.Key = Interface;
							NewPair.Value = InstanceDataSize;

							// Assume that some of our data is going to be 16 byte aligned, so enforce that
							// all per-instance data is aligned that way.
							InstanceDataSize += Align(Size, 16);
						}
					}

					if (bDataInterfacesHaveTickPrereqs == false)
					{
						bDataInterfacesHaveTickPrereqs = Interface->HasTickGroupPrereqs();
					}
					
					if( bDataInterfacesHaveTickPostreqs == false)
					{
						bDataInterfacesHaveTickPostreqs = Interface->HasTickGroupPostreqs();
					}

					if (bIsGPUScript)
					{
						Interface->SetUsedWithGPUScript(true);
						if(FNiagaraDataInterfaceProxy* Proxy = Interface->GetProxy())
						{
							// We need to store the name of each DI source variable here so that we can look it up later when looking for the iteration interface.
							Proxy->SourceDIName = Var.GetName();
						}
					}
					else if (bIsCPUScript)
					{
						Interface->SetUsedWithCPUScript(true);
					}
				}
			}
		}
	};

	CalcInstDataSize(InstanceParameters, false, false, false);//This probably should be a proper exec context.
	CalcInstDataSize(SystemSimulation->GetSpawnExecutionContext()->Parameters, true, false, true);
	CalcInstDataSize(SystemSimulation->GetUpdateExecutionContext()->Parameters, true, false, true);

	//Iterate over interfaces to get size for table and clear their interface bindings.
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
	{
		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
		if (Emitter == nullptr)
		{
			continue;
		}

		if (Emitter->IsDisabled())
		{
			continue;
		}

		const bool bGPUSimulation = Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim;

		CalcInstDataSize(Emitter->GetSpawnExecutionContext().Parameters, !bGPUSimulation, bGPUSimulation, false);
		CalcInstDataSize(Emitter->GetUpdateExecutionContext().Parameters, !bGPUSimulation, bGPUSimulation, false);
		for (int32 i = 0; i < Emitter->GetEventExecutionContexts().Num(); i++)
		{
			CalcInstDataSize(Emitter->GetEventExecutionContexts()[i].Parameters, !bGPUSimulation, bGPUSimulation, false);
		}

		if (Emitter->GetGPUContext() && Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim)
		{
			CalcInstDataSize(Emitter->GetGPUContext()->CombinedParamStore, !bGPUSimulation, bGPUSimulation, false);
		}

		//Also force a rebind while we're here.
		Emitter->DirtyDataInterfaces();
	}

	DataInterfaceInstanceData.SetNumUninitialized(InstanceDataSize);

	bDataInterfacesInitialized = true;
	PreTickDataInterfaces.Empty();
	PostTickDataInterfaces.Empty();

	GPUDataInterfaceInstanceDataSize = 0;
	GPUDataInterfaces.Empty();

	for (int32 i=0; i < DataInterfaceInstanceDataOffsets.Num(); ++i)
	{
		TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair = DataInterfaceInstanceDataOffsets[i];
		if (UNiagaraDataInterface* Interface = Pair.Key.Get())
		{
			check(IsAligned(&DataInterfaceInstanceData[Pair.Value], 16));

			if (Interface->HasPreSimulateTick())
			{
				PreTickDataInterfaces.Add(i);
			}

			if (Interface->HasPostSimulateTick())
			{
				PostTickDataInterfaces.Add(i);
			}

			if (Interface->IsUsedWithGPUScript())
			{
				const int32 GPUDataSize = Interface->PerInstanceDataPassedToRenderThreadSize();
				if (GPUDataSize > 0)
				{
					GPUDataInterfaces.Emplace(Interface, Pair.Value);
					GPUDataInterfaceInstanceDataSize += Align(GPUDataSize, 16);
				}
			}

			//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
			bool bResult = Pair.Key->InitPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
			bDataInterfacesInitialized &= bResult;
			if (!bResult)
			{
				UE_LOG(LogNiagara, Error, TEXT("Error initializing data interface \"%s\" for system. %s"), *Interface->GetPathName(), *GetNameSafe(System));
			}
		}
		else
		{
			UE_LOG(LogNiagara, Error, TEXT("A data interface currently in use by an System has been destroyed."));
			bDataInterfacesInitialized = false;
		}
	}

	if (!bDataInterfacesInitialized && (!IsComplete() && !IsPendingSpawn()))
	{
		//Some error initializing the data interfaces so disable until we're explicitly reinitialized.
		UE_LOG(LogNiagara, Error, TEXT("Error initializing data interfaces. Completing system. %s"), *GetNameSafe(System));
		Complete(true);
		return;
	}

	//We have valid DI instance data so now generate the table of function calls.
	//When using the new exec contexts, each system instance builds it's own tables of DI function bindings for DI calls that require it.
	//i.e. User DIs or those with per instance data that are called from system scripts.
	{
		bool bSuccess = true;
		bSuccess &= SystemSimulation->GetSpawnExecutionContext()->GeneratePerInstanceDIFunctionTable(this, PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Spawn]);
		bSuccess &= SystemSimulation->GetUpdateExecutionContext()->GeneratePerInstanceDIFunctionTable(this, PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Update]);

		if (!bSuccess)
		{
			//Some error initializing the per instance function tables.
			UE_LOG(LogNiagara, Error, TEXT("Error initializing data interfaces. Completing system. %s"), *GetNameSafe(System));
			Complete(true);
			return;
		}
	}

	if (GetSystem()->NeedsGPUContextInitForDataInterfaces())
	{
		for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
		{
			if (Emitter->IsDisabled())
			{
				continue;
			}

			if (Emitter->GetGPUContext() && Emitter->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim)
			{
				Emitter->GetGPUContext()->OptionalContexInit(this);
			}
		}
	}

	//Initialize our DI Tick Stage handlers for the system spawn and update scripts.
	//If needed, the system script execution will use these to perform per instance pre and post tick operations on our DIs.
	SystemSpawnDIStageTickHandler.Init(SystemSimulation->GetSpawnExecutionContext()->Script, this);
	SystemUpdateDIStageTickHandler.Init(SystemSimulation->GetUpdateExecutionContext()->Script, this);
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
	{
		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
		if (Emitter == nullptr)
		{
			continue;
		}
		Emitter->InitDITickLists();
	}
}

void FNiagaraSystemInstance::ResolveUserDataInterfaceBindings()
{
	const TArray<UNiagaraDataInterface*>& InstanceParameterDataInterfaces = InstanceParameters.GetDataInterfaces();
	auto ResolveUserDIs = [&InstanceParameterDataInterfaces](FNiagaraParameterStore& TargetParameterStore, const UNiagaraScript* TargetScript)
	{
		TArrayView<const FNiagaraResolvedUserDataInterfaceBinding> ResolvedUserDataInterfaceBindings = TargetScript->GetResolvedUserDataInterfaceBindings();
		const TArray<UNiagaraDataInterface*>& TargetDataInterfaces = TargetParameterStore.GetDataInterfaces();
		for (const FNiagaraResolvedUserDataInterfaceBinding& ResolvedUserDIBinding : ResolvedUserDataInterfaceBindings)
		{
			if (ResolvedUserDIBinding.UserParameterStoreDataInterfaceIndex != INDEX_NONE &&
				ResolvedUserDIBinding.UserParameterStoreDataInterfaceIndex < InstanceParameterDataInterfaces.Num() &&
				ResolvedUserDIBinding.ScriptParameterStoreDataInterfaceIndex != INDEX_NONE &&
				ResolvedUserDIBinding.ScriptParameterStoreDataInterfaceIndex < TargetDataInterfaces.Num())
			{
				TargetParameterStore.SetDataInterface(
					InstanceParameterDataInterfaces[ResolvedUserDIBinding.UserParameterStoreDataInterfaceIndex],
					ResolvedUserDIBinding.ScriptParameterStoreDataInterfaceIndex);
			}
		}
	};
	
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
	{
		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
		if (Emitter == nullptr)
		{
			continue;
		}

		if (Emitter->GetSpawnExecutionContext().Script != nullptr)
		{
			ResolveUserDIs(Emitter->GetSpawnExecutionContext().Parameters, Emitter->GetSpawnExecutionContext().Script);
		}
		if (Emitter->GetUpdateExecutionContext().Script != nullptr)
		{
			ResolveUserDIs(Emitter->GetUpdateExecutionContext().Parameters, Emitter->GetUpdateExecutionContext().Script);
		}
		if (Emitter->GetGPUContext() != nullptr && Emitter->GetGPUContext()->GPUScript != nullptr)
		{
			ResolveUserDIs(Emitter->GetGPUContext()->CombinedParamStore, Emitter->GetGPUContext()->GPUScript);
		}
		for (FNiagaraScriptExecutionContext& EventContext : Emitter->GetEventExecutionContexts())
		{
			if (EventContext.Script != nullptr)
			{
				ResolveUserDIs(EventContext.Parameters, EventContext.Script);
			}
		}
	}
}

void FNiagaraSystemInstance::TickDataInterfaces(float DeltaSeconds, bool bPostSimulate)
{
	if (!GetSystem() || IsDisabled())
	{
		return;
	}

	bool bRebindVMFuncs = false;
	if (bPostSimulate)
	{
		for (int32 DIPairIndex : PostTickDataInterfaces)
		{
			TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair = DataInterfaceInstanceDataOffsets[DIPairIndex];
			if (UNiagaraDataInterface* Interface = Pair.Key.Get())
			{
				//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
				if (Interface->PerInstanceTickPostSimulate(&DataInterfaceInstanceData[Pair.Value], this, DeltaSeconds))
				{
					// Destroy per instance data in order to not cause any errors on check(...) inside DIs when initializing
					Interface->DestroyPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
					Interface->InitPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
					bRebindVMFuncs = true;
				}
			}
		}
	}
	else
	{
		for (int32 DIPairIndex : PreTickDataInterfaces)
		{
			TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair = DataInterfaceInstanceDataOffsets[DIPairIndex];
			if (UNiagaraDataInterface* Interface = Pair.Key.Get())
			{
				//Ideally when we make the batching changes, we can keep the instance data in big single type blocks that can all be updated together with a single virtual call.
				if (Interface->PerInstanceTick(&DataInterfaceInstanceData[Pair.Value], this, DeltaSeconds))
				{
					// Destroy per instance data in order to not cause any errors on check(...) inside DIs when initializing
					Interface->DestroyPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
					Interface->InitPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
					bRebindVMFuncs = true;
				}
			}
		}
	}

	// If any instance needed to reset, we need to re-cache their bindings, as some DI's require a rebind after reinit
	// TODO: Maybe make this only rebind for the DIs that were reinitialized.
	if (bRebindVMFuncs)
	{
		// Dirty data interfaces for emitters
		for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
		{
			//-TODO:Stateless: Avoid Casting
			FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
			if (Emitter == nullptr)
			{
				continue;
			}

			if (!Emitter->IsDisabled())
			{
				Emitter->DirtyDataInterfaces();
			}
		}

		// Rebind funcs for system scripts
		{
			PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Spawn].Reset();
			PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Update].Reset();

			bool bSuccess = true;
			bSuccess &= SystemSimulation->GetSpawnExecutionContext()->GeneratePerInstanceDIFunctionTable(this, PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Spawn]);
			bSuccess &= SystemSimulation->GetUpdateExecutionContext()->GeneratePerInstanceDIFunctionTable(this, PerInstanceDIFunctions[(int32)ENiagaraSystemSimulationScript::Update]);

			if (!bSuccess)
			{
				// Some error initializing the per instance function tables.
				UE_LOG(LogNiagara, Error, TEXT("Error rebinding VM functions after re-initializing data interface(s). Completing system. %s"), *GetNameSafe(System));
				Complete(true);
			}
		}
		}
	}

float FNiagaraSystemInstance::GetLODDistance()
{
	//In most cases this will have been set externally by the scalability manager.
	if (bLODDistanceIsValid)
	{
		return LODDistance;
	}

	constexpr float DefaultLODDistance = 0.0f;

	FNiagaraWorldManager* WorldManager = GetWorldManager();
	if (WorldManager == nullptr)
	{
		return DefaultLODDistance;
	}

	check(World);
	const FVector EffectLocation = WorldTransform.GetLocation() + (FVector(LWCTile) * FLargeWorldRenderScalar::GetTileSize());
	LODDistance = DefaultLODDistance;

	LODDistance = WorldManager->GetLODDistance(EffectLocation);

	bLODDistanceIsValid = true;
	return LODDistance;
}

ETickingGroup FNiagaraSystemInstance::CalculateTickGroup() const
{
	ETickingGroup NewMinTickGroup = (ETickingGroup)0;
	ETickingGroup NewMaxTickGroup = ETickingGroup::TG_MAX;

	// Debugging feature to force last tick group
	if (GNiagaraForceLastTickGroup)
	{
		return NiagaraLastTickGroup;
	}

	// Determine tick group
	switch ( TickBehavior )
	{
		default:
		case ENiagaraTickBehavior::UsePrereqs:
			// Handle attached component tick group
			if (UActorComponent * PrereqComponent = GetPrereqComponent())
			{
				//-TODO: This doesn't deal with 'DontCompleteUntil' on the prereq's tick, if we have to handle that it could mean continual TG demotion
				ETickingGroup PrereqTG = ETickingGroup(FMath::Max(PrereqComponent->PrimaryComponentTick.TickGroup, PrereqComponent->PrimaryComponentTick.EndTickGroup) + 1);

				// If we are attached to a skeletal mesh component and blend physics is enabled we can not tick until after that has completed
				// otherwise we may not get the correct final transform location leading to latency.
				if ( USkeletalMeshComponent* PrereqSMC = Cast<USkeletalMeshComponent>(PrereqComponent) )
				{
					PrereqTG = PrereqSMC->bBlendPhysics ? FMath::Max(PrereqTG, ETickingGroup(TG_EndPhysics + 1)) : PrereqTG;
				}

				NewMinTickGroup = FMath::Max(NewMinTickGroup, PrereqTG);
			}

			// Handle data interfaces that have tick dependencies
			if ( bDataInterfacesHaveTickPrereqs )
			{
				for (const TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
				{
					if (UNiagaraDataInterface* Interface = Pair.Key.Get())
					{
						ETickingGroup PrereqTG = Interface->CalculateTickGroup(&DataInterfaceInstanceData[Pair.Value]);
						NewMinTickGroup = FMath::Max(NewMinTickGroup, PrereqTG);
					}
				}
			}
			if ( bDataInterfacesHaveTickPostreqs )
			{
				for (const TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
				{
					if (UNiagaraDataInterface* Interface = Pair.Key.Get())
					{
						ETickingGroup PostReq = Interface->CalculateFinalTickGroup(&DataInterfaceInstanceData[Pair.Value]);
						NewMaxTickGroup = FMath::Min(NewMaxTickGroup, PostReq);
					}
				}
			}
			
			if(NewMinTickGroup > NewMaxTickGroup)
			{
				static UEnum* TGEnum = StaticEnum<ETickingGroup>();
				UE_LOG(LogNiagara, Warning, TEXT("Niagara Component has DIs with conflicting Tick Group Dependencies. This may lead to some incorrect behavior.\nSystem:%s\nMinTickGroup:%s\nMaxTickGroup:%s")
				, *System->GetName()
				, *TGEnum->GetDisplayNameTextByValue((int32)NewMinTickGroup).ToString()
				, *TGEnum->GetDisplayNameTextByValue((int32)NewMaxTickGroup).ToString());
			}

			// Clamp tick group to our range
			NewMinTickGroup = FMath::Clamp(NewMinTickGroup, NiagaraFirstTickGroup, NiagaraLastTickGroup);
			break;

		case ENiagaraTickBehavior::UseComponentTickGroup:
			if (USceneComponent* Component = AttachComponent.Get())
			{
				NewMinTickGroup = FMath::Clamp((ETickingGroup)Component->PrimaryComponentTick.TickGroup, NiagaraFirstTickGroup, NiagaraLastTickGroup);
			}
			else
			{
				NewMinTickGroup = NiagaraFirstTickGroup;
			}
			break;

		case ENiagaraTickBehavior::ForceTickFirst:
			NewMinTickGroup = NiagaraFirstTickGroup;
			break;

		case ENiagaraTickBehavior::ForceTickLast:
			NewMinTickGroup = NiagaraLastTickGroup;
			break;
	}


	//UE_LOG(LogNiagara, Log, TEXT("TickGroup: %s %d %d"), *Component->GetPathName(), (int32)TickBehavior, (int32)NewTickGroup);

	return NewMinTickGroup;
}

void FNiagaraSystemInstance::SetTickBehavior(ENiagaraTickBehavior NewTickBehavior)
{
	if (!::IsValid(System) || System->bRequireCurrentFrameData)
	{
		TickBehavior = NewTickBehavior;
	}
	else
	{
		// Tick as soon as possible
		TickBehavior = ENiagaraTickBehavior::ForceTickFirst;
	}
}

void FNiagaraSystemInstance::TickInstanceParameters_GameThread(float DeltaSeconds)
{
	// If we're associated with a scene component, update our cached transform (otherwise, assume it was previously set externally)
	if (AttachComponent.IsValid())
	{
		WorldTransform = AttachComponent->GetComponentToWorld();
		WorldTransform.AddToTranslation(FVector(LWCTile) * -FLargeWorldRenderScalar::GetTileSize());

		const FVector CurrentLocation = WorldTransform.GetLocation();
		if (DeltaSeconds > 0.0f)
		{
			if (FMath::IsNearlyZero(Age) && !System->IsInitialOwnerVelocityFromActor())
			{
				GatheredInstanceParameters.Velocity = FVector::ZeroVector;
			}
			else if (PreviousLocation.IsSet())
			{
				GatheredInstanceParameters.Velocity = (CurrentLocation - PreviousLocation.GetValue()) / DeltaSeconds;
			}
			else if (AActor* OwnerActor = AttachComponent->GetOwner())
			{
				GatheredInstanceParameters.Velocity = OwnerActor->GetVelocity();
			}
		}
		PreviousLocation = CurrentLocation;
	}
	else
	{
		GatheredInstanceParameters.Velocity = FVector::ZeroVector;
	}
	GatheredInstanceParameters.ComponentTrans = WorldTransform;

	GatheredInstanceParameters.EmitterCount = Emitters.Num();
	GatheredInstanceParameters.DeltaSeconds = DeltaSeconds;
	
	GatheredInstanceParameters.WorldDeltaSeconds = GetSystemSimulation()->GetTickInfo().EngineTick;	
	
	GatheredInstanceParameters.NumAlive = 0;

	//Bias the LastRenderTime slightly to account for any delay as it's written by the RT.
	check(World);
	GatheredInstanceParameters.TimeSeconds = float(World->TimeSeconds);				//LWC: Precision Loss
	GatheredInstanceParameters.RealTimeSeconds = float(World->RealTimeSeconds);		//LWC: Precision Loss

	// flip our buffered parameters
	FlipParameterBuffers();
	uint32 ParameterIndex = GetParameterIndex();

	FNiagaraSystemParameters& CurrentSystemParameters = SystemParameters[ParameterIndex];
	CurrentSystemParameters.EngineSystemAge = Age;
	CurrentSystemParameters.EngineTickCount = TickCount;
	CurrentSystemParameters.EngineTimeSinceRendered = FMath::Max(0.0f, GatheredInstanceParameters.TimeSeconds - LastRenderTime - GLastRenderTimeSafetyBias);
	CurrentSystemParameters.EngineExecutionState = static_cast<uint32>(RequestedExecutionState);
	CurrentSystemParameters.EngineLodDistance = GetLODDistance();
	CurrentSystemParameters.EngineLodDistanceFraction = CurrentSystemParameters.EngineLodDistance / MaxLODDistance;
	CurrentSystemParameters.SignificanceIndex = SignificanceIndex;
	CurrentSystemParameters.RandomSeed = RandomSeed + RandomSeedOffset;

	CurrentSystemParameters.CurrentTimeStep = GetSystemSimulation()->GetTickInfo().TickNumber;
	CurrentSystemParameters.NumTimeSteps = GetSystemSimulation()->GetTickInfo().TickCount;
	CurrentSystemParameters.TimeStepFraction = GetSystemSimulation()->GetTickInfo().TimeStepFraction;
	
	CurrentSystemParameters.NumParticles = 0;

	for (int32 i = 0; i < GatheredInstanceParameters.EmitterCount; ++i)
	{
		auto& CurrentEmitterParameters = EditEmitterParameters(i);

		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = Emitters[i]->AsStateful();
		if (Emitter && Emitter->GetExecutionState() != ENiagaraExecutionState::Disabled)
		{
			int32 NumParticles = Emitter->GetNumParticles();
			CurrentSystemParameters.NumParticles += NumParticles;

			CurrentEmitterParameters.EmitterNumParticles = NumParticles;
			CurrentEmitterParameters.EmitterTotalSpawnedParticles = Emitter->GetTotalSpawnedParticles();
			CurrentEmitterParameters.EmitterRandomSeed = Emitter->GetRandomSeed();
			CurrentEmitterParameters.EmitterInstanceSeed = Emitter->GetInstanceSeed();
			const FNiagaraEmitterScalabilitySettings& ScalabilitySettings = Emitter->GetScalabilitySettings();
			CurrentEmitterParameters.EmitterSpawnCountScale = ScalabilitySettings.bScaleSpawnCount ? ScalabilitySettings.SpawnCountScale : 1.0f;
			++GatheredInstanceParameters.NumAlive;
		}
		else
		{
			CurrentEmitterParameters.EmitterNumParticles = 0;
		}
	}

	if (OverrideParameters)
	{
		OverrideParameters->ResolvePositions(GetLWCConverter());
		OverrideParameters->Tick();
	}
}

void FNiagaraSystemInstance::TickInstanceParameters_Concurrent()
{
	uint32 ParameterIndex = GetParameterIndex();
	FNiagaraSystemParameters& CurrentSystemParameters = SystemParameters[ParameterIndex];
	FNiagaraOwnerParameters& CurrentOwnerParameters = OwnerParameters[ParameterIndex];

	const FMatrix LocalToWorld = GatheredInstanceParameters.ComponentTrans.ToMatrixWithScale();
	const FMatrix LocalToWorldNoScale = GatheredInstanceParameters.ComponentTrans.ToMatrixNoScale();

	const FVector Location = GatheredInstanceParameters.ComponentTrans.GetLocation();
	const FQuat Rotation = GatheredInstanceParameters.ComponentTrans.GetRotation();

	CurrentOwnerParameters.EngineLocalToWorld = FMatrix44f(LocalToWorld);						// LWC_TODO: Precision loss
	CurrentOwnerParameters.EngineWorldToLocal = FMatrix44f(LocalToWorld.Inverse());
	CurrentOwnerParameters.EngineLocalToWorldTransposed = FMatrix44f(LocalToWorld.GetTransposed());
	CurrentOwnerParameters.EngineWorldToLocalTransposed = CurrentOwnerParameters.EngineWorldToLocal.GetTransposed();
	CurrentOwnerParameters.EngineLocalToWorldNoScale = FMatrix44f(LocalToWorldNoScale);
	CurrentOwnerParameters.EngineWorldToLocalNoScale = FMatrix44f(LocalToWorldNoScale.Inverse());
	CurrentOwnerParameters.EngineRotation = FQuat4f(Rotation);									// LWC_TODO: precision loss
	CurrentOwnerParameters.EnginePosition = FVector3f(Location);								// LWC_TODO: precision loss
	CurrentOwnerParameters.EngineVelocity = FVector3f(GatheredInstanceParameters.Velocity);		// LWC_TODO: precision loss
	CurrentOwnerParameters.EngineXAxis = CurrentOwnerParameters.EngineRotation.GetAxisX();
	CurrentOwnerParameters.EngineYAxis = CurrentOwnerParameters.EngineRotation.GetAxisY();
	CurrentOwnerParameters.EngineZAxis = CurrentOwnerParameters.EngineRotation.GetAxisZ();
	CurrentOwnerParameters.EngineScale = (FVector3f)GatheredInstanceParameters.ComponentTrans.GetScale3D();
	CurrentOwnerParameters.EngineLWCTile = LWCTile;
	CurrentOwnerParameters.EngineLWCTile.W = float(FLargeWorldRenderScalar::GetTileSize());

	CurrentSystemParameters.EngineEmitterCount = GatheredInstanceParameters.EmitterCount;
	CurrentSystemParameters.EngineAliveEmitterCount = GatheredInstanceParameters.NumAlive;
	CurrentSystemParameters.SignificanceIndex = SignificanceIndex;
	CurrentSystemParameters.RandomSeed = RandomSeed + RandomSeedOffset;

	CurrentSystemParameters.CurrentTimeStep = GetSystemSimulation()->GetTickInfo().TickNumber;
	CurrentSystemParameters.NumTimeSteps = GetSystemSimulation()->GetTickInfo().TickCount;
	CurrentSystemParameters.TimeStepFraction = GetSystemSimulation()->GetTickInfo().TimeStepFraction;

	FNiagaraGlobalParameters& CurrentGlobalParameter = GlobalParameters[ParameterIndex];
	CurrentGlobalParameter.WorldDeltaTime = GatheredInstanceParameters.WorldDeltaSeconds;
	CurrentGlobalParameter.EngineDeltaTime = GatheredInstanceParameters.DeltaSeconds;
	CurrentGlobalParameter.EngineInvDeltaTime = GatheredInstanceParameters.DeltaSeconds > 0.0f ? 1.0f / GatheredInstanceParameters.DeltaSeconds : 0.0f;
	CurrentGlobalParameter.EngineRealTime = GatheredInstanceParameters.RealTimeSeconds;
	CurrentGlobalParameter.EngineTime = GatheredInstanceParameters.TimeSeconds;
	CurrentGlobalParameter.QualityLevel = FNiagaraPlatformSet::GetQualityLevel();

	InstanceParameters.Tick();
	InstanceParameters.MarkParametersDirty();
}

void FNiagaraSystemInstance::ClearEventDataSets()
{
	for (auto& EventDataSetIt : EmitterEventDataSetMap)
	{
		delete EventDataSetIt.Value;
	}

	EmitterEventDataSetMap.Empty();
}

FNiagaraDataSet*
FNiagaraSystemInstance::CreateEventDataSet(FName EmitterName, FName EventName)
{
	// TODO: find a better way of multiple events trying to write to the same data set;
	// for example, if two analytical collision primitives want to send collision events, they need to push to the same data set
	FNiagaraDataSet*& OutSet = EmitterEventDataSetMap.FindOrAdd(EmitterEventKey(EmitterName, EventName));

	if (!OutSet)
	{
		OutSet = new FNiagaraDataSet();
	}

	return OutSet;
}

FNiagaraDataSet*
FNiagaraSystemInstance::GetEventDataSet(FName EmitterName, FName EventName) const
{
	FNiagaraDataSet* const* OutDataSet = EmitterEventDataSetMap.Find(EmitterEventKey(EmitterName, EventName));

	return OutDataSet ? *OutDataSet : nullptr;
}

#if WITH_EDITORONLY_DATA

bool FNiagaraSystemInstance::UsesCollection(const UNiagaraParameterCollection* Collection)const
{
	if (::IsValid(System))
	{
		if (System->UsesCollection(Collection))
		{
			return true;
		}
	}
	return false;
}

#endif

void FNiagaraSystemInstance::InitEmitters()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInitEmitters);

	bHasGPUEmitters = false;

	LocalBounds = FBox(FVector::ZeroVector, FVector::ZeroVector);

	Emitters.Empty(false);
	if (::IsValid(System))
	{
		const TArray<FNiagaraEmitterHandle>& EmitterHandles = System->GetEmitterHandles();

		const bool bAllowComputeShaders = FNiagaraUtilities::AllowComputeShaders(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]);

		const int32 NumEmitters = EmitterHandles.Num();
		Emitters.Reserve(NumEmitters);
		for (int32 EmitterIdx=0; EmitterIdx < NumEmitters; ++EmitterIdx)
		{
			const FNiagaraEmitterHandle& EmitterHandle = EmitterHandles[EmitterIdx];

			//-TODO: We should not create emitter instances for disabled emitters
			const bool EmitterEnabled = EmitterHandle.GetIsEnabled();

			//-TODO:Stateless: Should this be a factory?
			if ( EmitterHandle.GetEmitterMode() == ENiagaraEmitterMode::Stateless )
			{
				Emitters.Emplace(MakeShared<FNiagaraStatelessEmitterInstance, ESPMode::ThreadSafe>(this));
			}
			else
			{
				Emitters.Emplace(MakeShared<FNiagaraEmitterInstanceImpl, ESPMode::ThreadSafe>(this));
			}
			FNiagaraEmitterInstanceRef EmitterInstance = Emitters.Last();

			if (System->bFixedBounds)
			{
				// be sure to set the system bounds first so that we can bypass work in the initialization of the emitter
				EmitterInstance->SetSystemFixedBoundsOverride(System->GetFixedBounds());
			}
			else if (EmitterEnabled)
			{
				if (const FVersionedNiagaraEmitterData* EmitterAsset = EmitterHandles[EmitterIdx].GetEmitterData())
				{
					if (EmitterAsset->CalculateBoundsMode == ENiagaraEmitterCalculateBoundMode::Fixed)
					{
						LocalBounds += EmitterAsset->FixedBounds;
					}
				}
			}

			EmitterInstance->Init(EmitterIdx);

			if (EmitterEnabled)
			{
				// Only set bHasGPUEmitters if we allow compute shaders on the platform
				if (bAllowComputeShaders)
				{
					bHasGPUEmitters |= EmitterInstance->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim;
				}
			}
		}

		if (System->bFixedBounds)
		{
			LocalBounds = System->GetFixedBounds();
		}
		else if (!LocalBounds.IsValid)
		{
			LocalBounds = FBox(ForceInitToZero);
		}
	}

	ResetParameters();
}

void FNiagaraSystemInstance::ManualTick(float DeltaSeconds, const FGraphEventRef& MyCompletionGraphEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_ComponentTickGT);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);

	if (IsDisabled())
	{
		return;
	}

	TSharedPtr<FNiagaraSystemSimulation, ESPMode::ThreadSafe> SystemSim = GetSystemSimulation();
	check(SystemSim.IsValid());
	check(IsInGameThread());
	check(bSolo);

	SystemSim->Tick_GameThread(DeltaSeconds, MyCompletionGraphEvent);
}

void FNiagaraSystemInstance::SimCacheTick_GameThread(UNiagaraSimCache* SimCache, float DesiredAge, float DeltaSeconds, const FGraphEventRef& MyCompletionGraphEvent)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_ComponentTickGT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_TickGT);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);

	check(IsInGameThread());
	check(bSolo);

	FNiagaraCrashReporterScope CRScope(this);

	FScopeCycleCounter SystemStat(System->GetStatID(true, false));

	if (IsComplete() || IsDisabled())
	{
		return;
	}

	// If the attached component is marked pending kill the instance is no longer valid
	if (GetAttachComponent() == nullptr)
	{
		Complete(true);
		return;
	}

	if ( DesiredAge > SimCache->GetStartSeconds() + SimCache->GetDurationSeconds() )
	{
		Complete(false);
		return;
	}

	// Early out if our age and desired age are virtually equal as there's nothing to do
	if (FMath::IsNearlyEqual(Age, DesiredAge) )
	{
		return;
	}

	CachedDeltaSeconds = DeltaSeconds;
	//FixedBounds_CNC = FixedBounds_GT;

	// We still need to tick the override parameters as these might pass through to material bindings
	//TickInstanceParameters_GameThread(DeltaSeconds);
	if (OverrideParameters)
	{
		OverrideParameters->ResolvePositions(GetLWCConverter());
		OverrideParameters->Tick();
	}

	//TickDataInterfaces(DeltaSeconds, false);

	Age = DesiredAge;
	TickCount += 1;

#if WITH_EDITOR
	//// We need to tick the rapid iteration parameters when in the editor
	//for (auto& EmitterInstance : Emitters)
	//{
	//	if (EmitterInstance->ShouldTick())
	//	{
	//		EmitterInstance->TickRapidIterationParameters();
	//	}
	//}
#endif

	//-OPT: On cooked builds we do not need to do this every frame
	if ( SimCache->CanRead(GetSystem()) == false )
	{
		Complete(false);
		return;
	}

	//-TODO: This can run async
	const bool bCacheValid = SimCache->Read(Age, this);
	if (bCacheValid == false)
	{
		Complete(false);
		return;
	}

	if ( SystemInstanceState == ENiagaraSystemInstanceState::PendingSpawn )
	{
		SystemSimulation->SetInstanceState(this, ENiagaraSystemInstanceState::Running);
	}

	SystemSimulation->SimCachePostTick_Concurrent(DeltaSeconds, MyCompletionGraphEvent);

	if (OnPostTickDelegate.IsBound())
	{
		OnPostTickDelegate.Execute();
	}
}

void FNiagaraSystemInstance::SimCacheTick_Concurrent(UNiagaraSimCache* SimCache)
{
	//-OPT: Move SimCache read from GT to concurrent work
}

void FNiagaraSystemInstance::DumpStalledInfo()
{
	TStringBuilder<128> Builder;
	Builder.Appendf(TEXT("System (%s)\n"), *GetNameSafe(GetSystem()));
	Builder.Appendf(TEXT("ConcurrentTickGraphEvent Complete (%d)\n"), ConcurrentTickGraphEvent ? ConcurrentTickGraphEvent->IsComplete() : true);
	Builder.Appendf(TEXT("ConcurrentTickBatchGraphEvent Complete (%d)\n"), ConcurrentTickBatchGraphEvent ? ConcurrentTickBatchGraphEvent->IsComplete() : true);
	Builder.Appendf(TEXT("FinalizePending (%d)\n"), FinalizeRef.IsPending());
	Builder.Appendf(TEXT("SystemInstanceIndex (%d)\n"), SystemInstanceIndex);
	Builder.Appendf(TEXT("SystemInstanceState (%d)\n"), SystemInstanceState);

	UE_LOG(LogNiagara, Fatal, TEXT("FNiagaraSystemInstance is stalled.\n%s"), Builder.ToString());
}

void FNiagaraSystemInstance::WaitForConcurrentTickDoNotFinalize(bool bEnsureComplete)
{
	check(IsInGameThread());

	// Wait for any concurrent ticking for our task
	const uint64 StartCycles = FPlatformTime::Cycles64();
	bool bDidWait = false;

	// Wait for system concurrent tick
	if (ConcurrentTickGraphEvent && !ConcurrentTickGraphEvent->IsComplete())
	{
		CSV_SCOPED_SET_WAIT_STAT(Effects);
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemWaitForAsyncTick);
		PARTICLE_PERF_STAT_CYCLES_GT(FParticlePerfStatsContext(GetWorld(), GetSystem(), Cast<UFXSystemComponent>(GetAttachComponent())), Wait);
		bDidWait = true;

		extern int32 GNiagaraSystemSimulationTaskStallTimeout;
		if (GNiagaraSystemSimulationTaskStallTimeout > 0)
		{
			const double EndTimeoutSeconds = FPlatformTime::Seconds() + (double(GNiagaraSystemSimulationTaskStallTimeout) / 1000.0);
			LowLevelTasks::BusyWaitUntil(
				[this, EndTimeoutSeconds]()
				{
					if (FPlatformTime::Seconds() > EndTimeoutSeconds)
					{
						DumpStalledInfo();
						return true;
					}
					return ConcurrentTickGraphEvent->IsComplete();
				}
			);
		}
		else
		{
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(ConcurrentTickGraphEvent, ENamedThreads::GameThread_Local);
		}
	}

	// Wait for instance concurrent tick
	if (ConcurrentTickBatchGraphEvent && !ConcurrentTickBatchGraphEvent->IsComplete())
	{
		CSV_SCOPED_SET_WAIT_STAT(Effects);
		SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemWaitForAsyncTick);
		PARTICLE_PERF_STAT_CYCLES_GT(FParticlePerfStatsContext(GetWorld(), GetSystem(), Cast<UFXSystemComponent>(GetAttachComponent())), Wait);
		bDidWait = true;
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(ConcurrentTickBatchGraphEvent, ENamedThreads::GameThread_Local);
	}

	if (bDidWait)
	{
		ensureAlwaysMsgf(!bEnsureComplete, TEXT("FNiagaraSystemInstance::WaitForConcurrentTickDoNotFinalize - Async Work not complete and is expected to be. %s"), *GetSystem()->GetPathName());

		const double StallTimeMS = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);
		if ((GWaitForAsyncStallWarnThresholdMS > 0.0f) && (StallTimeMS > GWaitForAsyncStallWarnThresholdMS))
		{
			//-TODO: This should be put back to a warning once EngineTests no longer cause it show up.  The reason it's triggered is because we pause in latent actions right after a TG running Niagara sims.
			UE_LOG(LogNiagara, Log, TEXT("Niagara Effect stalled GT for %g ms. Component(%s) System(%s)"), StallTimeMS, *GetFullNameSafe(AttachComponent.Get()), *GetFullNameSafe(GetSystem()));
		}
	}

	ConcurrentTickGraphEvent = nullptr;
	ConcurrentTickBatchGraphEvent = nullptr;
}

void FNiagaraSystemInstance::WaitForConcurrentTickAndFinalize(bool bEnsureComplete)
{
	WaitForConcurrentTickDoNotFinalize(bEnsureComplete);

	if (FinalizeRef.IsPending())
	{
		FinalizeTick_GameThread();
	}
}

bool FNiagaraSystemInstance::HandleCompletion()
{
	bool bEmittersCompleteOrDisabled = true;
	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		bEmittersCompleteOrDisabled &= Emitter->HandleCompletion();
	}

	bool bCompletedAlready = IsComplete();
	if (bCompletedAlready || bEmittersCompleteOrDisabled || World->bIsTearingDown)
	{
		//UE_LOG(LogNiagara, Log, TEXT("Completion Achieved"));
		Complete(false);
		return true;
	}

	return false;
}

void FNiagaraSystemInstance::Tick_GameThread(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_TickGT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);

	FNiagaraCrashReporterScope CRScope(this);

	FScopeCycleCounter SystemStat(System->GetStatID(true, false));

	// We should have no pending async operations, but wait to be safe
	WaitForConcurrentTickAndFinalize(true);

	// If the attached component is marked pending kill the instance is no longer valid
	if ( GetAttachComponent() == nullptr )
	{
		Complete(true);
		return;
	}

	if (IsComplete())
	{
		return;
	}

	// If the interfaces have changed in a meaningful way, we need to potentially rebind and update the values.
	if (OverrideParameters->GetInterfacesDirty())
	{
		Reset(EResetMode::ReInit);
		return;
	}

	CachedDeltaSeconds = DeltaSeconds;
	FixedBounds_CNC = FixedBounds_GT;

	TickInstanceParameters_GameThread(DeltaSeconds);

	TickDataInterfaces(DeltaSeconds, false);

	Age += DeltaSeconds;
	TickCount += 1;
	
#if WITH_EDITOR
	// We need to tick the rapid iteration parameters when in the editor
	for (const FNiagaraEmitterInstanceRef& EmitterRef : Emitters)
	{
		//-TODO:Stateless: Avoid Casting
		FNiagaraEmitterInstanceImpl* Emitter = EmitterRef->AsStateful();
		if (Emitter == nullptr)
		{
			continue;
		}
		if (Emitter->ShouldTick())
		{
			Emitter->TickRapidIterationParameters();
		}
	}
#endif
}

void FNiagaraSystemInstance::Tick_Concurrent(bool bEnqueueGPUTickIfNeeded)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_TickCNC);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT_CNC);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);
	FScopeCycleCounterUObject AdditionalScope(GetSystem(), GET_STATID(STAT_NiagaraOverview_GT_CNC));

	FNiagaraCrashReporterScope CRScope(this);

	// Reset values that will be accumulated during emitter tick.
	TotalGPUParamSize = 0;
	ActiveGPUEmitterCount = 0;
	GPUParamIncludeInterpolation = false;

	if (IsComplete() || !::IsValid(System) || CachedDeltaSeconds < SMALL_NUMBER)
	{
		return;
	}

	const int32 NumEmitters = Emitters.Num();
	const TConstArrayView<FNiagaraEmitterExecutionIndex> EmitterExecutionOrder = GetEmitterExecutionOrder();
	checkSlow(EmitterExecutionOrder.Num() <= NumEmitters);

	//Determine if any of our emitters should be ticking.
	TBitArray<TInlineAllocator<8>> EmittersShouldTick;
	EmittersShouldTick.Init(false, NumEmitters);

	bool bHasTickingEmitters = false;
	for (const FNiagaraEmitterExecutionIndex& EmitterExecIdx : EmitterExecutionOrder)
	{
		const FNiagaraEmitterInstance& Emitter = Emitters[EmitterExecIdx.EmitterIndex].Get();
		if (Emitter.ShouldTick())
		{
			bHasTickingEmitters = true;
			EmittersShouldTick.SetRange(EmitterExecIdx.EmitterIndex, 1, true);
		}
	}

	if ( !bHasTickingEmitters )
	{
		return;
	}

	FScopeCycleCounter SystemStat(System->GetStatID(true, true));

	for (const FNiagaraEmitterExecutionIndex& EmitterExecIdx : EmitterExecutionOrder)
	{
		if (EmittersShouldTick[EmitterExecIdx.EmitterIndex])
		{
			FNiagaraEmitterInstance& Emitter = Emitters[EmitterExecIdx.EmitterIndex].Get();
			Emitter.PreTick();
		}
	}

	int32 TotalCombinedParamStoreSize = 0;

	// now tick all emitters
	for (const FNiagaraEmitterExecutionIndex& EmitterExecIdx : EmitterExecutionOrder)
	{
		FNiagaraEmitterInstance& Emitter = Emitters[EmitterExecIdx.EmitterIndex].Get();
		if (EmittersShouldTick[EmitterExecIdx.EmitterIndex])
		{
			Emitter.Tick(CachedDeltaSeconds);
		}

		//-TODO:Stateless: Avoid Casting
		if (!Emitter.IsComplete() && Emitter.GetSimTarget() == ENiagaraSimTarget::GPUComputeSim)
		{
			if (FNiagaraEmitterInstanceImpl* StatefulEmitter = Emitter.AsStateful())
			{
				// Handle edge case where an emitter was set to inactive on the first frame by scalability
				// Since it will not tick we should not execute a GPU tick for it, this test must be symeterical with FNiagaraGPUSystemTick::Init
				const bool bIsInactive = (StatefulEmitter->GetExecutionState() == ENiagaraExecutionState::Inactive) || (StatefulEmitter->GetExecutionState() == ENiagaraExecutionState::InactiveClear);
				if (StatefulEmitter->HasTicked() || !bIsInactive)
				{
					if (const FNiagaraComputeExecutionContext* GPUContext = StatefulEmitter->GetGPUContext())
					{
						const int32 InterpFactor = GPUContext->HasInterpolationParameters ? 2 : 1;

						TotalCombinedParamStoreSize += InterpFactor * GPUContext->GetConstantBufferSize();
						GPUParamIncludeInterpolation = GPUContext->HasInterpolationParameters || GPUParamIncludeInterpolation;
						ActiveGPUEmitterCount++;
					}
				}
			}
		}
	}

	if (ActiveGPUEmitterCount)
	{
		const int32 InterpFactor = GPUParamIncludeInterpolation ? 2 : 1;

		TotalGPUParamSize = InterpFactor * (sizeof(FNiagaraGlobalParameters) + sizeof(FNiagaraSystemParameters) + sizeof(FNiagaraOwnerParameters));
		TotalGPUParamSize += InterpFactor * ActiveGPUEmitterCount * sizeof(FNiagaraEmitterParameters);
		TotalGPUParamSize += TotalCombinedParamStoreSize;
	}

	// Update local bounds
	if ( FixedBounds_CNC.IsValid )
	{
		LocalBounds = FixedBounds_CNC;
	}
	else if ( System->bFixedBounds )
	{
		LocalBounds = System->GetFixedBounds();
	}
	else
	{
		FBox NewDynamicBounds(ForceInit);
		FBox NewFixedBounds(ForceInit);
		for (const auto& Emitter : Emitters)
		{
			if (Emitter->AreBoundsDynamic())
			{
				NewDynamicBounds += Emitter->GetBounds();
			}
			else
			{
				NewFixedBounds += Emitter->GetBounds();
			}
		}

		LocalBounds = FBox(ForceInit);
		if (NewDynamicBounds.IsValid)
		{
			FVector Center = NewDynamicBounds.GetCenter();
			FVector Extent = NewDynamicBounds.GetExtent();
			if (GNiagaraEmitterBoundsDynamicSnapValue > 1.0f)
			{
				Extent.X = FMath::CeilToDouble(Extent.X / GNiagaraEmitterBoundsDynamicSnapValue) * GNiagaraEmitterBoundsDynamicSnapValue;
				Extent.Y = FMath::CeilToDouble(Extent.Y / GNiagaraEmitterBoundsDynamicSnapValue) * GNiagaraEmitterBoundsDynamicSnapValue;
				Extent.Z = FMath::CeilToDouble(Extent.Z / GNiagaraEmitterBoundsDynamicSnapValue) * GNiagaraEmitterBoundsDynamicSnapValue;
			}
			Extent = Extent * GNiagaraEmitterBoundsDynamicExpandMultiplier;
			LocalBounds += FBox(Center - Extent, Center + Extent);
		}
		if (NewFixedBounds.IsValid)
		{
			const FVector Center = NewFixedBounds.GetCenter();
			const FVector Extent = NewFixedBounds.GetExtent() * GNiagaraEmitterBoundsFixedExpandMultiplier;
			LocalBounds += FBox(Center - Extent, Center + Extent);
		}

		// In the case that we have no bounds initialize to a sensible default value to avoid NaNs later on
		if (LocalBounds.IsValid == false)
		{
			// we provide a small amount of volume to our default because small (read subpixel) volumes in screen
			// space can still create problems with culling.  This system will still have flickering problems as
			// this box becomes small in screen space, but it's an improvement.
			LocalBounds = FBox(-FVector::OneVector, FVector::OneVector);
		}
	}

	//Enqueue a GPU tick for this sim if we're allowed to do so from a concurrent thread.
	//If we're batching our tick passing we may still need to enqueue here if not called from the regular async task. The caller will tell us with bEnqueueGPUTickIfNeeded.
	FNiagaraSystemSimulation* Sim = SystemSimulation.Get();
	check(Sim);
	ENiagaraGPUTickHandlingMode Mode = Sim->GetGPUTickHandlingMode();
	if (Mode == ENiagaraGPUTickHandlingMode::Concurrent || (Mode == ENiagaraGPUTickHandlingMode::ConcurrentBatched && bEnqueueGPUTickIfNeeded))
	{
		GenerateAndSubmitGPUTick();
	}
}

void FNiagaraSystemInstance::OnSimulationDestroyed()
{
	// This notifies us that the simulation we're holding a reference to is being abandoned by the world manager and we should also
	// release our reference
	ensureMsgf(!IsSolo(), TEXT("OnSimulationDestroyed should only happen for systems referencing a simulation from the world manager"));
	if (SystemSimulation.IsValid())
	{
		if (SystemInstanceIndex != INDEX_NONE)
		{
			SystemSimulation->SetInstanceState(this, ENiagaraSystemInstanceState::None);
		}
		UnbindParameters();
		SystemSimulation = nullptr;
	}
}

void FNiagaraSystemInstance::FinalizeTick_GameThread(bool bEnqueueGPUTickIfNeeded)
{
	FNiagaraCrashReporterScope CRScope(this);

	SCOPE_CYCLE_COUNTER(STAT_NiagaraOverview_GT);
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSystemInst_FinalizeGT);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Effects);
	LLM_SCOPE(ELLMTag::Niagara);

	// Ensure concurrent work is complete and clear the finalize ref
	check(ConcurrentTickGraphEvent == nullptr || ConcurrentTickGraphEvent->IsComplete());
	ConcurrentTickGraphEvent = nullptr;
	check(ConcurrentTickBatchGraphEvent == nullptr || ConcurrentTickBatchGraphEvent->IsComplete());
	ConcurrentTickBatchGraphEvent = nullptr;
	FinalizeRef.ConditionalClear();

	//Temporarily force FX to update their own LODDistance on frames where it is not provided by the scalability manager.
	//TODO: Lots of FX wont need an accurate per frame value so implement a good way for FX to opt into this. FORT-248457
	//Don't reset if the LODDistance is overridden.
	if(!bLODDistanceIsOverridden)
	{
		bLODDistanceIsValid = false;
	}

	if (!HandleCompletion())
	{
		//Post tick our interfaces.
		TickDataInterfaces(CachedDeltaSeconds, true);

		//Enqueue a GPU tick for this sim if we have to do this from the GameThread.
		//If we're batching our tick passing we may still need to enqueue here if not called from the regular finalize task. The caller will tell us with bEnqueueGPUTickIfNeeded.
		FNiagaraSystemSimulation* Sim = SystemSimulation.Get();
		checkf(Sim != nullptr,
			TEXT("SystemSimulation is nullptr during Finalize and the System(%s) AttachComponent(%s) ActualExecutionState(%d) SystemInstanceIndex(%d) SystemInstanceState(%s)"),
			*GetFullNameSafe(System), *GetFullNameSafe(AttachComponent.Get()), ActualExecutionState, SystemInstanceIndex,
			*StaticEnum<ENiagaraSystemInstanceState>()->GetValueAsString(SystemInstanceState)
		);
		

		ENiagaraGPUTickHandlingMode Mode = Sim->GetGPUTickHandlingMode();
		if (Mode == ENiagaraGPUTickHandlingMode::GameThread || (Mode == ENiagaraGPUTickHandlingMode::GameThreadBatched && bEnqueueGPUTickIfNeeded))
		{
			GenerateAndSubmitGPUTick();
		}

		if (OnPostTickDelegate.IsBound())
		{
			OnPostTickDelegate.Execute();
		}
	}

	if (DeferredResetMode != EResetMode::None)
	{
		const EResetMode ResetMode = DeferredResetMode;
		DeferredResetMode = EResetMode::None;

		Reset(ResetMode);
	}
}

void FNiagaraSystemInstance::GenerateAndSubmitGPUTick()
{
	if (NeedsGPUTick())
	{
		ensure(!IsComplete());
		FNiagaraGPUSystemTick GPUTick;
		InitGPUTick(GPUTick);

		// We will give the data over to the render thread. It is responsible for freeing it.
		// We no longer own it and cannot modify it after this point.
		// @todo We are taking a copy of the object here. This object is small so this overhead should
		// not be very high. And we avoid making a bunch of small allocations here.
		ENQUEUE_RENDER_COMMAND(FNiagaraGiveSystemInstanceTickToRT)(
			[RT_Proxy=SystemGpuComputeProxy.Get(), GPUTick](FRHICommandListImmediate& RHICmdList) mutable
			{
				RT_Proxy->QueueTick(GPUTick);
			}
		);
	}
}

void FNiagaraSystemInstance::InitGPUTick(FNiagaraGPUSystemTick& OutTick)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraInitGPUSystemTick);
	check(SystemGpuComputeProxy.IsValid());
	OutTick.Init(this);

	//if (GPUTick.DIInstanceData)
	//{
	//	uint8* BasePointer = (uint8*)GPUTick.DIInstanceData->PerInstanceDataForRT;

	//	//UE_LOG(LogNiagara, Log, TEXT("GT Testing (dipacket) %p (baseptr) %p"), GPUTick.DIInstanceData, BasePointer);
	//	for (auto& Pair : GPUTick.DIInstanceData->InterfaceProxiesToOffsets)
	//	{
	//		FNiagaraDataInterfaceProxy* Proxy = Pair.Key;
	//		UE_LOG(LogNiagara, Log, TEXT("\tGT (proxy) %p (size) %u"), Proxy, Proxy->PerInstanceDataPassedToRenderThreadSize());
	//	}
	//}
}

#if WITH_EDITOR
void FNiagaraSystemInstance::RaiseNeedsUIResync()
{
	bNeedsUIResync = true;
}

bool FNiagaraSystemInstance::HandleNeedsUIResync()
{
	bool bRet = bNeedsUIResync;
	bNeedsUIResync = false;
	return bRet;
}
#endif

#if WITH_EDITORONLY_DATA
bool FNiagaraSystemInstance::GetIsolateEnabled() const
{
	if (::IsValid(System))
	{
		return System->GetIsolateEnabled();
	}
	return false;
}
#endif

void FNiagaraSystemInstance::DestroyDataInterfaceInstanceData()
{
	for (TPair<TWeakObjectPtr<UNiagaraDataInterface>, int32>& Pair : DataInterfaceInstanceDataOffsets)
	{
		if (UNiagaraDataInterface* Interface = Pair.Key.Get())
		{
			Interface->DestroyPerInstanceData(&DataInterfaceInstanceData[Pair.Value], this);
		}
	}

	DataInterfaceInstanceDataOffsets.Empty();
	DataInterfaceInstanceData.Empty();
	PreTickDataInterfaces.Empty();
	PostTickDataInterfaces.Empty();
	GPUDataInterfaces.Empty();
}

FNiagaraEmitterInstancePtr FNiagaraSystemInstance::GetSimulationForHandle(const FNiagaraEmitterHandle& EmitterHandle) const
{
	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		if ( Emitter->GetEmitterHandle().GetId() == EmitterHandle.GetId() )
		{
			return Emitter;
		}
	}
	return nullptr;
}

TConstArrayView<FNiagaraEmitterExecutionIndex> FNiagaraSystemInstance::GetEmitterExecutionOrder() const
{
	if (SystemSimulation != nullptr)
	{
		const UNiagaraSystem* NiagaraSystem = SystemSimulation->GetSystem();
		if (ensure(NiagaraSystem != nullptr))
		{
			return NiagaraSystem->GetEmitterExecutionOrder();
		}
	}
	return MakeArrayView<FNiagaraEmitterExecutionIndex>(nullptr, 0);
}

FBox FNiagaraSystemInstance::GetSystemFixedBounds() const
{
	if (FixedBounds_GT.IsValid)
	{
		return FixedBounds_GT;
	}
	else
	{
		if (::IsValid(System) && System->bFixedBounds)
		{
			return System->GetFixedBounds();
		}
	}
	return FBox(ForceInit);
}

void FNiagaraSystemInstance::SetEmitterFixedBounds(FName EmitterName, const FBox& InLocalBounds)
{
	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		if ( Emitter->GetEmitterHandle().GetName() == EmitterName )
		{
			Emitter->SetFixedBounds(InLocalBounds);
			return;
		}
	}

	// Failed to find emitter
	UE_LOG(LogNiagara, Warning, TEXT("SetEmitterFixedBounds: Failed to find Emitter(%s) System(%s) Component(%s)"), *EmitterName.ToString(), *GetNameSafe(System), *GetFullNameSafe(AttachComponent.Get()));
}

FBox FNiagaraSystemInstance::GetEmitterFixedBounds(FName EmitterName) const
{
	for (const FNiagaraEmitterInstanceRef& Emitter : Emitters)
	{
		if (Emitter->GetEmitterHandle().GetName() == EmitterName)
		{
			return Emitter->GetFixedBounds();
		}
	}

	// Failed to find emitter
	UE_LOG(LogNiagara, Warning, TEXT("GetEmitterFixedBounds: Failed to find Emitter(%s) System(%s) Component(%s)"), *EmitterName.ToString(), *GetNameSafe(System), *GetFullNameSafe(AttachComponent.Get()));
	return FBox(ForceInit);
}

FNiagaraEmitterInstance* FNiagaraSystemInstance::GetEmitterByID(FNiagaraEmitterID InID)const
{
	//Currently the emitter ID is the direct index of the emitter in the Emitters array, though this may not always be the case.
	int32 EmitterIndex = InID.ID;
	return Emitters.IsValidIndex(EmitterIndex) ? &Emitters[EmitterIndex].Get() : nullptr;
}

void FNiagaraSystemInstance::SetForceSolo(bool bInForceSolo)
{
	// We may be forced into solo mode so if we match nothing to do here
	bForceSolo = bInForceSolo;
	if ( bSolo == bInForceSolo )
	{
		return;
	}

	if (bForceSolo != bSolo)
	{
		SetSolo(bForceSolo);
	}
}

void FNiagaraSystemInstance::EvaluateBoundFunction(FName FunctionName, bool& UsedOnCpu, bool& UsedOnGpu) const
{
	auto ScriptUsesFunction = [&](UNiagaraScript* Script)
	{
		if (Script)
		{
			const bool IsGpuScript = UNiagaraScript::IsGPUScript(Script->Usage);

			if (!UsedOnGpu && IsGpuScript)
			{
				for (const FNiagaraDataInterfaceGPUParamInfo& DIParamInfo : Script->GetDataInterfaceGPUParamInfos())
				{
					auto GpuFunctionPredicate = [&](const FNiagaraDataInterfaceGeneratedFunction& DIFunction)
					{
						return DIFunction.DefinitionName == FunctionName;
					};

					if (DIParamInfo.GeneratedFunctions.ContainsByPredicate(GpuFunctionPredicate))
					{
						UsedOnGpu = true;
						break;
					}
				}
			}

			if (!UsedOnCpu && !IsGpuScript)
			{
				auto CpuFunctionPredicate = [&](const FVMExternalFunctionBindingInfo& BindingInfo)
				{
					return BindingInfo.Name == FunctionName;
				};

				const FNiagaraVMExecutableData& VMExecData =  Script->GetVMExecutableData();
				if (VMExecData.CalledVMExternalFunctions.ContainsByPredicate(CpuFunctionPredicate))
				{
					UsedOnCpu = true;
				}
			}
		}
	};

	if (::IsValid(System))
	{
		System->ForEachScript(ScriptUsesFunction);
	}
}

#if WITH_EDITOR
FNiagaraSystemInstance::FOnInitialized& FNiagaraSystemInstance::OnInitialized()
{
	return OnInitializedDelegate;
}

FNiagaraSystemInstance::FOnReset& FNiagaraSystemInstance::OnReset()
{
	return OnResetDelegate;
}

FNiagaraSystemInstance::FOnDestroyed& FNiagaraSystemInstance::OnDestroyed()
{
	return OnDestroyedDelegate;
}
#endif

const FString& FNiagaraSystemInstance::GetCrashReporterTag()const
{
	if(CrashReporterTag.IsEmpty())
	{
		UNiagaraSystem* Sys = GetSystem();
		UNiagaraComponent* Component = Cast<UNiagaraComponent>(AttachComponent.Get());
		USceneComponent* AttachParent = Component ? Component->GetAttachParent() : AttachComponent.Get();

		const FString& CompName = GetFullNameSafe(Component);
		const FString& SystemName = GetFullNameSafe(Sys);
		const FString& AttachName = GetFullNameSafe(AttachParent);

		CrashReporterTag = FString::Printf(TEXT("SystemInstance | System: %s | bSolo: %s | Component: %s | AttachedTo: %s |"), *SystemName, IsSolo() ? TEXT("true") : TEXT("false"), *CompName, *AttachName);
	}
	return CrashReporterTag;
}

void FNiagaraSystemInstance::InitSystemState()
{
	const FNiagaraSystemStateData& SystemStateData = System->GetSystemStateData();
	SystemState_RandomStream.Initialize(RandomSeed + RandomSeedOffset);

	SystemState_LoopCount			= 0;
	SystemState_CurrentLoopDuration = SystemState_RandomStream.FRandRange(SystemStateData.LoopDuration.Min, SystemStateData.LoopDuration.Max);
	SystemState_CurrentLoopDelay	= SystemState_RandomStream.FRandRange(SystemStateData.LoopDelay.Min, SystemStateData.LoopDelay.Max);
	SystemState_CurrentLoopAgeStart	= 0.0f;
	SystemState_CurrentLoopAgeEnd	= SystemState_CurrentLoopAgeStart + SystemState_CurrentLoopDelay + SystemState_CurrentLoopDuration;
}

void FNiagaraSystemInstance::TickSystemState()
{
	const uint32 ParameterIndex = GetParameterIndex();
	const FNiagaraSystemParameters& CurrentSystemParameters = SystemParameters[ParameterIndex];
	ENiagaraExecutionState ExecutionState = static_cast<ENiagaraExecutionState>(CurrentSystemParameters.EngineExecutionState);

	if (ExecutionState == ENiagaraExecutionState::Active)
	{
		if (Age < SystemState_CurrentLoopAgeEnd)
		{
			return;
		}

		const FNiagaraSystemStateData& SystemStateData = System->GetSystemStateData();
		if (SystemStateData.bIgnoreSystemState == false)
		{
			const ENiagaraExecutionState InactiveExecutionState = SystemStateData.InactiveResponse == ENiagaraSystemInactiveResponse::Kill ? ENiagaraExecutionState::Complete : ENiagaraExecutionState::Inactive;

			if (SystemStateData.LoopBehavior == ENiagaraLoopBehavior::Once)
			{
				SetActualExecutionState(InactiveExecutionState);
				return;
			}

			// Keep looping until we find out which loop we are in as a small loop age + large DT could result in crossing multiple loops
			do
			{
				++SystemState_LoopCount;
				if (SystemStateData.LoopBehavior == ENiagaraLoopBehavior::Multiple && SystemState_LoopCount >= SystemStateData.LoopCount)
				{
					SetActualExecutionState(InactiveExecutionState);
					return;
				}

				if (SystemStateData.bRecalculateDurationEachLoop)
				{
					SystemState_CurrentLoopDuration = SystemState_RandomStream.FRandRange(SystemStateData.LoopDuration.Min, SystemStateData.LoopDuration.Max);
				}

				if (SystemStateData.bDelayFirstLoopOnly)
				{
					SystemState_CurrentLoopDelay = 0.0f;
				}
				else if (SystemStateData.bRecalculateDelayEachLoop)
				{
					SystemState_CurrentLoopDelay = SystemState_RandomStream.FRandRange(SystemStateData.LoopDelay.Min, SystemStateData.LoopDelay.Max);
				}

				SystemState_CurrentLoopAgeStart	= SystemState_CurrentLoopAgeEnd;
				SystemState_CurrentLoopAgeEnd	= SystemState_CurrentLoopAgeStart + SystemState_CurrentLoopDelay + SystemState_CurrentLoopDuration;
			} while (Age >= SystemState_CurrentLoopAgeEnd);
		}
	}
	// Waiting on emitters to complete
	//else if (ExecutionState == ENiagaraExecutionState::Inactive)
	//{
	//}

	// Update the actual execution state
	SetActualExecutionState(ExecutionState);
}
