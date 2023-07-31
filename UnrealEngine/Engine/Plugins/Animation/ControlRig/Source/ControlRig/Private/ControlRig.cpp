// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRig.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/RuntimeErrors.h"
#include "IControlRigObjectBinding.h"
#include "HelperUtil.h"
#include "ObjectTrace.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "ControlRigObjectVersion.h"
#include "Rigs/RigHierarchyController.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationPoseData.h"
#include "Internationalization/TextKey.h"
#if WITH_EDITOR
#include "ControlRigModule.h"
#include "Modules/ModuleManager.h"
#include "RigVMModel/RigVMNode.h"
#include "Engine/Blueprint.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "RigVMDeveloperTypeUtils.h"
#endif// WITH_EDITOR
#include "ControlRigComponent.h"
#include "Constraints/ControlRigTransformableHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRig)

#define LOCTEXT_NAMESPACE "ControlRig"

DEFINE_LOG_CATEGORY(LogControlRig);

DECLARE_STATS_GROUP(TEXT("ControlRig"), STATGROUP_ControlRig, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Control Rig Execution"), STAT_RigExecution, STATGROUP_ControlRig, );
DEFINE_STAT(STAT_RigExecution);

const FName UControlRig::OwnerComponent("OwnerComponent");

//CVar to specify if we should create a float control for each curve in the curve container
//By default we don't but it may be useful to do so for debugging
static TAutoConsoleVariable<int32> CVarControlRigCreateFloatControlsForCurves(
	TEXT("ControlRig.CreateFloatControlsForCurves"),
	0,
	TEXT("If nonzero we create a float control for each curve in the curve container, useful for debugging low level controls."),
	ECVF_Default);

// CVar to disable all control rig execution 
static TAutoConsoleVariable<int32> CVarControlRigDisableExecutionAll(TEXT("ControlRig.DisableExecutionAll"), 0, TEXT("if nonzero we disable all execution of Control Rigs."));

// CVar to disable swapping to nativized vms 
static TAutoConsoleVariable<int32> CVarControlRigDisableNativizedVMs(TEXT("ControlRig.DisableNativizedVMs"), 1, TEXT("if nonzero we disable swapping to nativized VMs."));

static bool bControlRigUseVMSnapshots = false;
static FAutoConsoleVariableRef CVarControlRigUseVMSnapshots(
	TEXT("ControlRig.UseVMSnapshots"),
	bControlRigUseVMSnapshots,
	TEXT("If True the VM will try to reuse previous initializations of the same rig."));

UControlRig::UControlRig(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DeltaTime(0.0f)
	, AbsoluteTime(0.0f)
	, FramesPerSecond(0.0f)
	, bAccumulateTime(true)
	, LatestExecutedState(EControlRigState::Invalid)
#if WITH_EDITOR
	, ControlRigLog(nullptr)
	, bEnableControlRigLogging(true)
#endif
#if WITH_EDITOR
	, bEnableAnimAttributeTrace(false)
#endif 
	, DataSourceRegistry(nullptr)
	, EventQueue()
	, EventQueueToRun()
#if WITH_EDITOR
	, PreviewInstance(nullptr)
#endif
	, bRequiresInitExecution(false)
	, bRequiresConstructionEvent(false)
	, bCopyHierarchyBeforeConstruction(true)
	, bResetInitialTransformsBeforeConstruction(true)
	, bManipulationEnabled(false)
	, InitBracket(0)
	, UpdateBracket(0)
	, PreConstructionBracket(0)
	, PostConstructionBracket(0)
	, InteractionBracket(0)
	, InterRigSyncBracket(0)
	, ControlUndoBracketIndex(0)
	, InteractionType((uint8)EControlRigInteractionType::None)
	, bInteractionJustBegan(false)
#if WITH_EDITORONLY_DATA
	, VMSnapshotBeforeExecution(nullptr)
#endif
	, DebugBoneRadiusMultiplier(1.f)
#if WITH_EDITOR
	, bRecordSelectionPoseForConstructionMode(true)
	, bIsClearingTransientControls(false)
#endif
#if UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM
	, ProfilingRunsLeft(0)
	, AccumulatedCycles(0)
#endif

{
	EventQueue.Add(FRigUnit_BeginExecution::EventName);
}

void UControlRig::BeginDestroy()
{
	Super::BeginDestroy();
	InitializedEvent.Clear();
	PreConstructionEvent.Clear();
	PostConstructionEvent.Clear();
	PreForwardsSolveEvent.Clear();
	PostForwardsSolveEvent.Clear();
	ExecutedEvent.Clear();
	SetInteractionRig(nullptr);

	if (VM)
	{
		VM->ExecutionReachedExit().RemoveAll(this);
	}

#if WITH_EDITOR
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		if(UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
		{
			if (!CDO->HasAnyFlags(RF_BeginDestroyed))
			{
				if (CDO->GetHierarchy())
				{
					CDO->GetHierarchy()->UnregisterListeningHierarchy(GetHierarchy());
				}
			}
		}
	}
#endif

#if WITH_EDITORONLY_DATA
	if (VMSnapshotBeforeExecution)
	{
		VMSnapshotBeforeExecution = nullptr;
	}
#endif

	// on destruction clear out the initialized snapshots
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		for (TPair<uint32, TObjectPtr<URigVM>>& Pair : InitializedVMSnapshots)
		{
			if (IsValid(Pair.Value))
			{
				TObjectPtr<URigVM> VMSnapshot = Pair.Value;
				VMSnapshot->RemoveFromRoot();				
			}
		}
		InitializedVMSnapshots.Reset();
	}

	TRACE_OBJECT_LIFETIME_END(this);
}

UWorld* UControlRig::GetWorld() const
{
	if (ObjectBinding.IsValid())
	{
		AActor* HostingActor = ObjectBinding->GetHostingActor();
		if (HostingActor)
		{
			return HostingActor->GetWorld();
		}

		UObject* Owner = ObjectBinding->GetBoundObject();
		if (Owner)
		{
			return Owner->GetWorld();
		}
	}

	UObject* Outer = GetOuter();
	if (Outer)
	{
		return Outer->GetWorld();
	}

	return nullptr;
}

void UControlRig::Initialize(bool bInitRigUnits)
{
	TRACE_OBJECT_LIFETIME_BEGIN(this);

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_Initialize);

	if(IsInitializing())
	{
		UE_LOG(LogControlRig, Warning, TEXT("%s: Initialize is being called recursively."), *GetPathName());
		return;
	}

	// recompute the hash used to differentiate VMs based on their memory layout 
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		CachedMemoryHash = 0;

		if(VM)
		{
			CachedMemoryHash = HashCombine(
				VM->GetLiteralMemory()->GetMemoryHash(),
				VM->GetWorkMemory()->GetMemoryHash()
			);
		}
	}

	if (IsTemplate())
	{
		// don't initialize template class 
		return;
	}

	InitializeFromCDO();
	InstantiateVMFromCDO();

	// Create the data source registry here to avoid UObject creation from Non-Game Threads
	GetDataSourceRegistry();

	// Create the Hierarchy Controller here to avoid UObject creation from Non-Game Threads
	GetHierarchy()->GetController(true);
	
	// should refresh mapping 
	RequestConstruction();

	if (bInitRigUnits)
	{
		RequestInit();
	}
	
	GetHierarchy()->OnModified().RemoveAll(this);
	GetHierarchy()->OnModified().AddUObject(this, &UControlRig::HandleHierarchyModified);
	GetHierarchy()->OnEventReceived().RemoveAll(this);
	GetHierarchy()->OnEventReceived().AddUObject(this, &UControlRig::HandleHierarchyEvent);
	GetHierarchy()->UpdateVisibilityOnProxyControls();
}

void UControlRig::InitializeFromCDO()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// copy CDO property you need to here
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// similar to FControlRigBlueprintCompilerContext::CopyTermDefaultsToDefaultObject,
		// where CDO is initialized from BP there,
		// we initialize all other instances of Control Rig from the CDO here
		UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();

		// copy hierarchy
		{
			PostInitInstanceIfRequired();

			FRigHierarchyValidityBracket ValidityBracketA(GetHierarchy());
			FRigHierarchyValidityBracket ValidityBracketB(CDO->GetHierarchy());
			
			TGuardValue<bool> Guard(GetHierarchy()->GetSuspendNotificationsFlag(), true);
			GetHierarchy()->CopyHierarchy(CDO->GetHierarchy());
			GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
		}

#if WITH_EDITOR
		// current hierarchy should always mirror CDO's hierarchy whenever a change of interest happens
		CDO->GetHierarchy()->RegisterListeningHierarchy(GetHierarchy());
#endif

		// notify clients that the hierarchy has changed
		GetHierarchy()->Notify(ERigHierarchyNotification::HierarchyReset, nullptr);

		// copy draw container
		DrawContainer = CDO->DrawContainer;

		// copy hierarchy settings
		HierarchySettings = CDO->HierarchySettings;
		
		// increment the procedural limit based on the number of elements in the CDO
		if(const URigHierarchy* CDOHierarchy = CDO->GetHierarchy())
		{
			HierarchySettings.ProceduralElementLimit += CDOHierarchy->Num();
		}

		// copy vm settings
		VMRuntimeSettings = CDO->VMRuntimeSettings;
	}
}

void UControlRig::Evaluate_AnyThread()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_Evaluate);

	// we can have other systems trying to poke into running instances of Control Rigs
	// on the anim thread and query data, such as
	// UControlRigSkeletalMeshComponent::RebuildDebugDrawSkeleton,
	// using a lock here to prevent them from having an inconsistent view of the rig at some
	// intermediate stage of evaluation, for example, during evaluate, we can have a call
	// to copy hierarchy, which empties the hierarchy for a short period of time
	// and we don't want other systems to see that.
	FScopeLock EvaluateLock(&GetEvaluateMutex());
	
	// create a copy since we need to change it here temporarily,
	// and UI / the rig may change the event queue while it is running
	EventQueueToRun = EventQueue;

	if(InteractionType != (uint8)EControlRigInteractionType::None)
	{
		if(EventQueueToRun.IsEmpty())
		{
			EventQueueToRun.Add(FRigUnit_InteractionExecution::EventName);
		}
		else if(!EventQueueToRun.Contains(FRigUnit_PrepareForExecution::EventName))
		{
			// insert just before the last event so the interaction runs prior to
			// forward solve or backwards solve.
			EventQueueToRun.Insert(FRigUnit_InteractionExecution::EventName, EventQueueToRun.Num() - 1);
		}
	}

	// execute the construction event prior to everything else
	if(bRequiresConstructionEvent)
	{
		if(!EventQueueToRun.Contains(FRigUnit_PrepareForExecution::EventName))
		{
			EventQueueToRun.Insert(FRigUnit_PrepareForExecution::EventName, 0);
		}
	}
	
	for (const FName& EventName : EventQueueToRun)
	{
		Execute(EControlRigState::Update, EventName);

#if WITH_EDITOR
		if (VM)
		{
			const FRigVMBreakpoint& Breakpoint = VM->GetHaltedAtBreakpoint(); 
			if (Breakpoint.IsValid())
			{
				// make sure that the instruction index for the breakpoint is part
				// of the current entry.
				const FRigVMByteCode& ByteCode = VM->GetByteCode();
				const int32 CurrentEntryIndex = ByteCode.FindEntryIndex(EventName);
				if(CurrentEntryIndex != INDEX_NONE)
				{
					const FRigVMByteCodeEntry& CurrentEntry = ByteCode.GetEntry(CurrentEntryIndex);
					if(Breakpoint.InstructionIndex >= CurrentEntry.InstructionIndex)
					{
						int32 LastInstructionIndexInEntry;
						if(CurrentEntryIndex == ByteCode.NumEntries() - 1)
						{
							LastInstructionIndexInEntry = ByteCode.GetNumInstructions();
						}
						else
						{
							const int32 NextEntryIndex = CurrentEntryIndex + 1;
							LastInstructionIndexInEntry = ByteCode.GetEntry(NextEntryIndex).InstructionIndex - 1;
						}

						if(Breakpoint.InstructionIndex <= LastInstructionIndexInEntry)
						{
							break;
						}
					}
				}
			}
		}
#endif
	}

	EventQueueToRun.Reset();
}


TArray<FRigVMExternalVariable> UControlRig::GetExternalVariables() const
{
	return GetExternalVariablesImpl(true);
}

TArray<FRigVMExternalVariable> UControlRig::GetExternalVariablesImpl(bool bFallbackToBlueprint) const
{
	TArray<FRigVMExternalVariable> ExternalVariables;

	for (TFieldIterator<FProperty> PropertyIt(GetClass()); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if(Property->IsNative())
		{
			continue;
		}

		FRigVMExternalVariable ExternalVariable = FRigVMExternalVariable::Make(Property, (UObject*)this);
		if(!ExternalVariable.IsValid())
		{
			UE_LOG(LogControlRig, Warning, TEXT("%s: Property '%s' of type '%s' is not supported."), *GetClass()->GetName(), *Property->GetName(), *Property->GetCPPType());
			continue;
		}

		ExternalVariables.Add(ExternalVariable);
	}

#if WITH_EDITOR

	if (bFallbackToBlueprint)
	{
		// if we have a difference in the blueprint variables compared to us - let's 
		// use those instead. the assumption here is that the blueprint is dirty and
		// hasn't been compiled yet.
		if (UBlueprint* Blueprint = Cast<UBlueprint>(GetClass()->ClassGeneratedBy))
		{
			TArray<FRigVMExternalVariable> BlueprintVariables;
			for (const FBPVariableDescription& VariableDescription : Blueprint->NewVariables)
			{
				FRigVMExternalVariable ExternalVariable = RigVMTypeUtils::ExternalVariableFromBPVariableDescription(VariableDescription);
				if (ExternalVariable.TypeName.IsNone())
				{
					continue;
				}

				ExternalVariable.Memory = nullptr;

				BlueprintVariables.Add(ExternalVariable);
			}

			if (ExternalVariables.Num() != BlueprintVariables.Num())
			{
				return BlueprintVariables;
			}

			TMap<FName, int32> NameMap;
			for (int32 Index = 0; Index < ExternalVariables.Num(); Index++)
			{
				NameMap.Add(ExternalVariables[Index].Name, Index);
			}

			for (FRigVMExternalVariable BlueprintVariable : BlueprintVariables)
			{
				const int32* Index = NameMap.Find(BlueprintVariable.Name);
				if (Index == nullptr)
				{
					return BlueprintVariables;
				}

				FRigVMExternalVariable ExternalVariable = ExternalVariables[*Index];
				if (ExternalVariable.bIsArray != BlueprintVariable.bIsArray ||
					ExternalVariable.bIsPublic != BlueprintVariable.bIsPublic ||
					ExternalVariable.TypeName != BlueprintVariable.TypeName ||
					ExternalVariable.TypeObject != BlueprintVariable.TypeObject)
				{
					return BlueprintVariables;
				}
			}
		}
	}
#endif

	return ExternalVariables;
}

UControlRig::FAnimAttributeContainerPtrScope::FAnimAttributeContainerPtrScope(UControlRig* InControlRig,
	UE::Anim::FStackAttributeContainer& InExternalContainer)
{
	ControlRig = InControlRig;
	ControlRig->ExternalAnimAttributeContainer = &InExternalContainer;
}

UControlRig::FAnimAttributeContainerPtrScope::~FAnimAttributeContainerPtrScope()
{
	// control rig should not hold on to this container since it is stack allocated
	// and should not be used outside of stack, see FPoseContext
	ControlRig->ExternalAnimAttributeContainer = nullptr;
}

TArray<FRigVMExternalVariable> UControlRig::GetPublicVariables() const
{
	TArray<FRigVMExternalVariable> PublicVariables;
	TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariables();
	for (const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		if (ExternalVariable.bIsPublic)
		{
			PublicVariables.Add(ExternalVariable);
		}
	}
	return PublicVariables;
}

FRigVMExternalVariable UControlRig::GetPublicVariableByName(const FName& InVariableName) const
{
	if (FProperty* Property = GetPublicVariableProperty(InVariableName))
	{
		return FRigVMExternalVariable::Make(Property, (UObject*)this);
	}
	return FRigVMExternalVariable();
}

TArray<FName> UControlRig::GetScriptAccessibleVariables() const
{
	TArray<FRigVMExternalVariable> PublicVariables = GetPublicVariables();
	TArray<FName> Names;
	for (const FRigVMExternalVariable& PublicVariable : PublicVariables)
	{
		Names.Add(PublicVariable.Name);
	}
	return Names;
}

FName UControlRig::GetVariableType(const FName& InVariableName) const
{
	FRigVMExternalVariable PublicVariable = GetPublicVariableByName(InVariableName);
	if (PublicVariable.IsValid(true /* allow nullptr */))
	{
		return PublicVariable.TypeName;
	}
	return NAME_None;
}

FString UControlRig::GetVariableAsString(const FName& InVariableName) const
{
#if WITH_EDITOR
	if (const FProperty* Property = GetClass()->FindPropertyByName(InVariableName))
	{
		FString Result;
		const uint8* Container = (const uint8*)this;
		if (FBlueprintEditorUtils::PropertyValueToString(Property, Container, Result, nullptr))
		{
			return Result;
		}
	}
#endif
	return FString();
}

bool UControlRig::SetVariableFromString(const FName& InVariableName, const FString& InValue)
{
#if WITH_EDITOR
	if (const FProperty* Property = GetClass()->FindPropertyByName(InVariableName))
	{
		uint8* Container = (uint8*)this;
		return FBlueprintEditorUtils::PropertyValueFromString(Property, InValue, Container, nullptr);
	}
#endif
	return false;
}

bool UControlRig::SupportsEvent(const FName& InEventName) const
{
	if (VM)
	{
		return VM->ContainsEntry(InEventName);
	}
	return false;
}

TArray<FName> UControlRig::GetSupportedEvents() const
{
	if (VM)
	{
		return VM->GetEntryNames();
	}
	return TArray<FName>();
}

AActor* UControlRig::GetHostingActor() const
{
	return ObjectBinding ? ObjectBinding->GetHostingActor() : nullptr;
}

#if WITH_EDITOR
FText UControlRig::GetCategory() const
{
	return LOCTEXT("DefaultControlRigCategory", "Animation|ControlRigs");
}

FText UControlRig::GetToolTipText() const
{
	return LOCTEXT("DefaultControlRigTooltip", "ControlRig");
}
#endif

void UControlRig::SetDeltaTime(float InDeltaTime)
{
	DeltaTime = InDeltaTime;
}

void UControlRig::SetAbsoluteTime(float InAbsoluteTime, bool InSetDeltaTimeZero)
{
	if(InSetDeltaTimeZero)
	{
		DeltaTime = 0.f;
	}
	AbsoluteTime = InAbsoluteTime;
	bAccumulateTime = false;
}

void UControlRig::SetAbsoluteAndDeltaTime(float InAbsoluteTime, float InDeltaTime)
{
	AbsoluteTime = InAbsoluteTime;
	DeltaTime = InDeltaTime;
}

void UControlRig::SetFramesPerSecond(float InFramesPerSecond)
{
	FramesPerSecond = InFramesPerSecond;	
}

float UControlRig::GetCurrentFramesPerSecond() const
{
	if(FramesPerSecond > SMALL_NUMBER)
	{
		return FramesPerSecond;
	}
	if(DeltaTime > SMALL_NUMBER)
	{
		return 1.f / DeltaTime;
	}
	return 60.f;
}

void UControlRig::InstantiateVMFromCDO()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		SwapVMToNativizedIfRequired();

		UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
		if (VM && CDO && CDO->VM)
		{
			if(!VM->IsNativized())
			{
				// reference the literal memory + byte code
				// only defer if called from worker thread,
				// which should be unlikely
				VM->CopyFrom(CDO->VM, !IsInGameThread(), true);
			}
		}
		else if (VM)
		{
			VM->Reset();
		}
		else
		{
			ensure(false);
		}
	}

	if(VM)
	{
		CachedMemoryHash = HashCombine(
			VM->GetLiteralMemory()->GetMemoryHash(),
			VM->GetWorkMemory()->GetMemoryHash()
		);
	}

	RequestInit();
}

void UControlRig::CopyExternalVariableDefaultValuesFromCDO()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
		TArray<FRigVMExternalVariable> CurrentVariables = GetExternalVariablesImpl(false);
		TArray<FRigVMExternalVariable> CDOVariables = CDO->GetExternalVariablesImpl(false);
		if (ensure(CurrentVariables.Num() == CDOVariables.Num()))
		{
			for (int32 i=0; i<CurrentVariables.Num(); ++i)
			{
				FRigVMExternalVariable& Variable = CurrentVariables[i];
				FRigVMExternalVariable& CDOVariable = CDOVariables[i];
				Variable.Property->CopyCompleteValue(Variable.Memory, CDOVariable.Memory);
			}
		}
	}
}

bool UControlRig::Execute(const EControlRigState InState, const FName& InEventName)
{
	if(!CanExecute())
	{
		return false;
	}

	if(EventQueueToRun.IsEmpty())
	{
		EventQueueToRun = EventQueue;
	}
	
	const bool bIsEventInQueue = EventQueueToRun.Contains(InEventName);
	const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
	const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;
	const bool bIsInitializingMemory = InState == EControlRigState::Init;
	const bool bIsExecutingInstructions = InState == EControlRigState::Update;
	const bool bIsConstructionEvent = InEventName == FRigUnit_PrepareForExecution::EventName;
	const bool bIsForwardSolve = InEventName == FRigUnit_BeginExecution::EventName;
	const bool bIsInteractionEvent = InEventName == FRigUnit_InteractionExecution::EventName;

	ensure(!HasAnyFlags(RF_ClassDefaultObject));
	
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ControlRig_Execute);
	
	LatestExecutedState = InState;

	if (VM)
	{
		if (VM->GetOuter() != this)
		{
			InstantiateVMFromCDO();
		}

		if (bIsInitializingMemory)
		{
			VM->ClearExternalVariables();

			TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariablesImpl(false);
			for (FRigVMExternalVariable ExternalVariable : ExternalVariables)
			{
				VM->AddExternalVariable(ExternalVariable);
			}
			
#if WITH_EDITOR
			// setup the hierarchy's controller log function
			if(URigHierarchyController* HierarchyController = GetHierarchy()->GetController(true))
			{
				HierarchyController->LogFunction = [this](EMessageSeverity::Type InSeverity, const FString& Message)
				{
					const FRigVMExtendedExecuteContext& Context = GetVM()->GetContext();
					if(ControlRigLog)
					{
						ControlRigLog->Report(InSeverity, Context.PublicData.FunctionName, Context.PublicData.InstructionIndex, Message);
					}
					else
					{
						LogOnce(InSeverity, Context.PublicData.InstructionIndex, Message);
					}
				};
			}
#endif
		}
#if WITH_EDITOR
		// default to always clear data after each execution
		// only set a valid first entry event later when execution
		// has passed the initialization stage and there are multiple events present in one evaluation
		// first entry event is used to determined when to clear data during an evaluation
		VM->SetFirstEntryEventInEventQueue(NAME_None);
#endif
	}

#if WITH_EDITOR
	if (bIsInDebugMode)
	{
		if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
		{
			// Copy the breakpoints. This will not override the state of the breakpoints
			DebugInfo.SetBreakpoints(CDO->DebugInfo.GetBreakpoints());

			// If there are any breakpoints, create the Snapshot VM if it hasn't been created yet
			if (DebugInfo.GetBreakpoints().Num() > 0)
			{
				GetSnapshotVM();
			}
		}

		if(VM)
		{
			VM->SetDebugInfo(&DebugInfo);
		}
	}
	else if(VM)
	{
		VM->SetDebugInfo(nullptr);
	}
#endif

	bool bJustRanInit = false;
	if (bRequiresInitExecution)
	{
		bRequiresInitExecution = false;

		if (!bIsInitializingMemory)
		{
			// if init is required we'll run init on the whole bytecode
			if(!Execute(EControlRigState::Init, NAME_None))
			{
				return false;
			}
			bJustRanInit = true;
		}
	}

	FRigUnitContext Context;

	// setup the draw interface for debug drawing
	if(!bIsEventInQueue || bIsEventFirstInQueue)
	{
		DrawInterface.Reset();
	}
	Context.DrawInterface = &DrawInterface;

	// setup the animation attribute container
	Context.AnimAttributeContainer = ExternalAnimAttributeContainer;

	// draw container contains persistent draw instructions, 
	// so we cannot call Reset(), which will clear them,
	// instead, we re-initialize them from the CDO
	if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
	{
		DrawContainer = CDO->DrawContainer;
	}
	Context.DrawContainer = &DrawContainer;

	// setup the data source registry
	Context.DataSourceRegistry = GetDataSourceRegistry();

	// reset the time and caches during init
	if (bIsInitializingMemory)
	{
		AbsoluteTime = DeltaTime = 0.f;
		NameCache.Reset();
	}

	// setup the context with further fields
	Context.DeltaTime = DeltaTime;
	Context.AbsoluteTime = AbsoluteTime;
	Context.FramesPerSecond = GetCurrentFramesPerSecond();
	Context.InteractionType = InteractionType;
	Context.ElementsBeingInteracted = ElementsBeingInteracted;
	Context.State = InState;

	// allow access to the hierarchy
	Context.Hierarchy = GetHierarchy();
	Context.HierarchySettings = HierarchySettings;
	check(Context.Hierarchy);

	// allow access to the default hierarchy to allow to reset
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UControlRig* CDO = Cast<UControlRig>(GetClass()->GetDefaultObject()))
		{
			if(URigHierarchy* DefaultHierarchy = CDO->GetHierarchy())
			{
				Context.Hierarchy->DefaultHierarchyPtr = DefaultHierarchy;
			}
		}
	}

	// disable any controller access outside of the construction event
	FRigHierarchyEnableControllerBracket DisableHierarchyController(Context.Hierarchy, bIsConstructionEvent);

	// setup the context with further fields
	Context.ToWorldSpaceTransform = FTransform::Identity;
	Context.OwningComponent = nullptr;
	Context.OwningActor = nullptr;
	Context.World = nullptr;
	Context.NameCache = &NameCache;

	if (!OuterSceneComponent.IsValid())
	{
		USceneComponent* SceneComponentFromRegistry = Context.DataSourceRegistry->RequestSource<USceneComponent>(UControlRig::OwnerComponent);
		if (SceneComponentFromRegistry)
		{
			OuterSceneComponent = SceneComponentFromRegistry;
		}
		else
		{
			UObject* Parent = this;
			while (Parent)
			{
				Parent = Parent->GetOuter();
				if (Parent)
				{
					if (USceneComponent* SceneComponent = Cast<USceneComponent>(Parent))
					{
						OuterSceneComponent = SceneComponent;
						break;
					}
				}
			}
		}
	}

	// given the outer scene component configure
	// the transform lookups to map transforms from rig space to world space
	if (OuterSceneComponent.IsValid())
	{
		Context.ToWorldSpaceTransform = OuterSceneComponent->GetComponentToWorld();
		Context.OwningComponent = OuterSceneComponent.Get();
		Context.OwningActor = Context.OwningComponent->GetOwner();
		Context.World = Context.OwningComponent->GetWorld();
	}
	else
	{
		if (ObjectBinding.IsValid())
		{
			AActor* HostingActor = ObjectBinding->GetHostingActor();
			if (HostingActor)
			{
				Context.OwningActor = HostingActor;
				Context.World = HostingActor->GetWorld();
			}
			else if (UObject* Owner = ObjectBinding->GetBoundObject())
			{
				Context.World = Owner->GetWorld();
			}
		}

		if (Context.World == nullptr)
		{
			if (UObject* Outer = GetOuter())
			{
				Context.World = Outer->GetWorld();
			}
		}
	}

	// if we have any referenced elements dirty them
	if(GetHierarchy())
	{
		GetHierarchy()->UpdateReferences(&Context);
	}

#if WITH_EDITOR
	// setup the log and VM settings
	Context.Log = ControlRigLog;
	if (ControlRigLog != nullptr)
	{
		ControlRigLog->Reset();
		UpdateVMSettings();
	}
#endif

	// guard against recursion
	if(IsExecuting())
	{
		UE_LOG(LogControlRig, Warning, TEXT("%s: Execute is being called recursively."), *GetPathName());
		return false;
	}
	if(bIsConstructionEvent)
	{
		if(IsRunningPreConstruction() || IsRunningPostConstruction())
		{
			UE_LOG(LogControlRig, Warning, TEXT("%s: Construction is being called recursively."), *GetPathName());
			return false;
		}
	}

	bool bSuccess = true;

	// we'll special case the construction event here
	if (bIsConstructionEvent && !bIsInitializingMemory)
	{
		// remember the previous selection
		const TArray<FRigElementKey> PreviousSelection = GetHierarchy()->GetSelectedKeys();

		// construction mode means that we are running the construction event
		// constantly for testing purposes.
		const bool bConstructionModeEnabled = IsConstructionModeEnabled();
		{
			if (PreConstructionForUIEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PreConstructionBracket);
				PreConstructionForUIEvent.Broadcast(this, EControlRigState::Update, FRigUnit_PrepareForExecution::EventName);
			}

#if WITH_EDITOR
			// apply the selection pose for the construction mode
			// we are doing this here to make sure the brackets below have the right data
			if(bConstructionModeEnabled)
			{
				ApplySelectionPoseForConstructionMode(InEventName);
			}
#endif

			// disable selection notifications from the hierarchy
			TGuardValue<bool> DisableSelectionNotifications(GetHierarchy()->GetController(true)->bSuspendSelectionNotifications, true);
			{				
				// save the current state of all pose elements to preserve user intention, since construction event can
				// run in between forward events
				// the saved pose is reapplied to the rig after construction event as the pose scope goes out of scope
				TUniquePtr<FPoseScope> PoseScope;
				if (!bConstructionModeEnabled)
				{
					// only do this in non-construction mode because 
					// when construction mode is enabled, the control values are cleared before reaching here (too late to save them)
					PoseScope = MakeUnique<FPoseScope>(this, ERigElementType::ToResetAfterConstructionEvent);
				}
				
				{
					// Copy the hierarchy from the default object onto this one
#if WITH_EDITOR
					FTransientControlScope TransientControlScope(GetHierarchy());
	#endif
					{
						// maintain the initial pose if it ever was set by the client
						FRigPose InitialPose;
						if(!bResetInitialTransformsBeforeConstruction)
						{
							InitialPose = GetHierarchy()->GetPose(true, ERigElementType::ToResetAfterConstructionEvent, FRigElementKeyCollection());
						}

						if(bCopyHierarchyBeforeConstruction)
						{
							GetHierarchy()->ResetToDefault();
						}

						if(InitialPose.Num() > 0)
						{
							GetHierarchy()->SetPose(InitialPose, ERigTransformType::InitialLocal);
						}
					}

					{
	#if WITH_EDITOR
						TUniquePtr<FTransientControlPoseScope> TransientControlPoseScope;
						if (bConstructionModeEnabled)
						{
							// save the transient control value, it should not be constantly reset in construction mode
							TransientControlPoseScope = MakeUnique<FTransientControlPoseScope>(this);
						}
	#endif
						// reset the pose to initial such that construction event can run from a deterministic initial state
						GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
					}

					if (PreConstructionEvent.IsBound())
					{
						FControlRigBracketScope BracketScope(PreConstructionBracket);
						PreConstructionEvent.Broadcast(this, EControlRigState::Update, FRigUnit_PrepareForExecution::EventName);
					}

					bSuccess = ExecuteUnits(Context, FRigUnit_PrepareForExecution::EventName);
					
				} // destroy FTransientControlScope
				
				if (PostConstructionEvent.IsBound())
				{
					FControlRigBracketScope BracketScope(PostConstructionBracket);
					PostConstructionEvent.Broadcast(this, EControlRigState::Update, FRigUnit_PrepareForExecution::EventName);
				}
			}
			
			// set it here to reestablish the selection. the notifications
			// will be eaten since we still have the bSuspend flag on in the controller.
			GetHierarchy()->GetController()->SetSelection(PreviousSelection);
			
		} // destroy DisableSelectionNotifications

		if (bConstructionModeEnabled)
		{
#if WITH_EDITOR
			TUniquePtr<FTransientControlPoseScope> TransientControlPoseScope;
			if (bConstructionModeEnabled)
			{
				// save the transient control value, it should not be constantly reset in construction mode
				TransientControlPoseScope = MakeUnique<FTransientControlPoseScope>(this);
			}
#endif
			GetHierarchy()->ResetPoseToInitial(ERigElementType::Bone);
		}

		// synchronize the selection now with the new hierarchy after running construction
		const TArray<const FRigBaseElement*> CurrentSelection = GetHierarchy()->GetSelectedElements();
		for(const FRigBaseElement* SelectedElement : CurrentSelection)
		{
			if(!PreviousSelection.Contains(SelectedElement->GetKey()))
			{
				GetHierarchy()->Notify(ERigHierarchyNotification::ElementSelected, SelectedElement);
			}
		}
		for(const FRigElementKey& PreviouslySelectedKey : PreviousSelection)
		{
			if(const FRigBaseElement* PreviouslySelectedElement = GetHierarchy()->Find(PreviouslySelectedKey))
			{
				if(!CurrentSelection.Contains(PreviouslySelectedElement))
				{
					GetHierarchy()->Notify(ERigHierarchyNotification::ElementDeselected, PreviouslySelectedElement);
				}
			}
		}
	}
	else
	{
#if WITH_EDITOR
		// only set a valid first entry event when execution
		// has passed the initialization stage and there are multiple events present
		if (EventQueueToRun.Num() >= 2 && VM && !bIsInitializingMemory)
		{
			VM->SetFirstEntryEventInEventQueue(EventQueueToRun[0]);
		}

		// Transform Overrride is generated using a Transient Control 
		ApplyTransformOverrideForUserCreatedBones();

		if (bEnableAnimAttributeTrace && ExternalAnimAttributeContainer != nullptr)
		{
			InputAnimAttributeSnapshot.CopyFrom(*ExternalAnimAttributeContainer);
		}
#endif
		
		if (bIsExecutingInstructions && bIsForwardSolve)
		{
			if (PreForwardsSolveEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PreForwardsSolveBracket);
				PreForwardsSolveEvent.Broadcast(this, EControlRigState::Update, FRigUnit_BeginExecution::EventName);
			}
		}

		bSuccess = ExecuteUnits(Context, InEventName);

#if WITH_EDITOR
		if (bEnableAnimAttributeTrace && ExternalAnimAttributeContainer != nullptr)
		{
			OutputAnimAttributeSnapshot.CopyFrom(*ExternalAnimAttributeContainer);
		}
#endif

		if (bIsExecutingInstructions && bIsForwardSolve)
		{
			if (PostForwardsSolveEvent.IsBound())
			{
				FControlRigBracketScope BracketScope(PostForwardsSolveBracket);
				PostForwardsSolveEvent.Broadcast(this, EControlRigState::Update, FRigUnit_BeginExecution::EventName);
			}
		}
		
		if (bIsInitializingMemory && !bIsForwardSolve)
		{
			bSuccess = ExecuteUnits(Context, FRigUnit_BeginExecution::EventName);
		}
	}

#if WITH_EDITOR

	// for the last event in the queue - clear the log message queue
	if (ControlRigLog != nullptr && bEnableControlRigLogging && !bIsInitializingMemory)
	{
		if (bJustRanInit)
		{
			ControlRigLog->KnownMessages.Reset();
			LoggedMessages.Reset();
		}
		else if(bIsEventLastInQueue)
		{
			for (const FControlRigLog::FLogEntry& Entry : ControlRigLog->Entries)
			{
				if (Entry.FunctionName == NAME_None || Entry.InstructionIndex == INDEX_NONE || Entry.Message.IsEmpty())
				{
					continue;
				}

				FString PerInstructionMessage = 
					FString::Printf(
						TEXT("Instruction[%d] '%s': '%s'"),
						Entry.InstructionIndex,
						*Entry.FunctionName.ToString(),
						*Entry.Message
					);

				LogOnce(Entry.Severity, Entry.InstructionIndex, PerInstructionMessage);
			}
		}
	}
#endif

	if (bIsInitializingMemory)
	{
		if (InitializedEvent.IsBound())
		{
			FControlRigBracketScope BracketScope(InitBracket);
			InitializedEvent.Broadcast(this, EControlRigState::Init, InEventName);
		}
	}
	else if (bIsExecutingInstructions)
	{
		if(!bIsEventInQueue || bIsEventLastInQueue) 
		{
			DeltaTime = 0.f;
		}

		if (ExecutedEvent.IsBound())
		{
			FControlRigBracketScope BracketScope(UpdateBracket);
			ExecutedEvent.Broadcast(this, EControlRigState::Update, InEventName);
		}
	}

	// close remaining undo brackets from hierarchy
	while(ControlUndoBracketIndex > 0)
	{
		FRigEventContext EventContext;
		EventContext.Event = ERigEvent::CloseUndoBracket;
		EventContext.SourceEventName = InEventName;
		EventContext.LocalTime = Context.AbsoluteTime;
		HandleHierarchyEvent(GetHierarchy(), EventContext);
	}

	if (Context.DrawInterface && Context.DrawContainer && bIsEventLastInQueue && bIsExecutingInstructions) 
	{
		Context.DrawInterface->Instructions.Append(Context.DrawContainer->Instructions);

		FRigHierarchyValidityBracket ValidityBracket(GetHierarchy());
		
		GetHierarchy()->ForEach<FRigControlElement>([this](FRigControlElement* ControlElement) -> bool
		{
			const FRigControlSettings& Settings = ControlElement->Settings;

			if (Settings.IsVisible() &&
				!Settings.bIsTransientControl &&
				Settings.bDrawLimits &&
				Settings.LimitEnabled.Contains(FRigControlLimitEnabled(true, true)))
			{
				FTransform Transform = GetHierarchy()->GetGlobalControlOffsetTransformByIndex(ControlElement->GetIndex());
				FControlRigDrawInstruction Instruction(EControlRigDrawSettings::Lines, Settings.ShapeColor, 0.f, Transform);

				switch (Settings.ControlType)
				{
					case ERigControlType::Float:
					{
						if(Settings.LimitEnabled[0].IsOff())
						{
							break;
						}

						FVector MinPos = FVector::ZeroVector;
						FVector MaxPos = FVector::ZeroVector;

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								MinPos.X = Settings.MinimumValue.Get<float>();
								MaxPos.X = Settings.MaximumValue.Get<float>();
								break;
							}
							case ERigControlAxis::Y:
							{
								MinPos.Y = Settings.MinimumValue.Get<float>();
								MaxPos.Y = Settings.MaximumValue.Get<float>();
								break;
							}
							case ERigControlAxis::Z:
							{
								MinPos.Z = Settings.MinimumValue.Get<float>();
								MaxPos.Z = Settings.MaximumValue.Get<float>();
								break;
							}
						}

						Instruction.Positions.Add(MinPos);
						Instruction.Positions.Add(MaxPos);
						break;
					}
					case ERigControlType::Integer:
					{
						if(Settings.LimitEnabled[0].IsOff())
						{
							break;
						}

						FVector MinPos = FVector::ZeroVector;
						FVector MaxPos = FVector::ZeroVector;

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								MinPos.X = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.X = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
							case ERigControlAxis::Y:
							{
								MinPos.Y = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.Y = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
							case ERigControlAxis::Z:
							{
								MinPos.Z = (float)Settings.MinimumValue.Get<int32>();
								MaxPos.Z = (float)Settings.MaximumValue.Get<int32>();
								break;
							}
						}

						Instruction.Positions.Add(MinPos);
						Instruction.Positions.Add(MaxPos);
						break;
					}
					case ERigControlType::Vector2D:
					{
						if(Settings.LimitEnabled.Num() < 2)
						{
							break;
						}
						if(Settings.LimitEnabled[0].IsOff() && Settings.LimitEnabled[1].IsOff())
						{
							break;
						}

						Instruction.PrimitiveType = EControlRigDrawSettings::LineStrip;
						FVector3f MinPos = Settings.MinimumValue.Get<FVector3f>();
						FVector3f MaxPos = Settings.MaximumValue.Get<FVector3f>();

						switch (Settings.PrimaryAxis)
						{
							case ERigControlAxis::X:
							{
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MinPos.Y));
								Instruction.Positions.Add(FVector(0.f, MaxPos.X, MinPos.Y));
								Instruction.Positions.Add(FVector(0.f, MaxPos.X, MaxPos.Y));
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MaxPos.Y));
								Instruction.Positions.Add(FVector(0.f, MinPos.X, MinPos.Y));
								break;
							}
							case ERigControlAxis::Y:
							{
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MinPos.Y));
								Instruction.Positions.Add(FVector(MaxPos.X, 0.f, MinPos.Y));
								Instruction.Positions.Add(FVector(MaxPos.X, 0.f, MaxPos.Y));
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MaxPos.Y));
								Instruction.Positions.Add(FVector(MinPos.X, 0.f, MinPos.Y));
								break;
							}
							case ERigControlAxis::Z:
							{
								Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, 0.f));
								Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, 0.f));
								break;
							}
						}
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						FVector3f MinPos = FVector3f::ZeroVector;
						FVector3f MaxPos = FVector3f::ZeroVector;

						// we only check the first three here
						// since we only consider translation anyway
						// for scale it's also the first three
						if(Settings.LimitEnabled.Num() < 3)
						{
							break;
						}
						if(!Settings.LimitEnabled[0].IsOn() && !Settings.LimitEnabled[1].IsOn() && !Settings.LimitEnabled[2].IsOn())
						{
							break;
						}

						switch (Settings.ControlType)
						{
							case ERigControlType::Position:
							case ERigControlType::Scale:
							{
								MinPos = Settings.MinimumValue.Get<FVector3f>();
								MaxPos = Settings.MaximumValue.Get<FVector3f>();
								break;
							}
							case ERigControlType::Transform:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FTransform_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FTransform_Float>().GetTranslation();
								break;
							}
							case ERigControlType::TransformNoScale:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FTransformNoScale_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FTransformNoScale_Float>().GetTranslation();
								break;
							}
							case ERigControlType::EulerTransform:
							{
								MinPos = Settings.MinimumValue.Get<FRigControlValue::FEulerTransform_Float>().GetTranslation();
								MaxPos = Settings.MaximumValue.Get<FRigControlValue::FEulerTransform_Float>().GetTranslation();
								break;
							}
						}

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));

						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MinPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MinPos.X, MaxPos.Y, MaxPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MinPos.Z));
						Instruction.Positions.Add(FVector(MaxPos.X, MaxPos.Y, MaxPos.Z));
						break;
					}
				}

				if (Instruction.Positions.Num() > 0)
				{
					DrawInterface.Instructions.Add(Instruction);
				}
			}

			return true;
		});
	}

	if(bIsInteractionEvent)
	{
		bInteractionJustBegan = false;
	}

	if(bIsConstructionEvent)
	{
		bRequiresConstructionEvent = false;
	}

	return bSuccess;
}

bool UControlRig::ExecuteUnits(FRigUnitContext& InOutContext, const FName& InEventName)
{
	if (VM)
	{
		static constexpr TCHAR InvalidatedVMFormat[] = TEXT("%s: Invalidated VM - aborting execution.");
		if(VM->IsNativized())
		{
			if(!IsValidLowLevel() ||
				!VM->IsValidLowLevel())
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		else
		{
			// sanity check the validity of the VM to ensure stability.
			if(!IsValidLowLevel() ||
				!VM->IsValidLowLevel() ||
				!VM->GetLiteralMemory()->IsValidLowLevel() ||
				!VM->GetWorkMemory()->IsValidLowLevel())
			{
				UE_LOG(LogControlRig, Warning, InvalidatedVMFormat, *GetClass()->GetName());
				return false;
			}
		}
		
#if UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM
		const uint64 StartCycles = FPlatformTime::Cycles64();
		if(ProfilingRunsLeft <= 0)
		{
			ProfilingRunsLeft = UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM;
			AccumulatedCycles = 0;
		}
#endif
		
		const bool bUseInitializationSnapshots = bControlRigUseVMSnapshots && !VM->IsNativized();
		const bool bUseDebuggingSnapshots = !VM->IsNativized();
		
		TArray<URigVMMemoryStorage*> LocalMemory = VM->GetLocalMemoryArray();
		TArray<void*> AdditionalArguments;
		AdditionalArguments.Add(&InOutContext);

		bool bSuccess = true;

		if (InOutContext.State == EControlRigState::Init)
		{
			if(IsInGameThread() && bUseInitializationSnapshots)
			{
				const uint32 SnapshotHash = GetHashForInitializeVMSnapShot();
				UControlRig* CDO = Cast<UControlRig>(GetClass()->GetDefaultObject());

				bool bIsValidSnapshot = false;
				if(SnapshotHash != 0)
				{
					TObjectPtr<URigVM>* InitializedVMSnapshotPtr = CDO->InitializedVMSnapshots.Find(SnapshotHash);
					if(InitializedVMSnapshotPtr && *InitializedVMSnapshotPtr)
					{
						const URigVM* InitializedVMSnapshot = InitializedVMSnapshotPtr->Get();

						if(VM->WorkMemoryStorageObject->GetClass() == InitializedVMSnapshot->WorkMemoryStorageObject->GetClass() &&
							InitializedVMSnapshot->WorkMemoryStorageObject->IsValidLowLevel())
						{
							InitializedVMSnapshots.Reset();

							VM->WorkMemoryStorageObject->CopyFrom(InitializedVMSnapshot->WorkMemoryStorageObject);
							VM->InvalidateCachedMemory();
							VM->Initialize(LocalMemory, AdditionalArguments, false);
							bIsValidSnapshot = true;
						}
						else
						{
							CDO->InitializedVMSnapshots.Remove(SnapshotHash);
						}
					}
				}
				
				if(!bIsValidSnapshot)
				{
					VM->Initialize(LocalMemory, AdditionalArguments);

					// objects assigned to transient properties need transient flag, otherwise it might get exported during save
					URigVM* InitializedVMSnapshot = NewObject<URigVM>(CDO, NAME_None, RF_Public | RF_Transient);
					InitializedVMSnapshot->WorkMemoryStorageObject = NewObject<URigVMMemoryStorage>(InitializedVMSnapshot, VM->GetWorkMemory()->GetClass());
					InitializedVMSnapshot->WorkMemoryStorageObject->CopyFrom(VM->WorkMemoryStorageObject);

					CDO->InitializedVMSnapshots.Add(SnapshotHash, InitializedVMSnapshot);

					// GC won't consider some subobjects that are created after the constructor as part of the CDO,
					// so even if CDO is rooted and references these sub objects, 
					// it is not enough to keep them alive.
					// Hence, we have to add them to root here.
					InitializedVMSnapshot->AddToRoot();
				}
			}
			else
			{
				bSuccess = VM->Initialize(LocalMemory, AdditionalArguments);
			}
			bRequiresConstructionEvent = true;
		}
		else
		{
#if WITH_EDITOR
			if(bUseDebuggingSnapshots)
			{
				if(URigVM* SnapShotVM = GetSnapshotVM(false)) // don't create it for normal runs
				{
					const bool bIsEventFirstInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun[0] == InEventName; 
					const bool bIsEventLastInQueue = !EventQueueToRun.IsEmpty() && EventQueueToRun.Last() == InEventName;

					if (VM->GetHaltedAtBreakpoint().IsValid())
					{
						if(bIsEventFirstInQueue)
						{
							VM->CopyFrom(SnapShotVM, false, false, false, true, true);
						}
					}
					else if(bIsEventLastInQueue)
					{
						SnapShotVM->CopyFrom(VM, false, false, false, true, true);
					}
				}
			}
#endif

			URigHierarchy* Hierarchy = GetHierarchy();
#if WITH_EDITOR

			bool bRecordTransformsAtRuntime = true;
			if(const UObject* Outer = GetOuter())
			{
				if(Outer->IsA<UControlRigComponent>())
				{
					bRecordTransformsAtRuntime = false;
				}
			}
			TGuardValue<bool> RecordTransformsPerInstructionGuard(Hierarchy->bRecordTransformsAtRuntime, bRecordTransformsAtRuntime);
			
			if(Hierarchy->bRecordTransformsAtRuntime)
			{
				Hierarchy->ReadTransformsAtRuntime.Reset();
				Hierarchy->WrittenTransformsAtRuntime.Reset();
			}
			
#endif
			FRigHierarchyExecuteContextBracket HierarchyContextGuard(Hierarchy, &VM->GetContext());

			bSuccess = VM->Execute(LocalMemory, AdditionalArguments, InEventName);
		}

#if UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM
		const uint64 EndCycles = FPlatformTime::Cycles64();
		const uint64 Cycles = EndCycles - StartCycles;
		AccumulatedCycles += Cycles;
		ProfilingRunsLeft--;
		if(ProfilingRunsLeft == 0)
		{
			const double Milliseconds = FPlatformTime::ToMilliseconds64(AccumulatedCycles);
			UE_LOG(LogControlRig, Display, TEXT("%s: %d runs took %.03lfms."), *GetClass()->GetName(), UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM, Milliseconds);
		}
#endif

		return bSuccess;
	}
	return false;
}

bool UControlRig::ContainsEvent(const FName& InEventName) const
{
	if(VM)
	{
		return VM->ContainsEntry(InEventName);
	}
	return false;
}

TArray<FName> UControlRig::GetEvents() const
{
	if(VM)
	{
		return VM->GetEntryNames();
	}
	return TArray<FName>();
}

bool UControlRig::ExecuteEvent(const FName& InEventName)
{
	if(ContainsEvent(InEventName))
	{
		TGuardValue<TArray<FName>> EventQueueGuard(EventQueue, {InEventName});
		Evaluate_AnyThread();
		return true;
	}
	return false;
}

void UControlRig::RequestInit()
{
	bRequiresInitExecution = true;
	RequestConstruction();
}

void UControlRig::RequestConstruction()
{
	bRequiresConstructionEvent = true;
}

void UControlRig::SetEventQueue(const TArray<FName>& InEventNames)
{
	EventQueue = InEventNames;
}

void UControlRig::UpdateVMSettings()
{
	if(VM)
	{
#if WITH_EDITOR

		// setup array handling and error reporting on the VM
		VMRuntimeSettings.SetLogFunction([this](EMessageSeverity::Type InSeverity, const FRigVMExecuteContext* InContext, const FString& Message)
		{
			check(InContext);

			if(ControlRigLog)
			{
				ControlRigLog->Report(InSeverity, InContext->FunctionName, InContext->InstructionIndex, Message);
			}
			else
			{
				LogOnce(InSeverity, InContext->InstructionIndex, Message);
			}
		});
#endif
		
		VM->SetRuntimeSettings(VMRuntimeSettings);
	}
}

URigVM* UControlRig::GetVM()
{
	if (VM == nullptr)
	{
		Initialize(true);
		check(VM);
	}
	return VM;
}

void UControlRig::GetMappableNodeData(TArray<FName>& OutNames, TArray<FNodeItem>& OutNodeItems) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	OutNames.Reset();
	OutNodeItems.Reset();

	check(DynamicHierarchy);

	// now add all nodes
	DynamicHierarchy->ForEach<FRigBoneElement>([&OutNames, &OutNodeItems, this](FRigBoneElement* BoneElement) -> bool
    {
		OutNames.Add(BoneElement->GetName());
		FRigElementKey ParentKey = DynamicHierarchy->GetFirstParent(BoneElement->GetKey());
		if(ParentKey.Type != ERigElementType::Bone)
		{
			ParentKey.Name = NAME_None;
		}

		const FTransform GlobalInitial = DynamicHierarchy->GetGlobalTransformByIndex(BoneElement->GetIndex(), true);
		OutNodeItems.Add(FNodeItem(ParentKey.Name, GlobalInitial));
		return true;
	});
}

UAnimationDataSourceRegistry* UControlRig::GetDataSourceRegistry()
{
	if (DataSourceRegistry)
	{
		if (DataSourceRegistry->GetOuter() != this)
		{
			DataSourceRegistry = nullptr;
		}
	}
	if (DataSourceRegistry == nullptr)
	{
		DataSourceRegistry = NewObject<UAnimationDataSourceRegistry>(this);
	}
	return DataSourceRegistry;
}

#if WITH_EDITORONLY_DATA

void UControlRig::PostReinstanceCallback(const UControlRig* Old)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	ObjectBinding = Old->ObjectBinding;
	Initialize();
}

#endif // WITH_EDITORONLY_DATA

void UControlRig::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::AddReferencedObjects(InThis, Collector);
}

#if WITH_EDITOR
//undo will clear out the transient Operators, need to recreate them
void UControlRig::PostEditUndo()
{
	Super::PostEditUndo();
}

#endif // WITH_EDITOR

bool UControlRig::CanExecute() const
{
	return CVarControlRigDisableExecutionAll->GetInt() == 0;
}

TArray<UControlRig*> UControlRig::FindControlRigs(UObject* Outer, TSubclassOf<UControlRig> OptionalClass)
{
	TArray<UControlRig*> Result;
	
	if(Outer == nullptr)
	{
		return Result; 
	}
	
	AActor* OuterActor = Cast<AActor>(Outer);
	if(OuterActor == nullptr)
	{
		OuterActor = Outer->GetTypedOuter<AActor>();
	}
	
	for (TObjectIterator<UControlRig> Itr; Itr; ++Itr)
	{
		UControlRig* RigInstance = *Itr;
		const UClass* RigInstanceClass = RigInstance ? RigInstance->GetClass() : nullptr;
		if (OptionalClass == nullptr || (RigInstanceClass && RigInstanceClass->IsChildOf(OptionalClass)))
		{
			if(RigInstance->IsInOuter(Outer))
			{
				Result.Add(RigInstance);
				continue;
			}

			if(OuterActor)
			{
				if(RigInstance->IsInOuter(OuterActor))
				{
					Result.Add(RigInstance);
					continue;
				}

				if (TSharedPtr<IControlRigObjectBinding> Binding = RigInstance->GetObjectBinding())
				{
					if (AActor* Actor = Binding->GetHostingActor())
					{
						if (Actor == OuterActor)
						{
							Result.Add(RigInstance);
							continue;
						}
					}
				}
			}
		}
	}

	return Result;
}

void UControlRig::Serialize(FArchive& Ar)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);
}

void UControlRig::PostLoad()
{
	Super::PostLoad();

	if(HasAnyFlags(RF_ClassDefaultObject))
	{
		if(DynamicHierarchy)
		{
			// Some dynamic hierarchy objects have been created using NewObject<> instead of CreateDefaultSubObjects.
			// Assets from that version require the dynamic hierarchy to be flagged as below.
			DynamicHierarchy->SetFlags(DynamicHierarchy->GetFlags() | RF_Public | RF_DefaultSubObject);
		}
	}

#if WITH_EDITORONLY_DATA
	if (VMSnapshotBeforeExecution)
	{
		// Some VMSnapshots might have been created without the Transient flag.
		// Assets from that version require the snapshot to be flagged as below.
		VMSnapshotBeforeExecution->SetFlags(VMSnapshotBeforeExecution->GetFlags() | RF_Transient);
	}
#endif

	FRigInfluenceMapPerEvent NewInfluences;
	for(int32 MapIndex = 0; MapIndex < Influences.Num(); MapIndex++)
	{
		FRigInfluenceMap Map = Influences[MapIndex];
		FName EventName = Map.GetEventName();

		if(EventName == TEXT("Update"))
		{
			EventName = FRigUnit_BeginExecution::EventName;
		}
		else if(EventName == TEXT("Inverse"))
		{
			EventName = FRigUnit_InverseExecution::EventName;
		}
		
		NewInfluences.FindOrAdd(EventName).Merge(Map, true);
	}
	Influences = NewInfluences;
}

TArray<FRigControlElement*> UControlRig::AvailableControls() const
{
	if(DynamicHierarchy)
	{
		return DynamicHierarchy->GetElementsOfType<FRigControlElement>();
	}
	return TArray<FRigControlElement*>();
}

FRigControlElement* UControlRig::FindControl(const FName& InControlName) const
{
	if(DynamicHierarchy == nullptr)
	{
		return nullptr;
	}
	return DynamicHierarchy->Find<FRigControlElement>(FRigElementKey(InControlName, ERigElementType::Control));
}

bool UControlRig::IsConstructionModeEnabled() const
{
	return EventQueueToRun.Num() == 1 && EventQueueToRun.Contains(FRigUnit_PrepareForExecution::EventName);
}

FTransform UControlRig::SetupControlFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform)
{
	if (IsConstructionModeEnabled())
	{
		FRigControlElement* ControlElement = FindControl(InControlName);
		if (ControlElement && !ControlElement->Settings.bIsTransientControl)
		{
			const FTransform ParentTransform = GetHierarchy()->GetParentTransform(ControlElement, ERigTransformType::CurrentGlobal);
			const FTransform OffsetTransform = InGlobalTransform.GetRelativeTransform(ParentTransform);
			GetHierarchy()->SetControlOffsetTransform(ControlElement, OffsetTransform, ERigTransformType::InitialLocal, true, false);
			GetHierarchy()->SetControlOffsetTransform(ControlElement, OffsetTransform, ERigTransformType::CurrentLocal, true, false);

			if(URigHierarchy* DefaultHierarchy = GetHierarchy()->GetDefaultHierarchy())
			{
				if(FRigControlElement* DefaultControlElement = DefaultHierarchy->Find<FRigControlElement>(ControlElement->GetKey()))
				{
					DefaultHierarchy->SetControlOffsetTransform(DefaultControlElement, OffsetTransform, ERigTransformType::InitialLocal, true, true);
					DefaultHierarchy->SetControlOffsetTransform(DefaultControlElement, OffsetTransform, ERigTransformType::CurrentLocal, true, true);
				}
			}
		}
	}
	return InGlobalTransform;
}

void UControlRig::CreateRigControlsForCurveContainer()
{
	const bool bCreateFloatControls = CVarControlRigCreateFloatControlsForCurves->GetInt() == 0 ? false : true;
	if(bCreateFloatControls && DynamicHierarchy)
	{
		URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
		if(Controller == nullptr)
		{
			return;
		}
		static const FString CtrlPrefix(TEXT("CTRL_"));

		DynamicHierarchy->ForEach<FRigCurveElement>([this, Controller](FRigCurveElement* CurveElement) -> bool
        {
			const FString Name = CurveElement->GetName().ToString();
			
			if (Name.Contains(CtrlPrefix) && !DynamicHierarchy->Contains(FRigElementKey(*Name, ERigElementType::Curve))) //-V1051
			{
				FRigControlSettings Settings;
				Settings.AnimationType = ERigControlAnimationType::AnimationChannel;
				Settings.ControlType = ERigControlType::Float;
				Settings.bIsCurve = true;
				Settings.bDrawLimits = false;

				FRigControlValue Value;
				Value.Set<float>(CurveElement->Value);

				Controller->AddControl(CurveElement->GetName(), FRigElementKey(), Settings, Value, FTransform::Identity, FTransform::Identity); 
			}

			return true;
		});

		ControlModified().AddUObject(this, &UControlRig::HandleOnControlModified);
	}
}

void UControlRig::HandleOnControlModified(UControlRig* Subject, FRigControlElement* Control, const FRigControlModifiedContext& Context)
{
	if (Control->Settings.bIsCurve && DynamicHierarchy)
	{
		const FRigControlValue Value = DynamicHierarchy->GetControlValue(Control, IsConstructionModeEnabled() ? ERigControlValueType::Initial : ERigControlValueType::Current);
		DynamicHierarchy->SetCurveValue(FRigElementKey(Control->GetName(), ERigElementType::Curve), Value.Get<float>());
	}	
}

void UControlRig::HandleExecutionReachedExit(const FName& InEventName)
{
#if WITH_EDITOR
	if (EventQueueToRun.Last() == InEventName)
	{
		if(URigVM* SnapShotVM = GetSnapshotVM(false))
		{
			SnapShotVM->CopyFrom(VM, false, false, false, true, true);
		}
		DebugInfo.ResetState();
		VM->SetBreakpointAction(ERigVMBreakpointAction::None);
	}
#endif
	
	if (LatestExecutedState != EControlRigState::Init && bAccumulateTime)
	{
		AbsoluteTime += DeltaTime;
	}
}

bool UControlRig::IsCurveControl(const FRigControlElement* InControlElement) const
{
	return InControlElement->Settings.bIsCurve;
}

FTransform UControlRig::GetControlGlobalTransform(const FName& InControlName) const
{
	if(DynamicHierarchy == nullptr)
	{
		return FTransform::Identity;
	}
	return DynamicHierarchy->GetGlobalTransform(FRigElementKey(InControlName, ERigElementType::Control), false);
}

bool UControlRig::SetControlGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, bool bNotify, const FRigControlModifiedContext& Context, bool bSetupUndo, bool bPrintPythonCommands, bool bFixEulerFlips)
{
	FTransform GlobalTransform = InGlobalTransform;
	ERigTransformType::Type TransformType = ERigTransformType::CurrentGlobal;
	if (IsConstructionModeEnabled())
	{
#if WITH_EDITOR
		if(bRecordSelectionPoseForConstructionMode)
		{
			SelectionPoseForConstructionMode.FindOrAdd(FRigElementKey(InControlName, ERigElementType::Control)) = GlobalTransform;
		}
#endif
		TransformType = ERigTransformType::InitialGlobal;
		GlobalTransform = SetupControlFromGlobalTransform(InControlName, GlobalTransform);
	}

	FRigControlValue Value = GetControlValueFromGlobalTransform(InControlName, GlobalTransform, TransformType);
	if (OnFilterControl.IsBound())
	{
		FRigControlElement* Control = FindControl(InControlName);
		if (Control)
		{
			OnFilterControl.Broadcast(this, Control, Value);
		}
	}

	SetControlValue(InControlName, Value, bNotify, Context, bSetupUndo, bPrintPythonCommands, bFixEulerFlips);
	return true;
}

FRigControlValue UControlRig::GetControlValueFromGlobalTransform(const FName& InControlName, const FTransform& InGlobalTransform, ERigTransformType::Type InTransformType)
{
	FRigControlValue Value;

	if (FRigControlElement* ControlElement = FindControl(InControlName))
	{
		if(DynamicHierarchy)
		{
			FTransform Transform = DynamicHierarchy->ComputeLocalControlValue(ControlElement, InGlobalTransform, InTransformType);
			Value.SetFromTransform(Transform, ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis);

			if (ShouldApplyLimits())
			{
				ControlElement->Settings.ApplyLimits(Value);
			}
		}
	}

	return Value;
}

void UControlRig::SetControlLocalTransform(const FName& InControlName, const FTransform& InLocalTransform, bool bNotify, const FRigControlModifiedContext& Context, bool bSetupUndo, bool bFixEulerFlips)
{
	if (FRigControlElement* ControlElement = FindControl(InControlName))
	{
		FRigControlValue Value;
		Value.SetFromTransform(InLocalTransform, ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis);

		if (OnFilterControl.IsBound())
		{
			OnFilterControl.Broadcast(this, ControlElement, Value);
			
		}
		SetControlValue(InControlName, Value, bNotify, Context, bSetupUndo, bFixEulerFlips);
	}
}

FTransform UControlRig::GetControlLocalTransform(const FName& InControlName)
{
	if(DynamicHierarchy == nullptr)
	{
		return FTransform::Identity;
	}
	return DynamicHierarchy->GetLocalTransform(FRigElementKey(InControlName, ERigElementType::Control));
}

const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& UControlRig::GetShapeLibraries() const
{
	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>* LibrariesPtr = &ShapeLibraries;

	if(!GetClass()->IsNative())
	{
		if (UControlRig* CDO = Cast<UControlRig>(GetClass()->GetDefaultObject()))
		{
			LibrariesPtr = &CDO->ShapeLibraries;
		}
	}

	const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& Libraries = *LibrariesPtr;
	for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : Libraries)
	{
		if (!ShapeLibrary.IsValid())
		{
			ShapeLibrary.LoadSynchronous();
		}
	}

	return Libraries;
}

void UControlRig::SelectControl(const FName& InControlName, bool bSelect)
{
	if(DynamicHierarchy)
	{
		if(URigHierarchyController* Controller = DynamicHierarchy->GetController(true))
		{
			Controller->SelectElement(FRigElementKey(InControlName, ERigElementType::Control), bSelect);
		}
	}
}

bool UControlRig::ClearControlSelection()
{
	if(DynamicHierarchy)
	{
		if(URigHierarchyController* Controller = DynamicHierarchy->GetController(true))
		{
			return Controller->ClearSelection();
		}
	}
	return false;
}

TArray<FName> UControlRig::CurrentControlSelection() const
{
	TArray<FName> SelectedControlNames;

	if(DynamicHierarchy)
	{
		TArray<const FRigBaseElement*> SelectedControls = DynamicHierarchy->GetSelectedElements(ERigElementType::Control);
		for (const FRigBaseElement* SelectedControl : SelectedControls)
		{
			SelectedControlNames.Add(SelectedControl->GetName());
		}
	}
	return SelectedControlNames;
}

bool UControlRig::IsControlSelected(const FName& InControlName)const
{
	if(DynamicHierarchy)
	{
		if(FRigControlElement* ControlElement = FindControl(InControlName))
		{
			return DynamicHierarchy->IsSelected(ControlElement);
		}
	}
	return false;
}

void UControlRig::HandleHierarchyModified(ERigHierarchyNotification InNotification, URigHierarchy* InHierarchy,
    const FRigBaseElement* InElement)
{
	switch(InNotification)
	{
		case ERigHierarchyNotification::ElementSelected:
		case ERigHierarchyNotification::ElementDeselected:
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>((FRigBaseElement*)InElement))
			{
				const bool bSelected = InNotification == ERigHierarchyNotification::ElementSelected;
				ControlSelected().Broadcast(this, ControlElement, bSelected);

				OnControlSelected_BP.Broadcast(this, *ControlElement, bSelected);
			}

#if WITH_EDITOR
			if(IsConstructionModeEnabled())
			{
				if(InElement->GetType() == ERigElementType::Control)
				{
					if(InNotification == ERigHierarchyNotification::ElementSelected)
					{
						SelectionPoseForConstructionMode.FindOrAdd(InElement->GetKey()) = GetHierarchy()->GetGlobalTransform(InElement->GetKey());
					}
					else
					{
						SelectionPoseForConstructionMode.Remove(InElement->GetKey());
					}
				}

				if(InNotification == ERigHierarchyNotification::ElementDeselected)
				{
					ClearTransientControls();
				}
			}
#endif
			break;
		}
		case ERigHierarchyNotification::ControlSettingChanged:
		case ERigHierarchyNotification::ControlShapeTransformChanged:
		{
			if(FRigControlElement* ControlElement = Cast<FRigControlElement>((FRigBaseElement*)InElement))
			{
				ControlModified().Broadcast(this, ControlElement, FRigControlModifiedContext(EControlRigSetKey::Never));
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

#if WITH_EDITOR

FName UControlRig::AddTransientControl(URigVMPin* InPin, FRigElementKey SpaceKey, FTransform OffsetTransform)
{
	if ((InPin == nullptr) || (DynamicHierarchy == nullptr))
	{
		return NAME_None;
	}

	if (InPin->GetCPPType() != TEXT("FVector") &&
		InPin->GetCPPType() != TEXT("FQuat") &&
		InPin->GetCPPType() != TEXT("FTransform"))
	{
		return NAME_None;
	}

	RemoveTransientControl(InPin);

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	URigVMPin* PinForLink = InPin->GetPinForLink();

	const FName ControlName = GetNameForTransientControl(InPin);
	FTransform ShapeTransform = FTransform::Identity;
	ShapeTransform.SetScale3D(FVector::ZeroVector);

	FRigControlSettings Settings;
	Settings.ControlType = ERigControlType::Transform;
	if (URigVMPin* ColorPin = PinForLink->GetNode()->FindPin(TEXT("Color")))
	{
		if (ColorPin->GetCPPType() == TEXT("FLinearColor"))
		{
			FRigControlValue Value;
			Settings.ShapeColor = Value.SetFromString<FLinearColor>(ColorPin->GetDefaultValue());
		}
	}
	Settings.bIsTransientControl = true;
	Settings.DisplayName = TEXT("Temporary Control");

	Controller->ClearSelection();

    const FRigElementKey ControlKey = Controller->AddControl(
    	ControlName,
    	SpaceKey,
    	Settings,
    	FRigControlValue::Make(FTransform::Identity),
    	OffsetTransform,
    	ShapeTransform, false);

	SetTransientControlValue(InPin);

	return ControlName;
}

bool UControlRig::SetTransientControlValue(URigVMPin* InPin)
{
	const FName ControlName = GetNameForTransientControl(InPin);
	if (FRigControlElement* ControlElement = FindControl(ControlName))
	{
		FString DefaultValue = InPin->GetPinForLink()->GetDefaultValue();
		if (!DefaultValue.IsEmpty())
		{
			if (InPin->GetCPPType() == TEXT("FVector"))
			{
				ControlElement->Settings.ControlType = ERigControlType::Position;
				FRigControlValue Value;
				Value.SetFromString<FVector>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
			else if (InPin->GetCPPType() == TEXT("FQuat"))
			{
				ControlElement->Settings.ControlType = ERigControlType::Rotator;
				FRigControlValue Value;
				Value.SetFromString<FRotator>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
			else
			{
				ControlElement->Settings.ControlType = ERigControlType::Transform;
				FRigControlValue Value;
				Value.SetFromString<FTransform>(DefaultValue);
				DynamicHierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, false);
			}
		}
		return true;
	}
	return false;
}

FName UControlRig::RemoveTransientControl(URigVMPin* InPin)
{
	if ((InPin == nullptr) || (DynamicHierarchy == nullptr))
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InPin);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		DynamicHierarchy->Notify(ERigHierarchyNotification::ElementDeselected, ControlElement);
		if(Controller->RemoveElement(ControlElement))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(FRigElementKey(ControlName, ERigElementType::Control));
#endif
			return ControlName;
		}
	}

	return NAME_None;
}

FName UControlRig::AddTransientControl(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return NAME_None;
	}

	if(DynamicHierarchy == nullptr)
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(DynamicHierarchy->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		SetTransientControlValue(InElement);
		return ControlName;
	}

	const int32 ElementIndex = DynamicHierarchy->GetIndex(InElement);
	if (ElementIndex == INDEX_NONE)
	{
		return NAME_None;
	}

	FTransform ShapeTransform = FTransform::Identity;
	ShapeTransform.SetScale3D(FVector::ZeroVector);

	FRigControlSettings Settings;
	Settings.ControlType = ERigControlType::Transform;
	Settings.bIsTransientControl = true;
	Settings.DisplayName = TEXT("Temporary Control");

	FRigElementKey Parent;
	switch (InElement.Type)
	{
		case ERigElementType::Bone:
		{
			Parent = DynamicHierarchy->GetFirstParent(InElement);
			break;
		}
		case ERigElementType::Null:
		{
			Parent = InElement;
			break;
		}
		default:
		{
			break;
		}
	}

	const FRigElementKey ControlKey = Controller->AddControl(
        ControlName,
        Parent,
        Settings,
        FRigControlValue::Make(FTransform::Identity),
        FTransform::Identity,
        ShapeTransform, false);

	if (InElement.Type == ERigElementType::Bone)
	{
		// don't allow transient control to modify forward mode poses when we
		// already switched to the construction mode
		if (!IsConstructionModeEnabled())
		{
			if(FRigBoneElement* BoneElement = DynamicHierarchy->Find<FRigBoneElement>(InElement))
			{
				// add a modify bone AnimNode internally that the transient control controls for imported bones only
				// for user created bones, refer to UControlRig::TransformOverrideForUserCreatedBones 
				if (BoneElement->BoneType == ERigBoneType::Imported)
				{ 
					if (PreviewInstance)
					{
						PreviewInstance->ModifyBone(InElement.Name);
					}
				}
				else if (BoneElement->BoneType == ERigBoneType::User)
				{
					// add an empty entry, which will be given the correct value in
					// SetTransientControlValue(InElement);
					TransformOverrideForUserCreatedBones.FindOrAdd(InElement.Name);
				}
			}
		}
	}

	SetTransientControlValue(InElement);

	return ControlName;
}

bool UControlRig::SetTransientControlValue(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return false;
	}

	if(DynamicHierarchy == nullptr)
	{
		return false;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		if (InElement.Type == ERigElementType::Bone)
		{
			if (IsConstructionModeEnabled())
			{
				// need to get initial because that is what construction mode uses
				// specifically, when user change the initial from the details panel
				// this will allow the transient control to react to that change
				const FTransform InitialLocalTransform = DynamicHierarchy->GetInitialLocalTransform(InElement);
				DynamicHierarchy->SetTransform(ControlElement, InitialLocalTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(ControlElement, InitialLocalTransform, ERigTransformType::CurrentLocal, true, false);
			}
			else
			{
				const FTransform LocalTransform = DynamicHierarchy->GetLocalTransform(InElement);
				DynamicHierarchy->SetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, false);

				if (FRigBoneElement* BoneElement = DynamicHierarchy->Find<FRigBoneElement>(InElement))
				{
					if (BoneElement->BoneType == ERigBoneType::Imported)
					{
						if (PreviewInstance)
						{
							if (FAnimNode_ModifyBone* Modify = PreviewInstance->FindModifiedBone(InElement.Name))
							{
								Modify->Translation = LocalTransform.GetTranslation();
								Modify->Rotation = LocalTransform.GetRotation().Rotator();
								Modify->TranslationSpace = EBoneControlSpace::BCS_ParentBoneSpace;
								Modify->RotationSpace = EBoneControlSpace::BCS_ParentBoneSpace;
							}
						}	
					}
					else if (BoneElement->BoneType == ERigBoneType::User)
					{
						if (FTransform* TransformOverride = TransformOverrideForUserCreatedBones.Find(InElement.Name))
						{
							*TransformOverride = LocalTransform;
						}	
					}
				}
			}
		}
		else if (InElement.Type == ERigElementType::Null)
		{
			const FTransform GlobalTransform = DynamicHierarchy->GetGlobalTransform(InElement);
			DynamicHierarchy->SetTransform(ControlElement, GlobalTransform, ERigTransformType::InitialGlobal, true, false);
			DynamicHierarchy->SetTransform(ControlElement, GlobalTransform, ERigTransformType::CurrentGlobal, true, false);
		}

		return true;
	}
	return false;
}

FName UControlRig::RemoveTransientControl(const FRigElementKey& InElement)
{
	if (!InElement.IsValid())
	{
		return NAME_None;
	}

	if(DynamicHierarchy == nullptr)
	{
		return NAME_None;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return NAME_None;
	}

	const FName ControlName = GetNameForTransientControl(InElement);
	if(FRigControlElement* ControlElement = FindControl(ControlName))
	{
		DynamicHierarchy->Notify(ERigHierarchyNotification::ElementDeselected, ControlElement);
		if(Controller->RemoveElement(ControlElement))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(FRigElementKey(ControlName, ERigElementType::Control));
#endif
			return ControlName;
		}
	}

	return NAME_None;
}

FName UControlRig::GetNameForTransientControl(URigVMPin* InPin) const
{
	check(InPin);
	check(DynamicHierarchy);
	
	const FString OriginalPinPath = InPin->GetOriginalPinFromInjectedNode()->GetPinPath();
	return DynamicHierarchy->GetSanitizedName(FString::Printf(TEXT("ControlForPin_%s"), *OriginalPinPath));
}

FString UControlRig::GetPinNameFromTransientControl(const FRigElementKey& InKey)
{
	FString Name = InKey.Name.ToString();
	if(Name.StartsWith(TEXT("ControlForPin_")))
	{
		Name.RightChopInline(14);
	}
	return Name;
}

FName UControlRig::GetNameForTransientControl(const FRigElementKey& InElement)
{
	if (InElement.Type == ERigElementType::Control)
	{
		return InElement.Name;
	}

	const FName EnumName = *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)InElement.Type).ToString();
	return *FString::Printf(TEXT("ControlForRigElement_%s_%s"), *EnumName.ToString(), *InElement.Name.ToString());
}

FRigElementKey UControlRig::GetElementKeyFromTransientControl(const FRigElementKey& InKey)
{
	if(InKey.Type != ERigElementType::Control)
	{
		return FRigElementKey();
	}
	
	static FString ControlRigForElementBoneName;
	static FString ControlRigForElementNullName;

	if (ControlRigForElementBoneName.IsEmpty())
	{
		ControlRigForElementBoneName = FString::Printf(TEXT("ControlForRigElement_%s_"),
            *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ERigElementType::Bone).ToString());
		ControlRigForElementNullName = FString::Printf(TEXT("ControlForRigElement_%s_"),
            *StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)ERigElementType::Null).ToString());
	}
	
	FString Name = InKey.Name.ToString();
	if(Name.StartsWith(ControlRigForElementBoneName))
	{
		Name.RightChopInline(ControlRigForElementBoneName.Len());
		return FRigElementKey(*Name, ERigElementType::Bone);
	}
	if(Name.StartsWith(ControlRigForElementNullName))
	{
		Name.RightChopInline(ControlRigForElementNullName.Len());
		return FRigElementKey(*Name, ERigElementType::Null);
	}
	
	return FRigElementKey();;
}

void UControlRig::ClearTransientControls()
{
	if(DynamicHierarchy == nullptr)
	{
		return;
	}

	URigHierarchyController* Controller = DynamicHierarchy->GetController(true);
	if(Controller == nullptr)
	{
		return;
	}

	if(bIsClearingTransientControls)
	{
		return;
	}
	TGuardValue<bool> ReEntryGuard(bIsClearingTransientControls, true);

	const TArray<FRigControlElement*> ControlsToRemove = DynamicHierarchy->GetTransientControls();
	for (FRigControlElement* ControlToRemove : ControlsToRemove)
	{
		const FRigElementKey KeyToRemove = ControlToRemove->GetKey();
		if(Controller->RemoveElement(ControlToRemove))
		{
#if WITH_EDITOR
			SelectionPoseForConstructionMode.Remove(KeyToRemove);
#endif
		}
	}
}

void UControlRig::ApplyTransformOverrideForUserCreatedBones()
{
	if(DynamicHierarchy == nullptr)
	{
		return;
	}
	
	for (const auto& Entry : TransformOverrideForUserCreatedBones)
	{
		DynamicHierarchy->SetLocalTransform(FRigElementKey(Entry.Key, ERigElementType::Bone), Entry.Value, false);
	}
}

void UControlRig::ApplySelectionPoseForConstructionMode(const FName& InEventName)
{
	FRigControlModifiedContext ControlValueContext;
	ControlValueContext.EventName = InEventName;

	TGuardValue<bool> DisableRecording(bRecordSelectionPoseForConstructionMode, false);
	for(const TPair<FRigElementKey, FTransform>& Pair : SelectionPoseForConstructionMode)
	{
		const FName ControlName = Pair.Key.Name;
		const FRigControlValue Value = GetControlValueFromGlobalTransform(ControlName, Pair.Value, ERigTransformType::InitialGlobal);
		SetControlValue(ControlName, Value, true, ControlValueContext, false, false, false);
	}
}

#endif

void UControlRig::HandleHierarchyEvent(URigHierarchy* InHierarchy, const FRigEventContext& InEvent)
{
	if (RigEventDelegate.IsBound())
	{
		RigEventDelegate.Broadcast(InHierarchy, InEvent);
	}

	switch (InEvent.Event)
	{
		case ERigEvent::RequestAutoKey:
		{
			int32 Index = InHierarchy->GetIndex(InEvent.Key);
			if (Index != INDEX_NONE && InEvent.Key.Type == ERigElementType::Control)
			{
				if(FRigControlElement* ControlElement = InHierarchy->GetChecked<FRigControlElement>(Index))
				{
					FRigControlModifiedContext Context;
					Context.SetKey = EControlRigSetKey::Always;
					Context.LocalTime = InEvent.LocalTime;
					Context.EventName = InEvent.SourceEventName;
					ControlModified().Broadcast(this, ControlElement, Context);
				}
			}
			break;
		}
		case ERigEvent::OpenUndoBracket:
		case ERigEvent::CloseUndoBracket:
		{
			const bool bOpenUndoBracket = InEvent.Event == ERigEvent::OpenUndoBracket;
			ControlUndoBracketIndex = FMath::Max<int32>(0, ControlUndoBracketIndex + (bOpenUndoBracket ? 1 : -1));
			ControlUndoBracket().Broadcast(this, bOpenUndoBracket);
			break;
		}
		default:
		{
			break;
		}
	}
}

void UControlRig::GetControlsInOrder(TArray<FRigControlElement*>& SortedControls) const
{
	SortedControls.Reset();

	if(DynamicHierarchy == nullptr)
	{
		return;
	}

	SortedControls = DynamicHierarchy->GetControls(true);
}

const FRigInfluenceMap* UControlRig::FindInfluenceMap(const FName& InEventName)
{
	if (UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
	{
		return CDO->Influences.Find(InEventName);
	}
	return nullptr;
}

void UControlRig::SetInteractionRig(UControlRig* InInteractionRig)
{
	if (InteractionRig == InInteractionRig)
	{
		return;
	}

	if (InteractionRig != nullptr)
	{
		InteractionRig->ControlModified().RemoveAll(this);
		InteractionRig->OnInitialized_AnyThread().RemoveAll(this);
		InteractionRig->OnExecuted_AnyThread().RemoveAll(this);
		InteractionRig->ControlSelected().RemoveAll(this);
		OnInitialized_AnyThread().RemoveAll(InteractionRig);
		OnExecuted_AnyThread().RemoveAll(InteractionRig);
		ControlSelected().RemoveAll(InteractionRig);
	}

	InteractionRig = InInteractionRig;

	if (InteractionRig != nullptr)
	{
		SetInteractionRigClass(InteractionRig->GetClass());

		InteractionRig->Initialize(true);
		InteractionRig->CopyPoseFromOtherRig(this);
		InteractionRig->RequestConstruction();
		InteractionRig->Execute(EControlRigState::Update, FRigUnit_BeginExecution::EventName);

		InteractionRig->ControlModified().AddUObject(this, &UControlRig::HandleInteractionRigControlModified);
		InteractionRig->OnInitialized_AnyThread().AddUObject(this, &UControlRig::HandleInteractionRigInitialized);
		InteractionRig->OnExecuted_AnyThread().AddUObject(this, &UControlRig::HandleInteractionRigExecuted);
		InteractionRig->ControlSelected().AddUObject(this, &UControlRig::HandleInteractionRigControlSelected, false);
		OnInitialized_AnyThread().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigInitialized);
		OnExecuted_AnyThread().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigExecuted);
		ControlSelected().AddUObject(ToRawPtr(InteractionRig), &UControlRig::HandleInteractionRigControlSelected, true);

		FControlRigBracketScope BracketScope(InterRigSyncBracket);
		InteractionRig->HandleInteractionRigExecuted(this, EControlRigState::Update, FRigUnit_BeginExecution::EventName);
	}
}

void UControlRig::SetInteractionRigClass(TSubclassOf<UControlRig> InInteractionRigClass)
{
	if (InteractionRigClass == InInteractionRigClass)
	{
		return;
	}

	InteractionRigClass = InInteractionRigClass;

	if(InteractionRigClass)
	{
		if(InteractionRig != nullptr)
		{
			if(InteractionRig->GetClass() != InInteractionRigClass)
			{
				SetInteractionRig(nullptr);
			}
		}

		if(InteractionRig == nullptr)
		{
			UControlRig* NewInteractionRig = NewObject<UControlRig>(this, InteractionRigClass);
			SetInteractionRig(NewInteractionRig);
		}
	}
}

#if WITH_EDITOR

void UControlRig::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRig))
	{
		SetInteractionRig(nullptr);
	}
	else if (PropertyAboutToChange && PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRigClass))
	{
		SetInteractionRigClass(nullptr);
	}
}

void UControlRig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRig))
	{
		UControlRig* NewInteractionRig = InteractionRig;
		SetInteractionRig(nullptr);
		SetInteractionRig(NewInteractionRig);
	}
	else if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRig, InteractionRigClass))
	{
		TSubclassOf<UControlRig> NewInteractionRigClass = InteractionRigClass;
		SetInteractionRigClass(nullptr);
		SetInteractionRigClass(NewInteractionRigClass);
		if (NewInteractionRigClass == nullptr)
		{
			SetInteractionRig(nullptr);
		}
	}
}
#endif

void UControlRig::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* UControlRig::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}

void UControlRig::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* UControlRig::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}

void UControlRig::CopyPoseFromOtherRig(UControlRig* Subject)
{
	check(DynamicHierarchy);
	check(Subject);
	URigHierarchy* OtherHierarchy = Subject->GetHierarchy();
	check(OtherHierarchy);

	for (FRigBaseElement* Element : *DynamicHierarchy)
	{
		FRigBaseElement* OtherElement = OtherHierarchy->Find(Element->GetKey());
		if(OtherElement == nullptr)
		{
			continue;
		}

		if(OtherElement->GetType() != Element->GetType())
		{
			continue;
		}

		if(FRigBoneElement* BoneElement = Cast<FRigBoneElement>(Element))
		{
			FRigBoneElement* OtherBoneElement = CastChecked<FRigBoneElement>(OtherElement);
			const FTransform Transform = OtherHierarchy->GetTransform(OtherBoneElement, ERigTransformType::CurrentLocal);
			DynamicHierarchy->SetTransform(BoneElement, Transform, ERigTransformType::CurrentLocal, true, false);
		}
		else if(FRigCurveElement* CurveElement = Cast<FRigCurveElement>(Element))
		{
			FRigCurveElement* OtherCurveElement = CastChecked<FRigCurveElement>(OtherElement);
			const float Value = OtherHierarchy->GetCurveValue(OtherCurveElement);
			DynamicHierarchy->SetCurveValue(CurveElement, Value, false);
		}
	}
}

void UControlRig::HandleInteractionRigControlModified(UControlRig* Subject, FRigControlElement* Control, const FRigControlModifiedContext& Context)
{
	check(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	if (Subject != InteractionRig)
	{
		return;
	}

	if (const FRigInfluenceMap* InfluenceMap = Subject->FindInfluenceMap(Context.EventName))
	{
		if (const FRigInfluenceEntry* InfluenceEntry = InfluenceMap->Find(Control->GetKey()))
		{
			for (const FRigElementKey& AffectedKey : *InfluenceEntry)
			{
				if (AffectedKey.Type == ERigElementType::Control)
				{
					if (FRigControlElement* AffectedControl = FindControl(AffectedKey.Name))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
				else if (AffectedKey.Type == ERigElementType::Bone)
				{
					// special case controls with a CONTROL suffix
					FName BoneControlName = *FString::Printf(TEXT("%s_CONTROL"), *AffectedKey.Name.ToString());
					if (FRigControlElement* AffectedControl = FindControl(BoneControlName))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
				else if(AffectedKey.Type == ERigElementType::Curve)
				{
					// special case controls with a CONTROL suffix
					FName CurveControlName = *FString::Printf(TEXT("%s_CURVE_CONTROL"), *AffectedKey.Name.ToString());
					if (FRigControlElement* AffectedControl = FindControl(CurveControlName))
					{
						QueuedModifiedControls.Add(AffectedControl->GetKey());
					}
				}
			}
		}
	}

}

void UControlRig::HandleInteractionRigInitialized(UControlRig* Subject, EControlRigState State, const FName& EventName)
{
	check(Subject);

	if (IsSyncingWithOtherRig())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);
	RequestInit();
}

void UControlRig::HandleInteractionRigExecuted(UControlRig* Subject, EControlRigState State, const FName& EventName)
{
	check(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	CopyPoseFromOtherRig(Subject);
	Execute(EControlRigState::Update, FRigUnit_InverseExecution::EventName);

	FRigControlModifiedContext Context;
	Context.EventName = FRigUnit_InverseExecution::EventName;
	Context.SetKey = EControlRigSetKey::DoNotCare;

	for (const FRigElementKey& QueuedModifiedControl : QueuedModifiedControls)
	{
		if(FRigControlElement* ControlElement = FindControl(QueuedModifiedControl.Name))
		{
			ControlModified().Broadcast(this, ControlElement, Context);
		}
	}
}

void UControlRig::HandleInteractionRigControlSelected(UControlRig* Subject, FRigControlElement* Control, bool bSelected, bool bInverted)
{
	check(Subject);

	if (IsSyncingWithOtherRig() || IsExecuting())
	{
		return;
	}
	if (Subject->IsSyncingWithOtherRig() || Subject->IsExecuting())
	{
		return;
	}
	FControlRigBracketScope BracketScope(InterRigSyncBracket);

	const FRigInfluenceMap* InfluenceMap = nullptr;
	if (bInverted)
	{
		InfluenceMap = FindInfluenceMap(FRigUnit_BeginExecution::EventName);
	}
	else
	{
		InfluenceMap = Subject->FindInfluenceMap(FRigUnit_BeginExecution::EventName);
	}

	if (InfluenceMap)
	{
		FRigInfluenceMap InvertedMap;
		if (bInverted)
		{
			InvertedMap = InfluenceMap->Inverse();
			InfluenceMap = &InvertedMap;
		}

		struct Local
		{
			static void SelectAffectedElements(UControlRig* ThisRig, const FRigInfluenceMap* InfluenceMap, const FRigElementKey& InKey, bool bSelected, bool bInverted)
			{
				if (const FRigInfluenceEntry* InfluenceEntry = InfluenceMap->Find(InKey))
				{
					for (const FRigElementKey& AffectedKey : *InfluenceEntry)
					{
						if (AffectedKey.Type == ERigElementType::Control)
						{
							ThisRig->SelectControl(AffectedKey.Name, bSelected);
						}

						if (bInverted)
						{
							if (AffectedKey.Type == ERigElementType::Control)
							{
								ThisRig->SelectControl(AffectedKey.Name, bSelected);
							}
						}
						else
						{
							if (AffectedKey.Type == ERigElementType::Control)
							{
								ThisRig->SelectControl(AffectedKey.Name, bSelected);
							}
							else if (AffectedKey.Type == ERigElementType::Bone ||
								AffectedKey.Type == ERigElementType::Curve)
							{
								FName ControlName = *FString::Printf(TEXT("%s_CONTROL"), *AffectedKey.Name.ToString());
								ThisRig->SelectControl(ControlName, bSelected);
							}
						}
					}
				}
			}
		};

		Local::SelectAffectedElements(this, InfluenceMap, Control->GetKey(), bSelected, bInverted);

		if (bInverted)
		{
			const FString ControlName = Control->GetName().ToString();
			if (ControlName.EndsWith(TEXT("_CONTROL")))
			{
				const FString BaseName = ControlName.Left(ControlName.Len() - 8);
				Local::SelectAffectedElements(this, InfluenceMap, FRigElementKey(*BaseName, ERigElementType::Bone), bSelected, bInverted);
				Local::SelectAffectedElements(this, InfluenceMap, FRigElementKey(*BaseName, ERigElementType::Curve), bSelected, bInverted);
			}
		}
	}
}

#if WITH_EDITOR

URigVM* UControlRig::GetSnapshotVM(bool bCreateIfNeeded)
{
#if WITH_EDITOR
	if ((VMSnapshotBeforeExecution == nullptr) && bCreateIfNeeded)
	{
		VMSnapshotBeforeExecution = NewObject<URigVM>(GetTransientPackage(), NAME_None, RF_Transient);
	}
	return VMSnapshotBeforeExecution;
#else
	return nullptr;
#endif
}

void UControlRig::LogOnce(EMessageSeverity::Type InSeverity, int32 InInstructionIndex, const FString& InMessage)
{
	if(LoggedMessages.Contains(InMessage))
	{
		return;
	}

	switch (InSeverity)
	{
		case EMessageSeverity::Error:
		{
			UE_LOG(LogControlRig, Error, TEXT("%s"), *InMessage);
			break;
		}
		case EMessageSeverity::PerformanceWarning:
		case EMessageSeverity::Warning:
		{
			UE_LOG(LogControlRig, Warning, TEXT("%s"), *InMessage);
			break;
		}
		case EMessageSeverity::Info:
		{
			UE_LOG(LogControlRig, Display, TEXT("%s"), *InMessage);
			break;
		}
		default:
		{
			break;
		}
	}

	LoggedMessages.Add(InMessage, true);
}

void UControlRig::AddBreakpoint(int32 InstructionIndex, URigVMNode* InNode, uint16 InDepth)
{
	DebugInfo.AddBreakpoint(InstructionIndex, InNode, InDepth);
}

bool UControlRig::ExecuteBreakpointAction(const ERigVMBreakpointAction BreakpointAction)
{
	if (VM->GetHaltedAtBreakpoint().IsValid())
	{
		VM->SetBreakpointAction(BreakpointAction);
		return true;
	}
	return false;
}

#endif // WITH_EDITOR

void UControlRig::SetBoneInitialTransformsFromAnimInstance(UAnimInstance* InAnimInstance)
{
	FMemMark Mark(FMemStack::Get());
	FCompactPose OutPose;
	OutPose.ResetToRefPose(InAnimInstance->GetRequiredBones());
	SetBoneInitialTransformsFromCompactPose(&OutPose);
}

void UControlRig::SetBoneInitialTransformsFromAnimInstanceProxy(const FAnimInstanceProxy* InAnimInstanceProxy)
{
	FMemMark Mark(FMemStack::Get());
	FCompactPose OutPose;
	OutPose.ResetToRefPose(InAnimInstanceProxy->GetRequiredBones());
	SetBoneInitialTransformsFromCompactPose(&OutPose);
}

void UControlRig::SetBoneInitialTransformsFromSkeletalMeshComponent(USkeletalMeshComponent* InSkelMeshComp, bool bUseAnimInstance)
{
	check(InSkelMeshComp);
	check(DynamicHierarchy);
	
	if (!bUseAnimInstance && (InSkelMeshComp->GetAnimInstance() != nullptr))
	{
		SetBoneInitialTransformsFromAnimInstance(InSkelMeshComp->GetAnimInstance());
	}
	else
	{
		SetBoneInitialTransformsFromSkeletalMesh(InSkelMeshComp->GetSkeletalMeshAsset());
	}
}


void UControlRig::SetBoneInitialTransformsFromSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	if (ensure(InSkeletalMesh))
	{ 
		SetBoneInitialTransformsFromRefSkeleton(InSkeletalMesh->GetRefSkeleton());
	}
}

void UControlRig::SetBoneInitialTransformsFromRefSkeleton(const FReferenceSkeleton& InReferenceSkeleton)
{
	check(DynamicHierarchy);

	DynamicHierarchy->ForEach<FRigBoneElement>([this, InReferenceSkeleton](FRigBoneElement* BoneElement) -> bool
	{
		if(BoneElement->BoneType == ERigBoneType::Imported)
		{
			const int32 BoneIndex = InReferenceSkeleton.FindBoneIndex(BoneElement->GetName());
			if (BoneIndex != INDEX_NONE)
			{
				const FTransform LocalInitialTransform = InReferenceSkeleton.GetRefBonePose()[BoneIndex];
				DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::InitialLocal, true, false);
				DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::CurrentLocal, true, false);
			}
		}
		return true;
	});
	bResetInitialTransformsBeforeConstruction = false;
	RequestConstruction();
}

void UControlRig::SetBoneInitialTransformsFromCompactPose(FCompactPose* InCompactPose)
{
	check(InCompactPose);

	if(!InCompactPose->IsValid())
	{
		return;
	}
	if(!InCompactPose->GetBoneContainer().IsValid())
	{
		return;
	}
	
	FMemMark Mark(FMemStack::Get());

	DynamicHierarchy->ForEach<FRigBoneElement>([this, InCompactPose](FRigBoneElement* BoneElement) -> bool
		{
			if (BoneElement->BoneType == ERigBoneType::Imported)
			{
				int32 MeshIndex = InCompactPose->GetBoneContainer().GetPoseBoneIndexForBoneName(BoneElement->GetName());
				if (MeshIndex != INDEX_NONE)
				{
					FCompactPoseBoneIndex CPIndex = InCompactPose->GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
					if (CPIndex != INDEX_NONE)
					{
						FTransform LocalInitialTransform = InCompactPose->GetRefPose(CPIndex);
						DynamicHierarchy->SetTransform(BoneElement, LocalInitialTransform, ERigTransformType::InitialLocal, true, false);
					}
				}
			}
			return true;
		});

	bResetInitialTransformsBeforeConstruction = false;
	RequestConstruction();
}

const FRigControlElementCustomization* UControlRig::GetControlCustomization(const FRigElementKey& InControl) const
{
	check(InControl.Type == ERigElementType::Control);

	if(const FRigControlElementCustomization* Customization = ControlCustomizations.Find(InControl))
	{
		return Customization;
	}

	if(DynamicHierarchy)
	{
		if(const FRigControlElement* ControlElement = DynamicHierarchy->Find<FRigControlElement>(InControl))
		{
			return &ControlElement->Settings.Customization;
		}
	}

	return nullptr;
}

void UControlRig::SetControlCustomization(const FRigElementKey& InControl, const FRigControlElementCustomization& InCustomization)
{
	check(InControl.Type == ERigElementType::Control);
	
	ControlCustomizations.FindOrAdd(InControl) = InCustomization;
}

void UControlRig::PostInitInstanceIfRequired()
{
	if(GetHierarchy() == nullptr || VM == nullptr)
	{
		if(HasAnyFlags(RF_ClassDefaultObject))
		{
			PostInitInstance(nullptr);
		}
		else
		{
			UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>();
			PostInitInstance(CDO);
		}
	}
}

void UControlRig::SwapVMToNativizedIfRequired(UClass* InNativizedClass)
{
	if (HasAnyFlags(RF_NeedPostLoad))
	{
		return;
	}
	if(VM == nullptr)
	{
		return;
	}

	const bool bNativizedVMDisabled = AreNativizedVMsDisabled();

	// GetNativizedClass can be pretty costly, let's try to skip this if it is not absolutely necessary
	if(InNativizedClass == nullptr && !bNativizedVMDisabled)
	{
		if(!HasAnyFlags(RF_ClassDefaultObject))
		{
			if(UControlRig* CDO = GetClass()->GetDefaultObject<UControlRig>())
			{
				if(CDO->VM)
				{
					if(CDO->VM->IsNativized())
					{
						InNativizedClass = CDO->VM->GetClass();
					}
					else
					{
						InNativizedClass = CDO->VM->GetNativizedClass(GetExternalVariables());
					}
				}
			}
		}
		else
		{
			InNativizedClass = VM->GetNativizedClass(GetExternalVariablesImpl(true));
		}
	}	

	if(VM->IsNativized())
	{
		if((InNativizedClass == nullptr) || bNativizedVMDisabled)
		{
			const EObjectFlags PreviousFlags = VM->GetFlags();
			VM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			VM->MarkAsGarbage();
			VM = NewObject<URigVM>(this, TEXT("VM"), PreviousFlags);
#if UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM
			ProfilingRunsLeft = 0;
			AccumulatedCycles = 0;
#endif
		}
	}
	else
	{
		if(InNativizedClass && !bNativizedVMDisabled)
		{
			const EObjectFlags PreviousFlags = VM->GetFlags();
			VM->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			VM->MarkAsGarbage();
			VM = NewObject<URigVM>(this, InNativizedClass, TEXT("VM"), PreviousFlags);
			VM->ExecutionReachedExit().AddUObject(this, &UControlRig::HandleExecutionReachedExit);
#if UE_CONTROLRIG_PROFILE_EXECUTE_UNITS_NUM
			ProfilingRunsLeft = 0;
			AccumulatedCycles = 0;
#endif
		}
	}
}

bool UControlRig::AreNativizedVMsDisabled()
{
	return (CVarControlRigDisableNativizedVMs->GetInt() != 0);
}

#if WITH_EDITORONLY_DATA
void UControlRig::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(URigVM::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(URigHierarchy::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(UAnimationDataSourceRegistry::StaticClass()));
}
#endif

void UControlRig::PostInitInstance(UControlRig* InCDO)
{
	const EObjectFlags SubObjectFlags =
	HasAnyFlags(RF_ClassDefaultObject) ?
		RF_Public | RF_DefaultSubObject :
		RF_Transient | RF_Transactional;

	// set up the VM
	VM = NewObject<URigVM>(this, TEXT("VM"), SubObjectFlags);

	// Cooked platforms will load these pointers from disk.
	// In certain scenarios RequiresCookedData wil be false but the PKG_FilterEditorOnly will still be set (UEFN)
	if (!FPlatformProperties::RequiresCookedData() && !GetClass()->RootPackageHasAnyFlags(PKG_FilterEditorOnly))
	{
		VM->GetMemoryByType(ERigVMMemoryType::Work, true);
		VM->GetMemoryByType(ERigVMMemoryType::Literal, true);
		VM->GetMemoryByType(ERigVMMemoryType::Debug, true);
	}

	VM->ExecutionReachedExit().AddUObject(this, &UControlRig::HandleExecutionReachedExit);
	UpdateVMSettings();

	// set up the hierarchy
	DynamicHierarchy = NewObject<URigHierarchy>(this, TEXT("DynamicHierarchy"), SubObjectFlags);

#if WITH_EDITOR
	const TWeakObjectPtr<UControlRig> WeakThis = this;
	DynamicHierarchy->OnUndoRedo().AddStatic(&UControlRig::OnHierarchyTransformUndoRedoWeak, WeakThis);
#endif

	if(!HasAnyFlags(RF_ClassDefaultObject) && InCDO)
	{
		InCDO->PostInitInstanceIfRequired();
		VM->CopyFrom(InCDO->GetVM());
		DynamicHierarchy->CopyHierarchy(InCDO->GetHierarchy());
	}
	else // we are the CDO
	{
		// for default objects we need to check if the CDO is rooted. specialized Control Rigs
		// such as the FK control rig may not have a root since they are part of a C++ package.

		// since the sub objects are created after the constructor
		// GC won't consider them part of the CDO, even if they have the sub object flags
		// so even if CDO is rooted and references these sub objects, 
		// it is not enough to keep them alive.
		// Hence, we have to add them to root here.
		if(GetClass()->IsNative())
		{
			VM->AddToRoot();
			DynamicHierarchy->AddToRoot();
		}

		// Clear the initialized VM snapshots
		InitializedVMSnapshots.Reset();
	}
}

uint32 UControlRig::GetHashForInitializeVMSnapShot()
{
	if(CachedMemoryHash == 0)
	{
		return 0;
	}
	
	uint32 Hash = GetHierarchy()->GetNameHash();

	const TArray<FRigVMExternalVariable> ExternalVariables = GetExternalVariablesImpl(false);

	Hash = HashCombine(Hash, GetTypeHash(ExternalVariables.Num()));

	for(const FRigVMExternalVariable& ExternalVariable : ExternalVariables)
	{
		Hash = HashCombine(ExternalVariable.GetTypeHash(), Hash);
	}

	Hash = HashCombine(CachedMemoryHash, Hash);

	return Hash;
}

UTransformableControlHandle* UControlRig::CreateTransformableControlHandle(
	UObject* InOuter,
	const FName& InControlName) const
{
	auto IsConstrainable = [this](const FName& InControlName)
	{
		const FRigControlElement* ControlElement = FindControl(InControlName);
		if (!ControlElement)
		{
			return false;
		}
		
		const FRigControlSettings& ControlSettings = ControlElement->Settings;
		if (ControlSettings.ControlType == ERigControlType::Bool ||
			ControlSettings.ControlType == ERigControlType::Float ||
			ControlSettings.ControlType == ERigControlType::Integer)
		{
			return false;
		}

		return true;
	};

	if (!IsConstrainable(InControlName))
	{
		return nullptr;
	}
	
	UTransformableControlHandle* CtrlHandle = NewObject<UTransformableControlHandle>(InOuter);
	ensure(CtrlHandle);
	CtrlHandle->SetFlags(RF_Transactional);
	CtrlHandle->ControlRig = this;
	CtrlHandle->ControlName = InControlName;
	CtrlHandle->RegisterDelegates();
	return CtrlHandle;
}

void UControlRig::OnHierarchyTransformUndoRedo(URigHierarchy* InHierarchy, const FRigElementKey& InKey, ERigTransformType::Type InTransformType, const FTransform& InTransform, bool bIsUndo)
{
	if(InKey.Type == ERigElementType::Control)
	{
		if(FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(InKey))
		{
			ControlModified().Broadcast(this, ControlElement, FRigControlModifiedContext(EControlRigSetKey::Never));
		}
	}
}

UControlRig::FPoseScope::FPoseScope(UControlRig* InControlRig, ERigElementType InFilter, const TArray<FRigElementKey>& InElements)
: ControlRig(InControlRig)
, Filter(InFilter)
{
	check(InControlRig);
	const TArrayView<const FRigElementKey> ElementView(InElements.GetData(), InElements.Num());
	CachedPose = InControlRig->GetHierarchy()->GetPose(false, InFilter, ElementView);
}

UControlRig::FPoseScope::~FPoseScope()
{
	check(ControlRig);

	ControlRig->GetHierarchy()->SetPose(CachedPose);
}

#if WITH_EDITOR

UControlRig::FTransientControlScope::FTransientControlScope(TObjectPtr<URigHierarchy> InHierarchy)
	:Hierarchy(InHierarchy)
{
	for (FRigControlElement* Control : Hierarchy->GetTransientControls())
	{
		FTransientControlInfo Info;
		Info.Name = Control->GetName();
		Info.Parent = Hierarchy->GetFirstParent(Control->GetKey());
		Info.Settings = Control->Settings;
		// preserve whatever value that was produced by this transient control at the moment
		Info.Value = Hierarchy->GetControlValue(Control->GetKey(),ERigControlValueType::Current);
		Info.OffsetTransform = Hierarchy->GetControlOffsetTransform(Control, ERigTransformType::CurrentLocal);
		Info.ShapeTransform = Hierarchy->GetControlShapeTransform(Control, ERigTransformType::CurrentLocal);
				
		SavedTransientControls.Add(Info);
	}
}

UControlRig::FTransientControlScope::~FTransientControlScope()
{
	if (URigHierarchyController* Controller = Hierarchy->GetController())
	{
		for (const FTransientControlInfo& Info : SavedTransientControls)
		{
			Controller->AddControl(
				Info.Name,
				Info.Parent,
				Info.Settings,
				Info.Value,
				Info.OffsetTransform,
				Info.ShapeTransform,
				false,
				false
			);
		}
	}
}

#endif
 
#undef LOCTEXT_NAMESPACE

